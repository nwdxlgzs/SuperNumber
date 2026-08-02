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

  
