# SuperNumber Lab (Web)

浏览器端 SuperNumber **蓝图节点**演练场（Emscripten）。可定义**定宽整数 / 大整数 / 任意 E·M 浮点 / 复数**，拖拽连线，**自定义函数**，运行图，导入导出工程（JSON v2），并导出标准 `sn_api` C 代码。

## 依赖

- emsdk / emcc（示例路径：`D:/project/emsdk-3.1.69`）
- Node.js（assemble、smoke、shell 生成）

## 一键构建（推荐）

**无参数 = 完整流水线**（assemble + modular + smoke + single-html）。不必给 PowerShell 加 switch。

仓库根目录：

```bat
cd /d D:\project\git\SuperALFPU
build.bat
rem 等同 build_web.bat
```

或进入 `webapp`：

```bat
cd /d D:\project\git\SuperALFPU\webapp
build.bat
rem 可选：build.bat modular | single | smoke | clean | nosmoke
```

PowerShell：

```powershell
cd D:\project\git\SuperALFPU\webapp
.\build.ps1
# 仅单页：
.\build.ps1 -SingleOnly
# 指定 emsdk：
.\build.ps1 -Emsdk D:\project\emsdk-3.1.69
# 清理后构建：
.\build.ps1 -Clean
# 跳过 smoke：
.\build.ps1 -SkipSmoke
```

构建流程：`assemble-app.js`（合并 `js/01..07` → `app.js`）→ emcc modular → smoke → single-html。

期望 smoke 输出含 bridge 版本与 `SMOKE_OK`。

### Makefile（备选，需已激活 emsdk）

```bat
call D:\project\emsdk-3.1.69\emsdk_env.bat
cd /d D:\project\git\SuperALFPU\webapp
mingw32-make all EMCC=emcc
mingw32-make smoke EMCC=emcc
mingw32-make single-html EMCC=emcc
```

`Makefile` 的 `EXPORTED_FUNCTIONS` 已与 `build.ps1` 对齐（含 `_snw_copy`、RNG 等）。

## 产物

| 文件 | 说明 |
|------|------|
| `app.js` | 由 `js/01..07-*.js` 组装 |
| `snlab.mjs` + `snlab.wasm` | modular（开发 / Node smoke） |
| `snlab-single.html` | 单文件页（WASM=0 + 内嵌 CSS/JS） |
| `shell-single.generated.html` | emcc shell（构建生成，可 gitignore） |

## 源码布局

```
webapp/
  js/01-catalog.js … 07-ui.js   # 前端分片（改这里）
  tools/assemble-app.js         # 合并为 app.js
  tools/build-single-shell.js   # 生成 single shell
  build.ps1 / build.bat         # 一键构建
  _smoke.mjs                    # Node 冒烟
  examples/                     # 工程 JSON 示例
web/sn_web_bridge.c             # WASM 导出桥
```

**改 `js/*` 后务必** `node tools/assemble-app.js` 或跑完整 `build.ps1`。

## 本地打开

### modular（开发）

模块加载不要用 `file://`。起本地 HTTP：

```bat
cd /d D:\project\git\SuperALFPU\webapp
npx --yes serve -p 8765 .
```

打开：http://127.0.0.1:8765/

### 单页

构建后双击 `snlab-single.html`，或同样用 HTTP 打开。

## 示例工程

| 文件 | 说明 |
|------|------|
| `examples/rsa_demo.json` | 完整 RSA：p,q→n,φ,e,d=modinv，powmod 加解密 |
| `examples/rsa_toy.json` | 固定 n,e,d 的 toy powmod |
| `examples/array_sum.json` | 数组求和 |

导入：顶栏「导入」或把 JSON 拖入画布（非空工程会询问覆盖）。

## 节点与注释

- 左侧分类 + 搜索 +「全部」页；函数可拖入。
- **注释**节点：便签样式，双击/检查器编辑，导出 C 为 `/* ... */`，不参与执行/连线。
- 控制流：Entry / Branch / While / For / Sequence / Gate … 对齐 UE 蓝图 exec 语义。

## 位宽说明

- 整数 / 移位 / 浮点 E·M：UI 与核心均无小硬顶；仅受内存与实现可表示范围约束。
- 极端超大位宽可能返回 `SN_ERR_NOMEM` / `SN_ERR_ARG`。

## 手机端

- 窄屏：顶栏 **节点 / 检查器 / 运行** 抽屉。
- 画布：双指平移/缩放；桌面中键 / 空格+左键 / Alt+左键 平移；Ctrl+滚轮缩放。

## 操作速查

| 操作 | 说明 |
|------|------|
| 平移 | 中键拖拽 · 空格+左键 · Alt+左键 |
| 缩放 | Ctrl+滚轮 · 双指捏合 |
| 移动节点 | 标题栏拖动，松手即停 |
| 添加节点 | 点击或拖入画布；常量拖入可配置 |
| 连线 | 从端口拖到端口松开（圆点命中优先于线） |
| 注释 | 流程分类 → 注释；双击编辑文本 |
| 运行 | 「运行」或 Enter / R |
| 工程 | JSON 导入/导出；导出 C → sn_api |
| 主题 | 深色/浅色；Ctrl+Alt+T |

## Node 冒烟

```bat
cd /d D:\project\git\SuperALFPU\webapp
node _smoke.mjs
```
## Tensor / Transformer nodes (Web runtime)

Array/vector/matrix ops run in the **browser JS runtime** (not yet exported to full SN C).

Key ML nodes under the **数据** palette tab:

| Node | Role |
|------|------|
| mat_add / mat_sub / mat_hadamard / mat_scale | elementwise / scale |
| 	en_unary / 	en_binop | generic elementwise |
| softmax_row / layer_norm / ms_norm | normalization |
| gelu / elu / esidual_add | activations / residual |
| ttention_sdp | scaled dot-product attention (out + weights) |
| sin_pe / gather / embedding | PE + embedding lookup |
| eshape / latten / mat_concat / mat_slice | layout |
| mha_split / mha_merge | multi-head rearrange |

Example: open / import examples/transformer_toy.json.

Unit tests (no WASM):

`at
node tools\tensor-unit.mjs
node tools\assemble-app.js
node --check app.js
`
