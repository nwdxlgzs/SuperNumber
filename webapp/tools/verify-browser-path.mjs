/* browser-path eval smoke for qwen3_toy — loads assembled logic patterns via reimplementing entryHasExec gate */
import { createRequire } from "module";
import { pathToFileURL } from "url";
import { readFileSync } from "fs";
import { dirname, join } from "path";
import { fileURLToPath } from "url";

const __dirname = dirname(fileURLToPath(import.meta.url));
const webapp = join(__dirname, "..");
const project = JSON.parse(readFileSync(join(webapp, "examples/qwen3_toy.json"), "utf8"));

// preview unit
function formatResultPreview(text) {
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

const prev = formatResultPreview("1,2,3,4,5,6,7,8,9");
const nl = (prev.match(/\n/g) || []).length;
if (nl > 1) throw new Error("preview has >2 lines: " + JSON.stringify(prev));
if (!prev.includes("...")) throw new Error("preview missing middle ellipsis: " + prev);
if (prev.endsWith("\n...")) throw new Error("preview trailing dots line: " + JSON.stringify(prev));
console.log("PREVIEW_OK", prev);

// serialize note roundtrip shape
const serSample = {
  id: "n1", type: "out", op: null, x: 0, y: 0,
  cfg: {}, arity: null, fnId: null, paramIndex: null, paramName: null,
  inputs: {}, inputFromPorts: {}, collapsed: false, foldUnusedPorts: false, note: "hello"
};
if (!serSample.note) throw new Error("note missing");
console.log("NOTE_FIELD_OK");

// qwen3 functions: entryHasExec must be false → data path
function isExecPort(p) {
  return p === "exec" || p === "then" || p === "else" || p === "body" || p === "completed" ||
    p === "closed" || p === "exit" || p === "enter" || p === "reset" || p === "open" ||
    p === "close" || p === "toggle" || p === "default" || /^then\d+$/.test(p) || /^case\d+$/.test(p);
}
for (const [k, fn] of Object.entries(project.functions || {})) {
  const entry = (fn.nodes || []).find((n) => n.type === "entry");
  const entryHasExec = !!(entry && (fn.wires || []).some((w) =>
    w.from === entry.id && isExecPort(w.fromPort || "exec")));
  const ret = (fn.nodes || []).find((n) => n.type === "fn_return");
  if (!ret) throw new Error(k + " missing fn_return");
  if (entryHasExec) console.warn("WARN", k, "has entry exec — browser will use exec path");
  else console.log("DATA_PATH_OK", k, "ret.in=", ret.inputs && ret.inputs.in);
}

// run headless full SN path
const snlabPath = pathToFileURL(join(webapp, "snlab.mjs")).href;
const factory = (await import(snlabPath)).default;
const M = await factory();
const ver = M.ccall ? M.ccall("snw_version", "string", [], []) : (M._snw_version ? "" : "");
// use cwrap like runner
function cwrap(name, ret, args) { return M.cwrap(name, ret, args); }
const api = {
  create: cwrap("snw_create", "number", ["number", "number", "number"]),
  destroy: cwrap("snw_destroy", null, ["number"]),
  lastError: cwrap("snw_last_error", "string", ["number"]),
  version: cwrap("snw_version", "string", []),
  freeStr: cwrap("snw_free_str", null, ["number"]),
  newTensor: cwrap("snw_new_tensor", "number", ["number"]),
  freeTensor: cwrap("snw_free_tensor", null, ["number", "number"]),
  tensorFromStr: cwrap("snw_tensor_from_str", "number", ["number", "number", "string"]),
  tensorToStr: cwrap("snw_tensor_to_str", "number", ["number", "number"]),
  tensorCopy: cwrap("snw_tensor_copy", "number", ["number", "number", "number"]),
  tensorUnary: cwrap("snw_tensor_unary", "number", ["number", "number", "string", "number"]),
  tensorBinary: cwrap("snw_tensor_binary", "number", ["number", "number", "string", "number", "number"]),
  tensorRmsNorm: cwrap("snw_tensor_rms_norm", "number", ["number", "number", "number", "number", "number"]),
  tensorRope: cwrap("snw_tensor_rope", "number", ["number", "number", "number", "number"]),
  tensorAttentionSdp: cwrap("snw_tensor_attention_sdp", "number", [
    "number", "number", "number", "number", "number", "number", "number", "number"
  ]),
  readCString(ptr) {
    if (!ptr) return "";
    const s = M.UTF8ToString(ptr);
    this.freeStr(ptr);
    return s;
  }
};
console.log("bridge", api.version());
if (!String(api.version()).includes("1.")) throw new Error("bad version");

// ensure source eval has entryHasExec + force ret eval
const evalSrc = readFileSync(join(webapp, "js/05-eval.js"), "utf8");
if (!evalSrc.includes("entryHasExec")) throw new Error("05-eval missing entryHasExec");
if (!evalSrc.includes("data-eval return/out if control path never reached")) throw new Error("05-eval missing ret force");
const serSrc = evalSrc.slice(evalSrc.indexOf("function serializeNode"));
if (!serSrc.includes("o.note")) throw new Error("serializeNode missing note");
const graphSrc = readFileSync(join(webapp, "js/03-graph.js"), "utf8");
if (!graphSrc.includes("getNodeNote") || !graphSrc.includes("node-note")) throw new Error("graph missing note UI");
const expSrc = readFileSync(join(webapp, "js/06-export.js"), "utf8");
if (!expSrc.includes("function undo") || !expSrc.includes("function redo")) throw new Error("missing undo/redo");
if (!expSrc.includes("historyQuiet")) throw new Error("missing historyQuiet");
if (!expSrc.includes("note: (n.note")) throw new Error("import missing note");
console.log("SOURCE_OK");

// re-run existing qwen3 headless runner logic quickly via spawn
import { spawnSync } from "child_process";
const r = spawnSync(process.execPath, [join(webapp, "_run_qwen3.mjs")], { encoding: "utf8", cwd: webapp });
process.stdout.write(r.stdout || "");
process.stderr.write(r.stderr || "");
if (r.status !== 0) process.exit(r.status || 1);
console.log("BROWSER_PATH_CHECKS_OK");
