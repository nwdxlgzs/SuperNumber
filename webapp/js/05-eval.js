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

  
