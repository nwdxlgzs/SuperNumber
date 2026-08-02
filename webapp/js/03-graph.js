/* ---------- Node lifecycle ---------- */
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

  
