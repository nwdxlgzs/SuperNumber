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
