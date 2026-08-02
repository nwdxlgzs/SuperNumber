/* assemble app.js from js/* parts */
const fs = require("node:fs");
const path = require("node:path");
const dir = path.join(__dirname, "..", "js");
const order = [
  "01-catalog.js",
  "02-runtime.js",
  "03-graph.js",
  "04-palette.js",
  "05-eval.js",
  "06-export.js",
  "07-ui.js",
];
let out = "";
for (const f of order) {
  const p = path.join(dir, f);
  if (!fs.existsSync(p)) throw new Error("missing " + f);
  out += fs.readFileSync(p, "utf8");
  if (!out.endsWith("\n")) out += "\n";
}
const dest = path.join(__dirname, "..", "app.js");
// backup existing if different
const prev = fs.existsSync(dest) ? fs.readFileSync(dest, "utf8") : "";
if (prev === out) {
  console.log("assembled identical", dest, "bytes", out.length);
} else {
  fs.writeFileSync(dest, out, "utf8");
  console.log("assembled updated", dest, "bytes", out.length, "prev", prev.length, "delta", out.length - prev.length);
}