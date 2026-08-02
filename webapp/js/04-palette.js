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

  
