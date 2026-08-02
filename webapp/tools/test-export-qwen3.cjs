const fs = require("node:fs");
const path = require("node:path");
const vm = require("node:vm");

const root = path.join(__dirname, '..');
const exportSrc = fs.readFileSync(path.join(root, "js", "06-export.js"), "utf8");
const runtimeSrc = fs.readFileSync(path.join(root, "js", "02-runtime.js"), "utf8");
const evalSrc = fs.readFileSync(path.join(root, "js", "05-eval.js"), "utf8");

function grabFn(src, name) {
  const idx = src.indexOf(`function ${name}`);
  if (idx < 0) throw new Error("missing " + name);
  let i = src.indexOf("{", idx);
  let d = 0, j = i;
  for (; j < src.length; j++) {
    if (src[j] === "{") d++;
    else if (src[j] === "}") {
      d--;
      if (d === 0) { j++; break; }
    }
  }
  return src.slice(idx, j);
}

const helpers = [
  grabFn(runtimeSrc, "isTensorNode"),
  grabFn(runtimeSrc, "isExecPort"),
  grabFn(runtimeSrc, "portsOf"),
  grabFn(runtimeSrc, "isExecPortOnNode"),
  grabFn(evalSrc, "topoSort"),
].join("\n");

// Strip top-level history vars from exportSrc that collide - wrap as IIFE returning exportC
// Actually 06-export has: exportProject, import..., undo..., emitGraphC, exportC, seed...
// historyPast is in 06-export from undo section BEFORE emitGraphC

const proj = JSON.parse(fs.readFileSync(path.join(root, "examples", "qwen3_toy.json"), "utf8"));
const project = { main: proj.main, functions: {} };
for (const [k, fn] of Object.entries(proj.functions || {})) {
  project.functions[k] = {
    id: fn.id || k,
    name: fn.name || k,
    params: fn.params || [],
    nodes: fn.nodes || [],
    wires: fn.wires || [],
  };
}

const code = `
${helpers}
function fmtBits(){return {e:11,m:52,nan:1};}
function parseCplxConst(){return {re:0,im:0};}
function status(){}
function updateUndoUi(){}
function serializeNode(n){return n;}
function defaultCfg(){return {portValues:{}};}
function $id(){return null;}
function remountGraph(){}
function ensureCanvasSize(){}
function drawWires(){}
function renderInspector(){}
function nodes(){ return project.main.nodes; }
function setNodes(){}
function setWires(){}
function currentGraph(){ return project.main; }
var nodeSeq=1, fnSeq=1, editScope="main", selectedId=null;
var project = ${JSON.stringify(project)};
${exportSrc}
exportC();
`;

let out;
try {
  out = vm.runInNewContext(code, { console }, { timeout: 15000, filename: "export-run.js" });
} catch (e) {
  console.error("exportC failed:", e.message);
  if (e.stack) console.error(e.stack.split("\n").slice(0, 20).join("\n"));
  process.exit(1);
}

fs.writeFileSync(path.join(__dirname, "_qwen3_export.sample.c"), out, "utf8");
console.log("wrote _qwen3_export.c bytes", out.length);

const checks = [
  [/static sn_status snlab_qwen3_attn[\s\S]*?\{\s*return SN_OK;\s*\}/, false, "attn not empty shell"],
  [/static sn_status snlab_qwen3_ffn[\s\S]*?\{\s*return SN_OK;\s*\}/, false, "ffn not empty shell"],
  [/static sn_status snlab_qwen3_block[\s\S]*?\{\s*return SN_OK;\s*\}/, false, "block not empty shell"],
  [/api->tensor\.matmul/, true, "has matmul"],
  [/api->tensor\.rms_norm/, true, "has rms_norm"],
  [/api->tensor\.rope/, true, "has rope"],
  [/api->tensor\.attention_sdp/, true, "has attention_sdp"],
  [/snlab_qwen3_attn/, true, "has attn symbol"],
  [/snlab_qwen3_ffn/, true, "has ffn symbol"],
  [/api->tensor\.copy\(ctx, out/, true, "tensor return"],
  [/sn_tensor \*out/, true, "tensor signature"],
];
let bad = 0;
for (const [re, want, label] of checks) {
  const hit = re.test(out);
  const ok = hit === want;
  console.log(ok ? "OK" : "FAIL", label, "hit="+hit);
  if (!ok) bad++;
}
const m2 = out.match(/static sn_status snlab_qwen3_attn[\s\S]*?\n\}/);
if (m2) console.log("--- attn ---\n", m2[0].slice(0, 1500));
const m3 = out.match(/static sn_status snlab_qwen3_block[\s\S]*?\n\}/);
if (m3) console.log("--- block head ---\n", m3[0].slice(0, 1200));
process.exit(bad ? 2 : 0);

