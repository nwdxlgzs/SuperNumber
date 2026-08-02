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
  
