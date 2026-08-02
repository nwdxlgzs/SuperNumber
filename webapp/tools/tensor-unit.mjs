/* Pure-JS unit tests for SuperALFPU web tensor / Transformer primitives.
 * Mirrors algorithms in webapp/js/05-eval.js (no WASM required).
 * Run: node tools/tensor-unit.mjs
 */
"use strict";

let failed = 0;
function assert(cond, msg) {
  if (!cond) {
    failed++;
    console.error("FAIL:", msg);
  } else {
    console.log("ok:", msg);
  }
}
function almostEqual(a, b, eps, msg) {
  eps = eps == null ? 1e-9 : eps;
  const ok = Math.abs(a - b) <= eps;
  assert(ok, (msg || "almostEqual") + " got " + a + " expected " + b);
}
function arrClose(a, b, eps, msg) {
  assert(a.length === b.length, (msg || "len") + " len " + a.length + " vs " + b.length);
  for (let i = 0; i < a.length; i++) almostEqual(a[i], b[i], eps, (msg || "arr") + "[" + i + "]");
}

function asMat(t) {
  if (t.kind === "mat") return { kind: "mat", rows: t.rows, cols: t.cols, data: t.data.slice() };
  return { kind: "mat", rows: 1, cols: t.data.length, data: t.data.slice() };
}
function matMul(A0, B0) {
  let A = A0.kind === "mat" ? A0 : { kind: "mat", rows: 1, cols: A0.data.length, data: A0.data };
  let B = B0.kind === "mat" ? B0 : { kind: "mat", rows: B0.data.length, cols: 1, data: B0.data };
  if (A.cols !== B.rows) throw new Error("dim");
  const data = new Array(A.rows * B.cols).fill(0);
  for (let i = 0; i < A.rows; i++)
    for (let k = 0; k < A.cols; k++) {
      const aik = A.data[i * A.cols + k];
      for (let j = 0; j < B.cols; j++) data[i * B.cols + j] += aik * B.data[k * B.cols + j];
    }
  return { kind: "mat", rows: A.rows, cols: B.cols, data };
}
function transpose(A) {
  const data = new Array(A.rows * A.cols);
  for (let r = 0; r < A.rows; r++)
    for (let c = 0; c < A.cols; c++) data[c * A.rows + r] = A.data[r * A.cols + c];
  return { kind: "mat", rows: A.cols, cols: A.rows, data };
}
function softmaxRows(A) {
  const data = new Array(A.rows * A.cols);
  for (let r = 0; r < A.rows; r++) {
    let m = -Infinity;
    for (let c = 0; c < A.cols; c++) m = Math.max(m, A.data[r * A.cols + c]);
    let s = 0;
    for (let c = 0; c < A.cols; c++) {
      const e = Math.exp(A.data[r * A.cols + c] - m);
      data[r * A.cols + c] = e;
      s += e;
    }
    for (let c = 0; c < A.cols; c++) data[r * A.cols + c] /= s;
  }
  return { kind: "mat", rows: A.rows, cols: A.cols, data };
}
function layerNorm(A, eps) {
  const data = new Array(A.rows * A.cols);
  for (let r = 0; r < A.rows; r++) {
    let mean = 0;
    for (let c = 0; c < A.cols; c++) mean += A.data[r * A.cols + c];
    mean /= A.cols;
    let v = 0;
    for (let c = 0; c < A.cols; c++) {
      const d = A.data[r * A.cols + c] - mean;
      v += d * d;
    }
    v /= A.cols;
    const inv = 1 / Math.sqrt(v + eps);
    for (let c = 0; c < A.cols; c++) data[r * A.cols + c] = (A.data[r * A.cols + c] - mean) * inv;
  }
  return { kind: "mat", rows: A.rows, cols: A.cols, data };
}
function gelu(x) {
  const c = Math.sqrt(2 / Math.PI);
  const u = c * (x + 0.044715 * x * x * x);
  return 0.5 * x * (1 + Math.tanh(u));
}
function sdpa(Q, K, V, scale) {
  if (scale == null) scale = 1 / Math.sqrt(Q.cols);
  let scores = matMul(Q, transpose(K));
  scores = { kind: "mat", rows: scores.rows, cols: scores.cols, data: scores.data.map((x) => x * scale) };
  const W = softmaxRows(scores);
  return { out: matMul(W, V), W };
}
function residual(a, b) {
  return { kind: "mat", rows: a.rows, cols: a.cols, data: a.data.map((x, i) => x + b.data[i]) };
}
function sinPE(seq, dim) {
  const data = new Array(seq * dim);
  for (let pos = 0; pos < seq; pos++) {
    for (let i = 0; i < dim; i++) {
      const half = Math.floor(i / 2);
      const den = Math.pow(10000, (2 * half) / dim);
      const ang = pos / den;
      data[pos * dim + i] = (i % 2 === 0) ? Math.sin(ang) : Math.cos(ang);
    }
  }
  return { kind: "mat", rows: seq, cols: dim, data };
}
function gather(table, indices) {
  const data = [];
  for (const r of indices) {
    for (let c = 0; c < table.cols; c++) data.push(table.data[r * table.cols + c]);
  }
  return { kind: "mat", rows: indices.length, cols: table.cols, data };
}

