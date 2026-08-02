/* SuperNumber Lab front-end (rebuilt). No global `$`. Module + single-file. */
(function () {
  "use strict";

  /* ---------- Op catalogs (aligned with sn_web_bridge / sn_api) ---------- */
  const CATALOG = {
    const: [
      { type: "const_f", label: "浮点常量", hint: "任意 E/M + NaN" },
      { type: "const_i", label: "定宽整数", hint: "位宽 + 有/无符号" },
      { type: "const_bi", label: "大整数", hint: "无限精度 / 科学计数法" },
      { type: "const_c", label: "复数常量", hint: "re,im 或 a+bi" },
      { type: "out", label: "显示输出", hint: "打印结果" }
    ],
    arith: [
      { type: "bin", op: "add", label: "a + b" },
      { type: "bin", op: "sub", label: "a − b" },
      { type: "bin", op: "mul", label: "a × b" },
      { type: "bin", op: "div", label: "a ÷ b" },
      { type: "bin", op: "rem", label: "rem / fmod" },
      { type: "un", op: "neg", label: "−a" },
      { type: "un", op: "abs", label: "|a|" },
      { type: "ter", op: "fma", label: "fma(a,b,c)" },
      { type: "cast", op: "to_float", label: "cast→float" },
      { type: "cast", op: "to_int", label: "cast→int" },
      { type: "copy", op: "copy", label: "赋值/拷贝", hint: "sn_value 拷贝 (out = src)" },
      { type: "assign", op: "assign", label: "赋值传递", hint: "值拷贝，可串数据流" }
    ],
    math: [
      { type: "un", op: "sin", label: "sin" }, { type: "un", op: "cos", label: "cos" },
      { type: "un", op: "tan", label: "tan" }, { type: "un", op: "asin", label: "asin" },
      { type: "un", op: "acos", label: "acos" }, { type: "un", op: "atan", label: "atan" },
      { type: "bin", op: "atan2", label: "atan2(y,x)" },
      { type: "un", op: "sinh", label: "sinh" }, { type: "un", op: "cosh", label: "cosh" },
      { type: "un", op: "tanh", label: "tanh" }, { type: "un", op: "asinh", label: "asinh" },
      { type: "un", op: "acosh", label: "acosh" }, { type: "un", op: "atanh", label: "atanh" },
      { type: "un", op: "exp", label: "exp" }, { type: "un", op: "exp2", label: "exp2" },
      { type: "un", op: "expm1", label: "expm1" }, { type: "un", op: "log", label: "log" },
      { type: "un", op: "log2", label: "log2" }, { type: "un", op: "log10", label: "log10" },
      { type: "un", op: "log1p", label: "log1p" }, { type: "bin", op: "pow", label: "pow" },
      { type: "un", op: "sqrt", label: "sqrt" }, { type: "un", op: "cbrt", label: "cbrt" },
      { type: "bin", op: "hypot", label: "hypot" },
      { type: "un", op: "ceil", label: "ceil" }, { type: "un", op: "floor", label: "floor" },
      { type: "un", op: "trunc", label: "trunc" }, { type: "un", op: "rint", label: "rint" },
      { type: "un", op: "nearbyint", label: "nearbyint" },
      { type: "un", op: "erf", label: "erf" }, { type: "un", op: "erfc", label: "erfc" },
      { type: "un", op: "tgamma", label: "tgamma" }, { type: "un", op: "lgamma", label: "lgamma" },
      { type: "un", op: "digamma", label: "digamma" }, { type: "un", op: "trigamma", label: "trigamma" },
      { type: "bin", op: "fmin", label: "fmin" }, { type: "bin", op: "fmax", label: "fmax" },
      { type: "bin", op: "fdim", label: "fdim" },
      { type: "bin", op: "copysign", label: "copysign" }, { type: "bin", op: "nextafter", label: "nextafter" }
    ],
    bit: [
      { type: "bin", op: "and", label: "and" }, { type: "bin", op: "or", label: "or" },
      { type: "bin", op: "xor", label: "xor" }, { type: "un", op: "not", label: "not" },
      { type: "shift", op: "shl", label: "shl (左移)" },
      { type: "shift", op: "shr", label: "shr (逻辑右移)" },
      { type: "shift", op: "sar", label: "sar (算术右移)" },
      { type: "crypto", op: "gcd", label: "gcd", arity: 2 },
      { type: "crypto", op: "lcm", label: "lcm", arity: 2 },
      { type: "crypto", op: "isqrt", label: "isqrt", arity: 1 },
      { type: "crypto", op: "popcount", label: "popcount", arity: 1 },
      { type: "crypto", op: "ctz", label: "ctz", arity: 1 },
      { type: "crypto", op: "modinv", label: "modinv", arity: 2 },
      { type: "crypto", op: "mulmod", label: "mulmod", arity: 3 },
      { type: "crypto", op: "powmod", label: "powmod", arity: 3 },
      { type: "crypto", op: "powmod_ct", label: "powmod_ct", arity: 3 }
    ],
    spec: [
      { type: "un", op: "j0", label: "j0" }, { type: "un", op: "j1", label: "j1" },
      { type: "un", op: "y0", label: "y0" }, { type: "un", op: "y1", label: "y1" },
      { type: "un", op: "i0", label: "i0" }, { type: "un", op: "i1", label: "i1" },
      { type: "un", op: "k0", label: "k0" }, { type: "un", op: "k1", label: "k1" },
      { type: "un", op: "ellipk", label: "K(m)" }, { type: "un", op: "ellipe", label: "E(m)" },
      { type: "bin", op: "ellipf", label: "F(φ|m)" }, { type: "bin", op: "ellipeinc", label: "E(φ|m)" },
      { type: "ter", op: "ellipiinc", label: "Π(n;φ|m)" },
      { type: "bin", op: "igamma", label: "igamma P" }, { type: "bin", op: "igammac", label: "igammac Q" },
      { type: "bin", op: "jacobi_sn", label: "sn(u|m)" }, { type: "bin", op: "jacobi_cn", label: "cn(u|m)" },
      { type: "bin", op: "jacobi_dn", label: "dn(u|m)" },
      { type: "ter", op: "ibeta", label: "ibeta" }, { type: "ter", op: "ibetac", label: "ibetac" }
    ],
    cplx: [
      { type: "cbin", op: "add", label: "c +" }, { type: "cbin", op: "sub", label: "c −" },
      { type: "cbin", op: "mul", label: "c ×" }, { type: "cbin", op: "div", label: "c ÷" },
      { type: "cbin", op: "pow", label: "cpow" },
      { type: "cun", op: "neg", label: "cneg" }, { type: "cun", op: "conj", label: "conj" },
      { type: "cun", op: "proj", label: "proj" }, { type: "cun", op: "sqrt", label: "csqrt" },
      { type: "cun", op: "exp", label: "cexp" }, { type: "cun", op: "log", label: "clog" },
      { type: "cun", op: "sin", label: "csin" }, { type: "cun", op: "cos", label: "ccos" },
      { type: "cun", op: "tan", label: "ctan" }, { type: "cun", op: "sinh", label: "csinh" },
      { type: "cun", op: "cosh", label: "ccosh" }, { type: "cun", op: "tanh", label: "ctanh" },
      { type: "cun", op: "asin", label: "casin" }, { type: "cun", op: "acos", label: "cacos" },
      { type: "cun", op: "atan", label: "catan" }, { type: "cun", op: "asinh", label: "casinh" },
      { type: "cun", op: "acosh", label: "cacosh" }, { type: "cun", op: "atanh", label: "catanh" },
      { type: "c2r", op: "re", label: "Re" }, { type: "c2r", op: "im", label: "Im" },
      { type: "c2r", op: "abs", label: "|z|" }, { type: "c2r", op: "arg", label: "arg" },
      { type: "r2c", op: "set_reim", label: "re+im·i" },
      { type: "r2c", op: "from_polar", label: "ρ∠θ" }
    ],
    flow: [
      /* UE-style exec control flow */
      { type: "entry", op: "entry", label: "Entry" },
      { type: "branch", op: "branch", label: "Branch / If" },
      { type: "while", op: "while", label: "While Loop" },
      { type: "for", op: "for", label: "For Loop" },
      { type: "sequence", op: "sequence", label: "Sequence" },
      { type: "do_once", op: "do_once", label: "DoOnce" },
      { type: "do_n", op: "do_n", label: "DoN" },
      { type: "gate", op: "gate", label: "Gate" },
      { type: "flip_flop", op: "flip_flop", label: "FlipFlop" },
      { type: "multi_gate", op: "multi_gate", label: "MultiGate" },
      { type: "switch", op: "switch", label: "Switch" },
      { type: "break", op: "break", label: "Break Loop" },
      { type: "continue", op: "continue", label: "Continue Loop" },
      { type: "reroute", op: "reroute", label: "Reroute (exec)" },
      { type: "do_while", op: "do_while", label: "DoWhile" },
      { type: "select_exec", op: "select_exec", label: "Select (exec)" },
      { type: "comment", op: "comment", label: "注释 / Comment" },
      { type: "set_var", op: "set", label: "Set var" },
      { type: "get_var", op: "get", label: "Get var" },
      { type: "print", op: "print", label: "Print" },
      { type: "out", op: "out", label: "Output (Print+exec)" },
      /* pure data select / compare / logic */
      { type: "cmp", op: "eq", label: "a == b" },
      { type: "cmp", op: "ne", label: "a != b" },
      { type: "cmp", op: "lt", label: "a < b" },
      { type: "cmp", op: "le", label: "a <= b" },
      { type: "cmp", op: "gt", label: "a > b" },
      { type: "cmp", op: "ge", label: "a >= b" },
      { type: "select", op: "select", label: "select(cond,then,else)" },
      { type: "logic", op: "and", label: "and" },
      { type: "logic", op: "or", label: "or" },
      { type: "logic", op: "xor", label: "xor" },
      { type: "logic", op: "not", label: "not" },
      { type: "logic", op: "nand", label: "nand" },
      { type: "logic", op: "nor", label: "nor" },
      { type: "logic", op: "xnor", label: "xnor" },
      { type: "logic", op: "implies", label: "implies (a→b)" },
      { type: "const_bool", op: "true", label: "true (1)" },
      { type: "const_bool", op: "false", label: "false (0)" },
      { type: "rng", op: "u64", label: "random u64" },
      { type: "rng", op: "u64_mod", label: "random [0,n)" },
      { type: "rng", op: "seed", label: "seed RNG" },
      { type: "cast", op: "to_float", label: "cast → float" },
      { type: "cast", op: "to_int", label: "cast → int" }
    ],
    data: [
      { type: "arr_lit", label: "Make Array", hint: "分号/逗号/空格分隔元素；或 JSON [..]" },
      { type: "vec_lit", label: "Make Vector", hint: "一维向量字面量" },
      { type: "mat_lit", label: "Make Matrix", hint: "行用 ; 分隔，列用 , 分隔" },
      { type: "arr_len", label: "Len", hint: "数组/向量长度" },
      { type: "arr_get", label: "Get [i]", hint: "按索引取元素" },
      { type: "arr_set", label: "Set [i]=v", hint: "返回新数组（不可变）" },
      { type: "arr_push", label: "Push", hint: "追加元素 → 新数组" },
      { type: "arr_slice", label: "Slice", hint: "slice(start,end) 半开区间" },
      { type: "arr_concat", label: "Concat", hint: "连接两个数组/向量" },
      { type: "arr_map", label: "Map +const", hint: "每个元素 + 标量（可改 op）" },
      { type: "arr_reduce", label: "Reduce sum", hint: "求和 / 积" },
      { type: "vec_dot", label: "Dot", hint: "向量点积 a·b" },
      { type: "vec_scale", label: "Scale", hint: "向量 × 标量" },
      { type: "vec_add", label: "Vec add", hint: "逐元相加" },
      { type: "mat_mul", label: "MatMul", hint: "矩阵乘法 A×B" },
      { type: "mat_dims", label: "Mat dims", hint: "输出 rows/cols（rows 在 out）" },
      { type: "mat_get", label: "Mat get", hint: "A[r,c]" },
      { type: "mat_set", label: "Mat set", hint: "返回新矩阵" },
      { type: "mat_transpose", label: "Transpose", hint: "Aᵀ" },
      { type: "mat_identity", label: "Identity", hint: "n×n 单位阵" },
      { type: "arr_from_range", label: "Range", hint: "整数序列 first..last" },
      /* elementwise / reshape / ML primitives (web tensor runtime) */
      { type: "mat_add", label: "Mat add", hint: "逐元 A+B（可广播）" },
      { type: "mat_sub", label: "Mat sub", hint: "逐元 A−B" },
      { type: "mat_hadamard", label: "Hadamard", hint: "逐元 A⊙B" },
      { type: "mat_scale", label: "Mat scale", hint: "矩阵/向量 × 标量" },
      { type: "ten_unary", label: "Ten unary", hint: "逐元 exp/relu/gelu/tanh/…" },
      { type: "ten_binop", label: "Ten binop", hint: "逐元 add/sub/mul/div" },
      { type: "mat_row_sum", label: "Row sum", hint: "按行求和 → 列向量" },
      { type: "mat_row_mean", label: "Row mean", hint: "按行均值" },
      { type: "mat_row_max", label: "Row max", hint: "按行最大值" },
      { type: "softmax_row", label: "Softmax row", hint: "按行 softmax（数值稳定）" },
      { type: "layer_norm", label: "LayerNorm", hint: "末维归一化 + γ/β 可选" },
      { type: "rms_norm", label: "RMSNorm", hint: "RMS 归一化" },
      { type: "reshape", label: "Reshape", hint: "改形状，元素总数不变" },
      { type: "flatten", label: "Flatten", hint: "矩阵/向量 → 一维" },
      { type: "mat_concat", label: "Mat concat", hint: "沿 axis 拼接" },
      { type: "mat_slice", label: "Mat slice", hint: "子矩阵 [r0:r1, c0:c1)" },
      { type: "mat_outer", label: "Outer", hint: "外积 a bᵀ" },
      { type: "mat_diag", label: "Diag", hint: "向量→对角 / 矩阵→对角" },
      { type: "gather", label: "Gather", hint: "按索引从表中取行（embedding）" },
      { type: "embedding", label: "Embedding", hint: "token ids → 查表（同 gather）" },
      { type: "sin_pe", label: "Sin PE", hint: "正弦位置编码 [seq×d]" },
      { type: "attention_sdp", label: "SDPA", hint: "scaled dot-product attention" },
      { type: "mha_split", label: "MHA split", hint: "[seq×d] 按 head 拆分再水平拼" },
      { type: "mha_merge", label: "MHA merge", hint: "多 head 拼回 [seq×d]" },
      { type: "gelu", label: "GELU", hint: "GELU 激活" },
      { type: "relu", label: "ReLU", hint: "max(0,x) 逐元" },
      { type: "silu", label: "SiLU", hint: "x·sigmoid(x) / Swish（Qwen3 FFN）" },
      { type: "swiglu", label: "SwiGLU", hint: "SiLU(gate)⊙up（门控 FFN）" },
      { type: "rope", label: "RoPE", hint: "旋转位置编码（Qwen3 用）" },
      { type: "residual_add", label: "Residual +", hint: "x + y（残差）" }
    ]
  };

  const LABELS = {
    digamma: "ψ digamma", trigamma: "ψ₁ trigamma",
    ellipk: "K(m)", ellipe: "E(m)", ellipf: "F(φ|m)", ellipeinc: "E(φ|m)",
    ellipiinc: "Π(n;φ|m)", igamma: "P(a,x)", igammac: "Q(a,x)",
    jacobi_sn: "sn(u|m)", jacobi_cn: "cn(u|m)", jacobi_dn: "dn(u|m)",
    ibeta: "Iₓ(a,b)", ibetac: "1−Iₓ", set_reim: "re+im·i", from_polar: "ρ∠θ",
    re: "Re", im: "Im", abs: "|·|", arg: "arg",
    shl: "shl", shr: "shr", sar: "sar",
    to_float: "→float", to_int: "→int",
    copy: "Copy", assign: "Assign",
    gcd: "gcd", lcm: "lcm", isqrt: "isqrt", popcount: "popcount", ctz: "ctz", modinv: "modinv", mulmod: "mulmod", powmod: "powmod", powmod_ct: "powmod_ct",
    eq: "==", ne: "!=", lt: "<", le: "<=", gt: ">", ge: ">=",
    select: "select", nand: "nand", nor: "nor", xnor: "xnor", implies: "⇒",
    entry: "Entry", branch: "Branch", while: "While", for: "For", sequence: "Sequence",
    do_once: "DoOnce", do_n: "DoN", gate: "Gate", flip_flop: "FlipFlop", multi_gate: "MultiGate",
    switch: "Switch", break: "Break", continue: "Continue", reroute: "Reroute",
    do_while: "DoWhile", select_exec: "SelectExec", comment: "注释",
    set: "Set", get: "Get", print: "Print", out: "Output",
    u64: "rand u64", u64_mod: "rand mod", seed: "seed RNG",
    true: "true", false: "false",
    arr_lit: "Array", vec_lit: "Vector", mat_lit: "Matrix",
    arr_len: "Len", arr_get: "Get", arr_set: "Set", arr_push: "Push",
    arr_slice: "Slice", arr_concat: "Concat", arr_map: "Map", arr_reduce: "Reduce",
    vec_dot: "Dot", vec_scale: "Scale", vec_add: "Vec+",
    mat_mul: "MatMul", mat_dims: "Dims", mat_get: "M[r,c]", mat_set: "M set",
    mat_transpose: "Aᵀ", mat_identity: "Iₙ", arr_from_range: "Range",
    mat_add: "A+B", mat_sub: "A−B", mat_hadamard: "A⊙B", mat_scale: "A×s",
    ten_unary: "f(·)", ten_binop: "a∘b", mat_row_sum: "Σrow", mat_row_mean: "μrow",
    mat_row_max: "maxrow", softmax_row: "softmax", layer_norm: "LayerNorm",
    rms_norm: "RMSNorm", reshape: "Reshape", flatten: "Flatten",
    mat_concat: "Concat", mat_slice: "Slice2D", mat_outer: "Outer", mat_diag: "Diag",
    gather: "Gather", embedding: "Embed", sin_pe: "SinPE", attention_sdp: "SDPA",
    mha_split: "MHA↦", mha_merge: "MHA↤", gelu: "GELU", relu: "ReLU",
    silu: "SiLU", swiglu: "SwiGLU", rope: "RoPE",
    residual_add: "x+y"
  };

  /* LABELS/CATALOG end — runtime continues in 02-runtime.js (same IIFE when assembled) */
let Module = null;
  let session = 0;
  let api = null;
  let nodeSeq = 1;
  let fnSeq = 1;
  let selectedId = null;
  let selectedIds = new Set();
  let selectedWire = null;
  let marquee = null; /* { x0,y0,x1,y1, pointerId } canvas coords */
  let linkFrom = null;
  let wireDraft = null; /* { fromId, fromPort, pointerId, x, y } */
  let drag = null; /* { id, ox, oy, pointerId } */
  let pan = null; /* { pointerId, lastX, lastY } */
  let spaceDown = false;
  let touchPan = null; /* { lastX, lastY } multi-touch pan */

  function closeDrawers() {
    const pal = $id("palette");
    const ins = $id("inspector");
    const bd = $id("drawerBackdrop");
    if (pal) pal.classList.remove("open");
    if (ins) ins.classList.remove("open");
    if (bd) { bd.classList.remove("open"); bd.hidden = true; }
  }
  function openDrawer(which) {
    const pal = $id("palette");
    const ins = $id("inspector");
    const bd = $id("drawerBackdrop");
    if (pal) pal.classList.toggle("open", which === "palette");
    if (ins) ins.classList.toggle("open", which === "inspector");
    if (bd) {
      const open = which === "palette" || which === "inspector";
      bd.classList.toggle("open", open);
      bd.hidden = !open;
    }
  }

  /* canvas pan/zoom (CSS transform on #world) */
  const view = { x: 0, y: 0, scale: 1 };
  const VIEW_SCALE_MIN = 0.15;
  const VIEW_SCALE_MAX = 4;
  const CANVAS_PAD = 480;
  const CANVAS_MIN_W = 2400;
  const CANVAS_MIN_H = 1600;
  let clipboard = null; /* cloned node template (no id/el/wires) */
  let ctxTarget = null; /* { kind:'node'|'wire'|'canvas', id?, wireIndex?, x, y } */
  let paletteTab = "all";
  let editScope = "main"; /* main | fn:<id> */

  /* project state */
  const project = {
    main: { nodes: [], wires: [] },
    functions: {} /* id -> { id, name, params: [name], nodes, wires } */
  };

  const $id = (id) => document.getElementById(id);
  const status = (t, kind) => {
    const s = $id("status");
    if (!s) return;
    s.textContent = t;
    s.classList.remove("err", "ok");
    if (kind === "err") s.classList.add("err");
    else if (kind === "ok") s.classList.add("ok");
  };
  const log = (t) => {
    const el = $id("log");
    if (!el) return;
    el.textContent += t + "\n";
    el.scrollTop = el.scrollHeight;
  };
  const clearLog = () => { const el = $id("log"); if (el) el.textContent = ""; };


  function escapeHtml(s) {
    return String(s == null ? "" : s)
      .replace(/&/g, "&amp;").replace(/</g, "&lt;").replace(/>/g, "&gt;")
      .replace(/"/g, "&quot;");
  }

  /* C highlighter for sn_api export preview */
  function highlightC(code) {
    const src = String(code == null ? "" : code);
    const KW = new Set([
      "auto","break","case","char","const","continue","default","do","double","else",
      "enum","extern","float","for","goto","if","inline","int","long","register",
      "restrict","return","short","signed","sizeof","static","struct","switch",
      "typedef","union","unsigned","void","volatile","while","_Bool","_Complex",
      "true","false","NULL"
    ]);
    const TYPES = new Set([
      "sn_ctx","sn_status","sn_value","sn_api","sn_op_opt","sn_float_fmt",
      "sn_cplx","sn_alloc","size_t","uint8_t","uint16_t","uint32_t","uint64_t",
      "int8_t","int16_t","int32_t","int64_t","ptrdiff_t","FILE"
    ]);
    let i = 0;
    let out = "";
    const n = src.length;
    const isIdStart = (c) => /[A-Za-z_]/.test(c);
    const isId = (c) => /[A-Za-z0-9_]/.test(c);
    const esc = (t) => escapeHtml(t);
    const span = (cls, t) => '<span class="tok ' + cls + '">' + esc(t) + "</span>";

    /* tokenize one "..." / '...' / <header.h> run starting at i */
    function takeQuoted(start, endQ, allowLt) {
      const q = src[start];
      let j = start + 1;
      while (j < n) {
        if (src[j] === "\\") { j += 2; continue; }
        if (src[j] === endQ) { j++; break; }
        if (src[j] === "\n") break;
        j++;
      }
      return j;
    }

    while (i < n) {
      const c = src[i];
      const c2 = src[i + 1];

      if (c === "/" && c2 === "/") {
        let j = i + 2;
        while (j < n && src[j] !== "\n") j++;
        out += span("tok-cm", src.slice(i, j));
        i = j;
        continue;
      }
      if (c === "/" && c2 === "*") {
        let j = i + 2;
        while (j < n && !(src[j] === "*" && src[j + 1] === "/")) j++;
        j = Math.min(n, j + 2);
        out += span("tok-cm", src.slice(i, j));
        i = j;
        continue;
      }

      /* preprocessor: keep directive as tok-pp, headers/strings as tok-str */
      if (c === "#") {
        let j = i + 1;
        while (j < n && /[ \t]/.test(src[j])) j++;
        while (j < n && isId(src[j])) j++;
        out += span("tok-pp", src.slice(i, j));
        i = j;
        while (i < n && src[i] !== "\n") {
          if (src[i] === "\\" && src[i + 1] === "\n") {
            out += esc(src.slice(i, i + 2));
            i += 2;
            continue;
          }
          if (src[i] === "/" && src[i + 1] === "/") {
            let k = i + 2;
            while (k < n && src[k] !== "\n") k++;
            out += span("tok-cm", src.slice(i, k));
            i = k;
            break;
          }
          if (src[i] === "/" && src[i + 1] === "*") {
            let k = i + 2;
            while (k < n && !(src[k] === "*" && src[k + 1] === "/")) k++;
            k = Math.min(n, k + 2);
            out += span("tok-cm", src.slice(i, k));
            i = k;
            continue;
          }
          if (src[i] === '"' || src[i] === "'") {
            const q = src[i];
            const end = takeQuoted(i, q, false);
            out += span("tok-str", src.slice(i, end));
            i = end;
            continue;
          }
          if (src[i] === "<") {
            let k = i + 1;
            while (k < n && src[k] !== ">" && src[k] !== "\n") k++;
            if (k < n && src[k] === ">") {
              k++;
              out += span("tok-str", src.slice(i, k));
              i = k;
              continue;
            }
          }
          if (isIdStart(src[i])) {
            let k = i + 1;
            while (k < n && isId(src[k])) k++;
            const word = src.slice(i, k);
            if (KW.has(word)) out += span("tok-kw", word);
            else out += span("tok-id", word);
            i = k;
            continue;
          }
          out += esc(src[i]);
          i++;
        }
        continue;
      }

      if (c === '"' || c === "'") {
        const q = c;
        let j = i + 1;
        while (j < n) {
          if (src[j] === "\\") { j += 2; continue; }
          if (src[j] === q) { j++; break; }
          if (src[j] === "\n") break;
          j++;
        }
        out += span("tok-str", src.slice(i, j));
        i = j;
        continue;
      }

      if (/[0-9]/.test(c) || (c === "." && /[0-9]/.test(c2 || ""))) {
        let j = i;
        if (src[j] === "0" && (src[j + 1] === "x" || src[j + 1] === "X")) {
          j += 2;
          while (j < n && /[0-9A-Fa-f]/.test(src[j])) j++;
        } else {
          while (j < n && /[0-9]/.test(src[j])) j++;
          if (src[j] === ".") {
            j++;
            while (j < n && /[0-9]/.test(src[j])) j++;
          }
          if (src[j] === "e" || src[j] === "E") {
            j++;
            if (src[j] === "+" || src[j] === "-") j++;
            while (j < n && /[0-9]/.test(src[j])) j++;
          }
        }
        while (j < n && /[uUlLfF]/.test(src[j])) j++;
        out += span("tok-num", src.slice(i, j));
        i = j;
        continue;
      }

      if (isIdStart(c)) {
        let j = i + 1;
        while (j < n && isId(src[j])) j++;
        const word = src.slice(i, j);
        let k = j;
        while (k < n && (src[k] === " " || src[k] === "\t")) k++;
        const isCall = src[k] === "(";
        if (KW.has(word)) out += span("tok-kw", word);
        else if (TYPES.has(word)) out += span("tok-type", word);
        else if (/^SN_[A-Z0-9_]+$/.test(word)) out += span("tok-macro", word);
        else if (isCall || /^sn_|^snlab_|^api$/.test(word)) out += span("tok-fn", word);
        else out += span("tok-id", word);
        i = j;
        continue;
      }

      if (/[{}()\[\];,.]/.test(c)) {
        out += span("tok-punc", c);
        i++;
        continue;
      }
      if (/[=+\-*/%&|^!~<>?:]/.test(c)) {
        let j = i + 1;
        while (j < n && /[=+\-*/%&|^!~<>?:]/.test(src[j])) j++;
        out += span("tok-op", src.slice(i, j));
        i = j;
        continue;
      }

      if (c === "\n" || c === "\r" || c === "\t" || c === " ") {
        out += c;
        i++;
        continue;
      }
      out += esc(c);
      i++;
    }
    return out;
  }

  function renderCPreview(code) {
    const prev = $id("cprev");
    if (!prev) return;
    const text = String(code == null ? "" : code);
    prev.classList.add("code", "hl");
    prev.setAttribute("data-raw", text);
    prev.innerHTML = highlightC(text);
  }

  const THEME_KEY = "snlab-theme";

  function currentTheme() {
    const t = document.documentElement.getAttribute("data-theme");
    return t === "light" ? "light" : "dark";
  }

  function updateThemeToggleUi(theme) {
    const btn = $id("btnTheme");
    if (!btn) return;
    const light = theme === "light";
    btn.setAttribute("aria-pressed", light ? "true" : "false");
    btn.setAttribute("title", light ? "切换为深色主题" : "切换为浅色主题");
    btn.setAttribute("aria-label", light ? "切换为深色主题" : "切换为浅色主题");
    const icon = btn.querySelector(".theme-icon");
    const label = btn.querySelector(".theme-label");
    if (icon) icon.textContent = light ? "\u2600" : "\u263E";
    if (label) label.textContent = light ? "浅色" : "深色";
  }

  function applyTheme(theme, opts) {
    const next = theme === "light" ? "light" : "dark";
    const animated = !opts || opts.animated !== false;
    const root = document.documentElement;
    const prev = currentTheme();
    if (prev === next && root.hasAttribute("data-theme")) {
      updateThemeToggleUi(next);
      return;
    }

    const finish = () => {
      root.setAttribute("data-theme", next);
      try { localStorage.setItem(THEME_KEY, next); } catch (_) {}
      updateThemeToggleUi(next);
      root.classList.remove("theme-switching");
    };

    if (!animated) {
      finish();
      return;
    }

    if (document.startViewTransition) {
      root.classList.add("theme-switching");
      const vt = document.startViewTransition(() => {
        root.setAttribute("data-theme", next);
      });
      vt.finished.finally(() => {
        try { localStorage.setItem(THEME_KEY, next); } catch (_) {}
        updateThemeToggleUi(next);
        root.classList.remove("theme-switching");
      });
      return;
    }

    root.classList.add("theme-switching");
    void root.offsetWidth;
    finish();
    window.setTimeout(() => root.classList.remove("theme-switching"), 380);
  }

  function initTheme() {
    let saved = null;
    try { saved = localStorage.getItem(THEME_KEY); } catch (_) {}
    if (saved !== "light" && saved !== "dark") {
      const prefersLight = window.matchMedia && window.matchMedia("(prefers-color-scheme: light)").matches;
      saved = prefersLight ? "light" : "dark";
    }
    applyTheme(saved, { animated: false });
  }

  function toggleTheme() {
    applyTheme(currentTheme() === "light" ? "dark" : "light", { animated: true });
  }

  function wrapApi() {
    const M = Module;
    const cwrap = M.cwrap.bind(M);
    return {
      create: cwrap("snw_create", "number", ["number", "number", "number"]),
      destroy: cwrap("snw_destroy", null, ["number"]),
      lastError: cwrap("snw_last_error", "string", ["number"]),
      setFormat: cwrap("snw_set_format", "number", ["number", "number", "number", "number"]),
      newValue: cwrap("snw_new_value", "number", ["number"]),
      freeValue: cwrap("snw_free_value", null, ["number", "number"]),
      setF64: cwrap("snw_set_f64", "number", ["number", "number", "number"]),
      setFromStr: cwrap("snw_set_from_str", "number", ["number", "number", "string", "number"]),
      setInt: cwrap("snw_set_int", "number", ["number", "number", "string", "number", "number", "number"]),
      setFloat: cwrap("snw_set_float", "number", ["number", "number", "string", "number", "number", "number"]),
      toStr: cwrap("snw_to_str", "number", ["number", "number", "number"]),
      freeStr: cwrap("snw_free_str", null, ["number"]),
      toF64: cwrap("snw_to_f64", "number", ["number", "number"]),
      unary: cwrap("snw_unary", "number", ["number", "number", "string", "number"]),
      binary: cwrap("snw_binary", "number", ["number", "number", "string", "number", "number"]),
      ternary: cwrap("snw_ternary", "number", ["number", "number", "string", "number", "number", "number"]),
      shift: cwrap("snw_shift", "number", ["number", "number", "string", "number", "number"]),
      crypto: cwrap("snw_crypto", "number", ["number", "number", "string", "number", "number", "number"]),
      copy: (typeof M._snw_copy === "function")
        ? cwrap("snw_copy", "number", ["number", "number", "number"])
        : null,
      cast: cwrap("snw_cast", "number", ["number", "number", "number", "string", "number", "number", "number"]),
      seedRng: cwrap("snw_seed_rng", "number", ["number", "number", "number"]),
      randomU64: cwrap("snw_random_u64", "number", ["number", "number"]),
      randomU64Mod: cwrap("snw_random_u64_mod", "number", ["number", "number", "number", "number"]),
      cmp: cwrap("snw_cmp", "number", ["number", "number", "number"]),
      version: cwrap("snw_version", "string", []),
      flags: cwrap("snw_flags", "number", ["number"]),
      clearFlags: cwrap("snw_clear_flags", null, ["number"]),
      newCplx: cwrap("snw_new_cplx", "number", ["number"]),
      freeCplx: cwrap("snw_free_cplx", null, ["number", "number"]),
      setCplxD: cwrap("snw_set_cplx_d", "number", ["number", "number", "number", "number"]),
      cplxSetReim: cwrap("snw_cplx_set_reim", "number", ["number", "number", "number", "number"]),
      cplxToStr: cwrap("snw_cplx_to_str", "number", ["number", "number"]),
      cplxUnary: cwrap("snw_cplx_unary", "number", ["number", "number", "string", "number"]),
      cplxBinary: cwrap("snw_cplx_binary", "number", ["number", "number", "string", "number", "number"]),
      cplxAbs: cwrap("snw_cplx_abs", "number", ["number", "number", "number"]),
      cplxArg: cwrap("snw_cplx_arg", "number", ["number", "number", "number"]),
      cplxRe: cwrap("snw_cplx_re", "number", ["number", "number", "number"]),
      cplxIm: cwrap("snw_cplx_im", "number", ["number", "number", "number"]),
      cplxFromPolar: cwrap("snw_cplx_from_polar", "number", ["number", "number", "number", "number"]),
      /* SN tensor (bridge 1.6+) — all ML matrix ops should go through these */
      newTensor: (typeof M._snw_new_tensor === "function")
        ? cwrap("snw_new_tensor", "number", ["number"]) : null,
      freeTensor: (typeof M._snw_free_tensor === "function")
        ? cwrap("snw_free_tensor", null, ["number", "number"]) : null,
      tensorFromStr: (typeof M._snw_tensor_from_str === "function")
        ? cwrap("snw_tensor_from_str", "number", ["number", "number", "string"]) : null,
      tensorToStr: (typeof M._snw_tensor_to_str === "function")
        ? cwrap("snw_tensor_to_str", "number", ["number", "number"]) : null,
      tensorDims: (typeof M._snw_tensor_dims === "function")
        ? cwrap("snw_tensor_dims", "number", ["number", "number", "number", "number"]) : null,
      tensorCopy: (typeof M._snw_tensor_copy === "function")
        ? cwrap("snw_tensor_copy", "number", ["number", "number", "number"]) : null,
      tensorUnary: (typeof M._snw_tensor_unary === "function")
        ? cwrap("snw_tensor_unary", "number", ["number", "number", "string", "number"]) : null,
      tensorBinary: (typeof M._snw_tensor_binary === "function")
        ? cwrap("snw_tensor_binary", "number", ["number", "number", "string", "number", "number"]) : null,
      tensorScale: (typeof M._snw_tensor_scale === "function")
        ? cwrap("snw_tensor_scale", "number", ["number", "number", "number", "number"]) : null,
      tensorRmsNorm: (typeof M._snw_tensor_rms_norm === "function")
        ? cwrap("snw_tensor_rms_norm", "number", ["number", "number", "number", "number", "number"]) : null,
      tensorLayerNorm: (typeof M._snw_tensor_layer_norm === "function")
        ? cwrap("snw_tensor_layer_norm", "number", ["number", "number", "number", "number", "number", "number"]) : null,
      tensorSinPe: (typeof M._snw_tensor_sin_pe === "function")
        ? cwrap("snw_tensor_sin_pe", "number", ["number", "number", "number", "number", "number"]) : null,
      tensorRope: (typeof M._snw_tensor_rope === "function")
        ? cwrap("snw_tensor_rope", "number", ["number", "number", "number", "number"]) : null,
      tensorReshape: (typeof M._snw_tensor_reshape === "function")
        ? cwrap("snw_tensor_reshape", "number", ["number", "number", "number", "number", "number"]) : null,
      tensorAttentionSdp: (typeof M._snw_tensor_attention_sdp === "function")
        ? cwrap("snw_tensor_attention_sdp", "number", ["number", "number", "number", "number", "number", "number", "number", "number"]) : null,
      tensorGetF64: (typeof M._snw_tensor_get_f64 === "function")
        ? cwrap("snw_tensor_get_f64", "number", ["number", "number", "number", "number", "number"]) : null,
      readCString(ptr) {
        if (!ptr) return "";
        const s = M.UTF8ToString(ptr);
        this.freeStr(ptr);
        return s;
      }
    };
  }

  function fmtBits() {
    return {
      e: parseInt($id("eBits").value, 10) || 11,
      m: parseInt($id("mBits").value, 10) || 52,
      nan: $id("nanEn").checked ? 1 : 0
    };
  }

  function ensureSession() {
    const f = fmtBits();
    if (session) api.destroy(session);
    session = api.create(f.e, f.m, f.nan);
    if (!session) throw new Error("snw_create failed");
  }

  function currentGraph() {
    if (editScope === "main") return project.main;
    const fid = editScope.slice(3);
    return project.functions[fid];
  }

  function nodes() { return currentGraph().nodes; }
  function wires() { return currentGraph().wires; }
  function setWires(w) { currentGraph().wires = w; }
  function setNodes(n) { currentGraph().nodes = n; }
  function nodeById(id) { return nodes().find((n) => n.id === id); }

  
  function formatResultPreview(text) {
    /* at most 2 lines; long lists: 1,2,3,...,7,8,9 (hover title = full) */
    const raw = String(text == null ? "" : text);
    if (!raw) return "";
    const s = raw.replace(/\r\n/g, "\n").replace(/\r/g, "\n").trim();
    const middleChars = (str, maxChars) => {
      const t = String(str || "");
      if (t.length <= maxChars) return t;
      const keep = Math.max(10, maxChars - 3);
      const head = Math.ceil(keep * 0.55);
      const tail = keep - head;
      return t.slice(0, head) + "..." + t.slice(t.length - tail);
    };
    const listMiddle = (str, maxTokens) => {
      const t = String(str || "").trim();
      if (!t) return t;
      if (t.indexOf(";") >= 0) {
        const rows = t.split(";").map((x) => x.trim()).filter(Boolean);
        if (rows.length <= 2) return rows.map((r) => listMiddle(r, maxTokens)).join("; ");
        return listMiddle(rows[0], maxTokens) + "; ...; " + listMiddle(rows[rows.length - 1], maxTokens);
      }
      const parts = t.split(",").map((x) => x.trim()).filter((x) => x.length);
      if (parts.length <= maxTokens) return parts.join(",");
      const headN = Math.max(2, Math.floor(maxTokens / 2));
      const tailN = Math.max(2, maxTokens - headN);
      return parts.slice(0, headN).join(",") + ",...," + parts.slice(parts.length - tailN).join(",");
    };
    const flat = s.replace(/\n+/g, "; ");
    const tokenish = flat.split(/[,;]/).filter((x) => x.trim().length);
    if (tokenish.length >= 6) return middleChars(listMiddle(flat, 8), 96);
    const lines = s.split("\n");
    if (lines.length <= 1) return middleChars(lines[0] || s, 96);
    return middleChars(lines[0], 90) + "\n" + middleChars(lines[lines.length - 1], 90);
  }
function selectNode(id, opts) {
    opts = opts || {};
    const additive = !!(opts.additive || opts.toggle);
    selectedWire = null;
    if (id == null) {
      selectedId = null;
      selectedIds.clear();
    } else if (additive) {
      if (selectedIds.has(id) && opts.toggle) {
        selectedIds.delete(id);
        selectedId = selectedIds.size ? Array.from(selectedIds).slice(-1)[0] : null;
      } else {
        selectedIds.add(id);
        selectedId = id;
      }
    } else {
      selectedIds.clear();
      selectedIds.add(id);
      selectedId = id;
    }
    nodes().forEach((n) => {
      if (n.el) n.el.classList.toggle("selected", selectedIds.has(n.id) || n.id === selectedId);
    });
    document.querySelectorAll("#wires path.selected").forEach((p) => p.classList.remove("selected"));
    renderInspector();
  }

  function clearSelection() {
    selectNode(null);
  }

  function selectedNodeList() {
    const list = [];
    selectedIds.forEach((id) => {
      const n = nodeById(id);
      if (n) list.push(n);
    });
    if (!list.length && selectedId) {
      const n = nodeById(selectedId);
      if (n) list.push(n);
    }
    return list;
  }

  function setSelection(ids) {
    selectedWire = null;
    selectedIds.clear();
    (ids || []).forEach((id) => { if (id != null) selectedIds.add(id); });
    selectedId = selectedIds.size ? Array.from(selectedIds).slice(-1)[0] : null;
    nodes().forEach((n) => {
      if (n.el) n.el.classList.toggle("selected", selectedIds.has(n.id));
    });
    document.querySelectorAll("#wires path.selected").forEach((p) => p.classList.remove("selected"));
    renderInspector();
  }

  function defaultCfg(type) {
    const f = fmtBits();
    if (type === "const_f") return { value: "1.0", e_bits: f.e, m_bits: f.m, nan: f.nan, portValues: {} };
    if (type === "const_i") return { value: "42", width: 32, signed: 1, base: 10, portValues: {} };
    if (type === "const_bi") return { value: "12345678901234567890", base: 10, portValues: {} };
    if (type === "const_c") return { value: "1,0.5", e_bits: f.e, m_bits: f.m, nan: f.nan, portValues: {} };
    if (type === "shift") return { bits: 1, portValues: {} };
    if (type === "cast") return { e_bits: f.e, m_bits: f.m, nan: f.nan, width: 32, signed: 1, portValues: {} };
    if (type === "rng") return { seed: "1", bound: "100", portValues: {} };
    if (type === "const_bool") return { value: "1", portValues: {} };
    if (type === "set_var" || type === "get_var") return { name: "x", portValues: {} };
    if (type === "for") return { first: "0", last: "10", portValues: {} };
    if (type === "sequence") return { count: 2, portValues: {} };
    if (type === "while") return { max_iter: "1000000", portValues: {} };
    if (type === "do_n") return { n: "1", portValues: {} };
    if (type === "gate") return { start_open: 1, portValues: {} };
    if (type === "multi_gate") return { count: 2, portValues: {} };
    if (type === "switch") return { cases: "0,1,2", portValues: {} };
    if (type === "do_while") return { max_iter: "1000000", portValues: {} };
    if (type === "select_exec") return { portValues: {} };
    if (type === "comment") return { text: "在此写注释…", portValues: {} };
    if (type === "arr_lit" || type === "vec_lit") return { value: "1, 2, 3, 4", portValues: {} };
    if (type === "mat_lit") return { value: "1,0; 0,1", rows: "2", cols: "2", portValues: {} };
    if (type === "arr_map") return { op: "add", portValues: {} };
    if (type === "arr_reduce") return { op: "sum", portValues: {} };
    if (type === "mat_identity") return { n: "3", portValues: {} };
    if (type === "arr_from_range") return { first: "0", last: "7", portValues: {} };
    if (type === "ten_unary") return { op: "exp", portValues: {} };
    if (type === "ten_binop") return { op: "add", portValues: {} };
    if (type === "reshape") return { rows: "2", cols: "2", portValues: {} };
    if (type === "mat_concat") return { axis: "0", portValues: {} };
    if (type === "mat_slice") return { r0: "0", r1: "-1", c0: "0", c1: "-1", portValues: {} };
    if (type === "layer_norm" || type === "rms_norm") return { eps: "1e-5", portValues: {} };
    if (type === "attention_sdp") return { scale: "auto", causal: 0, portValues: {} };
    if (type === "rope") return { base: "10000", portValues: {} };
    if (type === "silu" || type === "swiglu") return { portValues: {} };
    if (type === "sin_pe") return { seq: "4", dim: "8", base: "10000", portValues: {} };
    if (type === "mha_split" || type === "mha_merge") return { heads: "2", portValues: {} };
    return { portValues: {} };
  }

  function parseSwitchCases(n) {
    const raw = (n.cfg && n.cfg.cases != null) ? String(n.cfg.cases) : "0,1,2";
    return raw.split(/[,;\s]+/).map((x) => x.trim()).filter((x) => x.length).slice(0, 32);
  }

  function portsOf(n) {
    if (n.type.startsWith("const") || n.type === "fn_param" || n.type === "const_bool") return { in: [], out: ["out"] };
    if (n.type === "out") return { in: ["exec", "in"], out: ["exec"] };
    if (n.type === "fn_return") return { in: ["exec", "in"], out: [] };
    if (n.type === "un" || n.type === "cun" || n.type === "c2r" || n.type === "shift" || n.type === "cast" || n.type === "copy" || n.type === "assign")
      return { in: ["a"], out: ["out"] };
    if (n.type === "bin" || n.type === "cbin" || n.type === "r2c" || n.type === "cmp") return { in: ["a", "b"], out: ["out"] };
    if (n.type === "ter" || n.type === "select") return { in: ["a", "b", "c"], out: ["out"] };
    if (n.type === "logic") {
      if (n.op === "not") return { in: ["a"], out: ["out"] };
      return { in: ["a", "b"], out: ["out"] };
    }
    if (n.type === "rng") {
      if (n.op === "seed") return { in: ["a"], out: ["out"] };
      if (n.op === "u64_mod") return { in: ["a"], out: ["out"] };
      return { in: [], out: ["out"] };
    }
    if (n.type === "crypto") {
      let ar = n.arity;
      if (ar == null) {
        ar = (n.op === "isqrt" || n.op === "popcount" || n.op === "ctz") ? 1
          : (n.op === "mulmod" || n.op === "powmod" || n.op === "powmod_ct") ? 3 : 2;
      }
      if (ar <= 1) return { in: ["a"], out: ["out"] };
      if (ar <= 2) return { in: ["a", "b"], out: ["out"] };
      return { in: ["a", "b", "c"], out: ["out"] };
    }
    if (n.type === "fn_call") {
      const fn = project.functions[n.fnId];
      const names = (fn && fn.params) || [];
      /* UE-style: exec in/out + data params + out */
      return { in: ["exec"].concat(names.map((_, i) => "p" + i)), out: ["exec", "out"] };
    }
    /* ---- array / vector / matrix (web-side tensors) ---- */
    if (n.type === "arr_lit" || n.type === "vec_lit" || n.type === "mat_lit") return { in: [], out: ["out"] };
    if (n.type === "arr_len" || n.type === "mat_transpose") return { in: ["a"], out: ["out"] };
    if (n.type === "arr_get") return { in: ["a", "i"], out: ["out"] };
    if (n.type === "arr_set") return { in: ["a", "i", "v"], out: ["out"] };
    if (n.type === "arr_push") return { in: ["a", "v"], out: ["out"] };
    if (n.type === "arr_slice") return { in: ["a", "start", "end"], out: ["out"] };
    if (n.type === "arr_concat" || n.type === "vec_add" || n.type === "vec_dot" || n.type === "mat_mul")
      return { in: ["a", "b"], out: ["out"] };
    if (n.type === "arr_map" || n.type === "vec_scale") return { in: ["a", "s"], out: ["out"] };
    if (n.type === "arr_reduce") return { in: ["a"], out: ["out"] };
    if (n.type === "mat_dims") return { in: ["a"], out: ["out", "cols"] };
    if (n.type === "mat_get") return { in: ["a", "r", "c"], out: ["out"] };
    if (n.type === "mat_set") return { in: ["a", "r", "c", "v"], out: ["out"] };
    if (n.type === "mat_identity") return { in: ["n"], out: ["out"] };
    if (n.type === "arr_from_range") return { in: ["first", "last"], out: ["out"] };
    if (n.type === "mat_add" || n.type === "mat_sub" || n.type === "mat_hadamard" ||
        n.type === "ten_binop" || n.type === "residual_add" || n.type === "mat_outer")
      return { in: ["a", "b"], out: ["out"] };
    if (n.type === "mat_scale") return { in: ["a", "s"], out: ["out"] };
    if (n.type === "ten_unary" || n.type === "gelu" || n.type === "relu" ||
        n.type === "softmax_row" || n.type === "flatten" ||
        n.type === "mat_row_sum" || n.type === "mat_row_mean" || n.type === "mat_row_max" ||
        n.type === "mat_diag")
      return { in: ["a"], out: ["out"] };
    if (n.type === "layer_norm") return { in: ["a", "gamma", "beta"], out: ["out"] };
    if (n.type === "rms_norm") return { in: ["a", "gamma"], out: ["out"] };
    if (n.type === "reshape") return { in: ["a", "rows", "cols"], out: ["out"] };
    if (n.type === "mat_concat") return { in: ["a", "b"], out: ["out"] };
    if (n.type === "mat_slice") return { in: ["a", "r0", "r1", "c0", "c1"], out: ["out"] };
    if (n.type === "gather" || n.type === "embedding") return { in: ["table", "idx"], out: ["out"] };
    if (n.type === "sin_pe") return { in: ["seq", "dim"], out: ["out"] };
    if (n.type === "attention_sdp") return { in: ["q", "k", "v", "mask"], out: ["out", "weights"] };
    if (n.type === "rope") return { in: ["a"], out: ["out"] };
    if (n.type === "silu") return { in: ["a"], out: ["out"] };
    if (n.type === "swiglu") return { in: ["gate", "up"], out: ["out"] };
    if (n.type === "mha_split" || n.type === "mha_merge") return { in: ["a", "heads"], out: ["out"] };
    if (n.type === "entry") return { in: [], out: ["exec"] };
    if (n.type === "branch") return { in: ["exec", "cond"], out: ["then", "else"] };
    if (n.type === "while") return { in: ["exec", "cond"], out: ["body", "completed"] };
    if (n.type === "for") return { in: ["exec", "first", "last"], out: ["body", "index", "completed"] };
    if (n.type === "sequence") {
      const c = Math.max(2, Math.min(8, (n.cfg && n.cfg.count) | 0 || 2));
      const outs = [];
      for (let i = 0; i < c; i++) outs.push("then" + i);
      return { in: ["exec"], out: outs };
    }
    if (n.type === "do_once") return { in: ["exec", "reset"], out: ["completed", "closed"] };
    if (n.type === "do_n") return { in: ["exec", "n", "reset"], out: ["exit", "completed"] };
    if (n.type === "gate") return { in: ["enter", "open", "close", "toggle"], out: ["exit"] };
    if (n.type === "flip_flop") return { in: ["exec"], out: ["A", "B"] };
    if (n.type === "multi_gate") {
      const c = Math.max(2, Math.min(16, (n.cfg && n.cfg.count) | 0 || 2));
      const outs = [];
      for (let i = 0; i < c; i++) outs.push("out" + i);
      return { in: ["exec"], out: outs };
    }
    if (n.type === "switch") {
      const cases = parseSwitchCases(n);
      const outs = cases.map((_, i) => "case" + i);
      outs.push("default");
      return { in: ["exec", "sel"], out: outs };
    }
    if (n.type === "do_while") return { in: ["exec", "cond"], out: ["body", "completed"] };
    if (n.type === "select_exec") return { in: ["exec", "cond", "a", "b"], out: ["exec", "out"] };
    if (n.type === "comment") return { in: [], out: [] };
    if (n.type === "break" || n.type === "continue") return { in: ["exec"], out: [] };
    if (n.type === "reroute") return { in: ["exec"], out: ["exec"] };
    if (n.type === "set_var") return { in: ["exec", "value"], out: ["exec"] };
    if (n.type === "get_var") return { in: [], out: ["out"] };
    if (n.type === "print") return { in: ["exec", "value"], out: ["exec"] };
    return { in: [], out: [] };
  }


  /* ---- exec / fold helpers (must exist before graph render) ---- */

  function isFlowControlNode(n) {
    if (!n || !n.type) return false;
    switch (n.type) {
      case "entry": case "branch": case "while": case "for": case "sequence":
      case "do_once": case "do_n": case "gate": case "flip_flop": case "multi_gate":
      case "switch": case "break": case "continue": case "reroute":
      case "do_while": case "select_exec": case "comment":
      case "set_var": case "get_var": case "print": case "out":
      case "fn_return": case "fn_call":
        return true;
      default:
        return false;
    }
  }

  function isExecPort(port) {
    if (port == null || port === "") return false;
    const p = String(port);
    if (p === "exec" || p === "then" || p === "else" || p === "body" ||
        p === "completed" || p === "closed" || p === "exit" ||
        p === "A" || p === "B" || p === "default" ||
        p === "enter" || p === "open" || p === "close" || p === "toggle" ||
        p === "reset") return true;
    if (/^then\d+$/.test(p)) return true;
    if (/^out\d+$/.test(p)) return true;
    if (/^case\d+$/.test(p)) return true;
    return false;
  }

  function isExecPortOnNode(n, port) {
    if (!n || port == null) return isExecPort(port);
    const p = String(port);
    /* data outs that share names with flow pins */
    if (n.type === "mat_dims" && p === "cols") return false;
    if ((n.type === "for") && p === "index") return false;
    if ((n.type === "fn_call" || n.type === "out" || n.type === "print" ||
         n.type === "set_var" || n.type === "select_exec" || n.type === "fn_return") &&
        (p === "out" || p === "in" || p === "value" || p === "a" || p === "b" || p === "cond" ||
         p === "sel" || p === "first" || p === "last" || p === "n" || /^p\d+$/.test(p))) {
      if (p === "exec" || p === "then" || p === "else" || p === "body" ||
          p === "completed" || p === "closed" || p === "exit" ||
          p === "A" || p === "B" || p === "default" ||
          /^then\d+$/.test(p) || /^out\d+$/.test(p) || /^case\d+$/.test(p)) return true;
      if (p === "exec") return true;
      /* explicit data */
      if (p === "out" || p === "in" || p === "value" || p === "a" || p === "b" ||
          p === "cond" || p === "sel" || p === "first" || p === "last" || p === "n" ||
          /^p\d+$/.test(p)) return false;
    }
    if (n.type === "branch" || n.type === "while" || n.type === "for" ||
        n.type === "sequence" || n.type === "do_once" || n.type === "do_n" ||
        n.type === "gate" || n.type === "flip_flop" || n.type === "multi_gate" ||
        n.type === "switch" || n.type === "do_while" || n.type === "entry" ||
        n.type === "break" || n.type === "continue" || n.type === "reroute") {
      if (p === "cond" || p === "first" || p === "last" || p === "n" ||
          p === "sel" || p === "value" || p === "a" || p === "b" || p === "index") return false;
    }
    return isExecPort(p);
  }

  function portIsUsed(n, port, dir) {
    if (!n || port == null) return false;
    const g = (typeof currentGraph === "function") ? currentGraph() : null;
    const wires = (g && g.wires) || [];
    if (dir === "out") {
      return wires.some((w) => w.from === n.id && (w.fromPort || "out") === port);
    }
    /* default: input */
    if (n.inputs && n.inputs[port] != null && n.inputs[port] !== "") return true;
    return wires.some((w) => w.to === n.id && w.toPort === port);
  }

  function unusedDataPortCount(n) {
    if (!n) return 0;
    const ports = portsOf(n);
    let c = 0;
    (ports.in || []).forEach((p) => {
      if (isExecPortOnNode(n, p) || isExecPort(p)) return;
      if (!portIsUsed(n, p, "in")) c++;
    });
    (ports.out || []).forEach((p) => {
      if (isExecPortOnNode(n, p) || isExecPort(p)) return;
      /* folding mainly targets unused inputs; count unused outs lightly */
      if (!portIsUsed(n, p, "out")) { /* ignore pure outs for fold count */ }
    });
    return c;
  }

  function visiblePortsOf(n) {
    const all = portsOf(n);
    if (!n || !n.foldUnusedPorts) {
      return { in: (all.in || []).slice(), out: (all.out || []).slice() };
    }
    const vin = (all.in || []).filter((p) => {
      if (isExecPortOnNode(n, p) || isExecPort(p)) return true;
      return portIsUsed(n, p, "in");
    });
    const vout = (all.out || []).filter((p) => {
      if (isExecPortOnNode(n, p) || isExecPort(p)) return true;
      /* always show primary data out */
      if (p === "out" || p === "index" || p === "cols") return true;
      return portIsUsed(n, p, "out");
    });
    return { in: vin, out: vout };
  }

  function refreshCollapseHints() {
    const g = (typeof currentGraph === "function") ? currentGraph() : null;
    if (!g || !g.nodes) return;
    g.nodes.forEach((n) => {
      if (!n.el) return;
      const can = (unusedDataPortCount(n) > 0) || !!n.foldUnusedPorts || !!n.collapsed;
      n.el.classList.toggle("can-collapse", can);
      n.el.classList.toggle("ports-folded", !!n.foldUnusedPorts);
      n.el.classList.toggle("collapsed", !!n.collapsed);
      const btn = n.el.querySelector("[data-collapse]");
      if (btn) {
        btn.title = n.collapsed ? "expand node" :
          (n.foldUnusedPorts ? "show all ports" : "fold unused ports");
      }
    });
  }

  function setNodeCollapsed(n, collapsed) {
    if (!n) return;
    n.collapsed = !!collapsed;
    renderNode(n);
    refreshCollapseHints();
    drawWires();
  }

  function togglePortFold(n) {
    if (!n) return;
    if (n.collapsed) {
      n.collapsed = false;
      n.foldUnusedPorts = true;
    } else if (!n.foldUnusedPorts && unusedDataPortCount(n) > 0) {
      n.foldUnusedPorts = true;
    } else if (n.foldUnusedPorts) {
      n.foldUnusedPorts = false;
    } else {
      n.collapsed = true;
    }
    renderNode(n);
    refreshCollapseHints();
    drawWires();
  }

  function refreshNodeView(n, opts) {
    if (!n) return;
    opts = opts || {};
    const ports = portsOf(n);
    n.inputs = n.inputs || {};
    n.inputFromPorts = n.inputFromPorts || {};
    const g = currentGraph();
    g.wires = (g.wires || []).filter((w) => {
      if (w.to === n.id && ports.in.indexOf(w.toPort) < 0) return false;
      if (w.from === n.id) {
        const outs = ports.out || [];
        const fp = w.fromPort || "out";
        if (outs.indexOf(fp) < 0) return false;
      }
      return true;
    });
    ports.in.forEach((p) => { n.inputs[p] = null; n.inputFromPorts[p] = null; });
    (g.wires || []).forEach((w) => {
      if (w.to === n.id) {
        n.inputs[w.toPort] = w.from;
        n.inputFromPorts[w.toPort] = w.fromPort || "out";
      }
    });
    if (n.el && opts.soft) {
      /* if port set changed (e.g. sequence count), need full rebuild */
      const wantIn = (ports.in || []).join(",");
      const wantOut = (ports.out || []).join(",");
      const haveIn = Array.from(n.el.querySelectorAll(".port.in")).map((p) => p.dataset.port).join(",");
      const haveOut = Array.from(n.el.querySelectorAll(".port.out")).map((p) => p.dataset.port).join(",");
      /* also rebuild if inline-literal port set changed (wired/fold/literal) */
      const allIns = (ports.in || []).filter((p) => !(isExecPortOnNode(n, p) || isExecPort(p)));
      const wantLit = allIns.filter((p) => {
        const wired = (n.inputs && n.inputs[p] != null && n.inputs[p] !== "") ||
          (currentGraph().wires || []).some((w) => w.to === n.id && w.toPort === p);
        if (wired) return false;
        if (n.foldUnusedPorts && !portIsUsed(n, p, "in")) return false;
        return true;
      }).join(",");
      const haveLit = Array.from(n.el.querySelectorAll(".port-lit")).map((el) => el.getAttribute("data-port") || "").join(",");
      if (wantIn !== haveIn || wantOut !== haveOut || wantLit !== haveLit || !!n.collapsed !== n.el.classList.contains("collapsed")) {
        renderNode(n);
        drawWires();
        refreshCollapseHints();
      } else {
        const titleSpan = n.el.querySelector("header > span");
        if (titleSpan) {
          const tagHtml = (n.op && !String(n.type).startsWith("const"))
            ? '<span class="tag">' + escapeHtml(n.type) + "</span>"
            : "";
          titleSpan.innerHTML = escapeHtml(titleOf(n)) + tagHtml;
        }
        const meta = n.el.querySelector(".meta");
        if (meta) meta.textContent = metaOf(n) || "";
        const res = n.el.querySelector("[data-result]");
        if (res) {
          const full = n.result || "";
          res.setAttribute("title", full);
          res.setAttribute("data-full", full);
          res.textContent = (typeof formatResultPreview === "function")
            ? formatResultPreview(full) : full;
        }
        /* keep port-lit values in sync if present */
        n.el.querySelectorAll("input[data-port-lit]").forEach((inp) => {
          const p = inp.getAttribute("data-port-lit");
          const cur = (n.cfg && n.cfg.portValues && n.cfg.portValues[p] != null) ? String(n.cfg.portValues[p]) : "";
          if (document.activeElement !== inp && inp.value !== cur) inp.value = cur;
        });
        layoutPorts(n);
        drawWires();
        refreshCollapseHints();
      }
    } else {
      renderNode(n);
      if (!opts.skipInspector) renderInspector();
      drawWires();
      refreshCollapseHints();
    }
  }

  function truthySlot(slot) {
    if (slot == null || slot < 0) return false;
    const cs = api.readCString(api.toStr(session, slot, 10));
    return !(cs === "0" || cs === "-0" || cs === "" || String(cs).toLowerCase() === "nan");
  }

  function cloneSlot(slot, freelistR) {
    const o = allocReal();
    freelistR.push(o);
    if (api.copy) {
      must(api.copy(session, o, slot), "copy");
    } else {
      const ts = api.readCString(api.toStr(session, slot, 10));
      must(api.setFromStr(session, o, ts, 0), "clone");
    }
    return o;
  }

  function isComplexNode(n) {
    return n && (n.type === "const_c" || n.type === "cun" || n.type === "cbin" || n.type === "r2c");
  }

  function isTensorNode(n) {
    if (!n || !n.type) return false;
    if (/^(arr_|vec_|mat_)/.test(n.type)) return true;
    switch (n.type) {
      case "ten_unary": case "ten_binop": case "softmax_row": case "layer_norm": case "rms_norm":
      case "reshape": case "flatten": case "gather": case "embedding": case "sin_pe":
      case "attention_sdp": case "mha_split": case "mha_merge": case "gelu": case "relu":
      case "silu": case "rope": case "swiglu":
      case "residual_add":
        return true;
      default:
        return false;
    }
  }

  function domainClass(n) {
    if (isComplexNode(n) || n.type === "c2r") return " domain-c";
    if (n.type === "fn_call" || n.type === "fn_return") return " domain-fn";
    if (n.type === "fn_param") return " domain-param";
    if (isTensorNode(n)) return " domain-data";
    if (n.type === "cmp" || n.type === "select" || n.type === "logic" || n.type === "rng" || n.type === "const_bool" ||
        n.type === "entry" || n.type === "branch" || n.type === "while" || n.type === "for" ||
        n.type === "sequence" || n.type === "set_var" || n.type === "get_var" || n.type === "print" ||
        n.type === "out" || n.type === "do_once" || n.type === "do_n" || n.type === "gate" ||
        n.type === "flip_flop" || n.type === "multi_gate" || n.type === "switch" ||
        n.type === "break" || n.type === "continue" || n.type === "reroute" ||
        n.type === "do_while" || n.type === "select_exec")
      return " domain-flow";
    if (n.type === "comment") return " domain-comment";
    return "";
  }

  function titleOf(n) {
    if (n.type === "const_f") return "浮点";
    if (n.type === "const_i") return "定宽整数";
    if (n.type === "const_bi") return "大整数";
    if (n.type === "const_c") return "复数";
    if (n.type === "out") return "输出";
    if (n.type === "copy" || n.type === "assign") return n.type === "assign" ? "赋值" : "拷贝";
    if (n.type === "fn_param") return "参数 " + (n.paramName || n.paramIndex);
    if (n.type === "fn_return") return "返回";
    if (n.type === "fn_call") {
      const fn = project.functions[n.fnId];
      return "调用 " + ((fn && fn.name) || n.fnId);
    }
    if (isTensorNode(n)) return LABELS[n.type] || n.type;
    if (n.type === "shift") return LABELS[n.op] || n.op;
    if (n.type === "cast") return LABELS[n.op] || n.op;
    if (n.type === "entry") return "Entry";
    if (n.type === "branch") return "Branch";
    if (n.type === "while") return "While";
    if (n.type === "for") return "For Loop";
    if (n.type === "sequence") return "Sequence";
    if (n.type === "set_var") return "Set " + ((n.cfg && n.cfg.name) || "x");
    if (n.type === "get_var") return "Get " + ((n.cfg && n.cfg.name) || "x");
    if (n.type === "print") return "Print";
    if (n.type === "do_once") return "DoOnce";
    if (n.type === "do_n") return "DoN";
    if (n.type === "gate") return "Gate";
    if (n.type === "flip_flop") return "FlipFlop";
    if (n.type === "multi_gate") return "MultiGate";
    if (n.type === "switch") return "Switch";
    if (n.type === "break") return "Break";
    if (n.type === "continue") return "Continue";
    if (n.type === "reroute") return "Reroute";
    if (n.type === "do_while") return "DoWhile";
    if (n.type === "select_exec") return "Select";
    if (n.type === "comment") return "注释";
    if (n.type === "cmp") return LABELS[n.op] || n.op;
    if (n.type === "select") return "select";
    if (n.type === "logic") return "L " + (LABELS[n.op] || n.op);
    if (n.type === "rng") return LABELS[n.op] || ("rng " + n.op);
    if (n.type === "const_bool") return n.op === "false" ? "false" : "true";
    if (n.type === "crypto") return LABELS[n.op] || n.op;
    if (n.type === "cun") return "c" + (LABELS[n.op] || n.op);
    if (n.type === "cbin") return "c" + (n.op || "?");
    if (n.type === "c2r" || n.type === "r2c") return LABELS[n.op] || n.op;
    if (n.op && LABELS[n.op]) return LABELS[n.op];
    return n.op || n.type;
  }

  function metaOf(n) {
    if (n.type === "const_f")
      return "E=" + n.cfg.e_bits + " M=" + n.cfg.m_bits + " NaN=" + (n.cfg.nan ? "on" : "off") + "\n" + n.cfg.value;
    if (n.type === "const_i")
      return (n.cfg.signed ? "i" : "u") + n.cfg.width + " base" + n.cfg.base + "\n" + n.cfg.value;
    if (n.type === "const_bi")
      return "bigint base" + n.cfg.base + "\n" + n.cfg.value;
    if (n.type === "const_c")
      return "E/M " + n.cfg.e_bits + "/" + n.cfg.m_bits + "\n" + n.cfg.value;
    if (n.type === "shift") return "bits = " + (n.cfg.bits | 0);
    if (n.type === "cast" && n.op === "to_float")
      return "E=" + n.cfg.e_bits + " M=" + n.cfg.m_bits + " NaN=" + (n.cfg.nan ? "on" : "off");
    if (n.type === "cast" && n.op === "to_int")
      return (n.cfg.signed ? "i" : "u") + (n.cfg.width <= 0 ? "∞" : n.cfg.width);
    if (n.type === "rng" && n.op === "seed") return "seed ← a (或 cfg)";
    if (n.type === "rng" && n.op === "u64_mod") return "bound ← a";
    if (n.type === "select") return "cond? a : b  (ports a=cond,b=then,c=else)";
    if (n.type === "entry") return "exec → 控制流起点";
    if (n.type === "branch") return "if cond → then / else";
    if (n.type === "while") return "while cond { body } → completed";
    if (n.type === "for") return "for i in [first,last)";
    if (n.type === "sequence") return "顺序执行 " + ((n.cfg && n.cfg.count) || 2) + " 路";
    if (n.type === "set_var" || n.type === "get_var") return "var " + ((n.cfg && n.cfg.name) || "x");
    if (n.type === "print") return "Print (exec passthrough)";
    if (n.type === "out") return "Output with exec in/out";
    if (n.type === "do_once") return "first -> completed, then closed; reset";
    if (n.type === "do_n") return "first N -> exit, then completed";
    if (n.type === "gate") return (n.cfg && n.cfg.start_open) ? "start open" : "start closed";
    if (n.type === "flip_flop") return "toggle A/B";
    if (n.type === "multi_gate") return "rotate " + ((n.cfg && n.cfg.count) || 2);
    if (n.type === "switch") return "cases " + ((n.cfg && n.cfg.cases) || "0,1,2");
    if (n.type === "break") return "break nearest loop";
    if (n.type === "continue") return "continue nearest loop";
    if (n.type === "reroute") return "exec relay";
    if (n.type === "do_while") return "do { body } while cond → completed";
    if (n.type === "select_exec") return "exec: out = cond ? a : b";
    if (n.type === "comment") return String((n.cfg && n.cfg.text) || "note");
    if (n.type === "fn_param") return "index " + n.paramIndex;
    if (n.type === "fn_call") {
      const fn = project.functions[n.fnId];
      const names = (fn && fn.params) || [];
      if (names.length) return "exec · " + names.join(", ") + " → exec,out";
      return ((fn && fn.name) || n.fnId || "") + "() · exec";
    }
    if (n.type === "arr_lit" || n.type === "vec_lit")
      return String((n.cfg && n.cfg.value) || "").slice(0, 48);
    if (n.type === "mat_lit")
      return String((n.cfg && n.cfg.value) || "").slice(0, 48);
    if (n.type === "arr_map") return "elem " + ((n.cfg && n.cfg.op) || "add") + " s";
    if (n.type === "arr_reduce") return (n.cfg && n.cfg.op) || "sum";
    if (n.type === "mat_identity") return "I(" + ((n.cfg && n.cfg.n) || "n") + ")";
    if (n.type === "arr_from_range")
      return ((n.cfg && n.cfg.first) || "0") + ".." + ((n.cfg && n.cfg.last) || "n");
    if (n.type === "ten_unary") return "op=" + ((n.cfg && n.cfg.op) || "exp");
    if (n.type === "ten_binop") return "op=" + ((n.cfg && n.cfg.op) || "add");
    if (n.type === "reshape")
      return ((n.cfg && n.cfg.rows) || "?") + "×" + ((n.cfg && n.cfg.cols) || "?");
    if (n.type === "mat_concat") return "axis=" + ((n.cfg && n.cfg.axis) || "0");
    if (n.type === "layer_norm" || n.type === "rms_norm") return "ε=" + ((n.cfg && n.cfg.eps) || "1e-5");
    if (n.type === "rope") return "base=" + ((n.cfg && n.cfg.base) || "10000");
    if (n.type === "silu") return "SiLU";
    if (n.type === "swiglu") return "SwiGLU";
    if (n.type === "attention_sdp") {
      const sc = (n.cfg && n.cfg.scale) || "auto";
      return "scale=" + sc + (n.cfg && n.cfg.causal ? " causal" : "");
    }
    if (n.type === "sin_pe")
      return "seq=" + ((n.cfg && n.cfg.seq) || "n") + " d=" + ((n.cfg && n.cfg.dim) || "d");
    if (n.type === "mha_split" || n.type === "mha_merge")
      return "heads=" + ((n.cfg && n.cfg.heads) || "2");
    if (n.type === "mat_slice")
      return "r[" + ((n.cfg && n.cfg.r0) || "0") + ":" + ((n.cfg && n.cfg.r1) || "-1") +
        "] c[" + ((n.cfg && n.cfg.c0) || "0") + ":" + ((n.cfg && n.cfg.c1) || "-1") + "]";
    if (n.type === "fn_return") return "return value";
    if (n.type === "ter" || (n.type === "crypto" && (n.arity || 0) >= 3)) {
      const p = portsOf(n);
      return p.in.join(" · ");
    }
    return "";
  }

  function parseCplxConst(s) {
    const t = String(s || "0,0").trim();
    let m = t.match(/^\s*([+-]?\d*\.?\d+(?:[eE][+-]?\d+)?)\s*[,;]\s*([+-]?\d*\.?\d+(?:[eE][+-]?\d+)?)\s*$/);
    if (m) return { re: parseFloat(m[1]), im: parseFloat(m[2]) };
    m = t.match(/^\s*([+-]?\d*\.?\d+(?:[eE][+-]?\d+)?)\s*([+-])\s*(\d*\.?\d+(?:[eE][+-]?\d+)?)\s*[ij]\s*$/i);
    if (m) return { re: parseFloat(m[1]), im: parseFloat((m[2] === "-" ? "-" : "") + m[3]) };
    m = t.match(/^\s*([+-]?\d*\.?\d+(?:[eE][+-]?\d+)?)\s*[ij]\s*$/i);
    if (m) return { re: 0, im: parseFloat(m[1]) };
    const x = parseFloat(t);
    if (Number.isFinite(x)) return { re: x, im: 0 };
    return { re: 0, im: 0 };
  }

  
﻿/* ---------- Node lifecycle ---------- */
  function addNode(spec, x, y, extra) {
    if (typeof pushHistory === "function") pushHistory("添加节点");
    const id = "n" + (nodeSeq++);
    const type = spec.type;
    const n = {
      id, type,
      op: spec.op || null,
      x: x != null ? x : 80 + (nodes().length % 8) * 28,
      y: y != null ? y : 80 + (nodes().length % 8) * 26,
      cfg: Object.assign(defaultCfg(type), (extra && extra.cfg) || {}),
      arity: spec.arity || (extra && extra.arity) || null,
      fnId: (extra && extra.fnId) || null,
      paramIndex: (extra && extra.paramIndex) != null ? extra.paramIndex : null,
      paramName: (extra && extra.paramName) || null,
      inputs: {},
      inputFromPorts: {},
      collapsed: !!(extra && extra.collapsed),
      foldUnusedPorts: !!(extra && extra.foldUnusedPorts),
      note: (extra && extra.note != null) ? String(extra.note) : "",
      el: null,
      slot: -1,
      result: ""
    };
    if (!n.cfg) n.cfg = {};
    if (!n.cfg.portValues) n.cfg.portValues = {};
    portsOf(n).in.forEach((p) => { n.inputs[p] = null; });
    nodes().push(n);
    renderNode(n);
    refreshCollapseHints();
    ensureCanvasSize();
    selectNode(n.id);
    return n;
  }

  
  function formatResultPreview(text) {
    /* at most 2 lines; long lists: 1,2,3,...,7,8,9 (hover title = full) */
    const raw = String(text == null ? "" : text);
    if (!raw) return "";
    const s = raw.replace(/\r\n/g, "\n").replace(/\r/g, "\n").trim();
    const middleChars = (str, maxChars) => {
      const t = String(str || "");
      if (t.length <= maxChars) return t;
      const keep = Math.max(10, maxChars - 3);
      const head = Math.ceil(keep * 0.55);
      const tail = keep - head;
      return t.slice(0, head) + "..." + t.slice(t.length - tail);
    };
    const listMiddle = (str, maxTokens) => {
      const t = String(str || "").trim();
      if (!t) return t;
      if (t.indexOf(";") >= 0) {
        const rows = t.split(";").map((x) => x.trim()).filter(Boolean);
        if (rows.length <= 2) return rows.map((r) => listMiddle(r, maxTokens)).join("; ");
        return listMiddle(rows[0], maxTokens) + "; ...; " + listMiddle(rows[rows.length - 1], maxTokens);
      }
      const parts = t.split(",").map((x) => x.trim()).filter((x) => x.length);
      if (parts.length <= maxTokens) return parts.join(",");
      const headN = Math.max(2, Math.floor(maxTokens / 2));
      const tailN = Math.max(2, maxTokens - headN);
      return parts.slice(0, headN).join(",") + ",...," + parts.slice(parts.length - tailN).join(",");
    };
    const flat = s.replace(/\n+/g, "; ");
    const tokenish = flat.split(/[,;]/).filter((x) => x.trim().length);
    if (tokenish.length >= 6) return middleChars(listMiddle(flat, 8), 96);
    const lines = s.split("\n");
    if (lines.length <= 1) return middleChars(lines[0] || s, 96);
    return middleChars(lines[0], 90) + "\n" + middleChars(lines[lines.length - 1], 90);
  }
function removeNode(id) {
    if (selectedId === id) selectedId = null;
    if (typeof selectedIds !== "undefined" && selectedIds) selectedIds.delete(id);
    setWires(wires().filter((w) => w.from !== id && w.to !== id));
    setNodes(nodes().filter((n) => {
      if (n.id !== id) {
        Object.keys(n.inputs).forEach((k) => { if (n.inputs[k] === id) n.inputs[k] = null; });
        return true;
      }
      if (n.el) n.el.remove();
      return false;
    }));
    drawWires();
    refreshCollapseHints();
    renderInspector();
  }

  function cloneNodeTemplate(n) {
    if (!n) return null;
    return {
      type: n.type,
      op: n.op,
      cfg: JSON.parse(JSON.stringify(n.cfg || {})),
      arity: n.arity,
      fnId: n.fnId,
      paramIndex: n.paramIndex,
      paramName: n.paramName,
      note: n.note != null ? String(n.note) : ""
    };
  }

  function copySelectedNode() {
    const n = selectedId ? nodeById(selectedId) : null;
    if (!n) {
      status("没有选中的节点可复制", "err");
      return false;
    }
    clipboard = cloneNodeTemplate(n);
    status("已复制「" + titleOf(n) + "」", "ok");
    return true;
  }

  function cutSelectedNode() {
    if (typeof pushHistory === "function") pushHistory("剪切");
    if (!copySelectedNode()) return false;
    removeNode(selectedId);
    status("已剪切节点", "ok");
    return true;
  }

  function pasteClipboard(atX, atY) {
    if (typeof pushHistory === "function") pushHistory("粘贴");
    if (!clipboard) {
      status("剪贴板为空", "err");
      return null;
    }
    if (typeof historyQuiet !== "undefined") historyQuiet = true;
    const t = clipboard;
    /* disallow fn_param outside function; disallow recursive call of current fn */
    if (t.type === "fn_param" && editScope === "main") {
      status("参数节点只能放在函数画布", "err");
      if (typeof historyQuiet !== "undefined") historyQuiet = false;
      return null;
    }
    if (t.type === "fn_return" && editScope === "main") {
      status("返回节点只能放在函数画布", "err");
      if (typeof historyQuiet !== "undefined") historyQuiet = false;
      return null;
    }
    if (t.type === "fn_call" && editScope === "fn:" + t.fnId) {
      status("函数内不可调用自身", "err");
      if (typeof historyQuiet !== "undefined") historyQuiet = false;
      return null;
    }
    const x = atX != null ? atX : 100 + (nodes().length % 6) * 30;
    const y = atY != null ? atY : 100 + (nodes().length % 6) * 28;
    let n = null;
    try {
      n = addNode(
        { type: t.type, op: t.op, arity: t.arity },
        x, y,
        { cfg: t.cfg, arity: t.arity, fnId: t.fnId, paramIndex: t.paramIndex, paramName: t.paramName, note: t.note || "" }
      );
    } finally {
      if (typeof historyQuiet !== "undefined") historyQuiet = false;
    }
    status("已粘贴「" + titleOf(n) + "」", "ok");
    return n;
  }

  function duplicateSelectedNode() {
    const list = (typeof selectedNodeList === "function") ? selectedNodeList() : (selectedId ? [nodeById(selectedId)].filter(Boolean) : []);
    if (!list.length) {
      status("no selected node", "err");
      return null;
    }
    if (list.length === 1) {
      clipboard = cloneNodeTemplate(list[0]);
      return pasteClipboard(list[0].x + 36, list[0].y + 36);
    }
    const newIds = [];
    list.forEach((n, i) => {
      const t = cloneNodeTemplate(n);
      clipboard = t;
      const p = pasteClipboard(n.x + 36, n.y + 36);
      if (p) newIds.push(p.id);
    });
    if (newIds.length && typeof setSelection === "function") setSelection(newIds);
    status("duplicated " + newIds.length, "ok");
    return newIds[0] ? nodeById(newIds[0]) : null;
  }

  function disconnectNode(id) {
    if (typeof pushHistory === "function") pushHistory("断开连线");
    const n = nodeById(id);
    if (!n) return;
    const touched = new Set([id]);
    setWires(wires().filter((w) => {
      if (w.from === id || w.to === id) {
        touched.add(w.from);
        touched.add(w.to);
        return false;
      }
      return true;
    }));
    Object.keys(n.inputs || {}).forEach((k) => { n.inputs[k] = null; });
    nodes().forEach((m) => {
      Object.keys(m.inputs || {}).forEach((k) => {
        if (m.inputs[k] === id) {
          m.inputs[k] = null;
          touched.add(m.id);
        }
      });
    });
    touched.forEach((tid) => {
      const m = nodeById(tid);
      if (m) renderNode(m);
    });
    refreshCollapseHints();
    drawWires();
    if (selectedId && touched.has(selectedId)) renderInspector();
    status("已断开该节点全部连线", "ok");
  }

  function deleteSelectedWire() {
    if (selectedWire == null) return false;
    const w = wires()[selectedWire];
    const touched = [];
    if (w) {
      const n = nodeById(w.to);
      const src = nodeById(w.from);
      if (n) {
        n.inputs[w.toPort] = null;
        if (n.inputFromPorts) n.inputFromPorts[w.toPort] = null;
        touched.push(n);
      }
      if (src) touched.push(src);
      setWires(wires().filter((_, i) => i !== selectedWire));
    }
    selectedWire = null;
    touched.forEach((m) => renderNode(m));
    refreshCollapseHints();
    drawWires();
    if (selectedId && touched.some((m) => m.id === selectedId)) renderInspector();
    return true;
  }

  function deleteSelection() {
    if (typeof pushHistory === "function") pushHistory("删除");
    if (selectedWire != null) {
      deleteSelectedWire();
      status("deleted wire", "ok");
      return true;
    }
    const list = (typeof selectedNodeList === "function") ? selectedNodeList() : (selectedId ? [nodeById(selectedId)].filter(Boolean) : []);
    if (list.length) {
      list.forEach((n) => removeNode(n.id));
      if (typeof clearSelection === "function") clearSelection();
      else { selectedId = null; if (selectedIds) selectedIds.clear(); }
      status("deleted " + list.length + " node(s)", "ok");
      return true;
    }
    return false;
  }

  function editSelectedNode() {
    const n = selectedId ? nodeById(selectedId) : null;
    if (!n) {
      status("没有选中的节点", "err");
      return;
    }
    if (n.type.startsWith("const") || n.type === "shift" || n.type === "cast" ||
        (n.type === "rng" && (n.op === "seed" || n.op === "u64_mod"))) {
      openConfigDialog(n, false);
    } else {
      status("该节点无配置对话框（可在检查器查看）");
      renderInspector();
    }
  }

  function doRun() {
    try { runGraph(); } catch (err) {
      log("错误: " + err.message);
      status(err.message, "err");
    }
  }

  function doExportProject() {
    const blob = new Blob([JSON.stringify(exportProject(), null, 2)], { type: "application/json" });
    const a = document.createElement("a");
    a.href = URL.createObjectURL(blob);
    a.download = "snlab_project.json";
    a.click();
    URL.revokeObjectURL(a.href);
    status("已导出工程 JSON", "ok");
  }

  function doExportC() {
    const code = exportC();
    renderCPreview(code);
    const blob = new Blob([code], { type: "text/plain" });
    const a = document.createElement("a");
    a.href = URL.createObjectURL(blob);
    a.download = "snlab_export.c";
    a.click();
    URL.revokeObjectURL(a.href);
    status("已导出 C", "ok");
  }

  function canvasLocalFromClient(clientX, clientY) {
    return canvasPointFromClient(clientX, clientY);
  }

  function hideCtxMenu() {
    const m = $id("ctxMenu");
    if (!m) return;
    m.classList.add("hidden");
    m.setAttribute("aria-hidden", "true");
    m.innerHTML = "";
    ctxTarget = null;
  }

  function showCtxMenu(clientX, clientY, target) {
    const m = $id("ctxMenu");
    if (!m) return;
    ctxTarget = target;
    const items = buildCtxItems(target);
    m.innerHTML = "";
    items.forEach((it) => {
      if (it.sep) {
        const s = document.createElement("div");
        s.className = "ctx-sep";
        m.appendChild(s);
        return;
      }
      if (it.labelOnly) {
        const l = document.createElement("div");
        l.className = "ctx-label";
        l.textContent = it.label;
        m.appendChild(l);
        return;
      }
      const b = document.createElement("button");
      b.type = "button";
      b.className = "ctx-item" + (it.danger ? " danger" : "");
      b.disabled = !!it.disabled;
      b.innerHTML = "<span>" + escapeHtml(it.label) + "</span>" +
        (it.k ? '<span class="ctx-k">' + escapeHtml(it.k) + "</span>" : "");
      b.addEventListener("click", (e) => {
        e.preventDefault();
        e.stopPropagation();
        hideCtxMenu();
        try { it.action && it.action(); } catch (err) {
          status(err.message || String(err), "err");
        }
      });
      m.appendChild(b);
    });
    m.classList.remove("hidden");
    m.setAttribute("aria-hidden", "false");
    /* position, clamp to viewport */
    const pad = 6;
    m.style.left = "0px";
    m.style.top = "0px";
    const mw = m.offsetWidth || 220;
    const mh = m.offsetHeight || 240;
    let x = clientX;
    let y = clientY;
    if (x + mw > window.innerWidth - pad) x = window.innerWidth - mw - pad;
    if (y + mh > window.innerHeight - pad) y = window.innerHeight - mh - pad;
    if (x < pad) x = pad;
    if (y < pad) y = pad;
    m.style.left = x + "px";
    m.style.top = y + "px";
  }


  function getNodeNote(n) {
    if (!n) return "";
    if (n.note != null && String(n.note).length) return String(n.note);
    if (n.cfg && n.cfg.note != null) return String(n.cfg.note);
    return "";
  }

  function setNodeNote(n, text, opts) {
    if (!n || n.type === "comment") return;
    opts = opts || {};
    if (opts.history !== false && typeof pushHistory === "function") pushHistory("节点注释");
    const t = text == null ? "" : String(text);
    n.note = t;
    if (n.cfg && n.cfg.note != null) delete n.cfg.note;
    if (n.el) {
      /* refresh note bubble without full re-render (keeps focus when editing) */
      let bubble = n.el.querySelector(".node-note");
      if (!t) {
        if (bubble) bubble.remove();
        n.el.classList.remove("has-note");
      } else {
        if (!bubble) {
          bubble = document.createElement("div");
          bubble.className = "node-note";
          bubble.setAttribute("data-node-note", "1");
          bubble.setAttribute("contenteditable", "true");
          bubble.setAttribute("spellcheck", "false");
          n.el.appendChild(bubble);
          bindNodeNoteBubble(n, bubble);
        }
        if (document.activeElement !== bubble) bubble.textContent = t;
        n.el.classList.add("has-note");
      }
    } else {
      renderNode(n);
    }
    status(t ? "已更新节点注释" : "已清除节点注释", "ok");
  }

  function bindNodeNoteBubble(n, bubble) {
    if (!bubble || bubble._snNoteBound) return;
    bubble._snNoteBound = true;
    bubble.addEventListener("pointerdown", (e) => { e.stopPropagation(); selectNode(n.id); });
    bubble.addEventListener("click", (e) => e.stopPropagation());
    bubble.addEventListener("dblclick", (e) => {
      e.stopPropagation();
      e.preventDefault();
      bubble.focus();
      try {
        const sel = window.getSelection();
        const range = document.createRange();
        range.selectNodeContents(bubble);
        sel.removeAllRanges();
        sel.addRange(range);
      } catch (_) {}
    });
    let before = "";
    bubble.addEventListener("focus", () => {
      before = getNodeNote(n);
      bubble.classList.remove("empty-hint");
    });
    bubble.addEventListener("blur", () => {
      const t = (bubble.textContent || "").replace(/\u00a0/g, " ").trimEnd();
      if (t !== before) {
        if (typeof pushHistory === "function") pushHistory("节点注释");
        n.note = t;
        if (n.cfg && n.cfg.note != null) delete n.cfg.note;
      }
      if (!t) {
        bubble.remove();
        if (n.el) n.el.classList.remove("has-note");
        n.note = "";
      } else {
        bubble.textContent = t;
      }
    });
    bubble.addEventListener("keydown", (e) => {
      if (e.key === "Escape") {
        e.preventDefault();
        bubble.textContent = before;
        bubble.blur();
      }
      e.stopPropagation();
    });
  }

  function editNodeNoteDialog(n) {
    if (!n || n.type === "comment") {
      status("独立注释节点请直接编辑便签内容", "err");
      return;
    }
    const cur = getNodeNote(n);
    const next = window.prompt("节点注释（UE 风格附着气泡，留空清除）：", cur);
    if (next == null) return;
    setNodeNote(n, next);
  }

  function buildCtxItems(target) {
    const kind = target && target.kind;
    const items = [];
    items.push({ labelOnly: true, label: kind === "node" ? "节点" : kind === "wire" ? "连线" : "画布" });

    if (kind === "node") {
      const n = nodeById(target.id);
      items.push({ label: "选中并检查", k: "单击", action: () => selectNode(target.id) });
      if (n && (n.type.startsWith("const") || n.type === "shift" || n.type === "cast" ||
          n.type === "set_var" || n.type === "get_var" || n.type === "for" ||
          n.type === "sequence" || n.type === "while" ||
          (n.type === "rng" && (n.op === "seed" || n.op === "u64_mod")))) {
        items.push({ label: "编辑配置…", k: "F2", action: () => { selectNode(target.id); openConfigDialog(n, false); } });
      }
      if (n) {
        items.push({ label: "fold unused ports / expand", k: "▾", action: () => togglePortFold(n) });
        if (n.foldUnusedPorts) {
          items.push({ label: "show all ports", action: () => { n.foldUnusedPorts = false; renderNode(n); refreshCollapseHints(); drawWires(); } });
        }
        if (n.collapsed) {
          items.push({ label: "expand node", action: () => setNodeCollapsed(n, false) });
        } else {
          items.push({ label: "collapse node body", action: () => setNodeCollapsed(n, true) });
        }
        if (n.type !== "comment") {
          const hasNote = !!(getNodeNote(n));
          items.push({
            label: hasNote ? "编辑附着注释…" : "添加附着注释…",
            k: "N",
            action: () => { selectNode(target.id); editNodeNoteDialog(n); }
          });
          if (hasNote) {
            items.push({
              label: "清除附着注释",
              action: () => { selectNode(target.id); setNodeNote(n, ""); }
            });
          }
        }
      }
      items.push({ label: "复制", k: "Ctrl+C", action: () => { selectNode(target.id); copySelectedNode(); } });
      items.push({ label: "剪切", k: "Ctrl+X", action: () => { selectNode(target.id); cutSelectedNode(); } });
      items.push({ label: "复制一份", k: "Ctrl+D", action: () => { selectNode(target.id); duplicateSelectedNode(); } });
      items.push({ sep: true });
      items.push({ label: "断开全部连线", action: () => disconnectNode(target.id) });
      items.push({ label: "删除节点", k: "Del", danger: true, action: () => removeNode(target.id) });
    } else if (kind === "wire") {
      items.push({
        label: "删除连线", k: "Del", danger: true, action: () => {
          selectedWire = target.wireIndex;
          deleteSelectedWire();
          status("已删除连线", "ok");
        }
      });
    } else {
      /* canvas */
      items.push({ label: "运行图", k: "Enter", action: () => doRun() });
      items.push({
        label: "粘贴到此处", k: "Ctrl+V", disabled: !clipboard, action: () => {
          pasteClipboard(target.x, target.y);
        }
      });
      items.push({ sep: true });
      items.push({
        label: "添加 · 浮点常量", action: async () => {
          const n = addNode({ type: "const_f" }, target.x, target.y);
          await openConfigDialog(n, true);
        }
      });
      items.push({
        label: "添加 · 定宽整数", action: async () => {
          const n = addNode({ type: "const_i" }, target.x, target.y);
          await openConfigDialog(n, true);
        }
      });
      items.push({
        label: "添加 · 大整数", action: async () => {
          const n = addNode({ type: "const_bi" }, target.x, target.y);
          await openConfigDialog(n, true);
        }
      });
      items.push({
        label: "添加 · 显示输出", action: () => {
          addNode({ type: "out" }, target.x, target.y);
        }
      });
      items.push({
        label: "添加 · 独立注释便签", action: () => {
          addNode({ type: "comment" }, target.x, target.y);
        }
      });
      items.push({ sep: true });
      items.push({ label: "新建自定义函数…", k: "Ctrl+N", action: () => openNewFnDialog() });
      if (editScope !== "main") {
        items.push({ label: "返回主画布", k: "Ctrl+Home", action: () => { editScope = "main"; remountGraph(); } });
      }
      items.push({ label: "重置视图", k: "Ctrl+0", action: () => resetView() });
      items.push({ label: "清空画布", k: "Ctrl+⌫", danger: true, action: () => {
        if (confirm("清空当前画布全部节点？")) clearCanvas();
      } });
    }

    items.push({ sep: true });
    items.push({ label: "撤销", k: "Ctrl+Z", action: () => { if (typeof undo === "function") undo(); } });
    items.push({ label: "重做", k: "Ctrl+Y", action: () => { if (typeof redo === "function") redo(); } });
    items.push({ label: "导出工程", k: "Ctrl+S", action: () => doExportProject() });
    items.push({ label: "导出 C", k: "Ctrl+Shift+C", action: () => doExportC() });
    items.push({ label: "导入工程…", k: "Ctrl+O", action: () => { const f = $id("fileImport"); if (f) f.click(); } });
    items.push({ sep: true });
    items.push({ label: "帮助 / 快捷键", k: "?", action: () => { const d = $id("helpDlg"); if (d) d.showModal(); } });
    return items;
  }

  /* Port vertical step (px). Block grows with max(in,out) so labels/results never stack. */
  const PORT_STEP = 24;
  const PORT_PAD_TOP = 10;
  const PORT_PAD_BOT = 12;
  const BLOCK_HEADER_H = 34;

  function portDisplayName(n, port, dir) {
    if (dir === "out") {
      if (port === "exec") return "▶";
      if (port === "then") return "True";
      if (port === "else") return "False";
      if (port === "body") return "Loop";
      if (port === "completed") return "Done";
      if (port === "closed") return "Closed";
      if (port === "exit") return "Exit";
      if (port === "index") return "i";
      if (port === "A") return "A";
      if (port === "B") return "B";
      if (port === "default") return "Default";
      if (/^then\d+$/.test(port)) return "Then " + port.slice(4);
      if (/^out\d+$/.test(port)) return "Out " + port.slice(3);
      if (/^case\d+$/.test(port)) {
        const cases = parseSwitchCases(n);
        const idx = parseInt(port.slice(4), 10);
        return "Case " + (cases[idx] != null ? cases[idx] : idx);
      }
      if (port === "out") return "out";
      return port;
    }
    if (port === "exec" || port === "enter") return "▶";
    if (port === "reset") return "Reset";
    if (port === "open") return "Open";
    if (port === "close") return "Close";
    if (port === "toggle") return "Toggle";
    if (port === "cond") return "Cond";
    if (port === "value" || port === "in") return "Val";
    if (port === "sel") return "Sel";
    if (port === "n") return "N";
    if (port === "first") return "First";
    if (port === "last") return "Last";
    if (n.type === "select") {
      if (port === "a") return "cond";
      if (port === "b") return "then";
      if (port === "c") return "else";
    }
    if (n.type === "rng" && n.op === "seed" && port === "a") return "seed";
    if (n.type === "rng" && n.op === "u64_mod" && port === "a") return "bound";
    if (n.type === "fn_call") {
      if (port === "exec") return "exec";
      if (port === "out") return "out";
      const fn = project.functions[n.fnId];
      const names = (fn && fn.params) || [];
      const idx = parseInt(String(port).replace(/^p/, ""), 10);
      if (Number.isFinite(idx) && names[idx]) return names[idx];
    }
    if (port === "cols") return "cols";
    return port;
  }

  function layoutPorts(n) {
    if (!n.el) return;
    /* free comment sticky: size with content / user resize, no port layout */
    if (n.type === "comment") {
      n.el.style.maxWidth = "none";
      /* keep JS pixel width from fit(); do not force max-content clamp */
      if (!n.el.style.width || n.el.style.width === "max-content") {
        n.el.style.width = "max-content";
      }
      n.el.style.height = "auto";
      n.el.style.overflow = "visible";
      return;
    }
    if (n.collapsed) {
      n.el.style.minHeight = "";
      n.el.style.height = "auto";
      return;
    }
    const ports = visiblePortsOf(n);
    const nIn = ports.in.length;
    const nOut = ports.out.length;
    const nMax = Math.max(nIn, nOut, 0);

    const body = n.el.querySelector(".body");
    const header = n.el.querySelector("header");
    const headerH = (header && header.offsetHeight) || BLOCK_HEADER_H;

    /* Leave horizontal room for port labels so meta/result are not covered. */
    if (body) {
      body.style.paddingLeft = (nIn > 0 ? 36 : 12) + "px";
      body.style.paddingRight = (nOut > 0 ? 36 : 12) + "px";
    }

    /* Measure content height (meta + port-lits + result). */
    let contentH = 0;
    if (body) {
      const meta = body.querySelector(".meta");
      const lits = body.querySelector(".port-lits");
      const res = body.querySelector(".result");
      const mt = meta ? Math.max(meta.scrollHeight, meta.offsetHeight) + 6 : 0;
      const lt = lits ? Math.max(lits.scrollHeight, lits.offsetHeight) + 8 : 0;
      const rt = res ? Math.max(res.scrollHeight, res.offsetHeight) + 10 : 0;
      contentH = mt + lt + rt + 18; /* body vertical padding approx */
      if (contentH < 28) contentH = 28;
    }

    const portsH = nMax > 0
      ? (PORT_PAD_TOP + nMax * PORT_STEP + PORT_PAD_BOT)
      : 0;
    /* content and port stack never share the same vertical budget — take the larger */
    const bodyMin = Math.max(contentH, portsH, nMax > 0 ? 48 : 28);
    if (body) {
      body.style.minHeight = bodyMin + "px";
      body.style.height = "auto";
    }

    const totalMin = headerH + bodyMin;
    n.el.style.minHeight = totalMin + "px";
    n.el.style.height = "auto";
    n.el.dataset.portCount = String(nMax);
    /* widen when many ports / long meta so labels + result don't collide */
    const metaLen = ((n.cfg && (n.cfg.value || "")) + "").length;
    const litCount = body ? body.querySelectorAll(".port-lit").length : 0;
    if (nMax >= 3 || metaLen > 24 || litCount > 0) {
      const want = Math.min(460, 188 + Math.max(0, nMax - 2) * 24 + Math.min(100, Math.floor(metaLen / 2)) + litCount * 8);
      n.el.style.minWidth = want + "px";
    } else {
      n.el.style.minWidth = "";
    }

    /* Fixed step from top of body — does not compress when many args. */
    const baseY = headerH + PORT_PAD_TOP;
    ports.in.forEach((p, i) => {
      const el = n.el.querySelector('.port.in[data-port="' + p + '"]');
      const lab = n.el.querySelector('.port-label.in[data-port="' + p + '"]');
      const y = baseY + i * PORT_STEP + (PORT_STEP / 2) - 7;
      if (el) el.style.top = y + "px";
      if (lab) {
        lab.style.top = (y - 4) + "px";
        lab.textContent = portDisplayName(n, p, "in");
      }
    });
    ports.out.forEach((p, i) => {
      const el = n.el.querySelector('.port.out[data-port="' + p + '"]');
      const lab = n.el.querySelector('.port-label.out[data-port="' + p + '"]');
      const y = baseY + i * PORT_STEP + (PORT_STEP / 2) - 7;
      if (el) el.style.top = y + "px";
      if (lab) {
        lab.style.top = (y - 4) + "px";
        lab.textContent = portDisplayName(n, p, "out");
      }
    });
  }

  function renderNode(n) {
    const canvas = $id("canvas");
    if (n.el) n.el.remove();
    const el = document.createElement("div");
    const allPorts = portsOf(n);
    const ports = visiblePortsOf(n);
    const hasExec = (allPorts.in || []).some((p) => isExecPortOnNode(n, p) || isExecPort(p)) ||
      (allPorts.out || []).some((p) => isExecPortOnNode(n, p) || isExecPort(p));
    el.className = "block" + domainClass(n) +
      (hasExec || isFlowControlNode(n) ? " ctrl" : "") +
      (n.id === selectedId ? " selected" : "") +
      (n.collapsed ? " collapsed" : "") +
      (n.foldUnusedPorts ? " ports-folded" : "") +
      ((unusedDataPortCount(n) > 0 || n.foldUnusedPorts || n.collapsed) ? " can-collapse" : "") +
      (getNodeNote(n) ? " has-note" : "");
    el.style.left = n.x + "px";
    el.style.top = n.y + "px";
    el.dataset.id = n.id;

    let portHtml = "";
    if (!n.collapsed) {
      ports.in.forEach((p) => {
        const ex = (isExecPortOnNode(n, p) || isExecPort(p)) ? " exec" : "";
        portHtml += '<span class="port in' + ex + '" data-port="' + p + '" data-dir="in" title="' + p + '"></span>';
        portHtml += '<span class="port-label in" data-port="' + p + '">' + p + "</span>";
      });
      ports.out.forEach((p) => {
        const ex = (isExecPortOnNode(n, p) || isExecPort(p)) ? " exec" : "";
        const lab = (typeof portDisplayName === "function" ? portDisplayName(n, p, "out") : p);
        portHtml += '<span class="port out' + ex + '" data-port="' + p + '" data-dir="out" title="' + lab + '"></span>';
        portHtml += '<span class="port-label out" data-port="' + p + '">' + lab + "</span>";
      });
    }

    const meta = metaOf(n);
    const showBody = !n.collapsed;
    if (n.type === "comment") {
      el.className = "block domain-comment" + (n.id === selectedId ? " selected" : "");
      el.innerHTML =
        '<header class="comment-head"><span>注释</span>' +
        '<button type="button" class="icon" data-del title="delete">×</button></header>' +
        '<div class="body comment-body">' +
        '<textarea class="comment-text" data-comment-text rows="3" spellcheck="false">' +
        escapeHtml(String((n.cfg && n.cfg.text) || "")) +
        "</textarea></div>";
      canvas.appendChild(el);
      n.el = el;
      const ta = el.querySelector("[data-comment-text]");
      if (ta) {
        const fit = () => {
          const text = ta.value || "";
          const lines = text.split(/\n/);
          /* grow with content only — no max-width, no viewport / 88vw clamp */
          const probe = document.createElement("span");
          probe.style.cssText = "position:absolute;left:-99999px;top:0;white-space:pre;visibility:hidden;font:13px/1.4 'Segoe UI',system-ui,sans-serif;";
          let maxW = 80;
          for (let i = 0; i < lines.length; i++) {
            probe.textContent = lines[i].length ? lines[i] : " ";
            document.body.appendChild(probe);
            maxW = Math.max(maxW, Math.ceil(probe.getBoundingClientRect().width) + 28);
            document.body.removeChild(probe);
          }
          /* keep user resize if larger than content; never shrink below content */
          const curW = parseFloat(ta.style.width) || 0;
          const wantW = Math.max(80, maxW, curW);
          ta.style.width = wantW + "px";
          ta.style.maxWidth = "none";
          ta.style.minWidth = "80px";
          ta.style.height = "auto";
          const curH = parseFloat(ta.style.height) || 0;
          ta.style.height = Math.max(48, ta.scrollHeight, curH) + "px";
          ta.style.whiteSpace = "pre";
          ta.style.overflow = "visible";
          el.style.width = (wantW + 16) + "px";
          el.style.maxWidth = "none";
          el.style.minWidth = "80px";
          el.style.overflow = "visible";
          const bodyEl = el.querySelector(".comment-body");
          if (bodyEl) {
            bodyEl.style.width = wantW + "px";
            bodyEl.style.maxWidth = "none";
          }
        };
        fit();
        ta.addEventListener("input", () => {
          if (!n.cfg) n.cfg = {};
          n.cfg.text = ta.value;
          fit();
        });
        ta.addEventListener("pointerdown", (e) => e.stopPropagation());
        ta.addEventListener("dblclick", (e) => e.stopPropagation());
      }
      const header = el.querySelector("header");
      header.addEventListener("pointerdown", (e) => {
        if (e.target.closest("[data-del]")) return;
        if (e.button != null && e.button !== 0) return;
        if (spaceDown || e.altKey) return;
        e.preventDefault();
        e.stopPropagation();
        selectNode(n.id);
        selectedWire = null;
        endWireDraft(false);
        const pt = canvasPointFromClient(e.clientX, e.clientY);
        drag = {
          id: n.id,
          ox: pt.x - n.x,
          oy: pt.y - n.y,
          startX: pt.x,
          startY: pt.y,
          pointerId: e.pointerId,
          origins: { [n.id]: { x: n.x, y: n.y } },
          groupIds: [n.id],
          histPushed: false
        };
        try { header.setPointerCapture(e.pointerId); } catch (_) {}
        header.classList.add("dragging");
      });
      header.addEventListener("pointermove", (e) => {
        if (!drag || drag.id !== n.id) return;
        if (drag.pointerId != null && e.pointerId !== drag.pointerId) return;
        const pt = canvasPointFromClient(e.clientX, e.clientY);
        const dx = pt.x - drag.startX;
        const dy = pt.y - drag.startY;
        if (!drag.histPushed && (Math.abs(dx) > 0.5 || Math.abs(dy) > 0.5)) {
          const o = drag.origins && drag.origins[n.id];
          if (o) {
            const cx0 = n.x, cy0 = n.y;
            n.x = o.x; n.y = o.y;
            if (typeof pushHistory === "function") pushHistory("移动节点");
            n.x = cx0; n.y = cy0;
          }
          drag.histPushed = true;
        }
        n.x = (drag.origins[n.id] ? drag.origins[n.id].x : n.x) + dx;
        n.y = (drag.origins[n.id] ? drag.origins[n.id].y : n.y) + dy;
        el.style.left = n.x + "px";
        el.style.top = n.y + "px";
        ensureCanvasSize();
        drawWires();
        if (e && e.clientX != null) nudgeEdgeAutoPan(e.clientX, e.clientY, "drag");
      });
      const endDragC = (e) => {
        stopEdgeAutoPan();
        if (!drag || drag.id !== n.id) return;
        if (e && e.pointerId != null && drag.pointerId !== e.pointerId) return;
        const pid = drag.pointerId;
        /* history already pushed on first move when histPushed */
        drag = null;
        header.classList.remove("dragging");
        try { if (pid != null) header.releasePointerCapture(pid); } catch (_) {}
      };
      header.addEventListener("pointerup", endDragC);
      header.addEventListener("pointercancel", endDragC);
      header.addEventListener("lostpointercapture", () => {
        if (drag && drag.id === n.id) {
          stopEdgeAutoPan();
          drag = null;
          header.classList.remove("dragging");
        }
      });
      el.querySelector("[data-del]").addEventListener("click", (e) => {
        e.stopPropagation();
        removeNode(n.id);
      });
      el.addEventListener("pointerdown", (e) => {
        if (e.target.closest("[data-comment-text]") || e.target.closest("[data-del]")) return;
        selectNode(n.id);
      });
      el.addEventListener("dblclick", (e) => {
        if (e.target.closest("[data-comment-text]")) return;
        e.stopPropagation();
        openConfigDialog(n, false);
      });
      el.addEventListener("contextmenu", (e) => {
        e.preventDefault();
        e.stopPropagation();
        selectNode(n.id);
        showCtxMenu(e.clientX, e.clientY, { kind: "node", id: n.id });
      });
      return;
    }
    let litHtml = "";
    if (showBody) {
      const dataIns = allPorts.in.filter((p) => !(isExecPortOnNode(n, p) || isExecPort(p)));
      if (dataIns.length) {
        litHtml = '<div class="port-lits">';
        dataIns.forEach((p) => {
          /* wired pin: hide literal box (inputs map or live wires) */
          const wired = (n.inputs && n.inputs[p] != null && n.inputs[p] !== "") ||
            (currentGraph().wires || []).some((w) => w.to === n.id && w.toPort === p);
          if (wired) return;
          if (n.foldUnusedPorts && !portIsUsed(n, p, "in")) return;
          const cur = (n.cfg && n.cfg.portValues && n.cfg.portValues[p] != null) ? n.cfg.portValues[p] : "";
          litHtml += '<label class="port-lit" data-port="' + p + '"><span>' +
            escapeHtml(portDisplayName(n, p, "in")) +
            '</span><input type="text" data-port-lit="' + p + '" value="' + escapeHtml(String(cur)) +
            '" placeholder="literal" title="inline value when not wired" /></label>';
        });
        litHtml += "</div>";
      }
    }
    el.innerHTML =
      "<header>" +
      '<button type="button" class="icon collapse-btn" data-collapse title="fold ports">▾</button>' +
      "<span>" + escapeHtml(titleOf(n)) +
      (n.op && !n.type.startsWith("const") ? '<span class="tag">' + escapeHtml(n.type) + "</span>" : "") +
      '</span><button type="button" class="icon" data-del title="delete">×</button></header>' +
      (showBody
        ? ('<div class="body">' +
          (meta ? '<div class="meta">' + escapeHtml(meta) + "</div>" : "") +
          litHtml +
          '<div class="result" data-result title="' + escapeHtml(n.result || "") + '">' + escapeHtml(formatResultPreview ? formatResultPreview(n.result || "") : (n.result || "")) + "</div>" +
          "</div>" + portHtml)
        : '<div class="body collapsed-body"><div class="meta muted">folded · dblclick expand</div></div>');

    canvas.appendChild(el);
    n.el = el;

    /* UE attached note bubble under node */
    (function mountNote() {
      const text = getNodeNote(n);
      if (!text) return;
      const bubble = document.createElement("div");
      bubble.className = "node-note";
      bubble.setAttribute("data-node-note", "1");
      bubble.setAttribute("contenteditable", "true");
      bubble.setAttribute("spellcheck", "false");
      bubble.setAttribute("title", "双击编辑附着注释");
      bubble.textContent = text;
      el.appendChild(bubble);
      bindNodeNoteBubble(n, bubble);
    })();

    layoutPorts(n);
    requestAnimationFrame(() => { layoutPorts(n); drawWires(); });

    const header = el.querySelector("header");
    header.addEventListener("pointerdown", (e) => {
      if (e.target.closest("[data-del]") || e.target.closest("[data-collapse]")) return;
      if (e.button != null && e.button !== 0) return;
      if (spaceDown || e.altKey) return;
      e.preventDefault();
      e.stopPropagation();
      if (e.shiftKey || e.ctrlKey || e.metaKey) {
        selectNode(n.id, { additive: true, toggle: true });
      } else if (!(typeof selectedIds !== "undefined" && selectedIds.has(n.id))) {
        selectNode(n.id);
      } else {
        selectedId = n.id;
      }
      selectedWire = null;
      endWireDraft(false);
      const pt = canvasPointFromClient(e.clientX, e.clientY);
      const group = (typeof selectedNodeList === "function") ? selectedNodeList() : [n];
      const origins = {};
      group.forEach((gn) => { origins[gn.id] = { x: gn.x, y: gn.y }; });
      drag = {
        id: n.id,
        ox: pt.x - n.x,
        oy: pt.y - n.y,
        startX: pt.x,
        startY: pt.y,
        origins,
        groupIds: group.map((g) => g.id),
        pointerId: e.pointerId,
        histPushed: false
      };
      try { header.setPointerCapture(e.pointerId); } catch (_) {}
      header.classList.add("dragging");
    });
    header.addEventListener("pointermove", (e) => {
      if (!drag || drag.id !== n.id) return;
      if (drag.pointerId != null && e.pointerId !== drag.pointerId) return;
      const pt = canvasPointFromClient(e.clientX, e.clientY);
      const dx = pt.x - drag.startX;
      const dy = pt.y - drag.startY;
      if (!drag.histPushed && (Math.abs(dx) > 0.5 || Math.abs(dy) > 0.5)) {
        /* push pre-move snapshot once (positions still at origins) */
        const ids0 = drag.groupIds || [n.id];
        const cur = {};
        ids0.forEach((gid) => {
          const gn = nodeById(gid);
          if (gn) cur[gid] = { x: gn.x, y: gn.y };
        });
        ids0.forEach((gid) => {
          const gn = nodeById(gid);
          const o = drag.origins[gid];
          if (gn && o) { gn.x = o.x; gn.y = o.y; }
        });
        if (typeof pushHistory === "function") pushHistory("移动节点");
        ids0.forEach((gid) => {
          const gn = nodeById(gid);
          if (gn && cur[gid]) { gn.x = cur[gid].x; gn.y = cur[gid].y; }
        });
        drag.histPushed = true;
      }
      const ids = drag.groupIds || [n.id];
      ids.forEach((gid) => {
        const gn = nodeById(gid);
        if (!gn || !drag.origins[gid]) return;
        gn.x = drag.origins[gid].x + dx;
        gn.y = drag.origins[gid].y + dy;
        if (gn.el) {
          gn.el.style.left = gn.x + "px";
          gn.el.style.top = gn.y + "px";
        }
      });
      ensureCanvasSize();
      drawWires();
      if (e && e.clientX != null) nudgeEdgeAutoPan(e.clientX, e.clientY, "drag");
    });
    const endDrag = (e) => {
      stopEdgeAutoPan();
      if (!drag || drag.id !== n.id) return;
      if (e && e.pointerId != null && drag.pointerId !== e.pointerId) return;
      const pid = drag.pointerId;
      drag = null;
      header.classList.remove("dragging");
      try { if (pid != null) header.releasePointerCapture(pid); } catch (_) {}
    };
    header.addEventListener("pointerup", endDrag);
    header.addEventListener("pointercancel", endDrag);
    header.addEventListener("lostpointercapture", () => {
      if (drag && drag.id === n.id) {
        stopEdgeAutoPan();
        drag = null;
        header.classList.remove("dragging");
      }
    });

    const colBtn = el.querySelector("[data-collapse]");
    if (colBtn) {
      colBtn.addEventListener("click", (e) => {
        e.stopPropagation();
        togglePortFold(n);
      });
    }
    el.querySelectorAll("input[data-port-lit]").forEach((inp) => {
      inp.addEventListener("pointerdown", (e) => e.stopPropagation());
      inp.addEventListener("click", (e) => e.stopPropagation());
      const applyLit = () => {
        if (!n.cfg) n.cfg = {};
        if (!n.cfg.portValues) n.cfg.portValues = {};
        const p = inp.getAttribute("data-port-lit");
        n.cfg.portValues[p] = inp.value;
        if (n.el) {
          const metaEl = n.el.querySelector(".meta");
          if (metaEl) metaEl.textContent = metaOf(n);
        }
        /* Out / print preview for unconnected literals without full run */
        if ((n.type === "out" || n.type === "print" || n.type === "fn_return") &&
            (p === "in" || p === "value" || p === "a")) {
          const lit = String(inp.value || "").trim();
          if (lit) {
            n.result = lit;
            const resEl = n.el && n.el.querySelector("[data-result]");
            if (resEl) resEl.textContent = lit;
          } else {
            n.result = "";
            const resEl = n.el && n.el.querySelector("[data-result]");
            if (resEl) resEl.textContent = "";
          }
        }
        refreshCollapseHints();
        drawWires();
        if (selectedId === n.id) renderInspector();
      };
      inp.addEventListener("change", applyLit);
      inp.addEventListener("keydown", (e) => {
        if (e.key === "Enter") { e.preventDefault(); applyLit(); inp.blur(); }
      });
    });
    el.querySelector("[data-del]").addEventListener("click", (e) => {
      e.stopPropagation();
      removeNode(n.id);
    });

    el.addEventListener("dblclick", (e) => {
      if (e.target.closest(".port")) return;
      if (n.collapsed) { setNodeCollapsed(n, false); return; }
      if (n.type === "comment" || n.type.startsWith("const") || n.type === "shift" || n.type === "cast" ||
        (n.type === "rng" && (n.op === "seed" || n.op === "u64_mod")) ||
        n.type === "set_var" || n.type === "get_var" || n.type === "for" ||
        n.type === "sequence" || n.type === "while") {
        openConfigDialog(n, false);
      }
    });

    el.addEventListener("pointerdown", (e) => {
      if (e.target.closest(".port") || e.target.closest("[data-del]") || e.target.closest("header") || e.target.closest("[data-collapse]")) return;
      selectNode(n.id);
    });
    el.addEventListener("contextmenu", (e) => {
      e.preventDefault();
      e.stopPropagation();
      selectNode(n.id);
      showCtxMenu(e.clientX, e.clientY, { kind: "node", id: n.id });
    });

    el.querySelectorAll(".port").forEach((p) => {
      p.addEventListener("pointerdown", (e) => {
        e.stopPropagation();
        e.preventDefault();
        if (e.button != null && e.button !== 0) return;
        const port = p.dataset.port;
        const dir = p.dataset.dir;
        if (dir === "out") {
          if (linkFrom && linkFrom.dir === "in") {
            finishWireTo(n.id, linkFrom.nodeId, linkFrom.port, port);
          } else {
            beginWireDrag(n.id, port, e, "out");
          }
        } else {
          if (wireDraft && wireDraft.dir === "out") {
            const fromId = wireDraft.fromId;
            const fp = wireDraft.fromPort;
            wireDraft._done = true;
            finishWireTo(fromId, n.id, port, fp);
          } else if (linkFrom && linkFrom.dir !== "in") {
            const fromId = linkFrom.nodeId;
            finishWireTo(fromId, n.id, port, linkFrom.port);
          } else {
            beginWireDrag(n.id, port, e, "in");
          }
        }
      });
      /* Drop on port: pointerup on target port completes the wire even if hit-test misses. */
      p.addEventListener("pointerup", (e) => {
        if (!wireDraft || wireDraft._done) return;
        if (e.button != null && e.button !== 0) return;
        const port = p.dataset.port;
        const dir = p.dataset.dir || (p.classList.contains("out") ? "out" : "in");
        const sdir = wireDraft.dir || "out";
        const targetDir = sdir === "out" ? "in" : "out";
        if (dir !== targetDir) return;
        if (wireDraft.fromId === n.id) return;
        const sn = nodeById(wireDraft.fromId);
        const se = !!(isExecPortOnNode(sn, wireDraft.fromPort) || isExecPort(wireDraft.fromPort));
        const te = !!(isExecPortOnNode(n, port) || isExecPort(port));
        if (se !== te) return; /* no false magnet connect */
        e.stopPropagation();
        wireDraft._done = true;
        if (sdir === "out") finishWireTo(wireDraft.fromId, n.id, port, wireDraft.fromPort);
        else finishWireTo(n.id, wireDraft.fromId, wireDraft.fromPort, port);
      });
      p.addEventListener("pointerenter", () => {
        if (!wireDraft || wireDraft._done) return;
        const dir = p.dataset.dir || (p.classList.contains("out") ? "out" : "in");
        const sdir = wireDraft.dir || "out";
        const targetDir = sdir === "out" ? "in" : "out";
        if (dir !== targetDir) return;
        if (wireDraft.fromId === n.id) return;
        /* exec only magnets to exec; data only to data */
        const sn = nodeById(wireDraft.fromId);
        const se = !!(isExecPortOnNode(sn, wireDraft.fromPort) || isExecPort(wireDraft.fromPort));
        const te = !!(isExecPortOnNode(n, p.dataset.port) || isExecPort(p.dataset.port));
        if (se !== te) return;
        wireDraft.hover = {
          nodeId: n.id,
          port: p.dataset.port,
          dir: dir,
          el: p,
          dist: 0
        };
        highlightDropTarget(wireDraft.hover, targetDir);
      });
    });
  }

  function applyViewTransform() {
    const world = $id("world");
    const s = (view.scale > 0 && Number.isFinite(view.scale)) ? view.scale : 1;
    view.scale = s;
    if (world) {
      world.style.transform = "translate(" + view.x + "px," + view.y + "px) scale(" + s + ")";
    }
    const ws = $id("workspace");
    if (ws) {
      /* grid pans with view; size scales so cells stay under nodes */
      const g = 24 * s;
      ws.style.backgroundSize = g + "px " + g + "px";
      ws.style.backgroundPosition = view.x + "px " + view.y + "px";
    }
  }

  function setView(x, y, redraw) {
    view.x = x;
    view.y = y;
    applyViewTransform();
    if (redraw !== false) drawWires();
  }

  function setViewScale(scale, anchorClientX, anchorClientY, redraw) {
    let s = Number(scale);
    if (!Number.isFinite(s) || s <= 0) s = 1;
    s = Math.max(VIEW_SCALE_MIN, Math.min(VIEW_SCALE_MAX, s));
    const ws = $id("workspace");
    let px, py;
    if (ws && anchorClientX != null && anchorClientY != null) {
      const wr = ws.getBoundingClientRect();
      px = anchorClientX - wr.left;
      py = anchorClientY - wr.top;
    } else if (ws) {
      const wr = ws.getBoundingClientRect();
      px = wr.width / 2;
      py = wr.height / 2;
    } else {
      px = 0;
      py = 0;
    }
    const old = (view.scale > 0 && Number.isFinite(view.scale)) ? view.scale : 1;
    if (Math.abs(s - old) < 1e-12) {
      if (redraw !== false) drawWires();
      return;
    }
    /* keep world point under cursor fixed: wx = (px - x) / old */
    const wx = (px - view.x) / old;
    const wy = (py - view.y) / old;
    view.scale = s;
    view.x = px - wx * s;
    view.y = py - wy * s;
    applyViewTransform();
    if (redraw !== false) drawWires();
  }

  function zoomBy(factor, anchorClientX, anchorClientY) {
    const f = Number(factor);
    if (!Number.isFinite(f) || f <= 0) return;
    const old = (view.scale > 0 && Number.isFinite(view.scale)) ? view.scale : 1;
    setViewScale(old * f, anchorClientX, anchorClientY, true);
  }

  function panBy(dx, dy) {
    setView(view.x + dx, view.y + dy, true);
  }

  /* Edge auto-pan while wiring / dragging / marquee near viewport border (UE-style).
   * Important: do NOT mutate drag.startX/startY when the view pans.
   * canvasPointFromClient already tracks view.x/y, so re-applying positions from the
   * current client coords keeps the node glued under the cursor. Mutating startX was
   * double-compensating and caused cursor/node desync. */
  let edgePanRaf = null;
  let edgePanState = null; /* { clientX, clientY, mode: 'wire'|'drag'|'marquee', lastTs } */

  function stopEdgeAutoPan() {
    edgePanState = null;
    if (edgePanRaf != null) {
      try { cancelAnimationFrame(edgePanRaf); } catch (_) {}
      edgePanRaf = null;
    }
  }

  function edgePanSpeed(clientX, clientY, dtMs) {
    const ws = $id("workspace");
    if (!ws) return { dx: 0, dy: 0 };
    const r = ws.getBoundingClientRect();
    const margin = 48;
    /* max ~280 screen-px/s at full pressure (gentle; was ~1300px/s and felt runaway) */
    const maxPxPerSec = 280;
    const dt = Math.max(0.001, Math.min(0.05, (dtMs != null ? dtMs : 16.67) / 1000));
    const maxSp = maxPxPerSec * dt;
    let dx = 0, dy = 0;
    const leftDist = clientX - r.left;
    const rightDist = r.right - clientX;
    const topDist = clientY - r.top;
    const botDist = r.bottom - clientY;
    function ease(dist) {
      const t = Math.min(1, Math.max(0, (margin - dist) / margin));
      /* smoother near band edge, stronger only deep in margin */
      return t * t;
    }
    if (leftDist < margin) dx = maxSp * ease(leftDist);
    else if (rightDist < margin) dx = -maxSp * ease(rightDist);
    if (topDist < margin) dy = maxSp * ease(topDist);
    else if (botDist < margin) dy = -maxSp * ease(botDist);
    if (!dx && !dy) return { dx: 0, dy: 0 };
    return { dx: dx, dy: dy };
  }

  function applyDragPositionsFromClient(clientX, clientY) {
    if (!drag) return;
    if (drag.startX == null || drag.startY == null) {
      /* legacy ox/oy only — keep node under cursor via absolute grab offset */
      const pt = canvasPointFromClient(clientX, clientY);
      if (drag.ox != null && drag.id) {
        const gn = nodeById(drag.id);
        if (gn) {
          gn.x = pt.x - drag.ox;
          gn.y = pt.y - drag.oy;
          if (gn.el) {
            gn.el.style.left = gn.x + "px";
            gn.el.style.top = gn.y + "px";
          }
        }
      }
      ensureCanvasSize();
      drawWires();
      return;
    }
    const pt = canvasPointFromClient(clientX, clientY);
    const ddx = pt.x - drag.startX;
    const ddy = pt.y - drag.startY;
    const ids = drag.groupIds || (drag.id ? [drag.id] : []);
    ids.forEach((gid) => {
      const gn = nodeById(gid);
      if (!gn || !drag.origins || !drag.origins[gid]) return;
      gn.x = drag.origins[gid].x + ddx;
      gn.y = drag.origins[gid].y + ddy;
      if (gn.el) {
        gn.el.style.left = gn.x + "px";
        gn.el.style.top = gn.y + "px";
      }
    });
    ensureCanvasSize();
    drawWires();
  }

  function applyMarqueeFromClient(clientX, clientY) {
    if (!marquee) return;
    const pt = canvasPointFromClient(clientX, clientY);
    marquee.x1 = pt.x;
    marquee.y1 = pt.y;
    /* keep anchor fixed in world; only free corner follows cursor after pan */
    if (typeof updateMarqueeEl === "function") {
      try { updateMarqueeEl(); } catch (_) {}
    } else {
      const el = document.getElementById("marquee");
      if (el) {
        const x = Math.min(marquee.x0, marquee.x1);
        const y = Math.min(marquee.y0, marquee.y1);
        const w = Math.abs(marquee.x1 - marquee.x0);
        const h = Math.abs(marquee.y1 - marquee.y0);
        el.style.display = "block";
        el.style.left = x + "px";
        el.style.top = y + "px";
        el.style.width = w + "px";
        el.style.height = h + "px";
      }
    }
  }

  function edgeAutoPanTick(ts) {
    edgePanRaf = null;
    if (!edgePanState) return;
    const now = (typeof ts === "number" && Number.isFinite(ts)) ? ts : (performance.now ? performance.now() : Date.now());
    const last = edgePanState.lastTs != null ? edgePanState.lastTs : now;
    const dtMs = Math.max(0, Math.min(50, now - last));
    edgePanState.lastTs = now;
    const cx = edgePanState.clientX, cy = edgePanState.clientY;
    const sp = edgePanSpeed(cx, cy, dtMs);
    if (!(sp.dx || sp.dy)) {
      /* left the edge band: pause until next nudge */
      return;
    }
    /* pan view only — do not touch drag.startX/startY (see comment above) */
    panBy(sp.dx, sp.dy);
    if (edgePanState.mode === "drag" && drag) {
      applyDragPositionsFromClient(cx, cy);
    } else if (edgePanState.mode === "wire" && wireDraft) {
      const p = canvasPointFromClient(cx, cy);
      wireDraft.x = p.x;
      wireDraft.y = p.y;
      wireDraft.clientX = cx;
      wireDraft.clientY = cy;
      try {
        const fakeEv = { clientX: cx, clientY: cy };
        const hit = (typeof resolveHit === "function") ? resolveHit(fakeEv) : null;
        if (hit) wireDraft.hover = hit;
        const targetDir = wireDraft.dir === "out" ? "in" : "out";
        if (typeof highlightDropTarget === "function") highlightDropTarget(hit, targetDir);
      } catch (_) {}
      drawWires();
    } else if (edgePanState.mode === "marquee" && marquee) {
      applyMarqueeFromClient(cx, cy);
    }
    if (edgePanState) {
      edgePanRaf = requestAnimationFrame(edgeAutoPanTick);
    }
  }

  function nudgeEdgeAutoPan(clientX, clientY, mode) {
    if (clientX == null || clientY == null) return;
    const sp = edgePanSpeed(clientX, clientY, 16.67);
    if (!(sp.dx || sp.dy)) {
      if (edgePanState) stopEdgeAutoPan();
      return;
    }
    const prevMode = edgePanState && edgePanState.mode;
    edgePanState = {
      clientX: clientX,
      clientY: clientY,
      mode: mode || "wire",
      lastTs: (edgePanState && edgePanState.lastTs != null)
        ? edgePanState.lastTs
        : (performance.now ? performance.now() : Date.now())
    };
    if (prevMode && prevMode !== edgePanState.mode) {
      /* mode switch: reset dt baseline */
      edgePanState.lastTs = performance.now ? performance.now() : Date.now();
    }
    if (edgePanRaf == null) {
      edgePanRaf = requestAnimationFrame(edgeAutoPanTick);
    }
  }


  // workspace touch: two-finger pan + pinch zoom; single-finger empty pan on narrow screens
  (function setupTouchPan() {
    const ws = $id("workspace");
    if (!ws) return;
    let mode = null; /* "pan" | "pinch" */
    let last = null; /* { x, y } midpoint or single finger */
    let lastDist = 0;
    function midOf(t0, t1) {
      return { x: (t0.clientX + t1.clientX) / 2, y: (t0.clientY + t1.clientY) / 2 };
    }
    function distOf(t0, t1) {
      return Math.hypot(t1.clientX - t0.clientX, t1.clientY - t0.clientY);
    }
    function interactiveTouchTarget(target) {
      return !!(target && target.closest && target.closest(".block, .port, button, input, textarea, select, .ctx-menu, dialog"));
    }
    ws.addEventListener("touchstart", (e) => {
      if (e.touches.length === 2) {
        mode = "pinch";
        const t0 = e.touches[0], t1 = e.touches[1];
        last = midOf(t0, t1);
        lastDist = distOf(t0, t1) || 1;
        e.preventDefault();
      } else if (e.touches.length === 1) {
        if (interactiveTouchTarget(e.target)) return;
        if (window.matchMedia("(max-width: 1100px)").matches) {
          mode = "pan";
          last = { x: e.touches[0].clientX, y: e.touches[0].clientY };
          lastDist = 0;
        }
      }
    }, { passive: false });
    ws.addEventListener("touchmove", (e) => {
      if (!mode || !last) return;
      if (mode === "pinch" && e.touches.length >= 2) {
        const t0 = e.touches[0], t1 = e.touches[1];
        const mid = midOf(t0, t1);
        const dist = distOf(t0, t1) || 1;
        const dx = mid.x - last.x;
        const dy = mid.y - last.y;
        if (dx || dy) panBy(dx, dy);
        if (lastDist > 0) {
          const factor = dist / lastDist;
          if (Number.isFinite(factor) && factor > 0 && Math.abs(factor - 1) > 1e-4) {
            zoomBy(factor, mid.x, mid.y);
          }
        }
        last = mid;
        lastDist = dist;
        e.preventDefault();
        return;
      }
      if (mode === "pan" && e.touches.length === 1) {
        const x = e.touches[0].clientX, y = e.touches[0].clientY;
        panBy(x - last.x, y - last.y);
        last = { x, y };
        e.preventDefault();
      }
    }, { passive: false });
    const end = (e) => {
      if (e.touches && e.touches.length >= 2) {
        mode = "pinch";
        const t0 = e.touches[0], t1 = e.touches[1];
        last = midOf(t0, t1);
        lastDist = distOf(t0, t1) || 1;
        return;
      }
      if (e.touches && e.touches.length === 1 && mode === "pinch") {
        if (window.matchMedia("(max-width: 1100px)").matches && !interactiveTouchTarget(e.target)) {
          mode = "pan";
          last = { x: e.touches[0].clientX, y: e.touches[0].clientY };
          lastDist = 0;
          return;
        }
      }
      mode = null;
      last = null;
      lastDist = 0;
    };
    ws.addEventListener("touchend", end);
    ws.addEventListener("touchcancel", end);
  })();


  function resetView() {
    view.scale = 1;
    setView(0, 0, true);
    status("视图已重置到原点 (100%)", "ok");
  }

  function nodeExtent() {
    const list = nodes();
    let minX = 0, minY = 0, maxX = CANVAS_MIN_W, maxY = CANVAS_MIN_H;
    if (!list.length) {
      return { minX: -CANVAS_PAD, minY: -CANVAS_PAD, maxX: CANVAS_MIN_W, maxY: CANVAS_MIN_H };
    }
    minX = Infinity; minY = Infinity; maxX = -Infinity; maxY = -Infinity;
    list.forEach((n) => {
      const w = (n.el && n.el.offsetWidth) || 200;
      const h = (n.el && n.el.offsetHeight) || 120;
      minX = Math.min(minX, n.x);
      minY = Math.min(minY, n.y);
      maxX = Math.max(maxX, n.x + w);
      maxY = Math.max(maxY, n.y + h);
    });
    if (!Number.isFinite(minX)) {
      return { minX: -CANVAS_PAD, minY: -CANVAS_PAD, maxX: CANVAS_MIN_W, maxY: CANVAS_MIN_H };
    }
    return {
      minX: minX - CANVAS_PAD,
      minY: minY - CANVAS_PAD,
      maxX: maxX + CANVAS_PAD,
      maxY: maxY + CANVAS_PAD
    };
  }

  /* Resize wire SVG to cover all nodes (incl. negative coords). Canvas itself is unbounded. */
  function ensureCanvasSize() {
    const wires = $id("wires");
    if (!wires) return;
    const ext = nodeExtent();
    const w = Math.max(CANVAS_MIN_W, Math.ceil(ext.maxX - ext.minX));
    const h = Math.max(CANVAS_MIN_H, Math.ceil(ext.maxY - ext.minY));
    const ox = Math.floor(ext.minX);
    const oy = Math.floor(ext.minY);
    wires.style.left = ox + "px";
    wires.style.top = oy + "px";
    wires.style.width = w + "px";
    wires.style.height = h + "px";
    wires.setAttribute("width", w);
    wires.setAttribute("height", h);
    wires.setAttribute("viewBox", ox + " " + oy + " " + w + " " + h);
  }

  /* Logical canvas coords: account for world pan + scale transform. */
  function canvasPointFromClient(clientX, clientY) {
    const ws = $id("workspace");
    if (!ws) return { x: 0, y: 0 };
    const wr = ws.getBoundingClientRect();
    const s = (view.scale > 0 && Number.isFinite(view.scale)) ? view.scale : 1;
    return {
      x: (clientX - wr.left - view.x) / s,
      y: (clientY - wr.top - view.y) / s
    };
  }

  function canvasPointFromEvent(e) {
    return canvasPointFromClient(e.clientX, e.clientY);
  }

  /* Hit-test ports in viewport coords. preferDir: "in" | "out" | null (any). */
  function hitTestPortClient(clientX, clientY, excludeNodeId, preferDir, wantExec) {
    /* Prefer ports over wires; expanded hit box + port-label fallback. */
    const HIT_R = 110;
    let best = null;
    let bestD = HIT_R;
    const sel = preferDir === "out" ? "#canvas .port.out"
      : preferDir === "in" ? "#canvas .port.in"
      : "#canvas .port";
    document.querySelectorAll(sel).forEach((el) => {
      const block = el.closest(".block");
      if (!block) return;
      const nodeId = block.dataset.id;
      if (!nodeId || nodeId === excludeNodeId) return;
      const portName0 = el.dataset.port;
      if (wantExec != null) {
        const node0 = nodeById(nodeId);
        const pe = !!(isExecPortOnNode(node0, portName0) || isExecPort(portName0));
        if (pe !== !!wantExec) return;
      }
      const r = el.getBoundingClientRect();
      const pad = 12;
      const cx = r.left + r.width / 2;
      const cy = r.top + r.height / 2;
      const inside =
        clientX >= r.left - pad && clientX <= r.right + pad &&
        clientY >= r.top - pad && clientY <= r.bottom + pad;
      const d = Math.hypot(cx - clientX, cy - clientY);
      const score = inside ? Math.min(d, 4) : d;
      if (score <= bestD) {
        bestD = score;
        best = {
          nodeId: nodeId,
          port: el.dataset.port,
          dir: el.dataset.dir || (el.classList.contains("out") ? "out" : "in"),
          dist: d,
          el: el
        };
      }
    });
    if (!best || bestD > 28) {
      const labSel = preferDir === "out" ? "#canvas .port-label.out"
        : preferDir === "in" ? "#canvas .port-label.in"
        : "#canvas .port-label";
      document.querySelectorAll(labSel).forEach((lab) => {
        const block = lab.closest(".block");
        if (!block) return;
        const nodeId = block.dataset.id;
        if (!nodeId || nodeId === excludeNodeId) return;
        const r = lab.getBoundingClientRect();
        if (clientX < r.left - 6 || clientX > r.right + 6 || clientY < r.top - 6 || clientY > r.bottom + 6) return;
        const portName = lab.dataset.port;
        if (wantExec != null) {
          const nodeL = nodeById(nodeId);
          const peL = !!(isExecPortOnNode(nodeL, portName) || isExecPort(portName));
          if (peL !== !!wantExec) return;
        }
        const dir = lab.classList.contains("out") ? "out" : "in";
        if (preferDir && preferDir !== dir) return;
        const portEl = block.querySelector('.port.' + dir + '[data-port="' + portName + '"]');
        bestD = 0;
        best = {
          nodeId: nodeId,
          port: portName,
          dir: dir,
          dist: 0,
          el: portEl || lab
        };
      });
    }
    if (!best && document.elementsFromPoint) {
      const stack = document.elementsFromPoint(clientX, clientY) || [];
      for (let i = 0; i < stack.length; i++) {
        const el = stack[i];
        const port = el.closest && el.closest("#canvas .port");
        if (!port) continue;
        if (preferDir === "in" && !port.classList.contains("in")) continue;
        if (preferDir === "out" && !port.classList.contains("out")) continue;
        const block = port.closest(".block");
        const nodeId = block && block.dataset.id;
        if (!nodeId || nodeId === excludeNodeId) continue;
        if (wantExec != null) {
          const peE = !!(isExecPortOnNode(nodeById(nodeId), port.dataset.port) || isExecPort(port.dataset.port));
          if (peE !== !!wantExec) continue;
        }
        best = {
          nodeId: nodeId,
          port: port.dataset.port,
          dir: port.dataset.dir || (port.classList.contains("out") ? "out" : "in"),
          dist: 0,
          el: port
        };
        break;
      }
    }
    return best;
  }

  function hitTestInputPortClient(clientX, clientY, excludeNodeId) {
    return hitTestPortClient(clientX, clientY, excludeNodeId, "in");
  }

  function hitTestInputPortAt(x, y, excludeNodeId) {
    const canvas = $id("canvas");
    if (!canvas) return null;
    const cr = canvas.getBoundingClientRect();
    return hitTestInputPortClient(cr.left + x, cr.top + y, excludeNodeId);
  }

  function clearPortHighlights() {
    document.querySelectorAll(".port.linking, .port.drop-target").forEach((x) => {
      x.classList.remove("linking");
      x.classList.remove("drop-target");
    });
  }

  function highlightDropTarget(hit, preferDir) {
    if (hit && typeof wireDraft !== "undefined" && wireDraft && wireDraft.fromId != null) {
      const sn = nodeById(wireDraft.fromId);
      const se = !!(isExecPortOnNode(sn, wireDraft.fromPort) || isExecPort(wireDraft.fromPort));
      const tn = nodeById(hit.nodeId);
      const te = !!(isExecPortOnNode(tn, hit.port) || isExecPort(hit.port));
      if (se !== te) hit = null;
    }
    
    document.querySelectorAll(".port.drop-target").forEach((x) => x.classList.remove("drop-target"));
    if (!hit) return;
    if (hit.el) {
      hit.el.classList.add("drop-target");
      return;
    }
    const node = nodeById(hit.nodeId);
    if (!node || !node.el) return;
    const dir = preferDir || hit.dir || "in";
    const el = node.el.querySelector('.port.' + dir + '[data-port="' + hit.port + '"]');
    if (el) el.classList.add("drop-target");
  }

  function detachWireDocListeners() {
    if (!wireDraft) return;
    if (wireDraft._onMove) {
      document.removeEventListener("pointermove", wireDraft._onMove, true);
      document.removeEventListener("pointerup", wireDraft._onUp, true);
      document.removeEventListener("pointercancel", wireDraft._onCancel, true);
      document.removeEventListener("mouseup", wireDraft._onMouseUp, true);
      window.removeEventListener("pointerup", wireDraft._onUp, true);
      window.removeEventListener("mouseup", wireDraft._onMouseUp, true);
    }
    if (wireDraft._onOver) {
      document.removeEventListener("pointerover", wireDraft._onOver, true);
    }
    if (wireDraft.captureEl && wireDraft.pointerId != null) {
      try { wireDraft.captureEl.releasePointerCapture(wireDraft.pointerId); } catch (_) {}
    }
  }

  function endWireDraft(redraw) {
    detachWireDocListeners();
    stopEdgeAutoPan();
    wireDraft = null;
    if (redraw !== false) drawWires();
  }

  function finishWireTo(fromId, toId, toPort, fromPort) {
    if (!fromId || !toId || !toPort) return false;
    let ok = false;
    let errMsg = null;
    const stEl = $id("status");
    const stBefore = (stEl && stEl.textContent) || "";
    try {
      if (typeof pushHistory === "function") pushHistory("连线");
      if (typeof historyQuiet !== "undefined") historyQuiet = true;
      try {
        ok = !!connect(fromId, toId, toPort, fromPort);
      } finally {
        if (typeof historyQuiet !== "undefined") historyQuiet = false;
      }
      if (!ok && typeof historyPast !== "undefined" && historyPast.length) {
        /* connect failed: drop the optimistic history entry if it is ours */
        try {
          const last = historyPast[historyPast.length - 1];
          if (last && last.label === "连线") historyPast.pop();
          if (typeof updateUndoUi === "function") updateUndoUi();
        } catch (_) {}
      }
    } catch (err) {
      ok = false;
      errMsg = err && err.message ? err.message : String(err);
      if (typeof historyQuiet !== "undefined") historyQuiet = false;
      try {
        if (typeof historyPast !== "undefined" && historyPast.length) {
          const last = historyPast[historyPast.length - 1];
          if (last && last.label === "连线") historyPast.pop();
          if (typeof updateUndoUi === "function") updateUndoUi();
        }
      } catch (_) {}
    }
    endWireDraft(false);
    clearPortHighlights();
    linkFrom = null;
    const stAfter = (stEl && stEl.textContent) || "";
    /* Prefer connect()/auto-cast messages; only set generic ones if status unchanged */
    if (ok) {
      if (stAfter === stBefore || !stAfter) status("已连线", "ok");
    } else if (errMsg) {
      status("连线失败: " + errMsg, "err");
    } else if (stAfter === stBefore) {
      status("连线失败（自环 / 无法转换）", "err");
    }
    drawWires();
    return ok;
  }

  function beginWireDrag(fromId, fromPort, e, dir) {
    dir = dir || "out";
    endWireDraft(false);
    clearPortHighlights();
    const c = portCenter(fromId, fromPort, dir === "out" ? "out" : "in");
    const pt = canvasPointFromEvent(e);
    const pointerId = e.pointerId != null ? e.pointerId : 1;
    linkFrom = { nodeId: fromId, port: fromPort, dir: dir };

    const srcEl = nodeById(fromId) && nodeById(fromId).el
      ? nodeById(fromId).el.querySelector('.port.' + dir + '[data-port="' + fromPort + '"]')
      : null;
    if (srcEl) srcEl.classList.add("linking");

    /* When dragging from out, we look for in; from in, we look for out. */
    const targetDir = dir === "out" ? "in" : "out";
    const srcNode = nodeById(fromId);
    const srcWantExec = !!(isExecPortOnNode(srcNode, fromPort) || isExecPort(fromPort));
    const portsCompatible = (hit) => {
      if (!hit || !hit.nodeId || hit.port == null) return false;
      if (hit.nodeId === fromId) return false;
      const hn = nodeById(hit.nodeId);
      const he = !!(isExecPortOnNode(hn, hit.port) || isExecPort(hit.port));
      return he === srcWantExec;
    };

    const resolveHit = (ev) => {
      const cx = ev.clientX, cy = ev.clientY;
      let hit = hitTestPortClient(cx, cy, fromId, targetDir, srcWantExec);
      if (hit && !portsCompatible(hit)) hit = null;
      if (!hit && wireDraft && wireDraft.hover) {
        const sticky = wireDraft.hover;
        const node = nodeById(sticky.nodeId);
        const el = sticky.el || (node && node.el && node.el.querySelector('.port.' + targetDir + '[data-port="' + sticky.port + '"]'));
        if (el) {
          const r = el.getBoundingClientRect();
          const d = Math.hypot(r.left + r.width / 2 - cx, r.top + r.height / 2 - cy);
          if (d <= 96) {
          const cand = { nodeId: sticky.nodeId, port: sticky.port, dir: targetDir, dist: d, el: el };
          if (portsCompatible(cand)) hit = cand;
        }
        } else {
          hit = sticky;
        }
      }
      return hit;
    };

    const onMove = (ev) => {
      if (!wireDraft) return;
      if (wireDraft.pointerId != null && ev.pointerId != null && wireDraft.pointerId !== ev.pointerId) return;
      if (ev.cancelable) ev.preventDefault();
      const p = canvasPointFromEvent(ev);
      wireDraft.x = p.x;
      wireDraft.y = p.y;
      wireDraft.clientX = ev.clientX;
      wireDraft.clientY = ev.clientY;
      wireDraft.moved = wireDraft.moved || Math.hypot(p.x - wireDraft.startX, p.y - wireDraft.startY) > 4;
      const hit = resolveHit(ev);
      if (hit) wireDraft.hover = hit;
      highlightDropTarget(hit, targetDir);
      nudgeEdgeAutoPan(ev.clientX, ev.clientY, "wire");
      drawWires();
    };

    const completeUp = (ev) => {
      if (!wireDraft) return;
      if (wireDraft._done) return;
      wireDraft._done = true;
      if (ev && ev.cancelable) try { ev.preventDefault(); } catch (_) {}
      if (ev && ev.stopPropagation) try { ev.stopPropagation(); } catch (_) {}

      const p = ev ? canvasPointFromEvent(ev) : { x: wireDraft.x, y: wireDraft.y };
      wireDraft.x = p.x;
      wireDraft.y = p.y;
      const moved = wireDraft.moved || Math.hypot(p.x - wireDraft.startX, p.y - wireDraft.startY) > 4;
      const srcId = wireDraft.fromId;
      const sport = wireDraft.fromPort;
      const sdir = wireDraft.dir || "out";

      /* Prefer last highlighted target first — user saw the glow, so connect there. */
      let hit = null;
      const lit = document.querySelector("#canvas .port.drop-target");
      if (lit) {
        const block = lit.closest(".block");
        const nid = block && block.dataset.id;
        const dirOk = (targetDir === "in" && lit.classList.contains("in"))
          || (targetDir === "out" && lit.classList.contains("out"))
          || (!targetDir);
        if (nid && nid !== srcId && dirOk) {
          hit = {
            nodeId: nid,
            port: lit.dataset.port,
            dir: lit.dataset.dir || (lit.classList.contains("out") ? "out" : "in"),
            el: lit
          };
        }
      }
      if (hit && !portsCompatible(hit)) hit = null;
      if (!hit) hit = wireDraft.hover;
      if (hit && !portsCompatible(hit)) hit = null;
      if (hit && hit.nodeId === srcId) hit = null;
      if (hit && !portsCompatible(hit)) hit = null;
      if (!hit && ev) hit = resolveHit(ev);
      if (!hit && ev && document.elementsFromPoint) {
        const stack = document.elementsFromPoint(ev.clientX, ev.clientY) || [];
        for (let i = 0; i < stack.length; i++) {
          const el = stack[i];
          const port = el.closest && el.closest("#canvas .port");
          if (!port) continue;
          if (targetDir === "in" && !port.classList.contains("in")) continue;
          if (targetDir === "out" && !port.classList.contains("out")) continue;
          const block = port.closest(".block");
          const nid = block && block.dataset.id;
          if (!nid || nid === srcId) continue;
          {
            const cand = {
              nodeId: nid,
              port: port.dataset.port,
              dir: targetDir,
              el: port
            };
            if (!portsCompatible(cand)) continue;
            hit = cand;
          }
          break;
        }
      }
      if (!hit && ev) hit = hitTestPortClient(ev.clientX, ev.clientY, srcId, targetDir, srcWantExec);
      if (hit && !portsCompatible(hit)) hit = null;
      /* Last resort: large sticky radius around last hover */
      if (!hit && wireDraft.hover && wireDraft.hover.nodeId && wireDraft.hover.nodeId !== srcId) {
        /* Prefer last highlighted target if still reasonably near, or always if drop-target class still on */
        const el = wireDraft.hover.el;
        let accept = !!(el && el.classList && el.classList.contains("drop-target"));
        if (!accept && el) {
          const r = el.getBoundingClientRect();
          const cx = ev ? ev.clientX : wireDraft.clientX;
          const cy = ev ? ev.clientY : wireDraft.clientY;
          const d = Math.hypot(r.left + r.width / 2 - cx, r.top + r.height / 2 - cy);
          accept = d <= 180;
        }
        if (!accept && !el) accept = true; /* keep sticky hover without geometry */
        if (accept && portsCompatible(wireDraft.hover)) hit = wireDraft.hover;
      }

      if (hit && hit.nodeId && hit.port) {
        /* Normalize so finishWireTo always gets (outputNode, inputNode, inputPort, fromPort) */
        if (sdir === "out") {
          finishWireTo(srcId, hit.nodeId, hit.port, sport);
        } else {
          finishWireTo(hit.nodeId, srcId, sport, hit.port);
        }
        return;
      }

      endWireDraft(false);
      if (!moved) {
        linkFrom = { nodeId: srcId, port: sport, dir: sdir };
        clearPortHighlights();
        const el = nodeById(srcId) && nodeById(srcId).el
          ? nodeById(srcId).el.querySelector('.port.' + sdir + '[data-port="' + sport + '"]')
          : null;
        if (el) el.classList.add("linking");
        status(sdir === "out"
          ? "已选输出端口，再点输入端口完成连线；Esc 取消"
          : "已选输入端口，再点输出端口完成连线；Esc 取消");
        drawWires();
      } else {
        clearPortHighlights();
        linkFrom = null;
        status("未对准目标端口（靠近圆点再松手）");
        drawWires();
      }
    };

    const onUp = (ev) => completeUp(ev);
    const onMouseUp = (ev) => completeUp(ev);
    const onCancel = (ev) => {
      if (!wireDraft) return;
      if (wireDraft.pointerId != null && ev && ev.pointerId != null && wireDraft.pointerId !== ev.pointerId) return;
      if (wireDraft._done) return;
      wireDraft._done = true;
      endWireDraft(false);
      clearPortHighlights();
      linkFrom = null;
      status("已取消连线");
      drawWires();
    };

    wireDraft = {
      fromId: fromId,
      fromPort: fromPort,
      dir: dir,
      pointerId: pointerId,
      x: pt.x,
      y: pt.y,
      clientX: e.clientX,
      clientY: e.clientY,
      startX: c.x,
      startY: c.y,
      moved: false,
      hover: null,
      captureEl: srcEl,
      _done: false,
      _onMove: onMove,
      _onUp: onUp,
      _onMouseUp: onMouseUp,
      _onCancel: onCancel
    };

    /* Capture on document, not port — avoids stuck-follow and lets us hit other ports */
    document.addEventListener("pointermove", onMove, true);
    document.addEventListener("pointerup", onUp, true);
    document.addEventListener("pointercancel", onCancel, true);
    document.addEventListener("mouseup", onMouseUp, true);
    window.addEventListener("pointerup", onUp, true);
    window.addEventListener("mouseup", onMouseUp, true);
    if (e.preventDefault) e.preventDefault();
    if (e.stopPropagation) e.stopPropagation();
    status(dir === "out" ? "拖到输入端口松手连线" : "拖到输出端口松手连线");
    drawWires();
  }

  function outputDomain(n) {
    if (!n) return null;
    if (n.type === "const_c" || n.type === "cun" || n.type === "cbin" || n.type === "r2c") return "c";
    if (isTensorNode(n)) {
      /* scalar-producing tensor ops */
      if (n.type === "arr_len" || n.type === "arr_get" || n.type === "arr_reduce" ||
          n.type === "vec_dot" || n.type === "mat_get" || n.type === "mat_dims") return "r";
      return "t";
    }
    /* c2r / real ops / flow / rng / cast all produce real domain */
    return "r";
  }

  function inputDomain(n, port) {
    if (!n) return null;
    if (n.type === "cun" || n.type === "cbin" || n.type === "c2r") return "c";
    if (n.type === "r2c") return "r";
    if (isTensorNode(n)) {
      /* index / scalar ports are real; container ports are tensor */
      if (port === "i" || port === "v" || port === "s" || port === "r" || port === "c" ||
          port === "start" || port === "end" || port === "n" || port === "first" || port === "last") {
        if (port === "v" && (n.type === "arr_set" || n.type === "arr_push" || n.type === "mat_set")) return "r";
        if (port === "s") return "r";
        if (port === "i" || port === "r" || port === "c" || port === "start" || port === "end" ||
            port === "n" || port === "first" || port === "last") return "r";
      }
      if (port === "a" || port === "b") return "t";
      return "t";
    }
    return "r";
  }

  function connect(fromId, toId, toPort, fromPort) {
    if (fromId === toId) return false;
    const a = nodeById(fromId), b = nodeById(toId);
    if (!a || !b) return false;
    const ports = portsOf(b);
    if (!ports.in || ports.in.indexOf(toPort) < 0) return false;
    const outs = portsOf(a).out || [];
    fromPort = fromPort || (outs[0] || "out");
    if (outs.indexOf(fromPort) < 0) return false;

    /* exec ↔ data must match */
    const fromIsExec = isExecPortOnNode(a, fromPort) || isExecPort(fromPort);
    const toIsExec = isExecPortOnNode(b, toPort) || isExecPort(toPort);
    if (fromIsExec !== toIsExec) {
      status("exec pin cannot connect to data pin", "err");
      return false;
    }

    let srcId = fromId;
    let srcPort = fromPort;
    if (!fromIsExec) {
      const od = outputDomain(a), idm = inputDomain(b, toPort);
      if (od !== idm) {
        const mid = insertAutoConverter(srcId, a, b, toPort, od, idm);
        if (!mid) {
          status("类型不匹配: " + od + " → " + idm + "（无法自动转换）", "err");
          return false;
        }
        srcId = mid;
        srcPort = "out";
      }
    }
    /* replace existing */
    setWires(wires().filter((w) => !(w.to === toId && w.toPort === toPort)));
    if (!b.inputs) b.inputs = {};
    if (!b.inputFromPorts) b.inputFromPorts = {};
    b.inputs[toPort] = srcId;
    b.inputFromPorts[toPort] = srcPort;
    wires().push({ from: srcId, fromPort: srcPort, to: toId, toPort: toPort });
    /* connecting: uncollapse; always re-render so wired pins hide literal boxes */
    if (a.collapsed) a.collapsed = false;
    if (b.collapsed) b.collapsed = false;
    renderNode(a);
    renderNode(b);
    refreshCollapseHints();
    drawWires();
    if (selectedId === a.id || selectedId === b.id) renderInspector();
    return true;
  }

  /** Insert cast / r2c / c2r between from→to. Returns mid node id or null. */
  function insertAutoConverter(fromId, a, b, toPort, od, idm) {
    const ax = (a.x + b.x) / 2;
    const ay = (a.y + b.y) / 2;
    let mid = null;
    if (od === "r" && idm === "c") {
      /* real → complex: re+im*i with im=0 constant */
      const zero = addNode({ type: "const_f" }, ax - 40, ay + 60, {
        cfg: Object.assign(defaultCfg("const_f"), { value: "0" })
      });
      mid = addNode({ type: "r2c", op: "set_reim" }, ax, ay);
      /* wire from → re, zero → im without re-entrancy issues */
      forceWire(fromId, mid.id, "a");
      forceWire(zero.id, mid.id, "b");
      status("已自动插入 re+im·i（im=0）", "ok");
      return mid.id;
    }
    if (od === "c" && idm === "r") {
      mid = addNode({ type: "c2r", op: "re" }, ax, ay);
      forceWire(fromId, mid.id, "a");
      status("已自动插入 Re(z) 取实部", "ok");
      return mid.id;
    }
    /* same domain real: explicit cast node (int↔float still both domain r) */
    if (od === "r" && idm === "r") {
      const f = fmtBits();
      mid = addNode({ type: "cast", op: "to_float" }, ax, ay, {
        cfg: { e_bits: f.e, m_bits: f.m, nan: f.nan, width: 32, signed: 1 }
      });
      forceWire(fromId, mid.id, "a");
      status("已自动插入 cast→float（可双击改为→int）", "ok");
      return mid.id;
    }
    /* tensor mismatches: no silent cast */
    if (od === "t" || idm === "t") return null;
    return null;
  }

  function forceWire(fromId, toId, toPort, fromPort) {
    const b = nodeById(toId);
    if (!b) return;
    const a = nodeById(fromId);
    fromPort = fromPort || ((a && portsOf(a).out[0]) || "out");
    setWires(wires().filter((w) => !(w.to === toId && w.toPort === toPort)));
    if (!b.inputs) b.inputs = {};
    if (!b.inputFromPorts) b.inputFromPorts = {};
    b.inputs[toPort] = fromId;
    b.inputFromPorts[toPort] = fromPort;
    wires().push({ from: fromId, fromPort: fromPort, to: toId, toPort: toPort });
  }

  function portCenter(nodeId, port, dir) {
    const n = nodeById(nodeId);
    if (!n || !n.el) return { x: 0, y: 0 };
    const sel = dir === "out"
      ? '.port.out[data-port="' + port + '"]'
      : '.port.in[data-port="' + port + '"]';
    const el = n.el.querySelector(sel) || n.el.querySelector(dir === "out" ? ".port.out" : ".port.in");
    if (!el) return { x: n.x + (dir === "out" ? n.el.offsetWidth : 0), y: n.y + 40 };
    const r = el.getBoundingClientRect();
    const p = canvasPointFromClient(r.left + r.width / 2, r.top + r.height / 2);
    return p;
  }

  function drawWires() {
    const svg = $id("wires");
    if (!svg) return;
    ensureCanvasSize();
    let html = "";
    wires().forEach((w, idx) => {
      const fp = w.fromPort || "out";
      const a = portCenter(w.from, fp, "out");
      const b = portCenter(w.to, w.toPort, "in");
      const dx = Math.max(40, Math.abs(b.x - a.x) * 0.45);
      const d = "M " + a.x + " " + a.y + " C " + (a.x + dx) + " " + a.y + ", " + (b.x - dx) + " " + b.y + ", " + b.x + " " + b.y;
      const sel = selectedWire === idx ? " selected" : "";
      const fa = nodeById(w.from), tb = nodeById(w.to);
      const execCls = ((fa && isExecPortOnNode(fa, fp)) || isExecPort(fp) ||
        (tb && isExecPortOnNode(tb, w.toPort)) || isExecPort(w.toPort)) ? " exec" : "";
      html += '<path class="hit" data-wi="' + idx + '" d="' + d + '" />';
      html += '<path class="line' + sel + execCls + '" data-wi="' + idx + '" d="' + d + '" />';
    });
    if (wireDraft) {
      const sdir = wireDraft.dir || "out";
      const a = portCenter(wireDraft.fromId, wireDraft.fromPort || (sdir === "out" ? "out" : "a"), sdir === "out" ? "out" : "in");
      const b = { x: wireDraft.x, y: wireDraft.y };
      let d;
      if (sdir === "out") {
        const dx = Math.max(40, Math.abs(b.x - a.x) * 0.45);
        d = "M " + a.x + " " + a.y + " C " + (a.x + dx) + " " + a.y + ", " + (b.x - dx) + " " + b.y + ", " + b.x + " " + b.y;
      } else {
        const dx = Math.max(40, Math.abs(a.x - b.x) * 0.45);
        d = "M " + a.x + " " + a.y + " C " + (a.x - dx) + " " + a.y + ", " + (b.x + dx) + " " + b.y + ", " + b.x + " " + b.y;
      }
      html += '<path class="line draft" d="' + d + '" />';
    }
    svg.innerHTML = html;
    svg.classList.toggle("drafting", !!wireDraft);
    if (wireDraft) {
      svg.querySelectorAll("path.hit").forEach((p) => { p.style.pointerEvents = "none"; });
    }
    svg.querySelectorAll("path").forEach((p) => {
      p.addEventListener("pointerdown", (e) => {
        /* Ports must win: if pointer is near any port, ignore wire hit */
        if (wireDraft) return;
        if (hitTestPortClient(e.clientX, e.clientY, null, null)) return;
        e.stopPropagation();
        const wi = parseInt(p.dataset.wi, 10);
        selectedWire = wi;
        selectedId = null;
        if (selectedIds) selectedIds.clear();
        nodes().forEach((n) => { if (n.el) n.el.classList.remove("selected"); });
        drawWires();
        status("已选中连线 #" + wi + "（Delete 删除）");
      });
      p.addEventListener("contextmenu", (e) => {
        e.preventDefault();
        e.stopPropagation();
        const wi = parseInt(p.dataset.wi, 10);
        selectedWire = wi;
        selectedId = null;
        if (selectedIds) selectedIds.clear();
        nodes().forEach((n) => { if (n.el) n.el.classList.remove("selected"); });
        drawWires();
        showCtxMenu(e.clientX, e.clientY, { kind: "wire", wireIndex: wi });
      });
    });
  }

  function clearCanvasDom() {
    const canvas = $id("canvas");
    if (canvas) canvas.innerHTML = "";
    nodes().forEach((n) => { n.el = null; });
  }

  function remountGraph() {
    clearCanvasDom();
    const list = nodes().slice();
    list.forEach((n) => renderNode(n));
    applyViewTransform();
    ensureCanvasSize();
    drawWires();
    updateWorkspaceChrome();
    renderInspector();
    renderPalette();
  }

  function updateWorkspaceChrome() {
    const mode = $id("workspaceMode");
    const back = $id("btnBackMain");
    if (editScope === "main") {
      if (mode) mode.textContent = "主画布";
      if (back) back.classList.add("hidden");
    } else {
      const fn = project.functions[editScope.slice(3)];
      if (mode) mode.textContent = "函数编辑：" + ((fn && fn.name) || editScope);
      if (back) back.classList.remove("hidden");
    }
  }

  /* ---------- Config dialog / inspector ---------- */
  function openConfigDialog(n, isNew) {
    const dlg = $id("cfgDlg");
    const fields = $id("cfgFields");
    const title = $id("cfgTitle");
    if (!dlg || !fields) return Promise.resolve(false);
    title.textContent = (isNew ? "配置 · " : "编辑 · ") + titleOf(n);
    fields.innerHTML = buildConfigFields(n);
    bindPresetClicks(fields);
    dlg.returnValue = "";
    dlg.showModal();
    return new Promise((resolve) => {
      const onClose = () => {
        dlg.removeEventListener("close", onClose);
        if (dlg.returnValue === "ok") {
          try {
            if (typeof pushHistory === "function") pushHistory("编辑节点");
            applyConfigFields(n, fields);
            refreshNodeView(n);
            renderInspector();
            status("已更新节点配置", "ok");
            resolve(true);
          } catch (err) {
            console.error(err);
            status("更新配置失败: " + (err && err.message ? err.message : err), "err");
            resolve(false);
          }
        } else {
          if (isNew) removeNode(n.id);
          resolve(false);
        }
      };
      dlg.addEventListener("close", onClose);
    });
  }

  function floatPresetButtons() {
    const presets = [
      { name: "FP16", e: 5, m: 10 },
      { name: "FP32", e: 8, m: 23 },
      { name: "FP64", e: 11, m: 52 },
      { name: "FP128", e: 15, m: 112 },
      { name: "bfloat16", e: 8, m: 7 },
      { name: "tf32", e: 8, m: 10 },
      { name: "NVFP4", e: 2, m: 1 },
      { name: "FP8 E4M3", e: 4, m: 3 },
      { name: "FP8 E5M2", e: 5, m: 2 }
    ];
    return '<div class="preset-row" data-presets="float">' +
      presets.map((p) =>
        '<button type="button" class="btn sm preset" data-e="' + p.e + '" data-m="' + p.m + '">' + p.name + "</button>"
      ).join("") + "</div>";
  }
  function intPresetButtons() {
    const presets = [
      { name: "i8", w: 8, s: 1 }, { name: "u8", w: 8, s: 0 },
      { name: "i16", w: 16, s: 1 }, { name: "u16", w: 16, s: 0 },
      { name: "i32", w: 32, s: 1 }, { name: "u32", w: 32, s: 0 },
      { name: "i64", w: 64, s: 1 }, { name: "u64", w: 64, s: 0 },
      { name: "i128", w: 128, s: 1 }, { name: "u128", w: 128, s: 0 },
      { name: "i256", w: 256, s: 1 }, { name: "u256", w: 256, s: 0 }
    ];
    return '<div class="preset-row" data-presets="int">' +
      presets.map((p) =>
        '<button type="button" class="btn sm preset" data-w="' + p.w + '" data-s="' + p.s + '">' + p.name + "</button>"
      ).join("") + "</div>";
  }
  function bindPresetClicks(root, onChange) {
    if (!root) return;
    root.querySelectorAll(".preset-row[data-presets=\"float\"] .preset").forEach((btn) => {
      btn.addEventListener("click", (e) => {
        e.preventDefault();
        const eBits = root.querySelector('[name="e_bits"]');
        const mBits = root.querySelector('[name="m_bits"]');
        if (eBits) eBits.value = btn.dataset.e;
        if (mBits) mBits.value = btn.dataset.m;
        if (typeof onChange === "function") onChange();
      });
    });
    root.querySelectorAll(".preset-row[data-presets=\"int\"] .preset").forEach((btn) => {
      btn.addEventListener("click", (e) => {
        e.preventDefault();
        const w = root.querySelector('[name="width"]');
        const signed = root.querySelector('[name="signed"]');
        if (w) w.value = btn.dataset.w;
        if (signed) signed.value = btn.dataset.s;
        if (typeof onChange === "function") onChange();
      });
    });
  }

  function buildConfigFields(n) {
    const c = n.cfg || {};
    if (n.type === "const_f") {
      return '<div class="cfg-grid">' +
        '<label class="cfg-row full">值（支持科学计数法）<input name="value" value="' + escapeHtml(c.value) + '" /></label>' +
        '<div class="cfg-row full"><span class="hint">浮点格式预设（E/M 无上限，可手改）</span>' + floatPresetButtons() + "</div>" +
        '<div class="cfg-row"><label>E bits<input name="e_bits" type="number" min="2" value="' + (c.e_bits != null ? c.e_bits : 11) + '" title="无上限" /></label>' +
        '<label>M bits<input name="m_bits" type="number" min="1" value="' + (c.m_bits != null ? c.m_bits : 52) + '" title="无上限" /></label></div>' +
        '<label class="cfg-row full"><span><input name="nan" type="checkbox" ' + (c.nan ? "checked" : "") + ' /> 允许 NaN</span></label>' +
        "</div>";
    }
    if (n.type === "const_i") {
      return '<div class="cfg-grid">' +
        '<label class="cfg-row full">值<input name="value" value="' + escapeHtml(c.value) + '" /></label>' +
        '<div class="cfg-row full"><span class="hint">整数位宽预设</span>' + intPresetButtons() + "</div>" +
        '<div class="cfg-row"><label>位宽<input name="width" type="number" min="1" title="无上限（内存约束）" value="' + (c.width != null ? c.width : 32) + '" /></label>' +
        '<label>进制<input name="base" type="number" min="2" max="36" value="' + (c.base|0) + '" /></label></div>' +
        '<label class="cfg-row full">符号<select name="signed"><option value="1"' + (c.signed ? " selected" : "") + '>有符号</option><option value="0"' + (!c.signed ? " selected" : "") + '>无符号</option></select></label>' +
        "</div>";
    }
    if (n.type === "const_bi") {
      return '<div class="cfg-grid">' +
        '<label class="cfg-row full">值（大整数 / 科学计数法如 1.5e20）<textarea name="value">' + escapeHtml(c.value) + "</textarea></label>" +
        '<label class="cfg-row full">进制<input name="base" type="number" min="2" max="36" value="' + (c.base|0) + '" /></label>' +
        "</div>";
    }
    if (n.type === "const_c") {
      return '<div class="cfg-grid">' +
        '<label class="cfg-row full">值（re,im 或 a+bi）<input name="value" value="' + escapeHtml(c.value) + '" /></label>' +
        '<div class="cfg-row full"><span class="hint">浮点格式预设</span>' + floatPresetButtons() + "</div>" +
        '<div class="cfg-row"><label>E bits<input name="e_bits" type="number" min="2" value="' + (c.e_bits != null ? c.e_bits : 11) + '" title="无上限" /></label>' +
        '<label>M bits<input name="m_bits" type="number" min="1" value="' + (c.m_bits != null ? c.m_bits : 52) + '" title="无上限" /></label></div>' +
        '<label class="cfg-row full"><span><input name="nan" type="checkbox" ' + (c.nan ? "checked" : "") + ' /> 允许 NaN</span></label>' +
        "</div>";
    }
    if (n.type === "shift") {
      return '<div class="cfg-grid"><label class="cfg-row full">移位位数<input name="bits" type="number" min="0" title="无上限（内存约束）" value="' + (c.bits|0) + '" /></label></div>';
    }
    if (n.type === "cast" && n.op === "to_float") {
      return '<div class="cfg-grid">' +
        '<div class="cfg-row full"><span class="hint">浮点格式预设</span>' + floatPresetButtons() + "</div>" +
        '<div class="cfg-row"><label>E bits<input name="e_bits" type="number" min="2" value="' + (c.e_bits != null ? c.e_bits : 11) + '" title="无上限" /></label>' +
        '<label>M bits<input name="m_bits" type="number" min="1" value="' + (c.m_bits != null ? c.m_bits : 52) + '" title="无上限" /></label></div>' +
        '<label class="cfg-row full"><span><input name="nan" type="checkbox" ' + (c.nan ? "checked" : "") + ' /> 允许 NaN</span></label></div>';
    }
    if (n.type === "cast" && n.op === "to_int") {
      return '<div class="cfg-grid">' +
        '<div class="cfg-row full"><span class="hint">整数位宽预设</span>' + intPresetButtons() + "</div>" +
        '<label class="cfg-row full">位宽（0=bigint）<input name="width" type="number" min="0" title="0=bigint；定宽无上限（内存约束）" value="' + (c.width != null ? c.width : 32) + '" /></label>' +
        '<label class="cfg-row full">符号<select name="signed"><option value="1"' + (c.signed ? " selected" : "") + '>有符号</option><option value="0"' + (!c.signed ? " selected" : "") + '>无符号</option></select></label></div>';
    }

    if (n.type === "set_var" || n.type === "get_var") {
      return '<div class="cfg-grid"><label class="cfg-row full">变量名<input name="name" value="' + escapeHtml(c.name || "x") + '" /></label></div>';
    }
    if (n.type === "for") {
      return '<div class="cfg-grid">' +
        '<label class="cfg-row full">first（可连线覆盖）<input name="first" value="' + escapeHtml(c.first != null ? c.first : "0") + '" /></label>' +
        '<label class="cfg-row full">last（半开区间 [first,last)）<input name="last" value="' + escapeHtml(c.last != null ? c.last : "10") + '" /></label>' +
        "</div>";
    }
    if (n.type === "sequence") {
      return '<div class="cfg-grid"><label class="cfg-row full">顺序输出路数 (2–8)<input name="count" type="number" min="2" max="8" value="' + ((c.count|0) || 2) + '" /></label></div>';
    }
    if (n.type === "while") {
      return '<div class="cfg-grid"><label class="cfg-row full">max_iter<input name="max_iter" type="number" min="1" value="' + escapeHtml(c.max_iter != null ? c.max_iter : "1000000") + '" /></label></div>';
    }
    if (n.type === "do_n") {
      return '<div class="cfg-grid"><label class="cfg-row full">N (default)<input name="n" value="' + escapeHtml(c.n != null ? c.n : "1") + '" /></label></div>';
    }
    if (n.type === "gate") {
      return '<div class="cfg-grid"><label class="cfg-row full">start open<select name="start_open"><option value="1"' + ((c.start_open|0) ? " selected" : "") + '>open</option><option value="0"' + (!(c.start_open|0) ? " selected" : "") + '>closed</option></select></label></div>';
    }
    if (n.type === "multi_gate") {
      return '<div class="cfg-grid"><label class="cfg-row full">outputs (2-16)<input name="count" type="number" min="2" max="16" value="' + ((c.count|0) || 2) + '" /></label></div>';
    }
    if (n.type === "switch") {
      return '<div class="cfg-grid"><label class="cfg-row full">cases (comma-separated)<input name="cases" value="' + escapeHtml(c.cases != null ? c.cases : "0,1,2") + '" /></label></div>';
    }

    if (n.type === "do_while") {
      return '<div class="cfg-grid"><label class="cfg-row full">max_iter<input name="max_iter" type="number" min="1" value="' + escapeHtml(String(c.max_iter != null ? c.max_iter : "1000000")) + '" /></label></div>';
    }
    if (n.type === "comment") {
      return '<div class="cfg-grid"><label class="cfg-row full">注释内容<textarea name="text" rows="6" class="comment-cfg">' + escapeHtml(String(c.text != null ? c.text : "")) + '</textarea></label><p class="hint">注释不参与执行与连线；导出 C 时输出为 /* ... */。</p></div>';
    }

    if (n.type === "rng" && n.op === "seed") {
      return '<div class="cfg-grid"><label class="cfg-row full">默认 seed（未连接 a 时使用）<input name="seed" value="' + escapeHtml(c.seed != null ? c.seed : "1") + '" /></label>' +
        '<p class="hint">也可把整数接到输入 a，优先使用连接值。</p></div>';
    }
    if (n.type === "rng" && n.op === "u64_mod") {
      return '<div class="cfg-grid"><label class="cfg-row full">默认 bound（未连接 a 时）<input name="bound" value="' + escapeHtml(c.bound != null ? c.bound : "100") + '" /></label>' +
        '<p class="hint">均匀整数 [0, bound)。bound 必须 ≥1。</p></div>';
    }
    /* inline port values for binary/cmp/etc */
    if (n.type === "arr_lit" || n.type === "vec_lit") {
      return '<div class="cfg-grid">' +
        '<label class="cfg-row full">元素（逗号/空格/分号或 JSON）<textarea name="value" rows="3">' + escapeHtml(c.value || "") + "</textarea></label>" +
        "</div>";
    }
    if (n.type === "mat_lit") {
      return '<div class="cfg-grid">' +
        '<label class="cfg-row full">矩阵（行用 ; 列用 ,）<textarea name="value" rows="4">' + escapeHtml(c.value || "") + "</textarea></label>" +
        "</div>";
    }
    if (n.type === "arr_map") {
      return '<div class="cfg-grid"><label class="cfg-row full">逐元运算<select name="map_op">' +
        ["add","sub","mul","div"].map((o) => '<option value="' + o + '"' + ((c.op || "add") === o ? " selected" : "") + ">" + o + "</option>").join("") +
        "</select></label></div>";
    }
    if (n.type === "arr_reduce") {
      return '<div class="cfg-grid"><label class="cfg-row full">归约<select name="red_op">' +
        ["sum","prod","min","max"].map((o) => '<option value="' + o + '"' + ((c.op || "sum") === o ? " selected" : "") + ">" + o + "</option>").join("") +
        "</select></label></div>";
    }
    if (n.type === "mat_identity") {
      return '<div class="cfg-grid"><label class="cfg-row full">阶数 n<input name="n" value="' + escapeHtml(String(c.n != null ? c.n : "3")) + '" /></label></div>';
    }
    if (n.type === "arr_from_range") {
      return '<div class="cfg-grid"><div class="cfg-row"><label>first<input name="first" value="' + escapeHtml(String(c.first != null ? c.first : "0")) +
        '" /></label><label>last<input name="last" value="' + escapeHtml(String(c.last != null ? c.last : "7")) + '" /></label></div></div>';
    }
    if (n.type === "ten_unary") {
      return '<div class="cfg-grid"><label class="cfg-row full">逐元函数<select name="ten_op">' +
        ["exp","log","sqrt","square","neg","abs","sigmoid","tanh","relu","gelu","silu","softplus"].map((o) =>
          '<option value="' + o + '"' + ((c.op || "exp") === o ? " selected" : "") + ">" + o + "</option>").join("") +
        "</select></label></div>";
    }
    if (n.type === "ten_binop") {
      return '<div class="cfg-grid"><label class="cfg-row full">逐元二元<select name="ten_binop">' +
        ["add","sub","mul","div","max","min"].map((o) =>
          '<option value="' + o + '"' + ((c.op || "add") === o ? " selected" : "") + ">" + o + "</option>").join("") +
        "</select></label></div>";
    }
    if (n.type === "reshape") {
      return '<div class="cfg-grid"><div class="cfg-row"><label>rows<input name="rows" value="' + escapeHtml(String(c.rows != null ? c.rows : "2")) +
        '" /></label><label>cols<input name="cols" value="' + escapeHtml(String(c.cols != null ? c.cols : "2")) + '" /></label></div>' +
        '<p class="hint">元素总数必须不变；可用端口 rows/cols 覆盖。</p></div>';
    }
    if (n.type === "mat_concat") {
      return '<div class="cfg-grid"><label class="cfg-row full">axis (0=行堆叠, 1=列拼接)<input name="axis" value="' + escapeHtml(String(c.axis != null ? c.axis : "0")) + '" /></label></div>';
    }
    if (n.type === "mat_slice") {
      return '<div class="cfg-grid"><div class="cfg-row"><label>r0<input name="r0" value="' + escapeHtml(String(c.r0 != null ? c.r0 : "0")) +
        '" /></label><label>r1<input name="r1" value="' + escapeHtml(String(c.r1 != null ? c.r1 : "-1")) + '" /></label></div>' +
        '<div class="cfg-row"><label>c0<input name="c0" value="' + escapeHtml(String(c.c0 != null ? c.c0 : "0")) +
        '" /></label><label>c1<input name="c1" value="' + escapeHtml(String(c.c1 != null ? c.c1 : "-1")) + '" /></label></div>' +
        '<p class="hint">半开区间；-1 表示到末尾。</p></div>';
    }
    if (n.type === "layer_norm" || n.type === "rms_norm") {
      return '<div class="cfg-grid"><label class="cfg-row full">eps<input name="eps" value="' + escapeHtml(String(c.eps != null ? c.eps : "1e-5")) + '" /></label></div>';
    }
    if (n.type === "rope") {
      return '<div class="cfg-grid"><label class="cfg-row full">base<input name="base" value="' + escapeHtml(String(c.base != null ? c.base : "10000")) + '" /></label></div>';
    }
    if (n.type === "attention_sdp") {
      return '<div class="cfg-grid"><label class="cfg-row full">scale (auto=1/√d)<input name="scale" value="' + escapeHtml(String(c.scale != null ? c.scale : "auto")) + '" /></label>' +
        '<label class="cfg-row full"><span><input name="causal" type="checkbox"' + (c.causal ? " checked" : "") + '/> causal mask</span></label>' +
        '<p class="hint">Q/K/V 为 [seq×d]；可选 mask 同形状加到 scores。weights 为注意力权重。</p></div>';
    }
    if (n.type === "sin_pe") {
      return '<div class="cfg-grid"><div class="cfg-row"><label>seq<input name="seq" value="' + escapeHtml(String(c.seq != null ? c.seq : "4")) +
        '" /></label><label>dim<input name="dim" value="' + escapeHtml(String(c.dim != null ? c.dim : "8")) + '" /></label></div></div>';
    }
    if (n.type === "mha_split" || n.type === "mha_merge") {
      return '<div class="cfg-grid"><label class="cfg-row full">heads<input name="heads" value="' + escapeHtml(String(c.heads != null ? c.heads : "2")) + '" /></label></div>';
    }

    {
      const ports = portsOf(n);
      const dataIns = (ports.in || []).filter((p) => !(isExecPortOnNode(n, p) || isExecPort(p)));
      if (dataIns.length) {
        let h = '<div class="cfg-grid"><div class="cfg-row full"><span class="hint">port literals (used when not wired)</span></div>';
        dataIns.forEach((p) => {
          /* hide literal field for wired pins */
          const wired = (n.inputs && n.inputs[p] != null && n.inputs[p] !== "") ||
            (currentGraph().wires || []).some((w) => w.to === n.id && w.toPort === p);
          if (wired) return;
          const cur = (c.portValues && c.portValues[p] != null) ? c.portValues[p] : "";
          h += '<label class="cfg-row full">port ' + escapeHtml(p) +
            '<input name="pv_' + p + '" value="' + escapeHtml(String(cur)) + '" placeholder="optional literal" /></label>';
        });
        h += "</div>";
        return h;
      }
    }
    return "<p class=\"hint\">no extra config</p>";
  }

  function applyConfigFields(n, root) {
    const g = (name) => root.querySelector('[name="' + name + '"]');
    if (!n.cfg) n.cfg = {};
    if (n.type === "const_f" || n.type === "const_c") {
      n.cfg.value = g("value").value;
      n.cfg.e_bits = Math.max(2, Number(g("e_bits").value) || 11);
      n.cfg.m_bits = Math.max(1, Number(g("m_bits").value) || 52);
      n.cfg.nan = g("nan").checked ? 1 : 0;
    } else if (n.type === "const_i") {
      n.cfg.value = g("value").value;
      n.cfg.width = Math.max(1, Number(g("width").value) || 32);
      n.cfg.base = parseInt(g("base").value, 10) || 10;
      n.cfg.signed = parseInt(g("signed").value, 10) ? 1 : 0;
    } else if (n.type === "const_bi") {
      n.cfg.value = g("value").value;
      n.cfg.base = parseInt(g("base").value, 10) || 10;
    } else if (n.type === "shift") {
      n.cfg.bits = parseInt(g("bits").value, 10) || 0;
    } else if (n.type === "cast" && n.op === "to_float") {
      n.cfg.e_bits = Math.max(2, Number(g("e_bits").value) || 11);
      n.cfg.m_bits = Math.max(1, Number(g("m_bits").value) || 52);
      n.cfg.nan = g("nan").checked ? 1 : 0;
    } else if (n.type === "cast" && n.op === "to_int") {
      n.cfg.width = parseInt(g("width").value, 10) || 0;
      n.cfg.signed = parseInt(g("signed").value, 10) ? 1 : 0;
    } else if (n.type === "rng" && n.op === "seed" && g("seed")) {
      n.cfg.seed = g("seed").value;
    } else if (n.type === "rng" && n.op === "u64_mod" && g("bound")) {
      n.cfg.bound = g("bound").value;
    } else if ((n.type === "set_var" || n.type === "get_var") && g("name")) {
      n.cfg.name = (g("name").value || "x").trim() || "x";
    } else if (n.type === "for") {
      if (g("first")) n.cfg.first = g("first").value;
      if (g("last")) n.cfg.last = g("last").value;
    } else if (n.type === "sequence" && g("count")) {
      n.cfg.count = Math.max(2, Math.min(8, parseInt(g("count").value, 10) || 2));
    } else if ((n.type === "while" || n.type === "do_while") && g("max_iter")) {
      n.cfg.max_iter = String(Math.max(1, parseInt(g("max_iter").value, 10) || 1000000));
    } else if (n.type === "comment" && g("text")) {
      n.cfg.text = g("text").value;
    } else if (n.type === "do_n" && g("n")) {
      n.cfg.n = g("n").value;
    } else if (n.type === "gate" && g("start_open")) {
      n.cfg.start_open = parseInt(g("start_open").value, 10) ? 1 : 0;
    } else if (n.type === "multi_gate" && g("count")) {
      n.cfg.count = Math.max(2, Math.min(16, parseInt(g("count").value, 10) || 2));
    } else if (n.type === "switch" && g("cases")) {
      n.cfg.cases = g("cases").value;
    } else if ((n.type === "arr_lit" || n.type === "vec_lit" || n.type === "mat_lit") && g("value")) {
      n.cfg.value = g("value").value;
    } else if (n.type === "arr_map" && g("map_op")) {
      n.cfg.op = g("map_op").value || "add";
    } else if (n.type === "arr_reduce" && g("red_op")) {
      n.cfg.op = g("red_op").value || "sum";
    } else if (n.type === "mat_identity" && g("n")) {
      n.cfg.n = g("n").value;
    } else if (n.type === "arr_from_range") {
      if (g("first")) n.cfg.first = g("first").value;
      if (g("last")) n.cfg.last = g("last").value;
    } else if (n.type === "ten_unary" && g("ten_op")) {
      n.cfg.op = g("ten_op").value || "exp";
    } else if (n.type === "ten_binop" && g("ten_binop")) {
      n.cfg.op = g("ten_binop").value || "add";
    } else if (n.type === "reshape") {
      if (g("rows")) n.cfg.rows = g("rows").value;
      if (g("cols")) n.cfg.cols = g("cols").value;
    } else if (n.type === "mat_concat" && g("axis")) {
      n.cfg.axis = g("axis").value;
    } else if (n.type === "mat_slice") {
      if (g("r0")) n.cfg.r0 = g("r0").value;
      if (g("r1")) n.cfg.r1 = g("r1").value;
      if (g("c0")) n.cfg.c0 = g("c0").value;
      if (g("c1")) n.cfg.c1 = g("c1").value;
    } else if ((n.type === "layer_norm" || n.type === "rms_norm") && g("eps")) {
      n.cfg.eps = g("eps").value;
    } else if (n.type === "rope" && g("base")) {
      n.cfg.base = g("base").value;
    } else if (n.type === "attention_sdp") {
      if (g("scale")) n.cfg.scale = g("scale").value;
      n.cfg.causal = (g("causal") && g("causal").checked) ? 1 : 0;
    } else if (n.type === "sin_pe") {
      if (g("seq")) n.cfg.seq = g("seq").value;
      if (g("dim")) n.cfg.dim = g("dim").value;
    } else if ((n.type === "mha_split" || n.type === "mha_merge") && g("heads")) {
      n.cfg.heads = g("heads").value;
    }
    /* port literals pv_* */
    if (!n.cfg.portValues) n.cfg.portValues = {};
    root.querySelectorAll('[name^="pv_"]').forEach((el) => {
      const p = el.getAttribute("name").slice(3);
      if (el.value != null && String(el.value).trim() !== "") n.cfg.portValues[p] = el.value;
      else delete n.cfg.portValues[p];
    });
  }

  function renderInspector() {
    const empty = $id("inspectEmpty");
    const body = $id("inspectBody");
    if (!empty || !body) return;
    const n = selectedId ? nodeById(selectedId) : null;
    if (!n) {
      empty.classList.remove("hidden");
      body.classList.add("hidden");
      body.innerHTML = "";
      return;
    }
    empty.classList.add("hidden");
    body.classList.remove("hidden");
    let html = '<div class="inspect-form">';
    html += "<div><b>" + escapeHtml(titleOf(n)) + "</b> <span class=\"muted\">" + escapeHtml(n.id) + " · " + escapeHtml(n.type) + "</span></div>";
    html += buildConfigFields(n);
    html += '<div class="inspect-actions">';
    html += '<button type="button" class="btn sm primary" id="inspApply">应用配置</button>';
    html += '<button type="button" class="btn sm" id="inspEditDlg">对话框</button>';
    html += '<button type="button" class="btn sm danger" id="inspDel">删除</button>';
    html += "</div></div>";
    body.innerHTML = html;
    const apply = (silent) => {
      try {
        applyConfigFields(n, body);
        refreshNodeView(n, silent ? { soft: true, skipInspector: true } : {});
        if (!silent) status("已更新节点配置", "ok");
      } catch (err) {
        console.error(err);
        status("更新配置失败: " + (err && err.message ? err.message : err), "err");
      }
    };
    bindPresetClicks(body, () => apply(true));
    const applyBtn = body.querySelector("#inspApply");
    if (applyBtn) applyBtn.onclick = () => apply(false);
    const dlgBtn = body.querySelector("#inspEditDlg");
    if (dlgBtn) dlgBtn.onclick = () => openConfigDialog(n, false);
    const delBtn = body.querySelector("#inspDel");
    if (delBtn) delBtn.onclick = () => removeNode(n.id);
    body.querySelectorAll("input, select, textarea").forEach((el) => {
      const evt = (el.type === "checkbox" || el.tagName === "SELECT") ? "change" : "input";
      el.addEventListener(evt, () => apply(true));
      if (el.type === "number" || el.type === "text") el.addEventListener("change", () => apply(true));
    });
  }

  
/* ---------- Palette ---------- */
  function renderPalette() {
    const body = $id("paletteBody");
    if (!body) return;
    const q = (($id("paletteSearch") && $id("paletteSearch").value) || "").trim().toLowerCase();
    body.innerHTML = "";

    function addGroup(title, items, maker) {
      const filtered = items.filter((it) => {
        const s = ((it.label || "") + " " + (it.op || "") + " " + (it.type || "") + " " + (it.hint || "")).toLowerCase();
        return !q || s.includes(q);
      });
      if (!filtered.length) return;
      const g = document.createElement("div");
      g.className = "group";
      g.innerHTML = '<div class="label">' + escapeHtml(title) + "</div>";
      filtered.forEach((it) => {
        const b = document.createElement("button");
        b.type = "button";
        b.className = "chip" + (it.type && it.type.startsWith("const") ? " const" : "") + (it._fn ? " fn" : "");
        b.textContent = it.label || it.op || it.type;
        if (it.hint) b.title = it.hint + "（点击或拖到画布）";
        else b.title = "点击或拖到画布";
        b.draggable = true;
        b.addEventListener("click", () => maker(it));
        b.addEventListener("dragstart", (e) => {
          try {
            const payload = JSON.stringify(it);
            e.dataTransfer.setData("application/x-snlab-node", payload);
            e.dataTransfer.setData("application/json", payload);
            e.dataTransfer.setData("text/plain", payload);
            e.dataTransfer.setData("text", payload);
            e.dataTransfer.effectAllowed = "copy";
          } catch (_) {}
          b.classList.add("dragging-chip");
        });
        b.addEventListener("dragend", () => b.classList.remove("dragging-chip"));
        g.appendChild(b);
      });
      body.appendChild(g);
    }

    if (paletteTab === "fn") {
      if (editScope !== "main") {
        addGroup("函数内部", [
          { type: "fn_param", label: "参数引用" },
          { type: "fn_return", label: "返回" }
        ], (it) => {
          if (it.type === "fn_return") addNode({ type: "fn_return" });
          else {
            const fn = project.functions[editScope.slice(3)];
            const idx = 0;
            const name = (fn && fn.params[idx]) || "p0";
            addNode({ type: "fn_param" }, null, null, { paramIndex: idx, paramName: name });
            /* open small choice via inspector — create one per param */
            if (fn) {
              /* replace with all params buttons already listed below */
            }
          }
        });
        const fn = project.functions[editScope.slice(3)];
        if (fn && fn.params.length) {
          addGroup("参数", fn.params.map((name, i) => ({ type: "fn_param", label: name, _idx: i, _name: name })), (it) => {
            addNode({ type: "fn_param" }, null, null, { paramIndex: it._idx, paramName: it._name });
          });
        }
      }
      /* function list with edit / call / delete */
      {
        const g = document.createElement("div");
        g.className = "group";
        g.innerHTML = "<h4>自定义函数</h4>";
        const tools = document.createElement("div");
        tools.className = "fn-tools";
        const btnNew = document.createElement("button");
        btnNew.type = "button";
        btnNew.className = "btn primary sm";
        btnNew.textContent = "＋ 新建函数";
        btnNew.addEventListener("click", () => openNewFnDialog());
        tools.appendChild(btnNew);
        g.appendChild(tools);
        const list = Object.values(project.functions);
        if (!list.length) {
          const empty = document.createElement("div");
          empty.className = "muted pad";
          empty.textContent = "尚无函数。创建后可编辑、调用、删除。";
          g.appendChild(empty);
        }
        list.forEach((fn) => {
          const row = document.createElement("div");
          row.className = "fn-card" + (editScope === "fn:" + fn.id ? " active" : "");
          row.innerHTML =
            '<div class="fn-card-head"><strong>' + escapeHtml(fn.name) + "</strong>" +
            (editScope === "fn:" + fn.id ? '<span class="tag">编辑中</span>' : "") +
            '</div><div class="fn-card-meta">参数: ' +
            escapeHtml(fn.params.join(", ") || "无") +
            '</div><div class="fn-card-actions">' +
            '<button type="button" class="btn sm" data-act="call">放入画布</button>' +
            '<button type="button" class="btn sm" data-act="edit">' +
            (editScope === "fn:" + fn.id ? "继续编辑" : "编辑") +
            '</button>' +
            '<button type="button" class="btn sm ghost" data-act="sig">编辑签名</button>' +
            '<button type="button" class="btn sm danger ghost" data-act="del">删除</button>' +
            "</div>";
          row.draggable = true;
          row.title = "拖到画布创建调用节点，或点「放入画布」";
          const startFnDrag = (e) => {
            if (e.target && e.target.closest && e.target.closest("button")) {
              e.preventDefault();
              return;
            }
            if (editScope !== "main" && editScope === "fn:" + fn.id) {
              status("函数内部不可递归调用自身（可拆子函数）", "err");
              e.preventDefault();
              return;
            }
            const it = { type: "fn_call", fnId: fn.id, label: "调用 " + fn.name, _fn: true };
            try {
              const payload = JSON.stringify(it);
              e.dataTransfer.setData("application/x-snlab-node", payload);
              e.dataTransfer.setData("application/json", payload);
              e.dataTransfer.setData("text/plain", payload);
              e.dataTransfer.setData("text", payload);
              e.dataTransfer.effectAllowed = "copy";
            } catch (_) {}
            row.classList.add("dragging-chip");
          };
          row.addEventListener("dragstart", startFnDrag);
          row.addEventListener("dragend", () => row.classList.remove("dragging-chip"));
          const head = row.querySelector(".fn-card-head");
          if (head) {
            head.draggable = true;
            head.style.cursor = "grab";
            head.title = "拖到画布调用 " + fn.name;
            head.addEventListener("dragstart", (e) => {
              e.stopPropagation();
              startFnDrag(e);
            });
            head.addEventListener("dragend", () => row.classList.remove("dragging-chip"));
          }
          row.addEventListener("click", (ev) => {
            const btn = ev.target.closest("[data-act]");
            if (!btn) return;
            const act = btn.dataset.act;
            if (act === "call") {
              if (editScope !== "main" && editScope === "fn:" + fn.id) {
                status("函数内不可递归调用自身（本版）", "err");
                return;
              }
              addNode({ type: "fn_call" }, null, null, { fnId: fn.id });
            } else if (act === "edit") {
              editScope = "fn:" + fn.id;
              remountGraph();
              status("正在编辑函数 " + fn.name);
            } else if (act === "rename" || act === "sig") {
              openEditFnDialog(fn);
            } else if (act === "del") {
              if (!confirm("删除函数 " + fn.name + "？相关调用块也会移除。")) return;
              const purge = (gph) => {
                gph.nodes = gph.nodes.filter((n) => !(n.type === "fn_call" && n.fnId === fn.id));
                const ids = new Set(gph.nodes.map((n) => n.id));
                gph.wires = gph.wires.filter((w) => ids.has(w.from) && ids.has(w.to));
                gph.nodes.forEach((n) => {
                  Object.keys(n.inputs || {}).forEach((k) => {
                    if (n.inputs[k] != null && !ids.has(n.inputs[k])) n.inputs[k] = null;
                  });
                });
              };
              Object.values(project.functions).forEach(purge);
              purge(project.main);
              delete project.functions[fn.id];
              if (editScope === "fn:" + fn.id) editScope = "main";
              remountGraph();
              status("已删除函数 " + fn.name, "ok");
            }
          });
          g.appendChild(row);
        });
        body.appendChild(g);
      }
      /* also show standard ops for building function body */
      if (editScope !== "main") {
        addGroup("算术", CATALOG.arith, spawnFromCatalog);
        addGroup("数学", CATALOG.math, spawnFromCatalog);
      }
      return;
    }

    if (editScope !== "main" && (paletteTab === "const" || paletteTab === "cplx")) {
      /* allow const inside functions */
    }

    const map = {
      const: [["常量", CATALOG.const]],
      arith: [["算术 / 转换", CATALOG.arith]],
      math: [["初等 / 特殊函数", CATALOG.math]],
      bit: [["位运算 / 密码学", CATALOG.bit]],
      spec: [["椭圆 / 不完全 / Bessel", CATALOG.spec]],
      cplx: [["复数", CATALOG.cplx]],
      flow: [["控制流 / 逻辑 / 随机", CATALOG.flow]],
      data: [["数组 / 向量 / 矩阵 / Transformer", CATALOG.data]]
    };
    const searchAll = paletteTab === "all" || !!q;
    if (searchAll && paletteTab !== "fn") {
      Object.keys(map).forEach((k) => {
        (map[k] || []).forEach(([title, items]) => addGroup(title, items, spawnFromCatalog));
      });
      const fnItems = Object.values(project.functions).map((fn) => ({
        type: "fn_call", fnId: fn.id, label: "调用 " + fn.name, _fn: true
      }));
      if (fnItems.length) addGroup("自定义函数", fnItems, spawnFromCatalog);
    } else {
      (map[paletteTab] || []).forEach(([title, items]) => addGroup(title, items, spawnFromCatalog));
    }
  }

  async function spawnFromCatalog(it, x, y) {
    if (it && it.type === "fn_call" && it.fnId) {
      const n = addNode({ type: "fn_call" }, x, y, { fnId: it.fnId });
      selectNode(n.id);
      return n;
    }
    const n = addNode(it, x, y);
    if (n.type.startsWith("const") || n.type === "shift" || n.type === "cast" ||
        n.type === "set_var" || n.type === "get_var" || n.type === "for" ||
        n.type === "sequence" || n.type === "while" ||
        n.type === "arr_lit" || n.type === "vec_lit" || n.type === "mat_lit" ||
        n.type === "arr_map" || n.type === "arr_reduce" || n.type === "mat_identity" ||
        n.type === "arr_from_range" || n.type === "ten_unary" || n.type === "ten_binop" ||
        n.type === "reshape" || n.type === "mat_concat" || n.type === "mat_slice" ||
        n.type === "layer_norm" || n.type === "rms_norm" || n.type === "attention_sdp" ||
        n.type === "sin_pe" || n.type === "mha_split" || n.type === "mha_merge") {
      await openConfigDialog(n, true);
    }
    selectNode(n.id);
    return n;
  }

  function openNewFnDialog() {
    const dlg = $id("fnDlg");
    if (!dlg) return;
    const title = dlg.querySelector("h2");
    if (title) title.textContent = "新建自定义函数";
    $id("fnName").value = "my_fn" + (fnSeq);
    $id("fnArity").value = "2";
    $id("fnParams").value = "x,y";
    dlg.returnValue = "";
    dlg.dataset.mode = "new";
    dlg.dataset.fnId = "";
    dlg.showModal();
    bindFnArityParamsSync();
    const onClose = () => {
      dlg.removeEventListener("close", onClose);
      if (dlg.returnValue !== "ok") return;
      const name = ($id("fnName").value || "fn").trim();
      if (!/^[A-Za-z_][A-Za-z0-9_]*$/.test(name)) {
        status("函数名非法", "err");
        return;
      }
      if (dlg.dataset.mode === "edit") return;
      const params = normalizeParams($id("fnArity").value, $id("fnParams").value);
      const id = "f" + (fnSeq++);
      project.functions[id] = { id, name, params, nodes: [], wires: [] };
      editScope = "fn:" + id;
      remountGraph();
      addNode({ type: "entry" }, 40, 40);
      params.forEach((pn, i) => {
        addNode({ type: "fn_param" }, 60, 120 + i * 90, { paramIndex: i, paramName: pn });
      });
      addNode({ type: "fn_return" }, 420, 160);
      addNode({ type: "out" }, 420, 320);
      status("已创建函数 " + name + "（Entry / 参数 / 返回 / 输出）", "ok");
      renderPalette();
    };
    dlg.addEventListener("close", onClose);
  }

  
/* ---------- Topo / evaluate ---------- */

  let _fnArityBound = false;
  function bindFnArityParamsSync() {
    const arityEl = $id("fnArity");
    const paramsEl = $id("fnParams");
    if (!arityEl || !paramsEl || _fnArityBound) return;
    _fnArityBound = true;
    let syncing = false;
    arityEl.addEventListener("input", () => {
      if (syncing) return;
      syncing = true;
      const a = Math.max(0, Math.min(64, parseInt(arityEl.value, 10) || 0));
      let names = (paramsEl.value || "").split(",").map((s) => s.trim()).filter(Boolean);
      while (names.length < a) names.push("p" + names.length);
      if (names.length > a) names = names.slice(0, a);
      paramsEl.value = names.join(",");
      syncing = false;
    });
    paramsEl.addEventListener("input", () => {
      if (syncing) return;
      syncing = true;
      const names = (paramsEl.value || "").split(",").map((s) => s.trim()).filter(Boolean);
      arityEl.value = String(Math.min(64, names.length));
      syncing = false;
    });
  }

  function normalizeParams(arity, namesCsv) {
    let params = String(namesCsv || "").split(",").map((s) => s.trim()).filter(Boolean);
    let a = parseInt(arity, 10);
    if (!Number.isFinite(a) || a < 0) a = 0;
    if (a > 64) a = 64;
    if (!params.length && a > 0) params = Array.from({ length: a }, (_, i) => "p" + i);
    while (params.length < a) params.push("p" + params.length);
    params = params.slice(0, Math.max(0, a));
    const used = new Set();
    params = params.map((p, i) => {
      let name = /^[A-Za-z_][A-Za-z0-9_]*$/.test(p) ? p : ("p" + i);
      let base = name, n = 2;
      while (used.has(name)) { name = base + "_" + n; n++; }
      used.add(name);
      return name;
    });
    return params;
  }

  function syncFunctionSignature(fn, newParams) {
    if (!fn) return;
    fn.params = newParams.slice();
    const keepIds = new Set();
    fn.nodes = (fn.nodes || []).filter((n) => {
      if (n.type === "fn_param" && (n.paramIndex | 0) >= newParams.length) {
        if (n.el) n.el.remove();
        return false;
      }
      if (n.type === "fn_param") {
        const idx = n.paramIndex | 0;
        if (idx < newParams.length) n.paramName = newParams[idx];
      }
      keepIds.add(n.id);
      return true;
    });
    fn.wires = (fn.wires || []).filter((w) => keepIds.has(w.from) && keepIds.has(w.to));
    fn.nodes.forEach((n) => {
      Object.keys(n.inputs || {}).forEach((k) => {
        if (n.inputs[k] != null && !keepIds.has(n.inputs[k])) n.inputs[k] = null;
      });
    });
    const have = new Set((fn.nodes || []).filter((n) => n.type === "fn_param").map((n) => n.paramIndex | 0));
    const wasEditing = editScope === "fn:" + fn.id;
    newParams.forEach((pn, i) => {
      if (have.has(i)) return;
      if (wasEditing) {
        addNode({ type: "fn_param" }, 60, 120 + i * 90, { paramIndex: i, paramName: pn });
      } else {
        const id = "n" + (nodeSeq++);
        if (!fn.nodes) fn.nodes = [];
        fn.nodes.push({
          id, type: "fn_param", op: null,
          x: 60, y: 120 + i * 90,
          cfg: { portValues: {} },
          arity: null, fnId: null,
          paramIndex: i, paramName: pn,
          inputs: {}, inputFromPorts: {},
          collapsed: false, foldUnusedPorts: false,
          el: null, slot: -1, result: ""
        });
      }
    });
    (fn.nodes || []).forEach((n) => {
      if (n.type === "fn_param" && (n.paramIndex | 0) < newParams.length) {
        n.paramName = newParams[n.paramIndex | 0];
      }
    });
    function fixCalls(gph) {
      (gph.nodes || []).forEach((n) => {
        if (n.type !== "fn_call" || n.fnId !== fn.id) return;
        const valid = new Set(newParams.map((_, i) => "p" + i));
        valid.add("exec");
        gph.wires = (gph.wires || []).filter((w) => {
          if (w.to === n.id && String(w.toPort || "").startsWith("p") && !valid.has(w.toPort)) return false;
          return true;
        });
        n.inputs = n.inputs || {};
        Object.keys(n.inputs).forEach((k) => {
          if (k.startsWith("p") && !valid.has(k)) delete n.inputs[k];
        });
        portsOf(n).in.forEach((p) => {
          if (n.inputs[p] === undefined) n.inputs[p] = null;
        });
        if (n.el) renderNode(n);
      });
    }
    fixCalls(project.main);
    Object.values(project.functions).forEach(fixCalls);
    if (wasEditing) remountGraph();
    else { drawWires(); renderPalette(); }
  }

  function openEditFnDialog(fn) {
    if (!fn) return;
    const dlg = $id("fnDlg");
    if (!dlg) return;
    const title = dlg.querySelector("h2");
    if (title) title.textContent = "编辑函数 " + fn.name;
    $id("fnName").value = fn.name;
    $id("fnArity").value = String((fn.params || []).length);
    $id("fnParams").value = (fn.params || []).join(",");
    dlg.returnValue = "";
    dlg.dataset.mode = "edit";
    dlg.dataset.fnId = fn.id;
    dlg.showModal();
    bindFnArityParamsSync();
    const onClose = () => {
      dlg.removeEventListener("close", onClose);
      dlg.dataset.mode = "";
      dlg.dataset.fnId = "";
      if (title) title.textContent = "新建自定义函数";
      if (dlg.returnValue !== "ok") return;
      const name = ($id("fnName").value || "fn").trim();
      if (!/^[A-Za-z_][A-Za-z0-9_]*$/.test(name)) { status("函数名非法", "err"); return; }
      if (Object.values(project.functions).some((f) => f.id !== fn.id && f.name === name)) {
        status("名称已存在", "err"); return;
      }
      const params = normalizeParams($id("fnArity").value, $id("fnParams").value);
      fn.name = name;
      syncFunctionSignature(fn, params);
      status("已更新函数 " + name + " (" + params.length + " 参数)", "ok");
      renderPalette();
      remountGraph();
    };
    dlg.addEventListener("close", onClose);
  }

  function usedFnParams(fn) {
    /* Param is "used" only if its output is actually wired in the subgraph.
       Unused params may be omitted at call sites (default 0). */
    const used = new Set();
    if (!fn || !fn.nodes) return used;
    const wiredFrom = new Set();
    (fn.wires || []).forEach((w) => { if (w && w.from) wiredFrom.add(w.from); });
    fn.nodes.forEach((n) => {
      if (n.type === "fn_param" && n.paramIndex != null && wiredFrom.has(n.id)) {
        used.add(n.paramIndex | 0);
      }
    });
    return used;
  }

  function topoSort(graphNodes, graphWires) {
    const ids = graphNodes.map((n) => n.id);
    const indeg = {};
    const adj = {};
    ids.forEach((id) => { indeg[id] = 0; adj[id] = []; });
    graphWires.forEach((w) => {
      if (!indeg.hasOwnProperty(w.to) || !indeg.hasOwnProperty(w.from)) return;
      adj[w.from].push(w.to);
      indeg[w.to]++;
    });
    const q = ids.filter((id) => indeg[id] === 0);
    const order = [];
    while (q.length) {
      const u = q.shift();
      order.push(u);
      (adj[u] || []).forEach((v) => {
        indeg[v]--;
        if (indeg[v] === 0) q.push(v);
      });
    }
    if (order.length !== ids.length) throw new Error("图中存在环");
    return order.map((id) => graphNodes.find((n) => n.id === id));
  }

  function allocReal() {
    const id = api.newValue(session);
    if (id < 0) throw new Error("new_value failed");
    return id;
  }
  function allocCplx() {
    const id = api.newCplx(session);
    if (id < 0) throw new Error("new_cplx failed");
    return id;
  }
  function must(r, msg) {
    if (r !== 0) throw new Error(msg + ": " + api.lastError(session));
  }

  function evalGraph(graph, argSlots) {
    /* Dual mode:
       - pure data: topological eval (legacy)
       - with Entry: UE-style exec walk + demand-eval data deps
    */
    const real = {}; /* nodeId -> slot (data out default "out") */
    const realPort = {}; /* nodeId|port -> slot for multi data outs */
    const cplx = {};
    const freelistR = [];
    const freelistC = [];
    const tensors = Object.create(null); /* nodeId -> {kind, data, rows?, cols?, snId?} */
    const snT = Object.create(null); /* nodeId -> SN tensor slot id (bridge 1.6+) */
    const freelistT = [];
    const vars = Object.create(null); /* local var name -> slot */
    const evaluated = new Set(); /* pure data nodes fully evaluated */
    const byId = {};
    graph.nodes.forEach((n) => { byId[n.id] = n; });

    function setResult(n, text) {
      n.result = text == null ? "" : String(text);
      if (n.el) {
        const el = n.el.querySelector("[data-result]");
        if (el) {
          el.setAttribute("title", n.result);
          el.setAttribute("data-full", n.result);
          el.textContent = formatResultPreview(n.result);
        }
        layoutPorts(n);
      }
    }

    function formatResultPreview(text) {
      /* at most 2 lines; long lists: 1,2,3,...,7,8,9 */
      const raw = String(text == null ? "" : text);
      if (!raw) return "";
      const s = raw.replace(/\r\n/g, "\n").replace(/\r/g, "\n").trim();
      const middleChars = (str, maxChars) => {
        const t = String(str || "");
        if (t.length <= maxChars) return t;
        const keep = Math.max(10, maxChars - 3);
        const head = Math.ceil(keep * 0.55);
        const tail = keep - head;
        return t.slice(0, head) + "..." + t.slice(t.length - tail);
      };
      const listMiddle = (str, maxTokens) => {
        const t = String(str || "").trim();
        if (!t) return t;
        if (t.indexOf(";") >= 0) {
          const rows = t.split(";").map((x) => x.trim()).filter(Boolean);
          if (rows.length <= 2) return rows.map((r) => listMiddle(r, maxTokens)).join("; ");
          return listMiddle(rows[0], maxTokens) + "; ...; " + listMiddle(rows[rows.length - 1], maxTokens);
        }
        const parts = t.split(",").map((x) => x.trim()).filter((x) => x.length);
        if (parts.length <= maxTokens) return parts.join(",");
        const headN = Math.max(2, Math.floor(maxTokens / 2));
        const tailN = Math.max(2, maxTokens - headN);
        return parts.slice(0, headN).join(",") + ",...," + parts.slice(parts.length - tailN).join(",");
      };
      const flat = s.replace(/\n+/g, "; ");
      const tokenish = flat.split(/[,;]/).filter((x) => x.trim().length);
      if (tokenish.length >= 6) return middleChars(listMiddle(flat, 8), 96);
      const lines = s.split("\n");
      if (lines.length <= 1) return middleChars(lines[0] || s, 96);
      return middleChars(lines[0], 90) + "\n" + middleChars(lines[lines.length - 1], 90);
    }

    function wireFrom(fromId, fromPort) {
      fromPort = fromPort || "out";
      if (fromPort === "out" || fromPort === "index" || fromPort === "cols") {
        if (fromPort === "index" && realPort[fromId + "|index"] != null) return realPort[fromId + "|index"];
        if (fromPort === "cols" && realPort[fromId + "|cols"] != null) return realPort[fromId + "|cols"];
        return real[fromId];
      }
      if (realPort[fromId + "|" + fromPort] != null) return realPort[fromId + "|" + fromPort];
      return real[fromId];
    }

    function setDataOut(n, slot, port) {
      port = port || "out";
      if (port === "out") real[n.id] = slot;
      else realPort[n.id + "|" + port] = slot;
      if (port === "out" || port === "index") real[n.id] = slot;
    }

    function inputSlot(n, port) {
      const src = n.inputs && n.inputs[port];
      if (src != null) {
        const fp = (n.inputFromPorts && n.inputFromPorts[port]) || "out";
        ensureData(src, fp);
        return wireFrom(src, fp);
      }
      /* inline literal on unconnected data port */
      const lit = n.cfg && n.cfg.portValues && n.cfg.portValues[port];
      if (lit != null && String(lit).trim() !== "") {
        const o = allocReal(); freelistR.push(o);
        must(api.setFromStr(session, o, String(lit).trim(), 0), "lit " + port);
        return o;
      }
      return null;
    }

    function ensureData(nodeId, wantPort) {
      const n = byId[nodeId];
      if (!n) throw new Error("未知节点 " + nodeId);
      if (isFlowControlNode(n) && n.type !== "get_var" && n.type !== "for" && n.type !== "fn_call") {
        /* exec-produced data: for.index / select_exec.out / already-filled real */
        if (n.type === "for" && (wantPort === "index" || wantPort === "out")) {
          if (realPort[n.id + "|index"] != null || real[n.id] != null) return;
        } else if (n.type === "select_exec") {
          if (real[n.id] != null || realPort[n.id + "|out"] != null) return;
          return; /* must be driven by exec path */
        } else if (n.type !== "get_var") {
          return;
        }
      }
      if (evaluated.has(n.id) && n.type !== "get_var" && n.type !== "rng") return;
      evalDataNode(n);
      if (n.type !== "rng" && n.type !== "get_var") evaluated.add(n.id);
    }

    function slotToNumber(slot) {
      if (slot == null) return NaN;
      const s = api.readCString(api.toStr(session, slot, 10));
      const x = Number(String(s).trim());
      return Number.isFinite(x) ? x : NaN;
    }

    function slotToInt(slot, def) {
      const x = slotToNumber(slot);
      if (!Number.isFinite(x)) return def|0;
      return Math.trunc(x);
    }

    function makeScalarSlot(num) {
      const o = allocReal(); freelistR.push(o);
      must(api.setFromStr(session, o, String(num), 0), "scalar");
      return o;
    }

    function parseListLiteral(raw) {
      let t = String(raw == null ? "" : raw).trim();
      if (!t) return [];
      if (t.startsWith("[")) {
        try {
          const j = JSON.parse(t);
          if (Array.isArray(j)) return j.map((x) => Number(x));
        } catch (_) {}
      }
      return t.split(/[,;\s]+/).map((x) => x.trim()).filter((x) => x.length).map((x) => Number(x));
    }

    function parseMatrixLiteral(raw) {
      let t = String(raw == null ? "" : raw).trim();
      if (!t) return { rows: 0, cols: 0, data: [] };
      if (t.startsWith("[")) {
        try {
          const j = JSON.parse(t);
          if (Array.isArray(j) && j.length && Array.isArray(j[0])) {
            const rows = j.length, cols = j[0].length;
            const data = [];
            for (let r = 0; r < rows; r++) {
              for (let c = 0; c < cols; c++) data.push(Number(j[r][c]));
            }
            return { rows, cols, data };
          }
          if (Array.isArray(j)) {
            return { rows: 1, cols: j.length, data: j.map(Number) };
          }
        } catch (_) {}
      }
      const rowStrs = t.split(/[;\n]+/).map((x) => x.trim()).filter((x) => x.length);
      const rowsArr = rowStrs.map((rs) => rs.split(/[,\s]+/).map((x) => x.trim()).filter((x) => x.length).map(Number));
      const rows = rowsArr.length;
      const cols = rows ? Math.max.apply(null, rowsArr.map((r) => r.length)) : 0;
      const data = [];
      for (let r = 0; r < rows; r++) {
        for (let c = 0; c < cols; c++) data.push(Number(rowsArr[r][c] != null ? rowsArr[r][c] : 0));
      }
      return { rows, cols, data };
    }

    function formatTensor(t) {
      if (!t) return "?";
      if (t.kind === "mat") {
        const parts = [];
        for (let r = 0; r < t.rows; r++) {
          const row = [];
          for (let c = 0; c < t.cols; c++) row.push(String(t.data[r * t.cols + c]));
          parts.push(row.join(","));
        }
        return "[" + t.rows + "x" + t.cols + "] " + parts.join(";");
      }
      const arr = t.data || [];
      const head = arr.slice(0, 12).map(String).join(", ");
      return "[" + arr.length + "] " + head + (arr.length > 12 ? ",…" : "");
    }

    function cloneTensor(t) {
      return {
        kind: t.kind,
        rows: t.rows,
        cols: t.cols,
        data: (t.data || []).slice()
      };
    }

    function inputTensor(n, port) {
      const src = n.inputs && n.inputs[port];
      if (src != null) {
        const fp = (n.inputFromPorts && n.inputFromPorts[port]) || "out";
        ensureData(src, fp);
        if (fp !== "out" && tensors[src + "|" + fp] != null) return cloneTensor(tensors[src + "|" + fp]);
        if (tensors[src] != null) {
          if (fp === "weights" && byId[src] && byId[src]._attnWeights) return cloneTensor(byId[src]._attnWeights);
          return cloneTensor(tensors[src]);
        }
        /* promote scalar to 1-element array when needed */
        const slot = wireFrom(src, fp);
        if (slot != null) {
          return { kind: "arr", data: [slotToNumber(slot)] };
        }
      }
      return null;
    }

    function setTensorOut(n, t) {
      tensors[n.id] = t;
      if (t && t.snId != null) snT[n.id] = t.snId;
      setResult(n, formatTensor(t));
    }

    function hasSnTensorApi() {
      return !!(api && api.newTensor && api.tensorFromStr && api.tensorBinary && api.tensorToStr);
    }

    function allocT() {
      if (!api.newTensor) throw new Error("SN tensor API unavailable (rebuild WASM bridge 1.6+)");
      const id = api.newTensor(session);
      if (id < 0) throw new Error("snw_new_tensor failed");
      freelistT.push(id);
      return id;
    }

    function snMust(r, msg) {
      if (r !== 0) throw new Error((msg || "tensor op") + ": " + (api.lastError(session) || ("code " + r)));
    }

    function snTensorStr(id) {
      const ptr = api.tensorToStr(session, id);
      if (!ptr) throw new Error("tensor_to_str failed: " + (api.lastError(session) || ""));
      return api.readCString(ptr);
    }

    function parseSnMatStr(s) {
      const rowStrs = String(s || "").split(";").map((x) => x.trim()).filter((x) => x.length);
      const data = [];
      let cols = 0;
      for (let r = 0; r < rowStrs.length; r++) {
        const cells = rowStrs[r].split(",").map((x) => Number(String(x).trim()));
        if (!cols) cols = cells.length;
        for (let c = 0; c < cols; c++) data.push(Number.isFinite(cells[c]) ? cells[c] : 0);
      }
      return { kind: "mat", rows: rowStrs.length, cols: cols || 0, data };
    }

    function setSnTensorOut(n, tid) {
      snT[n.id] = tid;
      const s = snTensorStr(tid);
      const t = parseSnMatStr(s);
      t.snId = tid;
      tensors[n.id] = t;
      setResult(n, formatTensor(t));
    }

    function tensorToStrLiteral(t) {
      if (!t) return "";
      if (t.kind === "mat") {
        const parts = [];
        for (let r = 0; r < t.rows; r++) {
          const row = [];
          for (let c0 = 0; c0 < t.cols; c0++) row.push(String(t.data[r * t.cols + c0]));
          parts.push(row.join(","));
        }
        return parts.join(";");
      }
      return (t.data || []).map(String).join(",");
    }

    function ensureSnFromTensor(t) {
      if (!t) throw new Error("empty tensor");
      if (t.snId != null && t.snId >= 0) return t.snId;
      const tid = allocT();
      snMust(api.tensorFromStr(session, tid, tensorToStrLiteral(t)), "tensor_from_str");
      t.snId = tid;
      return tid;
    }

    function inputSnTensor(n, port) {
      const src = n.inputs && n.inputs[port];
      if (src != null) {
        const fp = (n.inputFromPorts && n.inputFromPorts[port]) || "out";
        ensureData(src, fp);
        if (snT[src] != null) return snT[src];
        if (fp !== "out" && snT[src + "|" + fp] != null) return snT[src + "|" + fp];
      }
      const t = inputTensor(n, port);
      if (!t) return -1;
      return ensureSnFromTensor(t);
    }

    function evalDataNode(n) {
      if (n.type === "fn_param") {
        const idx = n.paramIndex | 0;
        if (!argSlots || argSlots[idx] == null) throw new Error("param " + idx + " unbound");
        const arg = argSlots[idx];
        if (arg && typeof arg === "object" && arg.kind === "tensor") {
          const tid = arg.id;
          snT[n.id] = tid;
          try {
            const s = snTensorStr(tid);
            const t = parseSnMatStr(s);
            t.snId = tid;
            tensors[n.id] = t;
            setResult(n, "arg" + idx + " " + formatTensor(t));
          } catch (e) {
            setResult(n, "arg" + idx + " [tensor " + tid + "]");
          }
          return;
        }
        real[n.id] = (arg && typeof arg === "object" && arg.kind === "scalar") ? arg.id : arg;
        setResult(n, "arg" + idx);
        return;
      }
      if (n.type === "const_f") {
        const slot = allocReal(); freelistR.push(slot);
        must(api.setFloat(session, slot, n.cfg.value, Number(n.cfg.e_bits)||0, Number(n.cfg.m_bits)||0, n.cfg.nan ? 1 : 0), "const_f");
        real[n.id] = slot;
        setResult(n, api.readCString(api.toStr(session, slot, 10)));
        return;
      }
      if (n.type === "const_i") {
        const slot = allocReal(); freelistR.push(slot);
        must(api.setInt(session, slot, n.cfg.value, n.cfg.base|0, Number(n.cfg.width)||0, n.cfg.signed ? 1 : 0), "const_i");
        real[n.id] = slot;
        setResult(n, api.readCString(api.toStr(session, slot, 10)));
        return;
      }
      if (n.type === "const_bi") {
        const slot = allocReal(); freelistR.push(slot);
        must(api.setInt(session, slot, n.cfg.value, n.cfg.base|0, 0, 1), "const_bi");
        real[n.id] = slot;
        setResult(n, api.readCString(api.toStr(session, slot, 10)));
        return;
      }
      if (n.type === "const_c") {
        const z = allocCplx(); freelistC.push(z);
        const { re, im } = parseCplxConst(n.cfg.value);
        const reS = allocReal(), imS = allocReal();
        freelistR.push(reS, imS);
        must(api.setFloat(session, reS, String(re), Number(n.cfg.e_bits)||0, Number(n.cfg.m_bits)||0, n.cfg.nan ? 1 : 0), "c re");
        must(api.setFloat(session, imS, String(im), Number(n.cfg.e_bits)||0, Number(n.cfg.m_bits)||0, n.cfg.nan ? 1 : 0), "c im");
        must(api.cplxSetReim(session, z, reS, imS), "cplx set");
        cplx[n.id] = z;
        setResult(n, api.readCString(api.cplxToStr(session, z)));
        return;
      }
      if (n.type === "const_bool") {
        const o = allocReal(); freelistR.push(o);
        const v = (n.op === "false") ? "0" : "1";
        must(api.setFromStr(session, o, v, 0), "const_bool");
        real[n.id] = o; setResult(n, v); return;
      }
      if (n.type === "get_var") {
        const name = ((n.cfg && n.cfg.name) || "x").trim() || "x";
        if (vars[name] == null) throw new Error("变量 " + name + " 未赋值");
        real[n.id] = vars[name];
        setResult(n, api.readCString(api.toStr(session, vars[name], 10)));
        return;
      }
      if (n.type === "out" || n.type === "fn_return") {
        const src = n.inputs.in || n.inputs.a;
        if (!src) {
          const lit = n.cfg && n.cfg.portValues && (n.cfg.portValues.in != null ? n.cfg.portValues.in : n.cfg.portValues.value);
          if (lit != null && String(lit).trim() !== "") {
            const o = allocReal(); freelistR.push(o);
            must(api.setFromStr(session, o, String(lit).trim(), 0), "out lit");
            const t = api.readCString(api.toStr(session, o, 10));
            setResult(n, t); log((n.type === "out" ? "OUT " : "RET ") + t);
            n._retSlot = o; n._retCplx = null; real[n.id] = o;
            return;
          }
          setResult(n, "(未连接)"); return;
        }
        const fp = (n.inputFromPorts && (n.inputFromPorts.in || n.inputFromPorts.a)) || "out";
        ensureData(src, fp);
        let tenSrc = null;
        if (fp !== "out" && tensors[src + "|" + fp] != null) tenSrc = tensors[src + "|" + fp];
        else if (fp === "weights" && byId[src] && byId[src]._attnWeights) tenSrc = byId[src]._attnWeights;
        else if (tensors[src] != null) tenSrc = tensors[src];
        if (tenSrc != null) {
          const ten = cloneTensor(tenSrc);
          tensors[n.id] = ten;
          const t = formatTensor(ten);
          setResult(n, t); log((n.type === "out" ? "OUT " : "RET ") + t);
          n._retSlot = null; n._retCplx = null; n._retTensor = ten; n._retTensorSn = (ten && ten.snId != null) ? ten.snId : (snT[src] != null ? snT[src] : null);
          return;
        }
        if (cplx[src] != null) {
          const t = api.readCString(api.cplxToStr(session, cplx[src]));
          setResult(n, t); log((n.type === "out" ? "OUT " : "RET ") + t);
          n._retSlot = null; n._retCplx = cplx[src]; n._retTensor = null;
        } else {
          const slot = wireFrom(src, fp);
          if (slot == null) throw new Error("输出源无效");
          const t = api.readCString(api.toStr(session, slot, 10));
          setResult(n, t); log((n.type === "out" ? "OUT " : "RET ") + t);
          n._retSlot = slot; n._retCplx = null; n._retTensor = null;
          real[n.id] = slot;
        }
        return;
      }
      if (n.type === "copy" || n.type === "assign") {
        const a = inputSlot(n, "a"); if (a == null) throw new Error("copy 缺少 a");
        const o = cloneSlot(a, freelistR);
        real[n.id] = o; setResult(n, api.readCString(api.toStr(session, o, 10))); return;
      }
      if (n.type === "un") {
        const a = inputSlot(n, "a"); if (a == null) throw new Error(n.op + " 缺少 a");
        const o = allocReal(); freelistR.push(o);
        must(api.unary(session, o, n.op, a), n.op);
        real[n.id] = o; setResult(n, api.readCString(api.toStr(session, o, 10))); return;
      }
      if (n.type === "bin") {
        const a = inputSlot(n, "a"), b = inputSlot(n, "b");
        if (a == null || b == null) throw new Error(n.op + " 缺少输入");
        const o = allocReal(); freelistR.push(o);
        must(api.binary(session, o, n.op, a, b), n.op);
        real[n.id] = o; setResult(n, api.readCString(api.toStr(session, o, 10))); return;
      }
      if (n.type === "ter") {
        const a = inputSlot(n, "a"), b = inputSlot(n, "b"), c = inputSlot(n, "c");
        if (a == null || b == null || c == null) throw new Error(n.op + " 缺少输入");
        const o = allocReal(); freelistR.push(o);
        must(api.ternary(session, o, n.op, a, b, c), n.op);
        real[n.id] = o; setResult(n, api.readCString(api.toStr(session, o, 10))); return;
      }
      if (n.type === "crypto") {
        let ar = n.arity;
        if (ar == null) {
          ar = (n.op === "isqrt" || n.op === "popcount" || n.op === "ctz") ? 1
            : (n.op === "mulmod" || n.op === "powmod" || n.op === "powmod_ct") ? 3 : 2;
        }
        const a = inputSlot(n, "a");
        const b = ar >= 2 ? inputSlot(n, "b") : null;
        const c = ar >= 3 ? inputSlot(n, "c") : null;
        if (a == null || (ar >= 2 && b == null) || (ar >= 3 && c == null)) throw new Error(n.op + " 缺少输入");
        const o = allocReal(); freelistR.push(o);
        must(api.crypto(session, o, n.op, a, ar >= 2 ? b : -1, ar >= 3 ? c : -1), n.op);
        real[n.id] = o; setResult(n, api.readCString(api.toStr(session, o, 10))); return;
      }
      if (n.type === "shift") {
        const a = inputSlot(n, "a"); if (a == null) throw new Error("shift 缺少 a");
        const o = allocReal(); freelistR.push(o);
        must(api.shift(session, o, n.op, a, n.cfg.bits|0), n.op);
        real[n.id] = o; setResult(n, api.readCString(api.toStr(session, o, 10))); return;
      }
      if (n.type === "cast") {
        const a = inputSlot(n, "a"); if (a == null) throw new Error("cast 缺少 a");
        const o = allocReal(); freelistR.push(o);
        if (n.op === "to_float")
          must(api.cast(session, o, a, "float", Number(n.cfg.e_bits)||0, Number(n.cfg.m_bits)||0, n.cfg.nan ? 1 : 0), "to_float");
        else
          must(api.cast(session, o, a, "int", Number(n.cfg.width)||0, n.cfg.signed ? 1 : 0, 0), "to_int");
        real[n.id] = o; setResult(n, api.readCString(api.toStr(session, o, 10))); return;
      }
      if (n.type === "cun") {
        const a = cplx[n.inputs.a];
        if (a == null) { ensureData(n.inputs.a, "out"); }
        const zIn = cplx[n.inputs.a];
        if (zIn == null) throw new Error(n.op + " 缺少复数 a");
        const o = allocCplx(); freelistC.push(o);
        must(api.cplxUnary(session, o, n.op, zIn), "c" + n.op);
        cplx[n.id] = o; setResult(n, api.readCString(api.cplxToStr(session, o))); return;
      }
      if (n.type === "cbin") {
        if (n.inputs.a) ensureData(n.inputs.a, "out");
        if (n.inputs.b) ensureData(n.inputs.b, "out");
        const a = cplx[n.inputs.a], b = cplx[n.inputs.b];
        if (a == null || b == null) throw new Error(n.op + " 缺少复数输入");
        const o = allocCplx(); freelistC.push(o);
        must(api.cplxBinary(session, o, n.op, a, b), "c" + n.op);
        cplx[n.id] = o; setResult(n, api.readCString(api.cplxToStr(session, o))); return;
      }
      if (n.type === "c2r") {
        if (n.inputs.a) ensureData(n.inputs.a, "out");
        const z = cplx[n.inputs.a]; if (z == null) throw new Error(n.op + " 缺少 z");
        const o = allocReal(); freelistR.push(o);
        if (n.op === "re") must(api.cplxRe(session, o, z), "re");
        else if (n.op === "im") must(api.cplxIm(session, o, z), "im");
        else if (n.op === "abs") must(api.cplxAbs(session, o, z), "abs");
        else if (n.op === "arg") must(api.cplxArg(session, o, z), "arg");
        else throw new Error("unknown c2r");
        real[n.id] = o; setResult(n, api.readCString(api.toStr(session, o, 10))); return;
      }
      if (n.type === "r2c") {
        const a = inputSlot(n, "a"), b = inputSlot(n, "b");
        if (a == null || b == null) throw new Error(n.op + " 缺少输入");
        const o = allocCplx(); freelistC.push(o);
        if (n.op === "set_reim") must(api.cplxSetReim(session, o, a, b), "set_reim");
        else if (n.op === "from_polar") must(api.cplxFromPolar(session, o, a, b), "from_polar");
        else throw new Error("unknown r2c");
        cplx[n.id] = o; setResult(n, api.readCString(api.cplxToStr(session, o))); return;
      }
      if (n.type === "fn_call") {
        const fn = project.functions[n.fnId];
        if (!fn) throw new Error("unknown function " + n.fnId);
        const used = usedFnParams(fn);
        const args = fn.params.map((_, i) => {
          const src = n.inputs["p" + i];
          if (src != null) {
            const fp = (n.inputFromPorts && n.inputFromPorts["p" + i]) || "out";
            ensureData(src, fp);
            if (snT[src] != null) return { kind: "tensor", id: snT[src] };
            if (tensors[src] != null && hasSnTensorApi()) {
              return { kind: "tensor", id: ensureSnFromTensor(tensors[src]) };
            }
            if (real[src] != null) return real[src];
            if (cplx[src] != null) return { kind: "cplx", id: cplx[src] };
          }
          if (!used.has(i)) {
            const z = allocReal(); freelistR.push(z);
            must(api.setFromStr(session, z, "0", 0), "fn default arg");
            return z;
          }
          throw new Error(fn.name + " param " + (fn.params[i] || i) + " not connected");
        });
        const ret = evalFunction(fn, args);
        if (ret.tensor != null) {
          snT[n.id] = ret.tensor;
          freelistT.push(ret.tensor);
          try { setSnTensorOut(n, ret.tensor); }
          catch (_) { setResult(n, "[tensor " + ret.tensor + "]"); }
        } else if (ret.cplx != null) {
          cplx[n.id] = ret.cplx; freelistC.push(ret.cplx);
          setResult(n, api.readCString(api.cplxToStr(session, ret.cplx)));
        } else {
          real[n.id] = ret.slot; freelistR.push(ret.slot);
          setResult(n, api.readCString(api.toStr(session, ret.slot, 10)));
        }
        return;
      }
      if (n.type === "cmp") {
        const a = inputSlot(n, "a"), b = inputSlot(n, "b");
        if (a == null || b == null) throw new Error("cmp 缺少输入");
        const rel = api.cmp(session, a, b);
        let ok = 0;
        switch (n.op) {
          case "eq": ok = (rel === 0) ? 1 : 0; break;
          case "ne": ok = (rel !== 0) ? 1 : 0; break;
          case "lt": ok = (rel < 0) ? 1 : 0; break;
          case "le": ok = (rel <= 0) ? 1 : 0; break;
          case "gt": ok = (rel > 0) ? 1 : 0; break;
          case "ge": ok = (rel >= 0) ? 1 : 0; break;
          default: throw new Error("unknown cmp " + n.op);
        }
        const o = allocReal(); freelistR.push(o);
        must(api.setFromStr(session, o, String(ok), 0), "cmp");
        real[n.id] = o; setResult(n, String(ok)); return;
      }
      if (n.type === "select") {
        const cond = inputSlot(n, "a"), t = inputSlot(n, "b"), f = inputSlot(n, "c");
        if (cond == null || t == null || f == null) throw new Error("select 缺少输入");
        const truthy = truthySlot(cond);
        const src = truthy ? t : f;
        const o = allocReal(); freelistR.push(o);
        const ts = api.readCString(api.toStr(session, src, 10));
        must(api.setFromStr(session, o, ts, 0), "select copy");
        real[n.id] = o; setResult(n, ts); return;
      }
      if (n.type === "logic") {
        let ok = 0;
        if (n.op === "not") {
          const a = inputSlot(n, "a"); if (a == null) throw new Error("not 缺少 a");
          ok = truthySlot(a) ? 0 : 1;
        } else {
          const a = inputSlot(n, "a"), b = inputSlot(n, "b");
          if (a == null || b == null) throw new Error(n.op + " 缺少输入");
          const ta = truthySlot(a), tb = truthySlot(b);
          if (n.op === "and") ok = (ta && tb) ? 1 : 0;
          else if (n.op === "or") ok = (ta || tb) ? 1 : 0;
          else if (n.op === "xor") ok = (ta !== tb) ? 1 : 0;
          else if (n.op === "nand") ok = (ta && tb) ? 0 : 1;
          else if (n.op === "nor") ok = (ta || tb) ? 0 : 1;
          else if (n.op === "xnor") ok = (ta === tb) ? 1 : 0;
          else if (n.op === "implies") ok = (!ta || tb) ? 1 : 0;
          else throw new Error("unknown logic " + n.op);
        }
        const o = allocReal(); freelistR.push(o);
        must(api.setFromStr(session, o, String(ok), 0), "logic");
        real[n.id] = o; setResult(n, String(ok)); return;
      }
      if (n.type === "rng") {
        const o = allocReal(); freelistR.push(o);
        if (n.op === "seed") {
          let seedStr = (n.cfg && n.cfg.seed) != null ? String(n.cfg.seed) : "1";
          if (n.inputs.a != null) {
            const a = inputSlot(n, "a");
            if (a != null) seedStr = api.readCString(api.toStr(session, a, 10));
          }
          let seed = 0n;
          try {
            seed = BigInt(seedStr.trim());
            if (seed < 0n) seed = -seed;
          } catch (_) { seed = 1n; }
          const lo = Number(seed & 0xffffffffn);
          const hi = Number((seed >> 32n) & 0xffffffffn);
          must(api.seedRng(session, hi, lo), "seed_rng");
          must(api.setFromStr(session, o, seed.toString(), 0), "seed out");
          real[n.id] = o; setResult(n, "seeded " + seed.toString()); return;
        }
        if (n.op === "u64") {
          must(api.randomU64(session, o), "random_u64");
          real[n.id] = o; setResult(n, api.readCString(api.toStr(session, o, 10))); return;
        }
        if (n.op === "u64_mod") {
          let boundStr = (n.cfg && n.cfg.bound) != null ? String(n.cfg.bound) : "100";
          if (n.inputs.a != null) {
            const a = inputSlot(n, "a");
            if (a != null) boundStr = api.readCString(api.toStr(session, a, 10));
          }
          let bound = 100n;
          try { bound = BigInt(boundStr.trim()); if (bound < 1n) bound = 1n; } catch (_) { bound = 100n; }
          if (bound > 0xffffffffffffffffn) bound = 0xffffffffffffffffn;
          const lo = Number(bound & 0xffffffffn);
          const hi = Number((bound >> 32n) & 0xffffffffn);
          must(api.randomU64Mod(session, o, hi, lo), "random_u64_mod");
          real[n.id] = o; setResult(n, api.readCString(api.toStr(session, o, 10))); return;
        }
        throw new Error("unknown rng " + n.op);
      }
      /* ---- array / vector / matrix (JS tensors; RSA blocks, vectors) ---- */
      if (n.type === "arr_lit" || n.type === "vec_lit") {
        const nums = parseListLiteral(n.cfg && n.cfg.value);
        if (nums.some((x) => !Number.isFinite(x))) throw new Error("数组字面量含非数字");
        setTensorOut(n, { kind: n.type === "vec_lit" ? "vec" : "arr", data: nums });
        return;
      }
      if (n.type === "mat_lit") {
        const m = parseMatrixLiteral(n.cfg && n.cfg.value);
        if (!m.rows || !m.cols) throw new Error("empty matrix");
        if (m.data.some((x) => !Number.isFinite(x))) throw new Error("matrix literal non-numeric");
        if (hasSnTensorApi()) {
          const tid = allocT();
          const lit = tensorToStrLiteral({ kind: "mat", rows: m.rows, cols: m.cols, data: m.data });
          snMust(api.tensorFromStr(session, tid, lit), "mat_lit");
          setSnTensorOut(n, tid);
        } else {
          setTensorOut(n, { kind: "mat", rows: m.rows, cols: m.cols, data: m.data });
        }
        return;
      }
      if (n.type === "arr_from_range") {
        let first = 0, last = 0;
        const a = inputSlot(n, "first"), b = inputSlot(n, "last");
        if (a != null) first = slotToInt(a, 0);
        else if (n.cfg && n.cfg.first != null) first = parseInt(n.cfg.first, 10) || 0;
        if (b != null) last = slotToInt(b, 0);
        else if (n.cfg && n.cfg.last != null) last = parseInt(n.cfg.last, 10) || 0;
        if (last - first > 100000) throw new Error("Range 过大");
        const data = [];
        for (let i = first; i <= last; i++) data.push(i);
        setTensorOut(n, { kind: "arr", data });
        return;
      }
      if (n.type === "mat_identity") {
        let nn = 3;
        const s = inputSlot(n, "n");
        if (s != null) nn = slotToInt(s, 3);
        else if (n.cfg && n.cfg.n != null) nn = parseInt(n.cfg.n, 10) || 3;
        nn = Math.max(0, Math.min(256, nn));
        const data = new Array(nn * nn).fill(0);
        for (let i = 0; i < nn; i++) data[i * nn + i] = 1;
        setTensorOut(n, { kind: "mat", rows: nn, cols: nn, data });
        return;
      }
      if (n.type === "arr_len") {
        const t = inputTensor(n, "a");
        if (!t) throw new Error("Len 缺少数组");
        const len = t.kind === "mat" ? (t.rows * t.cols) : (t.data || []).length;
        const o = makeScalarSlot(len);
        real[n.id] = o; setResult(n, String(len)); return;
      }
      if (n.type === "arr_get") {
        const t = inputTensor(n, "a");
        const iSlot = inputSlot(n, "i");
        if (!t || iSlot == null) throw new Error("Get 缺少 a/i");
        const i = slotToInt(iSlot, 0);
        const arr = t.data || [];
        if (i < 0 || i >= arr.length) throw new Error("索引越界 " + i);
        const o = makeScalarSlot(arr[i]);
        real[n.id] = o; setResult(n, String(arr[i])); return;
      }
      if (n.type === "arr_set") {
        const t = inputTensor(n, "a");
        const iSlot = inputSlot(n, "i"), vSlot = inputSlot(n, "v");
        if (!t || iSlot == null || vSlot == null) throw new Error("Set 缺少输入");
        const i = slotToInt(iSlot, 0);
        const v = slotToNumber(vSlot);
        const out = cloneTensor(t);
        if (out.kind === "mat") {
          if (i < 0 || i >= out.data.length) throw new Error("索引越界");
          out.data[i] = v;
        } else {
          if (i < 0 || i >= out.data.length) throw new Error("索引越界");
          out.data[i] = v;
        }
        setTensorOut(n, out); return;
      }
      if (n.type === "arr_push") {
        const t = inputTensor(n, "a");
        const vSlot = inputSlot(n, "v");
        if (!t || vSlot == null) throw new Error("Push 缺少输入");
        if (t.kind === "mat") throw new Error("Push 不支持矩阵，请用 Set");
        const out = cloneTensor(t);
        out.data.push(slotToNumber(vSlot));
        setTensorOut(n, out); return;
      }
      if (n.type === "arr_slice") {
        const t = inputTensor(n, "a");
        const s0 = inputSlot(n, "start"), s1 = inputSlot(n, "end");
        if (!t) throw new Error("Slice 缺少 a");
        if (t.kind === "mat") throw new Error("Slice 请先展平或用 mat_get");
        const start = s0 != null ? slotToInt(s0, 0) : 0;
        const end = s1 != null ? slotToInt(s1, t.data.length) : t.data.length;
        const out = { kind: t.kind, data: t.data.slice(start, end) };
        setTensorOut(n, out); return;
      }
      if (n.type === "arr_concat") {
        const a = inputTensor(n, "a"), b = inputTensor(n, "b");
        if (!a || !b) throw new Error("Concat 缺少 a/b");
        if (a.kind === "mat" || b.kind === "mat") throw new Error("Concat 仅数组/向量");
        setTensorOut(n, { kind: a.kind === "vec" && b.kind === "vec" ? "vec" : "arr", data: a.data.concat(b.data) });
        return;
      }
      if (n.type === "arr_map") {
        const a = inputTensor(n, "a");
        const s = inputSlot(n, "s");
        if (!a || s == null) throw new Error("Map 缺少 a/s");
        if (a.kind === "mat") {
          /* map over all elements */
        }
        const sv = slotToNumber(s);
        const op = (n.cfg && n.cfg.op) || "add";
        const data = a.data.map((x) => {
          if (op === "sub") return x - sv;
          if (op === "mul") return x * sv;
          if (op === "div") return x / sv;
          return x + sv;
        });
        const out = cloneTensor(a); out.data = data;
        setTensorOut(n, out); return;
      }
      if (n.type === "arr_reduce") {
        const a = inputTensor(n, "a");
        if (!a) throw new Error("Reduce 缺少 a");
        const op = (n.cfg && n.cfg.op) || "sum";
        const arr = a.data || [];
        if (!arr.length) throw new Error("空数组");
        let acc = arr[0];
        for (let i = 1; i < arr.length; i++) {
          const x = arr[i];
          if (op === "prod") acc *= x;
          else if (op === "min") acc = Math.min(acc, x);
          else if (op === "max") acc = Math.max(acc, x);
          else acc += x;
        }
        const o = makeScalarSlot(acc);
        real[n.id] = o; setResult(n, String(acc)); return;
      }
      if (n.type === "vec_dot") {
        const a = inputTensor(n, "a"), b = inputTensor(n, "b");
        if (!a || !b) throw new Error("Dot 缺少 a/b");
        if (a.data.length !== b.data.length) throw new Error("点积长度不一致");
        let s = 0;
        for (let i = 0; i < a.data.length; i++) s += a.data[i] * b.data[i];
        const o = makeScalarSlot(s);
        real[n.id] = o; setResult(n, String(s)); return;
      }
      if (n.type === "vec_scale") {
        const a = inputTensor(n, "a");
        const s = inputSlot(n, "s");
        if (!a || s == null) throw new Error("Scale 缺少输入");
        const sv = slotToNumber(s);
        const out = cloneTensor(a);
        out.data = out.data.map((x) => x * sv);
        setTensorOut(n, out); return;
      }
      if (n.type === "vec_add") {
        const a = inputTensor(n, "a"), b = inputTensor(n, "b");
        if (!a || !b) throw new Error("Vec add 缺少 a/b");
        if (a.data.length !== b.data.length) throw new Error("向量长度不一致");
        const out = cloneTensor(a);
        out.data = a.data.map((x, i) => x + b.data[i]);
        setTensorOut(n, out); return;
      }
      if (n.type === "mat_dims") {
        const a = inputTensor(n, "a");
        if (!a || a.kind !== "mat") throw new Error("Mat dims 需要矩阵");
        const o = makeScalarSlot(a.rows);
        const c = makeScalarSlot(a.cols);
        real[n.id] = o;
        realPort[n.id + "|cols"] = c;
        setResult(n, a.rows + "x" + a.cols); return;
      }
      if (n.type === "mat_get") {
        const a = inputTensor(n, "a");
        const rS = inputSlot(n, "r"), cS = inputSlot(n, "c");
        if (!a || a.kind !== "mat" || rS == null || cS == null) throw new Error("Mat get 缺少输入");
        const r = slotToInt(rS, 0), c = slotToInt(cS, 0);
        if (r < 0 || c < 0 || r >= a.rows || c >= a.cols) throw new Error("矩阵索引越界");
        const v = a.data[r * a.cols + c];
        const o = makeScalarSlot(v);
        real[n.id] = o; setResult(n, String(v)); return;
      }
      if (n.type === "mat_set") {
        const a = inputTensor(n, "a");
        const rS = inputSlot(n, "r"), cS = inputSlot(n, "c"), vS = inputSlot(n, "v");
        if (!a || a.kind !== "mat" || rS == null || cS == null || vS == null) throw new Error("Mat set 缺少输入");
        const r = slotToInt(rS, 0), c = slotToInt(cS, 0), v = slotToNumber(vS);
        if (r < 0 || c < 0 || r >= a.rows || c >= a.cols) throw new Error("矩阵索引越界");
        const out = cloneTensor(a);
        out.data[r * out.cols + c] = v;
        setTensorOut(n, out); return;
      }
      if (n.type === "mat_transpose") {
        if (hasSnTensorApi()) {
          const a = inputSnTensor(n, "a"); if (a < 0) throw new Error("Transpose missing a");
          const o = allocT(); snMust(api.tensorUnary(session, o, "transpose", a), "transpose");
          setSnTensorOut(n, o); return;
        }
        const a = inputTensor(n, "a");
        if (!a || a.kind !== "mat") throw new Error("Transpose needs matrix");
        const data = new Array(a.rows * a.cols);
        for (let r = 0; r < a.rows; r++)
          for (let c0 = 0; c0 < a.cols; c0++)
            data[c0 * a.rows + r] = a.data[r * a.cols + c0];
        setTensorOut(n, { kind: "mat", rows: a.cols, cols: a.rows, data });
        return;
      }
      if (n.type === "mat_mul") {
        if (hasSnTensorApi()) {
          const a = inputSnTensor(n, "a"), b = inputSnTensor(n, "b");
          if (a < 0 || b < 0) throw new Error("MatMul missing a/b");
          const o = allocT();
          snMust(api.tensorBinary(session, o, "matmul", a, b), "mat_mul");
          setSnTensorOut(n, o); return;
        }
        const a = inputTensor(n, "a"), b = inputTensor(n, "b");
        if (!a || !b) throw new Error("MatMul missing a/b");
        const A = a.kind === "mat" ? a : { kind: "mat", rows: 1, cols: a.data.length, data: a.data };
        const B = b.kind === "mat" ? b : { kind: "mat", rows: b.data.length, cols: 1, data: b.data };
        if (A.cols !== B.rows) throw new Error("MatMul dim " + A.cols + " != " + B.rows);
        const data = new Array(A.rows * B.cols).fill(0);
        for (let i = 0; i < A.rows; i++)
          for (let k = 0; k < A.cols; k++)
            for (let j = 0; j < B.cols; j++)
              data[i * B.cols + j] += A.data[i * A.cols + k] * B.data[k * B.cols + j];
        setTensorOut(n, { kind: "mat", rows: A.rows, cols: B.cols, data });
        return;
      }

      /* ---- extended tensor / ML primitives (web-side) ---- */
      function asMat(t) {
        if (!t) return null;
        if (t.kind === "mat") return { kind: "mat", rows: t.rows, cols: t.cols, data: t.data.slice() };
        const d = (t.data || []).slice();
        return { kind: "mat", rows: 1, cols: d.length, data: d };
      }
      function asVec(t) {
        if (!t) return null;
        if (t.kind === "mat") return { kind: "vec", data: t.data.slice() };
        return { kind: t.kind || "arr", data: (t.data || []).slice() };
      }
      function tensorSize(t) {
        if (!t) return 0;
        if (t.kind === "mat") return (t.rows | 0) * (t.cols | 0);
        return (t.data || []).length;
      }
      function cfgNum(n, key, def) {
        if (n.cfg && n.cfg[key] != null && String(n.cfg[key]).trim() !== "") {
          const x = Number(n.cfg[key]);
          if (Number.isFinite(x)) return x;
        }
        return def;
      }
      function portOrCfgInt(n, port, cfgKey, def) {
        const s = inputSlot(n, port);
        if (s != null) return slotToInt(s, def);
        return Math.trunc(cfgNum(n, cfgKey, def));
      }
      function portOrCfgNum(n, port, cfgKey, def) {
        const s = inputSlot(n, port);
        if (s != null) {
          const x = slotToNumber(s);
          if (Number.isFinite(x)) return x;
        }
        return cfgNum(n, cfgKey, def);
      }
      function broadcastPair(a0, b0) {
        let A = asMat(a0), B = asMat(b0);
        if (!A || !B) throw new Error("需要两个张量");
        /* scalar 1x1 broadcast */
        if (A.rows === 1 && A.cols === 1 && (B.rows > 1 || B.cols > 1)) {
          const v = A.data[0];
          A = { kind: "mat", rows: B.rows, cols: B.cols, data: new Array(B.rows * B.cols).fill(v) };
        } else if (B.rows === 1 && B.cols === 1 && (A.rows > 1 || A.cols > 1)) {
          const v = B.data[0];
          B = { kind: "mat", rows: A.rows, cols: A.cols, data: new Array(A.rows * A.cols).fill(v) };
        }
        /* row vector broadcast to matrix rows */
        if (A.rows === 1 && B.rows > 1 && A.cols === B.cols) {
          const data = [];
          for (let r = 0; r < B.rows; r++) for (let c = 0; c < A.cols; c++) data.push(A.data[c]);
          A = { kind: "mat", rows: B.rows, cols: A.cols, data };
        } else if (B.rows === 1 && A.rows > 1 && A.cols === B.cols) {
          const data = [];
          for (let r = 0; r < A.rows; r++) for (let c = 0; c < B.cols; c++) data.push(B.data[c]);
          B = { kind: "mat", rows: A.rows, cols: B.cols, data };
        }
        /* col vector broadcast */
        if (A.cols === 1 && B.cols > 1 && A.rows === B.rows) {
          const data = [];
          for (let r = 0; r < A.rows; r++) for (let c = 0; c < B.cols; c++) data.push(A.data[r]);
          A = { kind: "mat", rows: A.rows, cols: B.cols, data };
        } else if (B.cols === 1 && A.cols > 1 && A.rows === B.rows) {
          const data = [];
          for (let r = 0; r < B.rows; r++) for (let c = 0; c < A.cols; c++) data.push(B.data[r]);
          B = { kind: "mat", rows: B.rows, cols: A.cols, data };
        }
        if (A.rows !== B.rows || A.cols !== B.cols) {
          throw new Error("广播失败 " + A.rows + "x" + A.cols + " vs " + B.rows + "x" + B.cols);
        }
        return [A, B];
      }
      function elemBinop(op, a, b) {
        const [A, B] = broadcastPair(a, b);
        const data = new Array(A.data.length);
        for (let i = 0; i < data.length; i++) {
          const x = A.data[i], y = B.data[i];
          if (op === "add") data[i] = x + y;
          else if (op === "sub") data[i] = x - y;
          else if (op === "mul" || op === "hadamard") data[i] = x * y;
          else if (op === "div") data[i] = x / y;
          else if (op === "max") data[i] = Math.max(x, y);
          else if (op === "min") data[i] = Math.min(x, y);
          else throw new Error("unknown ten binop " + op);
        }
        return { kind: "mat", rows: A.rows, cols: A.cols, data };
      }
      function geluApprox(x) {
        /* tanh approximation */
        const c = Math.sqrt(2 / Math.PI);
        const u = c * (x + 0.044715 * x * x * x);
        return 0.5 * x * (1 + Math.tanh(u));
      }
      function applyUnary(op, x) {
        switch (op) {
          case "neg": return -x;
          case "abs": return Math.abs(x);
          case "exp": return Math.exp(x);
          case "log": return Math.log(x);
          case "sqrt": return Math.sqrt(x);
          case "square": return x * x;
          case "sigmoid": return 1 / (1 + Math.exp(-x));
          case "tanh": return Math.tanh(x);
          case "relu": return x > 0 ? x : 0;
          case "gelu": return geluApprox(x);
          case "silu": case "swish": return x / (1 + Math.exp(-x));
          case "softplus": return Math.log1p(Math.exp(x));
          default: throw new Error("unknown ten unary " + op);
        }
      }
      function mapElems(t0, fn) {
        const A = asMat(t0);
        if (!A) throw new Error("需要张量");
        return { kind: "mat", rows: A.rows, cols: A.cols, data: A.data.map(fn) };
      }
      function rowReduce(t0, mode) {
        const A = asMat(t0);
        if (!A) throw new Error("需要矩阵");
        const data = new Array(A.rows);
        for (let r = 0; r < A.rows; r++) {
          let acc = mode === "max" ? -Infinity : mode === "min" ? Infinity : 0;
          for (let c = 0; c < A.cols; c++) {
            const v = A.data[r * A.cols + c];
            if (mode === "sum" || mode === "mean") acc += v;
            else if (mode === "max") acc = Math.max(acc, v);
            else if (mode === "min") acc = Math.min(acc, v);
          }
          if (mode === "mean") acc = A.cols ? acc / A.cols : 0;
          data[r] = acc;
        }
        return { kind: "mat", rows: A.rows, cols: 1, data };
      }
      function softmaxRows(t0) {
        const A = asMat(t0);
        if (!A) throw new Error("softmax 需要矩阵");
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
          const inv = s ? 1 / s : 0;
          for (let c = 0; c < A.cols; c++) data[r * A.cols + c] *= inv;
        }
        return { kind: "mat", rows: A.rows, cols: A.cols, data };
      }
      function layerNormRows(t0, gammaT, betaT, eps) {
        const A = asMat(t0);
        if (!A) throw new Error("LayerNorm 需要矩阵");
        const g = gammaT ? asMat(gammaT) : null;
        const b = betaT ? asMat(betaT) : null;
        const data = new Array(A.rows * A.cols);
        for (let r = 0; r < A.rows; r++) {
          let mean = 0;
          for (let c = 0; c < A.cols; c++) mean += A.data[r * A.cols + c];
          mean /= A.cols || 1;
          let var_ = 0;
          for (let c = 0; c < A.cols; c++) {
            const d = A.data[r * A.cols + c] - mean;
            var_ += d * d;
          }
          var_ /= A.cols || 1;
          const inv = 1 / Math.sqrt(var_ + eps);
          for (let c = 0; c < A.cols; c++) {
            let y = (A.data[r * A.cols + c] - mean) * inv;
            if (g) {
              const gv = g.kind === "mat" && g.rows === 1 ? g.data[c % g.cols] : g.data[c];
              y *= (gv != null && Number.isFinite(gv) ? gv : 1);
            }
            if (b) {
              const bv = b.kind === "mat" && b.rows === 1 ? b.data[c % b.cols] : b.data[c];
              y += (bv != null && Number.isFinite(bv) ? bv : 0);
            }
            data[r * A.cols + c] = y;
          }
        }
        return { kind: "mat", rows: A.rows, cols: A.cols, data };
      }
      function rmsNormRows(t0, gammaT, eps) {
        const A = asMat(t0);
        if (!A) throw new Error("RMSNorm 需要矩阵");
        const g = gammaT ? asMat(gammaT) : null;
        const data = new Array(A.rows * A.cols);
        for (let r = 0; r < A.rows; r++) {
          let ms = 0;
          for (let c = 0; c < A.cols; c++) {
            const v = A.data[r * A.cols + c];
            ms += v * v;
          }
          ms /= A.cols || 1;
          const inv = 1 / Math.sqrt(ms + eps);
          for (let c = 0; c < A.cols; c++) {
            let y = A.data[r * A.cols + c] * inv;
            if (g) {
              const gv = g.rows === 1 ? g.data[c % g.cols] : g.data[c];
              y *= (gv != null && Number.isFinite(gv) ? gv : 1);
            }
            data[r * A.cols + c] = y;
          }
        }
        return { kind: "mat", rows: A.rows, cols: A.cols, data };
      }
      function matMulCore(A0, B0) {
        let A = A0.kind === "mat" ? A0 : { kind: "mat", rows: 1, cols: A0.data.length, data: A0.data };
        let B = B0.kind === "mat" ? B0 : { kind: "mat", rows: B0.data.length, cols: 1, data: B0.data };
        if (A.cols !== B.rows) throw new Error("MatMul 维度 " + A.cols + " != " + B.rows);
        const data = new Array(A.rows * B.cols).fill(0);
        for (let i = 0; i < A.rows; i++) {
          for (let k = 0; k < A.cols; k++) {
            const aik = A.data[i * A.cols + k];
            for (let j = 0; j < B.cols; j++) data[i * B.cols + j] += aik * B.data[k * B.cols + j];
          }
        }
        return { kind: "mat", rows: A.rows, cols: B.cols, data };
      }
      function transposeMat(A) {
        const data = new Array(A.rows * A.cols);
        for (let r = 0; r < A.rows; r++)
          for (let c = 0; c < A.cols; c++)
            data[c * A.rows + r] = A.data[r * A.cols + c];
        return { kind: "mat", rows: A.cols, cols: A.rows, data };
      }
      function causalMask(n) {
        const data = new Array(n * n);
        for (let i = 0; i < n; i++)
          for (let j = 0; j < n; j++)
            data[i * n + j] = j <= i ? 0 : -1e9;
        return { kind: "mat", rows: n, cols: n, data };
      }

      if (n.type === "mat_add" || n.type === "residual_add") {
        if (hasSnTensorApi()) {
          const a = inputSnTensor(n, "a"), b = inputSnTensor(n, "b");
          if (a < 0 || b < 0) throw new Error(n.type + " missing a/b");
          const o = allocT();
          snMust(api.tensorBinary(session, o, "add", a, b), n.type);
          setSnTensorOut(n, o); return;
        }
        const a = inputTensor(n, "a"), b = inputTensor(n, "b");
        if (!a || !b) throw new Error(n.type + " missing a/b");
        setTensorOut(n, elemBinop("add", a, b)); return;
      }
      if (n.type === "mat_sub") {
        if (hasSnTensorApi()) {
          const a = inputSnTensor(n, "a"), b = inputSnTensor(n, "b");
          if (a < 0 || b < 0) throw new Error("Mat sub missing a/b");
          const o = allocT();
          snMust(api.tensorBinary(session, o, "sub", a, b), "mat_sub");
          setSnTensorOut(n, o); return;
        }
        const a = inputTensor(n, "a"), b = inputTensor(n, "b");
        if (!a || !b) throw new Error("Mat sub missing a/b");
        setTensorOut(n, elemBinop("sub", a, b)); return;
      }
      if (n.type === "mat_hadamard") {
        if (hasSnTensorApi()) {
          const a = inputSnTensor(n, "a"), b = inputSnTensor(n, "b");
          if (a < 0 || b < 0) throw new Error("Hadamard missing a/b");
          const o = allocT();
          snMust(api.tensorBinary(session, o, "hadamard", a, b), "hadamard");
          setSnTensorOut(n, o); return;
        }
        const a = inputTensor(n, "a"), b = inputTensor(n, "b");
        if (!a || !b) throw new Error("Hadamard missing a/b");
        setTensorOut(n, elemBinop("mul", a, b)); return;
      }
      if (n.type === "ten_binop") {
        const a = inputTensor(n, "a"), b = inputTensor(n, "b");
        if (!a || !b) throw new Error("ten_binop 缺少 a/b");
        setTensorOut(n, elemBinop((n.cfg && n.cfg.op) || "add", a, b)); return;
      }
      if (n.type === "mat_scale") {
        let s = 1;
        const sSlot = inputSlot(n, "s");
        if (sSlot != null) s = slotToNumber(sSlot);
        else if (n.cfg && n.cfg.portValues && n.cfg.portValues.s != null) s = Number(n.cfg.portValues.s);
        if (hasSnTensorApi()) {
          const a = inputSnTensor(n, "a");
          if (a < 0) throw new Error("Mat scale missing a");
          const o = allocT();
          snMust(api.tensorScale(session, o, a, s), "mat_scale");
          setSnTensorOut(n, o); return;
        }
        const a = inputTensor(n, "a");
        if (!a) throw new Error("Mat scale missing a");
        const A = asMat(a);
        setTensorOut(n, { kind: "mat", rows: A.rows, cols: A.cols, data: A.data.map((x) => x * s) });
        return;
      }
      if (n.type === "ten_unary") {
        const op = (n.cfg && n.cfg.op) || "exp";
        if (hasSnTensorApi() && ["neg","exp","tanh","relu","gelu","silu","swish","sqrt","abs"].indexOf(op) >= 0) {
          const a = inputSnTensor(n, "a"); if (a < 0) throw new Error("ten_unary missing a");
          const o = allocT(); snMust(api.tensorUnary(session, o, op === "swish" ? "silu" : op, a), "ten_unary");
          setSnTensorOut(n, o); return;
        }
        const a = inputTensor(n, "a");
        if (!a) throw new Error("ten_unary missing a");
        setTensorOut(n, mapElems(a, (x) => applyUnary(op, x))); return;
      }
      if (n.type === "gelu") {
        if (hasSnTensorApi()) {
          const a = inputSnTensor(n, "a"); if (a < 0) throw new Error("GELU missing a");
          const o = allocT(); snMust(api.tensorUnary(session, o, "gelu", a), "gelu");
          setSnTensorOut(n, o); return;
        }
        const a = inputTensor(n, "a"); if (!a) throw new Error("GELU missing a");
        setTensorOut(n, mapElems(a, geluApprox)); return;
      }
      if (n.type === "relu") {
        if (hasSnTensorApi()) {
          const a = inputSnTensor(n, "a"); if (a < 0) throw new Error("ReLU missing a");
          const o = allocT(); snMust(api.tensorUnary(session, o, "relu", a), "relu");
          setSnTensorOut(n, o); return;
        }
        const a = inputTensor(n, "a"); if (!a) throw new Error("ReLU missing a");
        setTensorOut(n, mapElems(a, (x) => (x > 0 ? x : 0))); return;
      }
      
      if (n.type === "silu") {
        if (hasSnTensorApi()) {
          const a = inputSnTensor(n, "a"); if (a < 0) throw new Error("SiLU missing a");
          const o = allocT(); snMust(api.tensorUnary(session, o, "silu", a), "silu");
          setSnTensorOut(n, o); return;
        }
        const a = inputTensor(n, "a"); if (!a) throw new Error("SiLU missing a");
        setTensorOut(n, mapElems(a, (x) => x / (1 + Math.exp(-x)))); return;
      }
      if (n.type === "swiglu") {
        /* SiLU(gate) ⊙ up */
        if (hasSnTensorApi()) {
          const g = inputSnTensor(n, "gate"), u = inputSnTensor(n, "up");
          if (g < 0 || u < 0) throw new Error("SwiGLU missing gate/up");
          const g2 = allocT(); snMust(api.tensorUnary(session, g2, "silu", g), "swiglu silu");
          const o = allocT(); snMust(api.tensorBinary(session, o, "hadamard", g2, u), "swiglu mul");
          setSnTensorOut(n, o); return;
        }
        const g = inputTensor(n, "gate"), u = inputTensor(n, "up");
        if (!g || !u) throw new Error("SwiGLU missing gate/up");
        const G = mapElems(g, (x) => x / (1 + Math.exp(-x)));
        setTensorOut(n, elemBinop("mul", G, u)); return;
      }
      if (n.type === "rope") {
        const base = cfgNum(n, "base", 10000);
        if (hasSnTensorApi() && api.tensorRope) {
          const a = inputSnTensor(n, "a"); if (a < 0) throw new Error("RoPE missing a");
          const o = allocT(); snMust(api.tensorRope(session, o, a, base), "rope");
          setSnTensorOut(n, o); return;
        }
        const a = inputTensor(n, "a"); if (!a) throw new Error("RoPE missing a");
        // JS fallback: even/odd rotate per row
        const A = asMat(a);
        const data = A.data.slice();
        const half = Math.floor(A.cols / 2);
        for (let r = 0; r < A.rows; r++) {
          for (let i = 0; i < half; i++) {
            const theta = r / Math.pow(base, (2 * i) / Math.max(1, A.cols));
            const cos = Math.cos(theta), sin = Math.sin(theta);
            const x0 = A.data[r * A.cols + 2 * i];
            const x1 = A.data[r * A.cols + 2 * i + 1];
            data[r * A.cols + 2 * i] = x0 * cos - x1 * sin;
            data[r * A.cols + 2 * i + 1] = x0 * sin + x1 * cos;
          }
        }
        setTensorOut(n, { kind: "mat", rows: A.rows, cols: A.cols, data }); return;
      }
if (n.type === "mat_row_sum") {
        const a = inputTensor(n, "a"); if (!a) throw new Error("Row sum 缺少 a");
        setTensorOut(n, rowReduce(a, "sum")); return;
      }
      if (n.type === "mat_row_mean") {
        const a = inputTensor(n, "a"); if (!a) throw new Error("Row mean 缺少 a");
        setTensorOut(n, rowReduce(a, "mean")); return;
      }
      if (n.type === "mat_row_max") {
        const a = inputTensor(n, "a"); if (!a) throw new Error("Row max 缺少 a");
        setTensorOut(n, rowReduce(a, "max")); return;
      }
      if (n.type === "softmax_row") {
        if (hasSnTensorApi()) {
          const a = inputSnTensor(n, "a"); if (a < 0) throw new Error("softmax missing a");
          const o = allocT(); snMust(api.tensorUnary(session, o, "softmax_row", a), "softmax");
          setSnTensorOut(n, o); return;
        }
        const a = inputTensor(n, "a"); if (!a) throw new Error("softmax missing a");
        setTensorOut(n, softmaxRows(a)); return;
      }
      if (n.type === "layer_norm") {
        const eps = cfgNum(n, "eps", 1e-5);
        if (hasSnTensorApi() && api.tensorLayerNorm) {
          const a = inputSnTensor(n, "a"); if (a < 0) throw new Error("LayerNorm missing a");
          let g = -1, b = -1;
          if (n.inputs && n.inputs.gamma != null) g = inputSnTensor(n, "gamma");
          if (n.inputs && n.inputs.beta != null) b = inputSnTensor(n, "beta");
          const o = allocT();
          snMust(api.tensorLayerNorm(session, o, a, g, b, eps), "layer_norm");
          setSnTensorOut(n, o); return;
        }
        const a = inputTensor(n, "a"); if (!a) throw new Error("LayerNorm missing a");
        const g = inputTensor(n, "gamma"), b = inputTensor(n, "beta");
        setTensorOut(n, layerNormRows(a, g, b, eps)); return;
      }
      if (n.type === "rms_norm") {
        const eps = cfgNum(n, "eps", 1e-5);
        if (hasSnTensorApi()) {
          const a = inputSnTensor(n, "a"); if (a < 0) throw new Error("RMSNorm missing a");
          let g = -1;
          if (n.inputs && n.inputs.gamma != null) g = inputSnTensor(n, "gamma");
          const o = allocT();
          snMust(api.tensorRmsNorm(session, o, a, g, eps), "rms_norm");
          setSnTensorOut(n, o); return;
        }
        const a = inputTensor(n, "a"); if (!a) throw new Error("RMSNorm missing a");
        const g = inputTensor(n, "gamma");
        setTensorOut(n, rmsNormRows(a, g, eps)); return;
      }
      if (n.type === "flatten") {
        const a = inputTensor(n, "a"); if (!a) throw new Error("Flatten 缺少 a");
        const data = a.kind === "mat" ? a.data.slice() : (a.data || []).slice();
        setTensorOut(n, { kind: "arr", data }); return;
      }
      if (n.type === "reshape") {
        let rows = portOrCfgInt(n, "rows", "rows", 0);
        let cols = portOrCfgInt(n, "cols", "cols", 0);
        if (hasSnTensorApi()) {
          const a = inputSnTensor(n, "a"); if (a < 0) throw new Error("Reshape missing a");
          // dims may need inference from JS view
          const t = inputTensor(n, "a");
          const N = t ? (t.kind === "mat" ? t.rows * t.cols : (t.data || []).length) : 0;
          if (rows <= 0 && cols > 0 && N) rows = Math.floor(N / cols);
          if (cols <= 0 && rows > 0 && N) cols = Math.floor(N / rows);
          if (rows <= 0 || cols <= 0) throw new Error("Reshape needs positive rows/cols");
          const o = allocT();
          snMust(api.tensorReshape(session, o, a, rows, cols), "reshape");
          setSnTensorOut(n, o); return;
        }
        const a = inputTensor(n, "a"); if (!a) throw new Error("Reshape missing a");
        const data = a.kind === "mat" ? a.data.slice() : (a.data || []).slice();
        const N = data.length;
        if (rows <= 0 && cols > 0) rows = Math.floor(N / cols);
        if (cols <= 0 && rows > 0) cols = Math.floor(N / rows);
        if (rows <= 0 || cols <= 0) throw new Error("Reshape needs positive rows/cols");
        if (rows * cols !== N) throw new Error("Reshape size " + N + " != " + rows + "x" + cols);
        setTensorOut(n, { kind: "mat", rows, cols, data }); return;
      }
      if (n.type === "mat_concat") {
        const a = inputTensor(n, "a"), b = inputTensor(n, "b");
        if (!a || !b) throw new Error("Concat 缺少 a/b");
        const A = asMat(a), B = asMat(b);
        const axis = portOrCfgInt(n, "axis", "axis", cfgNum(n, "axis", 0)) | 0;
        if (axis === 0) {
          if (A.cols !== B.cols) throw new Error("Concat axis0 要求 cols 相同");
          setTensorOut(n, { kind: "mat", rows: A.rows + B.rows, cols: A.cols, data: A.data.concat(B.data) });
        } else {
          if (A.rows !== B.rows) throw new Error("Concat axis1 要求 rows 相同");
          const cols = A.cols + B.cols;
          const data = new Array(A.rows * cols);
          for (let r = 0; r < A.rows; r++) {
            for (let c = 0; c < A.cols; c++) data[r * cols + c] = A.data[r * A.cols + c];
            for (let c = 0; c < B.cols; c++) data[r * cols + A.cols + c] = B.data[r * B.cols + c];
          }
          setTensorOut(n, { kind: "mat", rows: A.rows, cols, data });
        }
        return;
      }
      if (n.type === "mat_slice") {
        const a = inputTensor(n, "a"); if (!a) throw new Error("Slice 缺少 a");
        const A = asMat(a);
        let r0 = portOrCfgInt(n, "r0", "r0", 0);
        let r1 = portOrCfgInt(n, "r1", "r1", -1);
        let c0 = portOrCfgInt(n, "c0", "c0", 0);
        let c1 = portOrCfgInt(n, "c1", "c1", -1);
        if (r1 < 0) r1 = A.rows;
        if (c1 < 0) c1 = A.cols;
        r0 = Math.max(0, Math.min(A.rows, r0));
        r1 = Math.max(r0, Math.min(A.rows, r1));
        c0 = Math.max(0, Math.min(A.cols, c0));
        c1 = Math.max(c0, Math.min(A.cols, c1));
        const rows = r1 - r0, cols = c1 - c0;
        const data = new Array(rows * cols);
        for (let r = 0; r < rows; r++)
          for (let c = 0; c < cols; c++)
            data[r * cols + c] = A.data[(r0 + r) * A.cols + (c0 + c)];
        setTensorOut(n, { kind: "mat", rows, cols, data }); return;
      }
      if (n.type === "mat_outer") {
        const a = inputTensor(n, "a"), b = inputTensor(n, "b");
        if (!a || !b) throw new Error("Outer 缺少 a/b");
        const av = asVec(a).data, bv = asVec(b).data;
        const data = new Array(av.length * bv.length);
        for (let i = 0; i < av.length; i++)
          for (let j = 0; j < bv.length; j++)
            data[i * bv.length + j] = av[i] * bv[j];
        setTensorOut(n, { kind: "mat", rows: av.length, cols: bv.length, data }); return;
      }
      if (n.type === "mat_diag") {
        const a = inputTensor(n, "a"); if (!a) throw new Error("Diag 缺少 a");
        if (a.kind === "mat" && a.rows === a.cols) {
          const data = [];
          for (let i = 0; i < a.rows; i++) data.push(a.data[i * a.cols + i]);
          setTensorOut(n, { kind: "vec", data }); return;
        }
        const v = asVec(a).data;
        const n0 = v.length;
        const data = new Array(n0 * n0).fill(0);
        for (let i = 0; i < n0; i++) data[i * n0 + i] = v[i];
        setTensorOut(n, { kind: "mat", rows: n0, cols: n0, data }); return;
      }
      if (n.type === "gather" || n.type === "embedding") {
        const table = inputTensor(n, "table");
        const idxT = inputTensor(n, "idx");
        if (!table) throw new Error("Gather 缺少 table");
        const T = asMat(table);
        let indices = [];
        if (idxT) {
          indices = asVec(idxT).data.map((x) => Math.trunc(x));
        } else {
          const s = inputSlot(n, "idx");
          if (s == null) throw new Error("Gather 缺少 idx");
          indices = [slotToInt(s, 0)];
        }
        const data = [];
        for (let i = 0; i < indices.length; i++) {
          const r = indices[i];
          if (r < 0 || r >= T.rows) throw new Error("Gather 索引越界 " + r);
          for (let c = 0; c < T.cols; c++) data.push(T.data[r * T.cols + c]);
        }
        setTensorOut(n, { kind: "mat", rows: indices.length, cols: T.cols, data }); return;
      }
      if (n.type === "sin_pe") {
        const seq = Math.max(1, portOrCfgInt(n, "seq", "seq", cfgNum(n, "seq", 4)));
        const dim = Math.max(2, portOrCfgInt(n, "dim", "dim", cfgNum(n, "dim", 8)));
        const base = cfgNum(n, "base", 10000);
        if (hasSnTensorApi() && api.tensorSinPe) {
          const o = allocT();
          snMust(api.tensorSinPe(session, o, seq, dim, base), "sin_pe");
          setSnTensorOut(n, o); return;
        }
        const data = new Array(seq * dim);
        for (let pos = 0; pos < seq; pos++) {
          for (let i = 0; i < dim; i++) {
            const half = Math.floor(i / 2);
            const den = Math.pow(base > 0 ? base : 10000, (2 * half) / dim);
            const ang = pos / den;
            data[pos * dim + i] = (i % 2 === 0) ? Math.sin(ang) : Math.cos(ang);
          }
        }
        setTensorOut(n, { kind: "mat", rows: seq, cols: dim, data }); return;
      }
      if (n.type === "attention_sdp") {
        if (hasSnTensorApi() && api.tensorAttentionSdp) {
          const q = inputSnTensor(n, "q"), k = inputSnTensor(n, "k"), v = inputSnTensor(n, "v");
          if (q < 0 || k < 0 || v < 0) throw new Error("SDPA needs q/k/v");
          let scale = cfgNum(n, "scale", NaN);
          if (!Number.isFinite(scale) || String((n.cfg && n.cfg.scale) || "auto") === "auto") {
            // scale=0 in C means auto (1/sqrt(dk))
            scale = 0;
          }
          const causal = (n.cfg && n.cfg.causal) ? 1 : 0;
          const o = allocT();
          const w = allocT(); // optional weights buffer
          snMust(api.tensorAttentionSdp(session, o, w, q, k, v, causal, scale), "attention_sdp");
          setSnTensorOut(n, o);
          try {
            const ws = snTensorStr(w);
            const wt = parseSnMatStr(ws);
            wt.snId = w;
            n._attnWeights = wt;
            tensors[n.id + "|weights"] = wt;
            snT[n.id + "|weights"] = w;
          } catch (_) {}
          return;
        }
        const q = inputTensor(n, "q"), k = inputTensor(n, "k"), v = inputTensor(n, "v");
        if (!q || !k || !v) throw new Error("SDPA 需要 q/k/v");
        const Q = asMat(q), K = asMat(k), V = asMat(v);
        if (Q.cols !== K.cols) throw new Error("SDPA d_k 不一致");
        if (K.rows !== V.rows) throw new Error("SDPA K/V 序列长不一致");
        if (Q.rows !== K.rows && false) { /* allow different seq for cross-attn */ }
        let scale = cfgNum(n, "scale", NaN);
        if (!Number.isFinite(scale) || String((n.cfg && n.cfg.scale) || "auto") === "auto") {
          scale = 1 / Math.sqrt(Math.max(1, Q.cols));
        }
        const KT = transposeMat(K);
        let scores = matMulCore(Q, KT);
        scores = { kind: "mat", rows: scores.rows, cols: scores.cols, data: scores.data.map((x) => x * scale) };
        let mask = inputTensor(n, "mask");
        if (!mask && n.cfg && n.cfg.causal) mask = causalMask(Q.rows);
        if (mask) {
          const M = asMat(mask);
          if (M.rows !== scores.rows || M.cols !== scores.cols) {
            throw new Error("mask 形状 " + M.rows + "x" + M.cols + " != scores");
          }
          for (let i = 0; i < scores.data.length; i++) scores.data[i] += M.data[i];
        }
        const weights = softmaxRows(scores);
        const out = matMulCore(weights, V);
        tensors[n.id] = out;
        tensors[n.id + "|weights"] = weights;
        n._attnWeights = weights;
        setResult(n, formatTensor(out) + " |W " + formatTensor(weights));
        return;
      }
      if (n.type === "mha_split") {
        /* interpret as: input [seq, d], heads h → rearrange to [seq, d] with head blocks contiguous (no-op layout doc)
           For toy graphs we expose head size via reshape helper: out = same data, meta in result. */
        const a = inputTensor(n, "a"); if (!a) throw new Error("MHA split 缺少 a");
        const A = asMat(a);
        const heads = Math.max(1, portOrCfgInt(n, "heads", "heads", cfgNum(n, "heads", 2)));
        if (A.cols % heads !== 0) throw new Error("d=" + A.cols + " 不能被 heads=" + heads + " 整除");
        const dh = A.cols / heads;
        /* layout: [seq*heads, dh] by stacking heads as extra batch rows */
        const data = new Array(A.rows * heads * dh);
        for (let h = 0; h < heads; h++) {
          for (let r = 0; r < A.rows; r++) {
            for (let c = 0; c < dh; c++) {
              data[(h * A.rows + r) * dh + c] = A.data[r * A.cols + h * dh + c];
            }
          }
        }
        setTensorOut(n, { kind: "mat", rows: A.rows * heads, cols: dh, data }); return;
      }
      if (n.type === "mha_merge") {
        const a = inputTensor(n, "a"); if (!a) throw new Error("MHA merge 缺少 a");
        const A = asMat(a);
        const heads = Math.max(1, portOrCfgInt(n, "heads", "heads", cfgNum(n, "heads", 2)));
        if (A.rows % heads !== 0) throw new Error("rows 不能被 heads 整除");
        const seq = A.rows / heads;
        const dh = A.cols;
        const d = dh * heads;
        const data = new Array(seq * d);
        for (let h = 0; h < heads; h++) {
          for (let r = 0; r < seq; r++) {
            for (let c = 0; c < dh; c++) {
              data[r * d + h * dh + c] = A.data[(h * seq + r) * dh + c];
            }
          }
        }
        setTensorOut(n, { kind: "mat", rows: seq, cols: d, data }); return;
      }

      /* flow nodes are not pure data */
      if (isFlowControlNode(n) && n.type !== "get_var") return;
      throw new Error("未实现节点类型 " + n.type);
    }

    function execTargets(fromId, fromPort) {
      return (graph.wires || []).filter((w) => w.from === fromId && (w.fromPort || "out") === fromPort);
    }

    let execSteps = 0;
    const MAX_EXEC_STEPS = 2000000;
    const loopStack = [];
    /* reset runtime state for flow nodes each graph run */
    graph.nodes.forEach((gn) => {
      if (gn && (gn.type === "do_once" || gn.type === "do_n" || gn.type === "gate" ||
          gn.type === "flip_flop" || gn.type === "multi_gate")) {
        /* keep _rt across? no — fresh each run */
        gn._rt = null;
      }
    });

    function processExecNode(n, viaPort) {
      if (!n) return;
      if (++execSteps > MAX_EXEC_STEPS) throw new Error("control-flow step limit (possible infinite loop)");
      viaPort = viaPort || null;

      /* Gate control pins as alternate entries */
      if (n.type === "gate") {
        if (!n._rt) n._rt = { open: !!(n.cfg && n.cfg.start_open) };
        if (viaPort === "open") { n._rt.open = true; setResult(n, "opened"); return; }
        if (viaPort === "close") { n._rt.open = false; setResult(n, "closed"); return; }
        if (viaPort === "toggle") { n._rt.open = !n._rt.open; setResult(n, n._rt.open ? "opened" : "closed"); return; }
        /* enter falls through */
      }
      if (n.type === "do_once" && viaPort === "reset") {
        n._rt = { done: false };
        setResult(n, "reset");
        return;
      }
      if (n.type === "do_n" && viaPort === "reset") {
        n._rt = { count: 0 };
        setResult(n, "reset");
        return;
      }
      /* honor break/continue mid-body: stop further exec in same body fanout */
      if (loopStack.length) {
        const top = loopStack[loopStack.length - 1];
        if (top.broken || top.continued) return;
      }

      if (n.type === "entry") {
        setResult(n, "entry");
        execTargets(n.id, "exec").forEach((w) => processExecNode(byId[w.to], w.toPort));
        return;
      }
      if (n.type === "branch") {
        const cond = inputSlot(n, "cond");
        if (cond == null) throw new Error("Branch 缺少 cond");
        const t = truthySlot(cond);
        setResult(n, t ? "then" : "else");
        execTargets(n.id, t ? "then" : "else").forEach((w) => processExecNode(byId[w.to], w.toPort));
        return;
      }
      if (n.type === "sequence") {
        const c = Math.max(2, Math.min(8, (n.cfg && n.cfg.count) | 0 || 2));
        setResult(n, "seq×" + c);
        for (let i = 0; i < c; i++) {
          execTargets(n.id, "then" + i).forEach((w) => processExecNode(byId[w.to], w.toPort));
        }
        return;
      }
      if (n.type === "set_var") {
        const name = ((n.cfg && n.cfg.name) || "x").trim() || "x";
        const val = inputSlot(n, "value");
        if (val == null) throw new Error("Set " + name + " 缺少 value");
        const o = cloneSlot(val, freelistR);
        vars[name] = o;
        graph.nodes.forEach((gn) => {
          if (gn.type === "get_var" && (((gn.cfg && gn.cfg.name) || "x") === name)) evaluated.delete(gn.id);
        });
        setResult(n, name + "=" + api.readCString(api.toStr(session, o, 10)));
        execTargets(n.id, "exec").forEach((w) => processExecNode(byId[w.to], w.toPort));
        return;
      }
      if (n.type === "print") {
        const val = inputSlot(n, "value");
        const t = val != null ? api.readCString(api.toStr(session, val, 10)) : "(null)";
        setResult(n, t);
        log("PRINT " + t);
        execTargets(n.id, "exec").forEach((w) => processExecNode(byId[w.to], w.toPort));
        return;
      }
      if (n.type === "while") {
        const maxIter = Math.max(1, parseInt((n.cfg && n.cfg.max_iter) || "1000000", 10) || 1000000);
        let it = 0;
        const loopCtx = { broken: false, continued: false };
        loopStack.push(loopCtx);
        for (;;) {
          evaluated.clear();
          loopCtx.continued = false;
          const cond = inputSlot(n, "cond");
          if (cond == null) throw new Error("While missing cond");
          if (!truthySlot(cond)) break;
          if (++it > maxIter) throw new Error("While max_iter=" + maxIter);
          execTargets(n.id, "body").forEach((w) => processExecNode(byId[w.to], w.toPort));
          setResult(n, "iter " + it);
          if (loopCtx.broken) break;
        }
        loopStack.pop();
        setResult(n, "done " + it);
        execTargets(n.id, "completed").forEach((w) => processExecNode(byId[w.to], w.toPort));
        return;
      }
      if (n.type === "for") {
        let firstS = inputSlot(n, "first");
        let lastS = inputSlot(n, "last");
        if (firstS == null) {
          const o = allocReal(); freelistR.push(o);
          must(api.setFromStr(session, o, String((n.cfg && n.cfg.first) != null ? n.cfg.first : "0"), 0), "for first");
          firstS = o;
        }
        if (lastS == null) {
          const o = allocReal(); freelistR.push(o);
          must(api.setFromStr(session, o, String((n.cfg && n.cfg.last) != null ? n.cfg.last : "10"), 0), "for last");
          lastS = o;
        }
        let first = 0, last = 0;
        try { first = parseInt(api.readCString(api.toStr(session, firstS, 10)), 10); if (!Number.isFinite(first)) first = 0; } catch (_) { first = 0; }
        try { last = parseInt(api.readCString(api.toStr(session, lastS, 10)), 10); if (!Number.isFinite(last)) last = 0; } catch (_) { last = 0; }
        let count = 0;
        const loopCtx = { broken: false, continued: false };
        loopStack.push(loopCtx);
        for (let i = first; i < last; i++) {
          evaluated.clear();
          loopCtx.continued = false;
          const idx = allocReal(); freelistR.push(idx);
          must(api.setFromStr(session, idx, String(i), 0), "for idx");
          realPort[n.id + "|index"] = idx;
          real[n.id] = idx;
          setResult(n, "i=" + i);
          execTargets(n.id, "body").forEach((w) => processExecNode(byId[w.to], w.toPort));
          count++;
          if (loopCtx.broken) break;
        }
        loopStack.pop();
        setResult(n, "for done " + count);
        execTargets(n.id, "completed").forEach((w) => processExecNode(byId[w.to], w.toPort));
        return;
      }
      if (n.type === "out" || n.type === "fn_return") {
        evalDataNode(n);
        execTargets(n.id, "exec").forEach((w) => processExecNode(byId[w.to], w.toPort));
        return;
      }
      if (n.type === "fn_call") {
        /* demand-eval args + body, then continue exec chain */
        evalDataNode(n);
        execTargets(n.id, "exec").forEach((w) => processExecNode(byId[w.to], w.toPort));
        return;
      }
      if (n.type === "reroute") {
        setResult(n, ">>");
        execTargets(n.id, "exec").forEach((w) => processExecNode(byId[w.to], w.toPort));
        return;
      }
      if (n.type === "break") {
        setResult(n, "break");
        if (loopStack.length) loopStack[loopStack.length - 1].broken = true;
        return;
      }
      if (n.type === "continue") {
        setResult(n, "continue");
        if (loopStack.length) loopStack[loopStack.length - 1].continued = true;
        return;
      }

      if (n.type === "do_while") {
        const maxIter = Math.max(1, parseInt((n.cfg && n.cfg.max_iter) || "1000000", 10) || 1000000);
        let it = 0;
        const loopCtx = { broken: false, continued: false };
        loopStack.push(loopCtx);
        do {
          evaluated.clear();
          loopCtx.continued = false;
          execTargets(n.id, "body").forEach((w) => processExecNode(byId[w.to], w.toPort));
          if (loopCtx.broken) break;
          it++;
          if (it > maxIter) throw new Error("DoWhile max_iter=" + maxIter);
          const cond = inputSlot(n, "cond");
          if (cond == null) throw new Error("DoWhile missing cond");
          if (!truthySlot(cond)) break;
          setResult(n, "iter " + it);
        } while (true);
        loopStack.pop();
        setResult(n, "done " + it);
        execTargets(n.id, "completed").forEach((w) => processExecNode(byId[w.to], w.toPort));
        return;
      }
      if (n.type === "select_exec") {
        const cond = inputSlot(n, "cond");
        const a = inputSlot(n, "a");
        const b = inputSlot(n, "b");
        if (cond == null) throw new Error("Select missing cond");
        const pick = truthySlot(cond) ? a : b;
        if (pick == null) throw new Error("Select missing a/b");
        const o = cloneSlot(pick, freelistR);
        real[n.id] = o;
        realPort[n.id + "|out"] = o;
        setResult(n, api.readCString(api.toStr(session, o, 10)));
        execTargets(n.id, "exec").forEach((w) => processExecNode(byId[w.to], w.toPort));
        return;
      }
      if (n.type === "comment") {
        setResult(n, String((n.cfg && n.cfg.text) || "note"));
        return;
      }
      if (n.type === "do_once") {
        if (!n._rt) n._rt = { done: false };
        /* reset is an alternate entry: wired into reset pin as exec target */
        setResult(n, n._rt.done ? "closed" : "completed");
        if (!n._rt.done) {
          n._rt.done = true;
          execTargets(n.id, "completed").forEach((w) => processExecNode(byId[w.to], w.toPort));
        } else {
          execTargets(n.id, "closed").forEach((w) => processExecNode(byId[w.to], w.toPort));
        }
        return;
      }
      if (n.type === "do_n") {
        if (!n._rt) n._rt = { count: 0 };
        let nS = inputSlot(n, "n");
        let N = parseInt((n.cfg && n.cfg.n) || "1", 10) || 1;
        if (nS != null) {
          try { N = parseInt(api.readCString(api.toStr(session, nS, 10)), 10) || N; } catch (_) {}
        }
        if (N < 0) N = 0;
        if (n._rt.count < N) {
          n._rt.count++;
          setResult(n, "exit " + n._rt.count + "/" + N);
          execTargets(n.id, "exit").forEach((w) => processExecNode(byId[w.to], w.toPort));
        } else {
          setResult(n, "completed");
          execTargets(n.id, "completed").forEach((w) => processExecNode(byId[w.to], w.toPort));
        }
        return;
      }
      if (n.type === "gate") {
        if (!n._rt) n._rt = { open: !!(n.cfg && n.cfg.start_open) };
        /* enter path only when open */
        if (n._rt.open) {
          setResult(n, "open");
          execTargets(n.id, "exit").forEach((w) => processExecNode(byId[w.to], w.toPort));
        } else {
          setResult(n, "closed");
        }
        return;
      }
      if (n.type === "flip_flop") {
        if (!n._rt) n._rt = { a: true };
        const goA = n._rt.a;
        n._rt.a = !n._rt.a;
        setResult(n, goA ? "A" : "B");
        execTargets(n.id, goA ? "A" : "B").forEach((w) => processExecNode(byId[w.to], w.toPort));
        return;
      }
      if (n.type === "multi_gate") {
        if (!n._rt) n._rt = { i: 0 };
        const c = Math.max(2, Math.min(16, (n.cfg && n.cfg.count) | 0 || 2));
        const idx = n._rt.i % c;
        n._rt.i = (n._rt.i + 1) % c;
        setResult(n, "out" + idx);
        execTargets(n.id, "out" + idx).forEach((w) => processExecNode(byId[w.to], w.toPort));
        return;
      }
      if (n.type === "switch") {
        const cases = parseSwitchCases(n);
        let selS = inputSlot(n, "sel");
        let selStr = "0";
        if (selS != null) {
          try { selStr = api.readCString(api.toStr(session, selS, 10)); } catch (_) {}
        } else if (n.cfg && n.cfg.portValues && n.cfg.portValues.sel != null) {
          selStr = String(n.cfg.portValues.sel);
        }
        let hit = -1;
        for (let i = 0; i < cases.length; i++) {
          if (String(cases[i]) === String(selStr).trim()) { hit = i; break; }
        }
        if (hit >= 0) {
          setResult(n, "case " + cases[hit]);
          execTargets(n.id, "case" + hit).forEach((w) => processExecNode(byId[w.to], w.toPort));
        } else {
          setResult(n, "default");
          execTargets(n.id, "default").forEach((w) => processExecNode(byId[w.to], w.toPort));
        }
        return;
      }
      setResult(n, "no-exec:" + n.type);
    }

    function runExecFromEntry(entryNode) {
      processExecNode(entryNode);
    }

    const entry = graph.nodes.find((n) => n.type === "entry");
    const entryHasExec = !!(entry && (graph.wires || []).some((w) =>
      w.from === entry.id && isExecPort(w.fromPort || "exec")));
    if (entry && entryHasExec) {
      runExecFromEntry(entry);
      /* Out/Print only on exec path when Entry has exec outs */
    } else {
      /* pure data mode: topo, skip pure control nodes without data */
      const dataNodes = graph.nodes.filter((n) => {
        if (isFlowControlNode(n) && n.type !== "get_var" && n.type !== "out" && n.type !== "fn_return" && n.type !== "fn_call")
          return false;
        if (n.type === "entry" || n.type === "branch" || n.type === "while" ||
            n.type === "for" || n.type === "sequence" || n.type === "set_var" || n.type === "print" ||
            n.type === "do_once" || n.type === "do_n" || n.type === "gate" || n.type === "flip_flop" ||
            n.type === "multi_gate" || n.type === "switch" || n.type === "break" || n.type === "continue" ||
            n.type === "reroute" || n.type === "do_while" || n.type === "select_exec" || n.type === "comment")
          return false;
        return true;
      });
      const dataWires = (graph.wires || []).filter((w) => {
        const a = byId[w.from], b = byId[w.to];
        if (!a || !b) return false;
        if (isExecPort(w.fromPort) || isExecPort(w.toPort)) return false;
        return dataNodes.indexOf(a) >= 0 && dataNodes.indexOf(b) >= 0;
      });
      const order = topoSort(dataNodes, dataWires);
      order.forEach((n) => evalDataNode(n));
    }

    /* data-eval return/out if control path never reached them */
    (graph.nodes || []).forEach((n) => {
      if (n.type !== "fn_return" && n.type !== "out") return;
      if (n._retSlot != null || n._retCplx != null || n._retTensor != null || n._retTensorSn != null) return;
      try { evalDataNode(n); } catch (e) { /* keep */ }
    });

    let ret = { slot: null, cplx: null, tensor: null };
    const retNode = graph.nodes.find((n) => n.type === "fn_return");
    if (retNode) {
      ret.slot = retNode._retSlot;
      ret.cplx = retNode._retCplx;
      ret.tensor = retNode._retTensorSn != null ? retNode._retTensorSn
        : (retNode._retTensor && retNode._retTensor.snId != null ? retNode._retTensor.snId : null);
      if (ret.tensor == null && snT[retNode.id] != null) ret.tensor = snT[retNode.id];
      if (ret.tensor == null && tensors[retNode.id] && tensors[retNode.id].snId != null)
        ret.tensor = tensors[retNode.id].snId;
      if (ret.slot != null) {
        const o = allocReal();
        const t = api.readCString(api.toStr(session, ret.slot, 10));
        must(api.setFromStr(session, o, t, 0), "copy ret");
        ret.slot = o;
      }
      if (ret.tensor != null && hasSnTensorApi() && api.tensorCopy) {
        const o = allocT();
        snMust(api.tensorCopy(session, o, ret.tensor), "copy ret tensor");
        ret.tensor = o;
      }
    }
    return { real, cplx, freelistR, freelistC, freelistT, ret, vars, snT, tensors };
  }

  function evalFunction(fn, argSlots) {
    const r = evalGraph(fn, argSlots);
    if (r.ret.slot == null && r.ret.cplx == null && r.ret.tensor == null)
      throw new Error("function " + fn.name + " has no return");
    /* free intermediate SN values; keep returned tensor/scalar/cplx */
    const keepR = r.ret.slot;
    const keepC = r.ret.cplx;
    const keepT = r.ret.tensor;
    (r.freelistR || []).forEach((id) => {
      if (id !== keepR && api.freeValue) api.freeValue(session, id);
    });
    (r.freelistC || []).forEach((id) => {
      if (id !== keepC && api.freeCplx) api.freeCplx(session, id);
    });
    (r.freelistT || []).forEach((id) => {
      if (id !== keepT && api.freeTensor) api.freeTensor(session, id);
    });
    return r.ret;
  }

  function runGraph() {
    if (!api || !session) throw new Error("session 未就绪");
    clearLog();
    api.clearFlags(session);
    if (editScope === "main") {
      evalGraph(project.main, null);
    } else {
      const fn = project.functions[editScope.slice(3)];
      /* dry-run function with zeros */
      const args = fn.params.map(() => {
        const s = allocReal();
        must(api.setFromStr(session, s, "0", 0), "arg0");
        return s;
      });
      log("（函数编辑模式）以 0 填充参数试运行…");
      evalGraph(fn, args);
    }
    nodes().forEach((n) => layoutPorts(n));
    ensureCanvasSize();
    drawWires();
    renderCPreview(exportC());
    status("运行完成 flags=" + api.flags(session), "ok");
  }

  /* ---------- Import / export ---------- */
  function serializeNode(n) {
    const o = {
      id: n.id, type: n.type, op: n.op, x: n.x, y: n.y,
      cfg: n.cfg, arity: n.arity, fnId: n.fnId,
      paramIndex: n.paramIndex, paramName: n.paramName,
      inputs: Object.assign({}, n.inputs),
      inputFromPorts: Object.assign({}, n.inputFromPorts || {}),
      collapsed: !!n.collapsed,
      foldUnusedPorts: !!n.foldUnusedPorts
    };
    /* UE-style attached sticky note (string on node; free comment node is separate) */
    if (n.note != null && String(n.note).length) o.note = String(n.note);
    return o;
  }

  
function exportProject() {
    const fobj = {};
    Object.keys(project.functions).forEach((k) => {
      const fn = project.functions[k];
      fobj[k] = {
        id: fn.id, name: fn.name, params: fn.params.slice(),
        nodes: fn.nodes.map(serializeNode),
        wires: fn.wires.map((w) => ({ from: w.from, fromPort: w.fromPort || "out", to: w.to, toPort: w.toPort }))
      };
    });
    return {
      version: 3,
      format: fmtBits(),
      main: {
        nodes: project.main.nodes.map(serializeNode),
        wires: project.main.wires.map((w) => ({ from: w.from, fromPort: w.fromPort || "out", to: w.to, toPort: w.toPort }))
      },
      functions: fobj,
      nodeSeq, fnSeq
    };
  }

  function importProject(proj) {
    if (!proj || !proj.main) throw new Error("无效工程");
    editScope = "main";
    project.main = { nodes: [], wires: [] };
    project.functions = {};
    if (proj.format) {
      if ($id("eBits")) $id("eBits").value = proj.format.e;
      if ($id("mBits")) $id("mBits").value = proj.format.m;
      if ($id("nanEn")) $id("nanEn").checked = !!proj.format.nan;
    }
    function loadGraph(dst, src) {
      dst.nodes = (src.nodes || []).map((n) => ({
        id: n.id, type: n.type, op: n.op || null,
        x: n.x || 0, y: n.y || 0,
        cfg: n.cfg || defaultCfg(n.type),
        arity: n.arity || null, fnId: n.fnId || null,
        paramIndex: n.paramIndex != null ? n.paramIndex : null,
        paramName: n.paramName || null,
        inputs: n.inputs || {},
        inputFromPorts: n.inputFromPorts || {},
        collapsed: !!n.collapsed,
        foldUnusedPorts: !!n.foldUnusedPorts,
        note: (n.note != null ? String(n.note) : (n.cfg && n.cfg.note != null ? String(n.cfg.note) : "")),
        el: null, slot: -1, result: ""
      }));
      dst.wires = (src.wires || []).map((w) => ({
        from: w.from, fromPort: w.fromPort || "out", to: w.to, toPort: w.toPort
      }));
      /* ensure input keys + fromPort defaults */
      dst.nodes.forEach((n) => {
        if (!n.cfg) n.cfg = defaultCfg(n.type);
        if (!n.cfg.portValues) n.cfg.portValues = {};
        if (!n.inputFromPorts) n.inputFromPorts = {};
        if (!n.inputs) n.inputs = {};
        /* legacy out/print only had data "in" — keep pure-data mode working */
        const ports = portsOf(n);
        ports.in.forEach((p) => {
          if (n.inputs[p] === undefined) n.inputs[p] = null;
          if (n.inputs[p] && !n.inputFromPorts[p]) n.inputFromPorts[p] = "out";
        });
        /* drop unknown input keys that are no longer ports (except keep data) */
        Object.keys(n.inputs).forEach((p) => {
          if (ports.in.indexOf(p) < 0 && n.inputs[p] == null) delete n.inputs[p];
        });
      });
      /* rebind wires → inputs after port normalize */
      dst.nodes.forEach((n) => {
        portsOf(n).in.forEach((p) => { n.inputs[p] = null; n.inputFromPorts[p] = null; });
      });
      (dst.wires || []).forEach((w) => {
        const tn = dst.nodes.find((x) => x.id === w.to);
        if (!tn) return;
        const ports = portsOf(tn).in;
        let tp = w.toPort;
        if (ports.indexOf(tp) < 0) {
          /* map legacy out "in" still valid; exec wires ignored if port missing */
          if (tp === "a" && ports.indexOf("in") >= 0) tp = "in";
          else if (tp === "value" && ports.indexOf("in") >= 0) tp = "in";
          else return;
          w.toPort = tp;
        }
        tn.inputs[tp] = w.from;
        tn.inputFromPorts[tp] = w.fromPort || "out";
      });
    }
    loadGraph(project.main, proj.main);
    Object.keys(proj.functions || {}).forEach((k) => {
      const fn = proj.functions[k];
      project.functions[k] = { id: fn.id || k, name: fn.name, params: fn.params || [], nodes: [], wires: [] };
      loadGraph(project.functions[k], fn);
    });
    nodeSeq = proj.nodeSeq || (project.main.nodes.length + 10);
    fnSeq = proj.fnSeq || (Object.keys(project.functions).length + 1);
    ensureSession();
    remountGraph();
    if (!historyLock) resetHistory();
    status("工程已导入", "ok");
  }
  
  /* ---------- Undo / Redo (project snapshots) ---------- */
  const HISTORY_MAX = 80;
  let historyPast = [];
  let historyFuture = [];
  let historyLock = false;
  let historyQuiet = false; /* suppress nested pushHistory (e.g. auto-cast addNode during wire) */

  function cloneProjectState() {
    return JSON.parse(JSON.stringify(exportProject()));
  }

  function updateUndoUi() {
    const bu = document.getElementById("btnUndo");
    const br = document.getElementById("btnRedo");
    if (bu) {
      bu.disabled = !historyPast.length;
      bu.title = historyPast.length ? ("撤销: " + (historyPast[historyPast.length - 1].label || "编辑") + " (Ctrl+Z)") : "撤销 (Ctrl+Z)";
    }
    if (br) {
      br.disabled = !historyFuture.length;
      br.title = historyFuture.length ? "重做 (Ctrl+Y)" : "重做 (Ctrl+Y / Ctrl+Shift+Z)";
    }
  }

  function pushHistory(label) {
    if (historyLock || historyQuiet) return;
    try {
      historyPast.push({ label: label || "edit", snap: cloneProjectState(), scope: editScope });
      if (historyPast.length > HISTORY_MAX) historyPast.shift();
      historyFuture = [];
      updateUndoUi();
    } catch (e) {
      console.warn("pushHistory failed", e);
    }
  }

  function restoreProjectState(snap, scope) {
    historyLock = true;
    try {
      importProject(snap);
      if (scope && scope !== editScope) {
        if (scope === "main" || (String(scope).indexOf("fn:") === 0 && project.functions[String(scope).slice(3)])) {
          editScope = scope;
          remountGraph();
        }
      }
    } finally {
      historyLock = false;
      updateUndoUi();
    }
  }

  function undo() {
    if (!historyPast.length) { status("没有可撤销的操作"); return false; }
    const cur = { label: "current", snap: cloneProjectState(), scope: editScope };
    const prev = historyPast.pop();
    historyFuture.push(cur);
    restoreProjectState(prev.snap, prev.scope);
    status("已撤销" + (prev.label ? (" · " + prev.label) : ""), "ok");
    return true;
  }

  function redo() {
    if (!historyFuture.length) { status("没有可重做的操作"); return false; }
    const cur = { label: "current", snap: cloneProjectState(), scope: editScope };
    const next = historyFuture.pop();
    historyPast.push(cur);
    restoreProjectState(next.snap, next.scope);
    status("已重做", "ok");
    return true;
  }

  function resetHistory() {
    historyPast = [];
    historyFuture = [];
    updateUndoUi();
  }


  /** Format free-text into safe C block comment lines (no nesting break). */
  function cCommentBlock(text, indent) {
    const ind = indent != null ? indent : "  ";
    const raw = String(text == null ? "" : text).replace(/\r\n/g, "\n").replace(/\r/g, "\n");
    const tx = raw.trim();
    if (!tx) return [];
    const safe = tx.replace(/\*\//g, "* /").split("\n");
    if (safe.length === 1) return [ind + "/* " + safe[0] + " */"];
    const out = [ind + "/*"];
    safe.forEach((ln) => out.push(ind + " * " + ln));
    out.push(ind + " */");
    return out;
  }

  function nodeAttachedNote(n) {
    if (!n) return "";
    if (n.note != null && String(n.note).length) return String(n.note);
    if (n.cfg && n.cfg.note != null && String(n.cfg.note).length) return String(n.cfg.note);
    return "";
  }

function emitGraphC(lines, graph, fname, isFn) {
    const esc = (s) => String(s).replace(/\\/g, "\\\\").replace(/"/g, '\\"');
    const byId = {};
    graph.nodes.forEach((n) => { byId[n.id] = n; });
    /* ---------- Reachable set ----------
       Pure-data graphs (Qwen3 etc.) often have Entry without exec outs.
       Always reverse-mark from out / fn_return / print; if Entry has exec,
       also walk exec and merge. Never leave only Entry. */
    const reachable = new Set();
    function markDataDeps(nid, seen) {
      if (!nid || seen.has(nid)) return;
      seen.add(nid);
      reachable.add(nid);
      const n = byId[nid];
      if (!n) return;
      Object.keys(n.inputs || {}).forEach((p) => {
        if (n.inputs[p]) markDataDeps(n.inputs[p], seen);
      });
      /* also walk wires into this node (inputs map may lag) */
      (graph.wires || []).forEach((w) => {
        if (w.to === nid && !isExecPort(w.toPort) && !isExecPort(w.fromPort)) {
          markDataDeps(w.from, seen);
        }
      });
    }
    /* 1) reverse from data sinks — always */
    graph.nodes.forEach((n) => {
      if (n.type === "out" || n.type === "fn_return" || n.type === "print") {
        markDataDeps(n.id, new Set());
      }
    });
    /* 2) exec walk from Entry when present */
    const entry = graph.nodes.find((n) => n.type === "entry");
    if (entry) {
      const q = [entry.id];
      const seenE = new Set(q);
      reachable.add(entry.id);
      while (q.length) {
        const id = q.shift();
        (graph.wires || []).forEach((w) => {
          if (w.from !== id) return;
          const fp = w.fromPort || "exec";
          if (isExecPort(fp) || fp === "A" || fp === "B" ||
              /^out\d+$/.test(fp) || /^case\d+$/.test(fp) ||
              fp === "default" || fp === "then" || fp === "else" ||
              fp === "body" || fp === "completed" || fp === "closed" ||
              fp === "exit" || /^then\d+$/.test(fp) || fp === "exec") {
            if (!seenE.has(w.to)) { seenE.add(w.to); q.push(w.to); reachable.add(w.to); }
            markDataDeps(w.to, new Set());
          }
        });
      }
    }
    /* 3) still empty → emit all non-comment nodes */
    if (!reachable.size || (reachable.size === 1 && entry && reachable.has(entry.id))) {
      graph.nodes.forEach((n) => {
        if (n.type !== "comment") reachable.add(n.id);
      });
      graph.nodes.forEach((n) => {
        if (n.type === "out" || n.type === "fn_return" || n.type === "print") markDataDeps(n.id, new Set());
      });
    }
    const nodes = graph.nodes.filter((n) => reachable.has(n.id));
    const wires = (graph.wires || []).filter((w) => reachable.has(w.from) && reachable.has(w.to));
    let order;
    try { order = topoSort(nodes, wires); } catch (e) { order = nodes.slice(); }
    const ARITH_UN = new Set(["abs", "neg", "not"]);
    const ARITH_BIN = new Set(["add", "sub", "mul", "div", "rem"]);
    const ARITH_BIT = new Set(["and", "or", "xor"]);
    const MATH_BIN = new Set(["pow", "hypot", "atan2", "fmin", "fmax", "fdim", "ellipf", "ellipeinc", "igamma", "igammac", "jacobi_sn", "jacobi_cn", "jacobi_dn", "copysign", "nextafter"]);
    const MATH_TER = new Set(["ellipiinc", "ibeta", "ibetac"]);
    function nodeIsTensor(n) {
      if (!n) return false;
      if (typeof isTensorNode === "function") return isTensorNode(n);
      return /^(arr_|vec_|mat_)/.test(n.type || "") ||
        ["ten_unary","ten_binop","softmax_row","layer_norm","rms_norm","reshape","flatten",
         "gather","embedding","sin_pe","attention_sdp","mha_split","mha_merge","gelu","relu",
         "silu","rope","swiglu","residual_add"].indexOf(n.type) >= 0;
    }
    function srcIsTensor(srcId) {
      const sn = byId[srcId];
      if (!sn) return false;
      if (nodeIsTensor(sn)) return true;
      if (sn.type === "fn_param") {
        /* param domain: if any consumer in graph treats it as tensor, or graph mostly tensor */
        return graphIsTensor;
      }
      if (sn.type === "fn_call") {
        const fn = project.functions[sn.fnId];
        return fn && fn._exportTensor;
      }
      return false;
    }
    /* detect tensor graph: any tensor node, or fn_return from tensor */
    let graphIsTensor = false;
    graph.nodes.forEach((n) => {
      if (nodeIsTensor(n)) graphIsTensor = true;
    });
    const retN = graph.nodes.find((n) => n.type === "fn_return");
    if (retN && retN.inputs && retN.inputs.in && srcIsTensor(retN.inputs.in)) graphIsTensor = true;
    if (isFn) graph._exportTensor = graphIsTensor;
    function tidOf(id) {
      return "t_" + String(id).replace(/[^A-Za-z0-9_]/g, "_");
    }
    function vidOf(id) {
      return "v_" + String(id).replace(/[^A-Za-z0-9_]/g, "_");
    }
    /* ---- signature ---- */
    if (isFn) {
      const params = graph.params || [];
      const argParts = params.map((p, i) => {
        if (graphIsTensor) return "const sn_tensor *arg" + i;
        return "const sn_value *arg" + i;
      });
      const outTy = graphIsTensor ? "sn_tensor *out" : "sn_value *out";
      lines.push("/* function " + (graph.name || fname) + (graphIsTensor ? " [tensor]" : "") + " */");
      lines.push("static sn_status " + fname + "(sn_ctx *ctx, sn_api *api, " + outTy +
        (argParts.length ? ", " + argParts.join(", ") : "") + ") {");
    } else {
      lines.push("static void " + fname + "(sn_ctx *ctx, sn_api *api) {");
    }
    /* Free-standing comment nodes already dumped at file head in exportC(). */
    const emitted = new Set();
    let didReturn = false;
    function emitVal(n) {
      if (!n || emitted.has(n.id)) return;
      emitted.add(n.id);
      const v = vidOf(n.id);
      const t = tidOf(n.id);
      /* deps first */
      Object.keys(n.inputs || {}).forEach((p) => {
        if (n.inputs[p] && byId[n.inputs[p]]) emitVal(byId[n.inputs[p]]);
      });
      (graph.wires || []).forEach((w) => {
        if (w.to === n.id && byId[w.from] && !isExecPort(w.toPort)) emitVal(byId[w.from]);
      });
      /* inline literals on unconnected data pins → sn_value */
      const ports = (typeof portsOf === "function") ? portsOf(n) : { in: [], out: [] };
      (ports.in || []).forEach((p) => {
        if (isExecPort(p) || (typeof isExecPortOnNode === "function" && isExecPortOnNode(n, p))) return;
        if (n.inputs && n.inputs[p]) return;
        const lit = n.cfg && n.cfg.portValues && n.cfg.portValues[p];
        if (lit != null && String(lit).trim() !== "") {
          const lv = v + "_lit_" + p;
          if (!emitted.has(lv)) {
            lines.push("  sn_value " + lv + "; api->value.init(&" + lv + ");");
            lines.push("  sn_check(api->flt.from_str(ctx, &" + lv + ", \"" + esc(String(lit).trim()) +
              "\", " + (fmtBits().e|0) + ", " + (fmtBits().m|0) + ", " + (fmtBits().nan ? 1 : 0) +
              ", NULL), \"lit\");");
            emitted.add(lv);
          }
        }
      });
      function inRef(port) {
        if (n.inputs && n.inputs[port]) {
          const sid = n.inputs[port];
          if (srcIsTensor(sid)) return "&" + tidOf(sid);
          return "&" + vidOf(sid);
        }
        const lit = n.cfg && n.cfg.portValues && n.cfg.portValues[port];
        if (lit != null && String(lit).trim() !== "") return "&" + v + "_lit_" + port;
        return "NULL";
      }
      function tRef(port) {
        if (n.inputs && n.inputs[port]) {
          const sid = n.inputs[port];
          if (byId[sid] && byId[sid].type === "fn_param") return "arg" + (byId[sid].paramIndex|0);
          return "&" + tidOf(sid);
        }
        return "NULL";
      }
      function vRef(port) {
        if (n.inputs && n.inputs[port]) {
          const sid = n.inputs[port];
          if (byId[sid] && byId[sid].type === "fn_param") return "arg" + (byId[sid].paramIndex|0);
          return "&" + vidOf(sid);
        }
        const lit = n.cfg && n.cfg.portValues && n.cfg.portValues[port];
        if (lit != null && String(lit).trim() !== "") return "&" + v + "_lit_" + port;
        return "NULL";
      }
      if (n.type === "fn_param") {
        if (graphIsTensor) {
          lines.push("  const sn_tensor *" + t + " = arg" + (n.paramIndex|0) + ";");
          lines.push("  /* alias as pointer " + t + " */");
        } else {
          lines.push("  const sn_value *" + v + " = arg" + (n.paramIndex|0) + ";");
        }
        return;
      }
      if (n.type === "comment") {
        /* already emitted at function head */
        return;
      }
      /* UE-style attached note on this node -> C comment before its code
         (also covers pure control-flow nodes that emit no code). */
      {
        const noteTx = nodeAttachedNote(n);
        if (noteTx) cCommentBlock(noteTx, "  ").forEach((ln) => lines.push(ln));
      }
      if (n.type === "entry" || n.type === "reroute" || n.type === "break" || n.type === "continue" ||
          n.type === "sequence" || n.type === "do_once" || n.type === "do_n" || n.type === "gate" ||
          n.type === "flip_flop" || n.type === "multi_gate" || n.type === "branch" || n.type === "while" ||
          n.type === "for" || n.type === "switch" || n.type === "do_while" || n.type === "select_exec") {
        return;
      }
      /* ---- tensor nodes ---- */
      if (nodeIsTensor(n)) {
        const fb = (typeof fmtBits === "function") ? fmtBits() : { e: 11, m: 52, nan: 1 };
        lines.push("  sn_tensor " + t + "; api->tensor.init(&" + t + ");");
        if (n.type === "mat_lit" || n.type === "arr_lit" || n.type === "vec_lit") {
          const lit = esc(String((n.cfg && n.cfg.value) || "0"));
          lines.push("  sn_check(api->tensor.from_str(ctx, &" + t + ", \"" + lit + "\", " +
            (fb.e|0) + ", " + (fb.m|0) + ", " + (fb.nan ? 1 : 0) + ", NULL), \"mat_lit\");");
        } else if (n.type === "mat_mul" || (n.type === "ten_binop" && n.op === "matmul")) {
          lines.push("  sn_check(api->tensor.matmul(ctx, &" + t + ", " + tRef("a") + ", " + tRef("b") + ", NULL), \"mat_mul\");");
        } else if (n.type === "mat_add" || (n.type === "ten_binop" && (n.op || "add") === "add")) {
          lines.push("  sn_check(api->tensor.add(ctx, &" + t + ", " + tRef("a") + ", " + tRef("b") + ", NULL), \"mat_add\");");
        } else if (n.type === "mat_sub" || (n.type === "ten_binop" && n.op === "sub")) {
          lines.push("  sn_check(api->tensor.sub(ctx, &" + t + ", " + tRef("a") + ", " + tRef("b") + ", NULL), \"mat_sub\");");
        } else if (n.type === "mat_hadamard" || (n.type === "ten_binop" && n.op === "mul")) {
          lines.push("  sn_check(api->tensor.hadamard(ctx, &" + t + ", " + tRef("a") + ", " + tRef("b") + ", NULL), \"hadamard\");");
        } else if (n.type === "residual_add") {
          lines.push("  sn_check(api->tensor.add(ctx, &" + t + ", " + tRef("a") + ", " + tRef("b") + ", NULL), \"residual\");");
        } else if (n.type === "swiglu") {
          lines.push("  { sn_tensor _sg; api->tensor.init(&_sg);");
          lines.push("    sn_check(api->tensor.unary(ctx, &_sg, " + tRef("gate") + ", 5, NULL), \"silu\");");
          lines.push("    sn_check(api->tensor.hadamard(ctx, &" + t + ", &_sg, " + tRef("up") + ", NULL), \"swiglu\");");
          lines.push("    api->tensor.clear(ctx, &_sg); }");
        } else if (n.type === "rms_norm") {
          const gref = (n.inputs && n.inputs.gamma) ? tRef("gamma") : "NULL";
          const eps = (n.cfg && n.cfg.eps != null) ? String(n.cfg.eps) : "1e-6";
          lines.push("  sn_check(api->tensor.rms_norm(ctx, &" + t + ", " + tRef("a") + ", " + gref + ", " + eps + ", NULL), \"rms_norm\");");
        } else if (n.type === "layer_norm") {
          const gref = (n.inputs && n.inputs.gamma) ? tRef("gamma") : "NULL";
          const bref = (n.inputs && n.inputs.beta) ? tRef("beta") : "NULL";
          const eps = (n.cfg && n.cfg.eps != null) ? String(n.cfg.eps) : "1e-5";
          lines.push("  sn_check(api->tensor.layer_norm(ctx, &" + t + ", " + tRef("a") + ", " + gref + ", " + bref + ", " + eps + ", NULL), \"layer_norm\");");
        } else if (n.type === "rope") {
          const base = (n.cfg && n.cfg.base != null) ? String(n.cfg.base) : "10000.0";
          lines.push("  sn_check(api->tensor.rope(ctx, &" + t + ", " + tRef("a") + ", " + base + ", NULL), \"rope\");");
        } else if (n.type === "silu") {
          lines.push("  sn_check(api->tensor.unary(ctx, &" + t + ", " + tRef("a") + ", 5, NULL), \"silu\");");
        } else if (n.type === "gelu") {
          lines.push("  sn_check(api->tensor.unary(ctx, &" + t + ", " + tRef("a") + ", 4, NULL), \"gelu\");");
        } else if (n.type === "relu") {
          lines.push("  sn_check(api->tensor.unary(ctx, &" + t + ", " + tRef("a") + ", 3, NULL), \"relu\");");
        } else if (n.type === "softmax_row") {
          lines.push("  sn_check(api->tensor.softmax_row(ctx, &" + t + ", " + tRef("a") + ", NULL), \"softmax\");");
        } else if (n.type === "attention_sdp") {
          const causal = (n.cfg && n.cfg.causal) ? "1" : "0";
          lines.push("  sn_check(api->tensor.attention_sdp(ctx, &" + t + ", NULL, " +
            tRef("q") + ", " + tRef("k") + ", " + tRef("v") + ", " + causal + ", 0.0, NULL), \"sdpa\");");
        } else if (n.type === "sin_pe") {
          const seq = (n.cfg && n.cfg.seq) || "4";
          const dim = (n.cfg && n.cfg.dim) || "8";
          const base = (n.cfg && n.cfg.base) || "10000";
          lines.push("  sn_check(api->tensor.sin_pe(ctx, &" + t + ", " + seq + ", " + dim + ", " + base + ", NULL), \"sin_pe\");");
        } else if (n.type === "reshape") {
          const rows = (n.cfg && n.cfg.rows) || "1";
          const cols = (n.cfg && n.cfg.cols) || "1";
          lines.push("  sn_check(api->tensor.reshape(ctx, &" + t + ", " + tRef("a") + ", " + rows + ", " + cols + "), \"reshape\");");
        } else if (n.type === "mat_transpose") {
          lines.push("  sn_check(api->tensor.transpose(ctx, &" + t + ", " + tRef("a") + "), \"transpose\");");
        } else if (n.type === "ten_unary") {
          const opMap = { abs: 0, neg: 1, exp: 2, relu: 3, gelu: 4, silu: 5, tanh: 6, sigmoid: 7 };
          const op = opMap[n.op || "abs"] != null ? opMap[n.op || "abs"] : 0;
          lines.push("  sn_check(api->tensor.unary(ctx, &" + t + ", " + tRef("a") + ", " + op + ", NULL), \"ten_unary\");");
        } else {
          lines.push("  sn_check(api->tensor.copy(ctx, &" + t + ", " + tRef("a") + "), \"tensor_" + n.type + "\");");
        }
        return;
      }
      if (n.type === "fn_return") {
        const src = n.inputs && n.inputs.in;
        if (src) {
          if (graphIsTensor || srcIsTensor(src)) {
            const sn = byId[src];
            const srcExpr = (sn && sn.type === "fn_param")
              ? ("arg" + (sn.paramIndex|0))
              : ("&" + tidOf(src));
            lines.push("  sn_check(api->tensor.copy(ctx, out, " + srcExpr + "), \"return\");");
          } else {
            const sn = byId[src];
            const srcExpr = (sn && sn.type === "fn_param")
              ? ("arg" + (sn.paramIndex|0))
              : ("&" + vidOf(src));
            lines.push("  sn_check(api->value.copy(ctx, out, " + srcExpr + "), \"return\");");
          }
          lines.push("  return SN_OK;");
          didReturn = true;
        }
        return;
      }
      if (n.type === "fn_call") {
        const fn = project.functions[n.fnId];
        const nm = ((fn && fn.name) || n.fnId || "fn").replace(/[^A-Za-z0-9_]/g, "_");
        const params = (fn && fn.params) || [];
        const isTen = !!(fn && fn._exportTensor) || graphIsTensor;
        const args = params.map((_, i) => {
          const port = "p" + i;
          if (n.inputs && n.inputs[port]) {
            const sid = n.inputs[port];
            const sn = byId[sid];
            if (sn && sn.type === "fn_param") return "arg" + (sn.paramIndex|0);
            if (srcIsTensor(sid) || isTen) return "&" + tidOf(sid);
            return "&" + vidOf(sid);
          }
          return "NULL";
        }).join(", ");
        if (isTen) {
          lines.push("  sn_tensor " + t + "; api->tensor.init(&" + t + ");");
          lines.push("  sn_check(snlab_" + nm + "(ctx, api, &" + t + (args ? ", " + args : "") + "), \"call_" + nm + "\");");
        } else {
          lines.push("  sn_value " + v + "; api->value.init(&" + v + ");");
          lines.push("  sn_check(snlab_" + nm + "(ctx, api, &" + v + (args ? ", " + args : "") + "), \"call_" + nm + "\");");
        }
        return;
      }
      if (n.type === "print" || n.type === "out") {
        const srcPort = n.type === "print" ? "value" : "in";
        const sid = n.inputs && n.inputs[srcPort];
        if (sid && (srcIsTensor(sid) || (byId[sid] && nodeIsTensor(byId[sid])) || (byId[sid] && byId[sid].type === "fn_call" && project.functions[byId[sid].fnId] && project.functions[byId[sid].fnId]._exportTensor))) {
          const sn = byId[sid];
          const expr = (sn && sn.type === "fn_param") ? ("arg" + (sn.paramIndex|0)) : ("&" + tidOf(sid));
          lines.push("  { char *s = NULL; sn_check(api->tensor.to_str(ctx, &s, " + expr + "), \"out\");");
          lines.push("    printf(\"%s\\n\", s ? s : \"?\"); if (s) api->tensor.str_free(ctx, s); }");
        } else {
          const src = sid ? ("&" + vidOf(sid)) :
            ((n.cfg && n.cfg.portValues && n.cfg.portValues[srcPort]) ? ("&" + v + "_lit_" + srcPort) : null);
          if (src) {
            lines.push("  { char *s = NULL; sn_check(api->integer.to_str(ctx, &s, " + src + ", 10), \"out\");");
            lines.push("    printf(\"%s\\n\", s ? s : \"?\"); if (s) api->integer.str_free(ctx, s); }");
          }
        }
        return;
      }
      if (n.type === "const_f") {
        const fb = fmtBits();
        lines.push("  sn_value " + v + "; api->value.init(&" + v + ");");
        lines.push("  sn_check(api->flt.from_str(ctx, &" + v + ", \"" + esc(String((n.cfg && n.cfg.value) || "0")) +
          "\", " + (fb.e|0) + ", " + (fb.m|0) + ", " + (fb.nan ? 1 : 0) + ", NULL), \"const_f\");");
        return;
      }
      if (n.type === "const_i") {
        const w = (n.cfg && n.cfg.width) || 32;
        const sgn = (n.cfg && n.cfg.signed) ? 1 : 0;
        lines.push("  sn_value " + v + "; api->value.init(&" + v + ");");
        lines.push("  sn_check(api->integer.from_str(ctx, &" + v + ", \"" + esc(String((n.cfg && n.cfg.value) || "0")) +
          "\", 10, " + w + ", " + sgn + "), \"const_i\");");
        return;
      }
      if (n.type === "const_bi") {
        lines.push("  sn_value " + v + "; api->value.init(&" + v + ");");
        lines.push("  sn_check(api->integer.from_str_bigint(ctx, &" + v + ", \"" + esc(String((n.cfg && n.cfg.value) || "0")) +
          "\", 10), \"const_bi\");");
        return;
      }
      if (n.type === "const_bool") {
        lines.push("  sn_value " + v + "; api->value.init(&" + v + ");");
        lines.push("  sn_check(api->integer.i64(ctx, &" + v + ", " + (n.op === "false" ? "0" : "1") + "), \"bool\");");
        return;
      }
      if (n.type === "un" || n.type === "unary") {
        const op = n.op || "neg";
        lines.push("  sn_value " + v + "; api->value.init(&" + v + ");");
        if (ARITH_UN.has(op)) {
          lines.push("  sn_check(api->arith." + op + "(ctx, &" + v + ", " + vRef("a") + ", NULL), \"un\");");
        } else {
          lines.push("  sn_check(api->math." + op + "(ctx, &" + v + ", " + vRef("a") + ", NULL), \"un\");");
        }
        return;
      }
      if (n.type === "bin" || n.type === "binary") {
        const op = n.op || "add";
        lines.push("  sn_value " + v + "; api->value.init(&" + v + ");");
        if (ARITH_BIN.has(op) || ARITH_BIT.has(op)) {
          lines.push("  sn_check(api->arith." + op + "(ctx, &" + v + ", " + vRef("a") + ", " + vRef("b") + ", NULL), \"bin\");");
        } else if (MATH_BIN.has(op)) {
          lines.push("  sn_check(api->math." + op + "(ctx, &" + v + ", " + vRef("a") + ", " + vRef("b") + ", NULL), \"bin\");");
        } else {
          lines.push("  sn_check(api->arith.add(ctx, &" + v + ", " + vRef("a") + ", " + vRef("b") + ", NULL), \"bin\");");
        }
        return;
      }
      if (n.type === "cmp") {
        const op = n.op || "eq";
        lines.push("  sn_value " + v + "; api->value.init(&" + v + ");");
        lines.push("  { int rel = 0; sn_check(api->cmp.cmp(ctx, &rel, " + vRef("a") + ", " + vRef("b") + ", NULL), \"cmp\");");
        const cond =
          op === "eq" ? "rel == 0" :
          op === "ne" ? "rel != 0" :
          op === "lt" ? "rel < 0" :
          op === "le" ? "rel <= 0" :
          op === "gt" ? "rel > 0" :
          "rel >= 0";
        lines.push("    sn_check(api->integer.i64(ctx, &" + v + ", (" + cond + ") ? 1 : 0), \"cmp_bool\"); }");
        return;
      }
      if (n.type === "get_var") {
        const name = ((n.cfg && n.cfg.name) || "x").replace(/[^A-Za-z0-9_]/g, "_");
        lines.push("  sn_value " + v + " = var_" + name + "; /* Get */");
        return;
      }
      if (n.type === "set_var") {
        const name = ((n.cfg && n.cfg.name) || "x").replace(/[^A-Za-z0-9_]/g, "_");
        lines.push("  sn_value var_" + name + "; api->value.init(&var_" + name + ");");
        lines.push("  sn_check(api->value.copy(ctx, &var_" + name + ", " + vRef("value") + "), \"set\");");
        return;
      }
      if (n.type === "cast") {
        lines.push("  sn_value " + v + "; api->value.init(&" + v + ");");
        lines.push("  sn_check(api->value.copy(ctx, &" + v + ", " + vRef("a") + "), \"cast\");");
        return;
      }
      if (n.type === "copy" || n.type === "assign") {
        lines.push("  sn_value " + v + "; api->value.init(&" + v + ");");
        lines.push("  sn_check(api->value.copy(ctx, &" + v + ", " + vRef("a") + "), \"copy\");");
        return;
      }
      /* fallback: leave a comment so export is never silently empty */
      lines.push("  /* TODO export node type=" + n.type + " op=" + (n.op || "") + " id=" + n.id + " */");
    }
    order.forEach((n) => emitVal(n));
    if (isFn && !didReturn) {
      lines.push("  (void)out;");
      lines.push("  return SN_OK;");
    }
    lines.push("}");
  }

  function exportC() {
    const lines = [];
    lines.push("/* Generated by SuperNumber Lab - uses sn_api (include sn.h) */");
    /* Free-standing comment nodes (main + all functions): dump at file head. */
    const allFree = [];
    const pushFree = (nodes) => {
      (nodes || []).forEach((n) => {
        if (!n || n.type !== "comment") return;
        const tx = String((n.cfg && n.cfg.text) || "").trim();
        if (tx) allFree.push(tx);
      });
    };
    pushFree(project.main && project.main.nodes);
    Object.values(project.functions || {}).forEach((fn) => pushFree(fn.nodes));
    allFree.forEach((tx) => {
      cCommentBlock(tx, "").forEach((ln) => lines.push(ln));
    });
    if (allFree.length) lines.push("");
    lines.push("#include \"sn.h\"");
    lines.push("#include <stdio.h>");
    lines.push("");
    lines.push("static void sn_check(sn_status st, const char *what) {");
    lines.push("  if (st != SN_OK) { fprintf(stderr, \"%s failed: %d\\n\", what, (int)st); }");
    lines.push("}");
    lines.push("");
    /* pre-mark tensor functions so fn_call signatures match */
    Object.values(project.functions).forEach((fn) => {
      let ten = false;
      (fn.nodes || []).forEach((n) => {
        if (typeof isTensorNode === "function" ? isTensorNode(n) :
            /^(arr_|vec_|mat_)/.test(n.type || "") ||
            ["ten_unary","ten_binop","softmax_row","layer_norm","rms_norm","reshape","flatten",
             "gather","embedding","sin_pe","attention_sdp","gelu","relu","silu","rope","swiglu",
             "residual_add"].indexOf(n.type) >= 0) ten = true;
      });
      /* return from tensor */
      const ret = (fn.nodes || []).find((n) => n.type === "fn_return");
      if (ret && ret.inputs && ret.inputs.in) {
        const src = (fn.nodes || []).find((x) => x.id === ret.inputs.in);
        if (src && (typeof isTensorNode === "function" ? isTensorNode(src) : false)) ten = true;
        if (src && /^(arr_|vec_|mat_)/.test(src.type || "")) ten = true;
        if (src && src.type === "fn_call") ten = true; /* nested often tensor in this lab */
        if (src && ["residual_add","rms_norm","mat_mul","swiglu","attention_sdp","rope","silu"].indexOf(src.type) >= 0) ten = true;
      }
      fn._exportTensor = ten;
    });
    Object.values(project.functions).forEach((fn) => {
      const safe = String(fn.name || fn.id || "fn").replace(/[^A-Za-z0-9_]/g, "_");
      emitGraphC(lines, fn, "snlab_" + safe, true);
      lines.push("");
    });
    emitGraphC(lines, project.main, "snlab_main_run", false);
    lines.push("");
    lines.push("int main(void) {");
    lines.push("  sn_ctx ctx; sn_api api; sn_api_bind(&api); api.ctx.init(&ctx);");
    lines.push("  snlab_main_run(&ctx, &api);");
    lines.push("  api.ctx.fini(&ctx);");
    lines.push("  return 0;");
    lines.push("}");
    return lines.join("\n");
  }
  

  function seedGraphDefaults(g) {
    if (!g) return;
    if (!g.nodes) g.nodes = [];
    if (!g.wires) g.wires = [];
    if (g.nodes.length) return;
    const mk = (type, x, y) => {
      const id = "n" + (nodeSeq++);
      const n = {
        id, type, op: type === "entry" ? "entry" : null,
        x, y, cfg: Object.assign(defaultCfg(type), {}),
        arity: null, fnId: null, paramIndex: null, paramName: null,
        inputs: {}, inputFromPorts: {}, collapsed: false, foldUnusedPorts: false,
        el: null, slot: -1, result: ""
      };
      if (!n.cfg.portValues) n.cfg.portValues = {};
      portsOf(n).in.forEach((p) => { n.inputs[p] = null; });
      g.nodes.push(n);
      return n;
    };
    mk("entry", 60, 80);
    mk("out", 360, 80);
  }

  function isProjectEmpty() {
    const mainN = (project.main && project.main.nodes) ? project.main.nodes.length : 0;
    const fnN = Object.keys(project.functions || {}).length;
    if (fnN > 0) return false;
    if (mainN === 0) return true;
    return false;
  }

function clearCanvas() {
    nodes().forEach((n) => { if (n.el) n.el.remove(); });
    setNodes([]);
    setWires([]);
    selectedId = null;
    seedGraphDefaults(currentGraph());
    remountGraph();
    ensureCanvasSize();
    drawWires();
    renderInspector();
  }
  
/* ---------- UI bind ---------- */
  function bindUi() {
    initTheme();
  
  /* Palette → canvas drag & drop */
  (function setupPaletteDrop() {
    async function handlePaletteDrop(e) {
      e.preventDefault();
      e.stopPropagation();
      const files = e.dataTransfer && e.dataTransfer.files;
      if (files && files.length) {
        const f = files[0];
        if (f && (/\.json$/i.test(f.name) || (f.type && f.type.indexOf("json") >= 0))) {
          try {
            const textJ = await f.text();
            const proj = JSON.parse(textJ);
            if (proj && (proj.main || proj.functions || proj.nodes)) {
              if (!isProjectEmpty() && !confirm("当前工程非空。导入将覆盖当前工程（请先导出保存）。继续？")) {
                status("已取消导入", "err");
                return;
              }
              if (proj.main || proj.functions) importProject(proj);
              else importProject({ main: proj, functions: {}, nodeSeq: 1, fnSeq: 1 });
              status("已导入工程 " + (f.name || ""), "ok");
              return;
            }
          } catch (err) {
            status("JSON 导入失败: " + err.message, "err");
            return;
          }
        }
      }
      let raw = "";
      const types = ["application/x-snlab-node", "application/json", "text/plain", "text"];
      for (let i = 0; i < types.length && !raw; i++) {
        try { raw = e.dataTransfer.getData(types[i]); } catch (_) {}
      }
      if (!raw) return;
      let it = null;
      try { it = JSON.parse(raw); } catch (_) { return; }
      if (!it || !it.type) return;
      const pt = canvasPointFromClient(e.clientX, e.clientY);
      if (it.type === "fn_call") {
        if (!it.fnId || !project.functions[it.fnId]) {
          status("未知函数调用", "err");
          return;
        }
        if (editScope !== "main" && editScope === "fn:" + it.fnId) {
          status("函数内部不可递归调用自身（可拆子函数）", "err");
          return;
        }
        addNode({ type: "fn_call" }, pt.x - 80, pt.y - 20, { fnId: it.fnId });
        status("已放置调用 " + (it.label || project.functions[it.fnId].name || it.fnId), "ok");
        return;
      }
      await spawnFromCatalog(it, pt.x - 80, pt.y - 20);
      status("已放置 " + (it.label || it.type), "ok");
    }
    function allowDrop(e) {
      if (!e.dataTransfer) return;
      e.preventDefault();
      e.dataTransfer.dropEffect = "copy";
    }
    ["workspace", "canvas", "world", "wires"].forEach((id) => {
      const el = $id(id);
      if (!el || el.dataset.snlabPaletteDrop === "1") return;
      el.addEventListener("dragover", allowDrop);
      el.addEventListener("drop", handlePaletteDrop);
      el.dataset.snlabPaletteDrop = "1";
    });
  })();

  document.querySelectorAll("#paletteTabs .tab").forEach((t) => {
      t.addEventListener("click", () => {
        document.querySelectorAll("#paletteTabs .tab").forEach((x) => x.classList.remove("active"));
        t.classList.add("active");
        paletteTab = t.dataset.tab;
        renderPalette();
      });
    });
    if ($id("paletteSearch")) $id("paletteSearch").addEventListener("input", () => renderPalette());

    const on = (id, ev, fn) => { const el = $id(id); if (el) el.addEventListener(ev, fn); };
    on("btnRun", "click", () => { try { runGraph(); } catch (e) { log("错误: " + e.message); status(e.message, "err"); } });
    on("btnApplyFmt", "click", () => {
      try { ensureSession(); status("会话已重建 E/M/NaN", "ok"); } catch (e) { status(e.message, "err"); }
    });
    on("btnClear", "click", () => clearCanvas());
    on("btnBackMain", "click", () => { editScope = "main"; remountGraph(); });
    on("btnNewFn", "click", () => openNewFnDialog());
    on("btnHelp", "click", () => { const d = $id("helpDlg"); if (d) d.showModal(); });
    on("btnTheme", "click", () => toggleTheme());
    on("btnExportProj", "click", () => {
      const blob = new Blob([JSON.stringify(exportProject(), null, 2)], { type: "application/json" });
      const a = document.createElement("a");
      a.href = URL.createObjectURL(blob);
      a.download = "snlab_project.json";
      a.click();
      URL.revokeObjectURL(a.href);
    });
    on("btnImportProj", "click", () => $id("fileImport").click());
    on("fileImport", "change", async (ev) => {
      const f = ev.target.files && ev.target.files[0];
      if (!f) return;
      try {
        if (!isProjectEmpty() && !confirm("当前工程非空。导入将覆盖当前工程（请先导出保存）。继续？")) {
          status("已取消导入", "err");
          ev.target.value = "";
          return;
        }
        importProject(JSON.parse(await f.text()));
      } catch (e) { status("导入失败: " + e.message, "err"); }
      ev.target.value = "";
    });
    on("btnExportC", "click", () => {
      const code = exportC();
      renderCPreview(code);
      const blob = new Blob([code], { type: "text/plain" });
      const a = document.createElement("a");
      a.href = URL.createObjectURL(blob);
      a.download = "snlab_export.c";
      a.click();
      URL.revokeObjectURL(a.href);
    });

    /* cfg form submit: ok/cancel via button value */
    const cfgForm = $id("cfgForm");
    if (cfgForm) {
      cfgForm.addEventListener("submit", (e) => {
        /* method=dialog sets returnValue from submitter */
      });
    }

    window.addEventListener("keydown", (e) => {
      const tag = (e.target && e.target.tagName) || "";
      const typing = tag === "INPUT" || tag === "TEXTAREA" || tag === "SELECT" || (e.target && e.target.isContentEditable);
      const mod = e.ctrlKey || e.metaKey;

      if (e.key === "Escape") {
        hideCtxMenu();
        linkFrom = null;
        endWireDraft(true);
        document.querySelectorAll(".port.linking, .port.in.drop-target").forEach((p) => {
          p.classList.remove("linking");
          p.classList.remove("drop-target");
        });
        drag = null;
        document.querySelectorAll("header.dragging").forEach((h) => h.classList.remove("dragging"));
        const help = $id("helpDlg"); if (help && help.open) help.close();
        status("已取消");
        return;
      }

      if (typing) return;

      /* help */
      if (e.key === "?" || (e.shiftKey && e.key === "/")) {
        e.preventDefault();
        const d = $id("helpDlg"); if (d) d.showModal();
        return;
      }

      /* run: Enter / Ctrl+Enter / R */
      if (e.key === "Enter" || (!mod && (e.key === "r" || e.key === "R"))) {
        e.preventDefault();
        doRun();
        return;
      }

      /* delete */
      if (e.key === "Delete" || (e.key === "Backspace" && !mod)) {
        if (deleteSelection()) e.preventDefault();
        return;
      }

      /* clear canvas */
      if (mod && e.key === "Backspace") {
        e.preventDefault();
        if (confirm("清空当前画布全部节点？")) clearCanvas();
        return;
      }

      /* clipboard — allow native copy when text is selected */
      var selText = (window.getSelection && String(window.getSelection())) || "";
      if (mod && (e.key === "c" || e.key === "C") && !e.shiftKey) {
        if (selText.trim()) return;
        e.preventDefault();
        copySelectedNode();
        return;
      }
      if (mod && (e.key === "x" || e.key === "X")) {
        if (selText.trim()) return;
        e.preventDefault();
        cutSelectedNode();
        return;
      }
      if (mod && (e.key === "v" || e.key === "V")) {
        e.preventDefault();
        pasteClipboard();
        return;
      }
      if (mod && (e.key === "d" || e.key === "D")) {
        e.preventDefault();
        duplicateSelectedNode();
        return;
      }

      /* undo/redo */
      if (mod && !e.shiftKey && (e.key === "z" || e.key === "Z")) {
        e.preventDefault();
        if (typeof undo === "function") undo();
        return;
      }
      if (mod && ((e.key === "y" || e.key === "Y") || (e.shiftKey && (e.key === "z" || e.key === "Z")))) {
        e.preventDefault();
        if (typeof redo === "function") redo();
        return;
      }

      /* UE attached note on selected node */
      if (!mod && !e.altKey && (e.key === "n" || e.key === "N")) {
        const tag = (e.target && e.target.tagName) ? e.target.tagName.toLowerCase() : "";
        if (tag !== "input" && tag !== "textarea" && !(e.target && e.target.isContentEditable)) {
          const n = (typeof selectedId !== "undefined" && selectedId && typeof nodeById === "function")
            ? nodeById(selectedId) : null;
          if (n && n.type !== "comment" && typeof editNodeNoteDialog === "function") {
            e.preventDefault();
            editNodeNoteDialog(n);
            return;
          }
        }
      }

      /* edit */
      if (e.key === "F2" || (mod && (e.key === "e" || e.key === "E") && !e.shiftKey)) {
        e.preventDefault();
        editSelectedNode();
        return;
      }

      /* theme */
      if (mod && e.altKey && (e.key === "t" || e.key === "T")) {
        e.preventDefault();
        toggleTheme();
        return;
      }

      /* project / export */
      if (mod && (e.key === "s" || e.key === "S")) {
        e.preventDefault();
        doExportProject();
        return;
      }
      if (mod && e.shiftKey && (e.key === "c" || e.key === "C")) {
        e.preventDefault();
        doExportC();
        return;
      }
      if (mod && (e.key === "o" || e.key === "O")) {
        e.preventDefault();
        const f = $id("fileImport"); if (f) f.click();
        return;
      }
      if (mod && (e.key === "n" || e.key === "N")) {
        e.preventDefault();
        openNewFnDialog();
        return;
      }
      if (mod && e.key === "Home") {
        e.preventDefault();
        editScope = "main";
        remountGraph();
        status("已返回主画布");
        return;
      }
      if (mod && e.key === "0") {
        e.preventDefault();
        resetView();
        return;
      }
    });

    /* right-click on empty canvas / workspace */
    const workspace = $id("workspace");
    if (workspace) {
      workspace.addEventListener("contextmenu", (e) => {
        if (e.target.closest(".block") || e.target.closest("#wires path") || e.target.closest(".ctx-menu")) return;
        e.preventDefault();
        const pt = canvasLocalFromClient(e.clientX, e.clientY);
        showCtxMenu(e.clientX, e.clientY, { kind: "canvas", x: pt.x, y: pt.y });
      });
    }
    document.addEventListener("pointerdown", (e) => {
      const m = $id("ctxMenu");
      if (m && !m.classList.contains("hidden") && !e.target.closest("#ctxMenu")) hideCtxMenu();
    }, true);
    window.addEventListener("blur", () => hideCtxMenu());
    window.addEventListener("resize", () => hideCtxMenu());

    window.addEventListener("resize", () => drawWires());

    /* never leave a stuck drag if pointer is released outside the header */
    window.addEventListener("pointerup", (e) => {
      if (drag && (drag.pointerId == null || drag.pointerId === e.pointerId)) {
        drag = null;
        document.querySelectorAll("header.dragging").forEach((h) => h.classList.remove("dragging"));
      }
      if (pan && (pan.pointerId == null || pan.pointerId === e.pointerId)) {
        pan = null;
        const w = $id("workspace");
        if (w) w.classList.remove("panning");
      }
    }, true);
    window.addEventListener("pointercancel", () => {
      drag = null;
      pan = null;
      const ws0 = $id("workspace");
      if (ws0) ws0.classList.remove("panning");
      if (wireDraft && !wireDraft._done) {
        endWireDraft(true);
        linkFrom = null;
        clearPortHighlights();
      }
      document.querySelectorAll("header.dragging").forEach((el) => el.classList.remove("dragging"));
    }, true);

    /* ---------- Canvas pan (space+LMB / alt+LMB / wheel) + Ctrl+wheel zoom ---------- */
    function isEditableTarget(t) {
      if (!t || !t.closest) return false;
      return !!(t.closest("input, textarea, select, [contenteditable=true], dialog, .dlg, .ctx-menu"));
    }
    function wantsPanStart(e) {
      if (isEditableTarget(e.target)) return false;
      /* middle-button pan, or Space/Alt + LMB */
      if (e.button === 1) return true;
      if (e.button === 0 && (spaceDown || e.altKey)) return true;
      return false;
    }
    function beginPan(e) {
      if (pan) return;
      endWireDraft(false);
      clearPortHighlights();
      linkFrom = null;
      drag = null;
      pan = { pointerId: e.pointerId, lastX: e.clientX, lastY: e.clientY };
      const ws = $id("workspace");
      if (ws) {
        ws.classList.add("panning");
        try { ws.setPointerCapture(e.pointerId); } catch (_) {}
      }
      e.preventDefault();
      e.stopPropagation();
    }
    function movePan(e) {
      if (!pan) return;
      if (pan.pointerId != null && e.pointerId != null && pan.pointerId !== e.pointerId) return;
      const dx = e.clientX - pan.lastX;
      const dy = e.clientY - pan.lastY;
      pan.lastX = e.clientX;
      pan.lastY = e.clientY;
      panBy(dx, dy);
      e.preventDefault();
    }
    function endPan(e) {
      if (!pan) return;
      if (e && pan.pointerId != null && e.pointerId != null && pan.pointerId !== e.pointerId) return;
      const pid = pan.pointerId;
      pan = null;
      const ws = $id("workspace");
      if (ws) {
        ws.classList.remove("panning");
        try { if (pid != null) ws.releasePointerCapture(pid); } catch (_) {}
      }
    }

    const ws = $id("workspace");
    if (ws) {
      ws.addEventListener("pointerdown", (e) => {
        if (!wantsPanStart(e)) return;
        /* allow pan from empty area, canvas, or world — not from interactive chrome outside */
        if (e.target.closest(".ctx-menu, dialog, button, input, textarea, select, a")) return;
        if (e.button === 1) e.preventDefault(); /* suppress autoscroll */
        beginPan(e);
      });
      ws.addEventListener("auxclick", (e) => { if (e.button === 1) e.preventDefault(); });
      ws.addEventListener("pointermove", movePan);
      ws.addEventListener("pointerup", endPan);
      ws.addEventListener("pointercancel", endPan);
      ws.addEventListener("lostpointercapture", () => {
        if (pan) {
          pan = null;
          ws.classList.remove("panning");
        }
      });
      ws.addEventListener("wheel", (e) => {
        if (isEditableTarget(e.target)) return;
        e.preventDefault();
        /* Ctrl+wheel (or pinch-as-ctrl on trackpads) => zoom around cursor */
        if (e.ctrlKey || e.metaKey) {
          const dy = e.deltaY;
          /* normalize: pixel vs line deltas */
          const steps = e.deltaMode === 1 ? dy : dy / 100;
          const factor = Math.pow(1.08, -steps);
          zoomBy(factor, e.clientX, e.clientY);
          return;
        }
        const speed = e.shiftKey ? 1.25 : 1;
        if (e.shiftKey) panBy(-e.deltaY * speed, -e.deltaX * speed);
        else panBy(-e.deltaX * speed, -e.deltaY * speed);
      }, { passive: false });
      /* empty-canvas: marquee multi-select (or clear) */
      ws.addEventListener("pointerdown", (e) => {
        if (e.button !== 0 || spaceDown || e.altKey) return;
        if (e.target.closest(".block, .port, #wires path, button, input, textarea, select, .ctx-menu, .marquee")) return;
        selectedWire = null;
        endWireDraft(false);
        const pt = canvasPointFromClient(e.clientX, e.clientY);
        marquee = { x0: pt.x, y0: pt.y, x1: pt.x, y1: pt.y, pointerId: e.pointerId, additive: !!(e.shiftKey || e.ctrlKey || e.metaKey) };
        try { ws.setPointerCapture(e.pointerId); } catch (_) {}
        ensureMarqueeEl();
        updateMarqueeEl();
      });
      ws.addEventListener("pointermove", (e) => {
        if (!marquee || (marquee.pointerId != null && e.pointerId !== marquee.pointerId)) return;
        const pt = canvasPointFromClient(e.clientX, e.clientY);
        marquee.x1 = pt.x; marquee.y1 = pt.y;
        updateMarqueeEl();
        if (e.clientX != null && typeof nudgeEdgeAutoPan === "function") {
          nudgeEdgeAutoPan(e.clientX, e.clientY, "marquee");
        }
      });
      const endMarquee = (e) => {
        if (!marquee) return;
        if (e && marquee.pointerId != null && e.pointerId !== marquee.pointerId) return;
        if (typeof stopEdgeAutoPan === "function") stopEdgeAutoPan();
        const x0 = Math.min(marquee.x0, marquee.x1), x1 = Math.max(marquee.x0, marquee.x1);
        const y0 = Math.min(marquee.y0, marquee.y1), y1 = Math.max(marquee.y0, marquee.y1);
        const w = x1 - x0, h = y1 - y0;
        const additive = marquee.additive;
        marquee = null;
        hideMarqueeEl();
        if (w < 4 && h < 4) {
          if (!additive) {
            if (typeof clearSelection === "function") clearSelection();
            else {
              selectedId = null;
              if (selectedIds) selectedIds.clear();
              nodes().forEach((n) => { if (n.el) n.el.classList.remove("selected"); });
            }
          }
          drawWires();
          renderInspector();
          return;
        }
        const hit = [];
        nodes().forEach((n) => {
          const nw = (n.el && n.el.offsetWidth) || 160;
          const nh = (n.el && n.el.offsetHeight) || 80;
          const nx0 = n.x, ny0 = n.y, nx1 = n.x + nw, ny1 = n.y + nh;
          if (nx0 < x1 && nx1 > x0 && ny0 < y1 && ny1 > y0) hit.push(n.id);
        });
        if (additive && typeof selectedIds !== "undefined") {
          const set = new Set(selectedIds);
          hit.forEach((id) => set.add(id));
          if (typeof setSelection === "function") setSelection(Array.from(set));
        } else if (typeof setSelection === "function") {
          setSelection(hit);
        } else {
          selectedId = hit[0] || null;
        }
        status(hit.length ? ("selected " + hit.length) : "no nodes in box", hit.length ? "ok" : "");
        drawWires();
        renderInspector();
      };
      ws.addEventListener("pointerup", endMarquee);
      ws.addEventListener("pointercancel", endMarquee);

      function ensureMarqueeEl() {
        let el = document.getElementById("marquee");
        if (!el) {
          el = document.createElement("div");
          el.id = "marquee";
          el.className = "marquee";
          const canvas = $id("canvas");
          if (canvas) canvas.appendChild(el);
        }
        return el;
      }
      function updateMarqueeEl() {
        if (!marquee) return;
        const el = ensureMarqueeEl();
        const x = Math.min(marquee.x0, marquee.x1);
        const y = Math.min(marquee.y0, marquee.y1);
        const w = Math.abs(marquee.x1 - marquee.x0);
        const h = Math.abs(marquee.y1 - marquee.y0);
        el.style.display = "block";
        el.style.left = x + "px";
        el.style.top = y + "px";
        el.style.width = w + "px";
        el.style.height = h + "px";
      }
      function hideMarqueeEl() {
        const el = document.getElementById("marquee");
        if (el) el.style.display = "none";
      }
    }

    document.addEventListener("keydown", (e) => {
      if (e.code === "Space" && !isEditableTarget(e.target)) {
        if (!spaceDown) {
          spaceDown = true;
          const w = $id("workspace");
          if (w && !pan) w.classList.add("pan-ready");
        }
        /* prevent page scroll when space held over workspace */
        if (e.target === document.body || (e.target && e.target.closest && e.target.closest("#workspace"))) {
          e.preventDefault();
        }
      }
    }, true);
    document.addEventListener("keyup", (e) => {
      if (e.code === "Space") {
        spaceDown = false;
        const w = $id("workspace");
        if (w) w.classList.remove("pan-ready");
      }
    }, true);
    window.addEventListener("blur", () => {
      spaceDown = false;
      const w = $id("workspace");
      if (w) w.classList.remove("pan-ready", "panning");
    });

    applyViewTransform();
    ensureCanvasSize();
  }

  function bindMobileUi() {
    const btnPal = $id("btnTogglePalette");
    const btnIns = $id("btnToggleInspect");
    const btnRunM = $id("btnRunMobile");
    const bd = $id("drawerBackdrop");
    if (btnPal) btnPal.addEventListener("click", () => {
      const pal = $id("palette");
      if (pal && pal.classList.contains("open")) closeDrawers();
      else openDrawer("palette");
    });
    if (btnIns) btnIns.addEventListener("click", () => {
      const ins = $id("inspector");
      if (ins && ins.classList.contains("open")) closeDrawers();
      else openDrawer("inspector");
    });
    if (btnRunM) btnRunM.addEventListener("click", () => { const b = $id("btnRun"); if (b) b.click(); });
    if (bd) bd.addEventListener("click", closeDrawers);
    const palBody = $id("paletteBody");
    if (palBody) palBody.addEventListener("click", (e) => {
      if (window.matchMedia("(max-width: 760px)").matches && e.target.closest(".pitem, .fn-card, .btn")) {
        setTimeout(closeDrawers, 50);
      }
    });
  }

  async function boot() {
    bindUi();
    bindMobileUi();
    renderPalette();
    status("加载 WASM…");
    try {
      if (globalThis.__SNLAB_READY) {
        Module = await globalThis.__SNLAB_READY;
      } else if (typeof globalThis.createSnLab === "function") {
        Module = await globalThis.createSnLab();
      } else if (typeof createSnLab === "function") {
        Module = await createSnLab();
      } else if (typeof globalThis.Module === "object" && globalThis.Module && globalThis.Module.ccall) {
        Module = globalThis.Module;
      } else {
        const mod = await import("./snlab.mjs");
        const factory = mod.default || mod.createSnLab;
        Module = await factory();
      }
      api = wrapApi();
      ensureSession();
      if (!project.main.nodes.length) {
        seedGraphDefaults(project.main);
        remountGraph();
      }
      const ver = api.version();
      if ($id("verLabel")) $id("verLabel").textContent = ver;
      status("就绪 · " + ver, "ok");
      log(ver);
    } catch (e) {
      console.error(e);
      status("加载失败: " + e.message, "err");
      log(String(e));
    }
  }

  if (document.readyState === "loading") document.addEventListener("DOMContentLoaded", boot);
  else boot();
})();


  (function bindUndoButtons() {
    function bind() {
      const bu = document.getElementById("btnUndo");
      const br = document.getElementById("btnRedo");
      if (bu) bu.addEventListener("click", () => { if (typeof undo === "function") undo(); });
      if (br) br.addEventListener("click", () => { if (typeof redo === "function") redo(); });
      if (typeof updateUndoUi === "function") updateUndoUi();
    }
    if (document.readyState === "loading") document.addEventListener("DOMContentLoaded", bind);
    else bind();
  })();