/* ---- tests ---- */
{
  const A = { kind: "mat", rows: 2, cols: 2, data: [1, 2, 3, 4] };
  const B = { kind: "mat", rows: 2, cols: 2, data: [5, 6, 7, 8] };
  const C = matMul(A, B);
  arrClose(C.data, [19, 22, 43, 50], 1e-12, "matmul");
}
{
  const S = softmaxRows({ kind: "mat", rows: 1, cols: 3, data: [1, 2, 3] });
  const sum = S.data.reduce((a, b) => a + b, 0);
  almostEqual(sum, 1, 1e-12, "softmax sum");
  assert(S.data[2] > S.data[1] && S.data[1] > S.data[0], "softmax order");
}
{
  const X = { kind: "mat", rows: 1, cols: 4, data: [1, 2, 3, 4] };
  const Y = layerNorm(X, 1e-5);
  const mean = Y.data.reduce((a, b) => a + b, 0) / 4;
  almostEqual(mean, 0, 1e-6, "layernorm mean~0");
  const v = Y.data.reduce((a, b) => a + b * b, 0) / 4;
  almostEqual(v, 1, 1e-4, "layernorm var~1");
}
{
  almostEqual(gelu(0), 0, 1e-12, "gelu(0)");
  assert(gelu(1) > 0.8 && gelu(1) < 0.9, "gelu(1)~0.84");
}
{
  // Identity attention: Q=K=V = I => attention rows focus on self when scale large? use equal rows
  const Q = { kind: "mat", rows: 2, cols: 2, data: [1, 0, 0, 1] };
  const K = Q, V = { kind: "mat", rows: 2, cols: 2, data: [10, 20, 30, 40] };
  const { out, W } = sdpa(Q, K, V, 1);
  assert(out.rows === 2 && out.cols === 2, "sdpa shape");
  // row0 prefers key0 (dot 1 vs 0)
  assert(W.data[0] > W.data[1], "attn row0 prefers self");
  // out row0 closer to V row0
  assert(Math.abs(out.data[0] - 10) < Math.abs(out.data[0] - 30), "sdpa out row0 ~ V0");
}
{
  const pe = sinPE(4, 8);
  assert(pe.rows === 4 && pe.cols === 8, "sin_pe shape");
  almostEqual(pe.data[0], 0, 1e-12, "sin_pe pos0 dim0 = sin0 = 0");
  almostEqual(pe.data[1], 1, 1e-12, "sin_pe pos0 dim1 = cos0 = 1");
}
{
  const table = { kind: "mat", rows: 3, cols: 2, data: [1, 2, 3, 4, 5, 6] };
  const g = gather(table, [2, 0]);
  arrClose(g.data, [5, 6, 1, 2], 0, "gather");
}
{
  // Tiny transformer block: x + SDPA(x,x,x) then LN then residual FFN-ish gelu
  const x = { kind: "mat", rows: 4, cols: 4, data: [
    0.1, 0.2, 0.3, 0.4,
    0.4, 0.3, 0.2, 0.1,
    0.0, 0.5, 0.0, 0.5,
    0.5, 0.0, 0.5, 0.0
  ]};
  const { out: attn } = sdpa(x, x, x, null);
  const h = residual(x, attn);
  const n = layerNorm(h, 1e-5);
  const ffn = { kind: "mat", rows: n.rows, cols: n.cols, data: n.data.map(gelu) };
  const y = residual(n, ffn);
  assert(y.rows === 4 && y.cols === 4, "toy transformer shape");
  assert(y.data.every((v) => Number.isFinite(v)), "toy transformer finite");
  // deterministic fingerprint
  const fp = y.data.reduce((a, b, i) => a + b * (i + 1), 0);
  console.log("toy transformer fingerprint:", fp);
  assert(Math.abs(fp) > 0.01, "fingerprint nonzero");
}

if (failed) {
  console.error("\n" + failed + " test(s) failed");
  process.exit(1);
}
console.log("\nAll tensor unit tests passed.");