# SuperNumber

**SuperNumber**（前缀 `sn_`）是一个用 **C99** 编写的统一数值库：把定宽整数、无限位大整数、运行时指定 **E/M** 的浮点（可开关 NaN）、完整 **math.h 风格** 超越/特殊函数、复数，以及面向 Transformer 的二维张量算子，统一到带类型标签的 `sn_value` / `sn_tensor` 上。

设计目标是 **可嵌入、可多开、可防崩隔离**：没有库级全局可变状态；每个 `sn_ctx` 自带分配器、舍入、flags 与 RNG。对外推荐通过 **`sn_api` 函数指针表** 使用，降低导出符号面。

| | |
|---|---|
| 语言 | C99 + `stdint.h`（不强制 C89，也不强制 `__int128` / 汇编） |
| 许可证 | [MIT](LICENSE) |
| 主头文件 | [`include/sn.h`](include/sn.h) |
| 内部/测试扁平 API | [`include/sn_flat.h`](include/sn_flat.h) |
| 设计文档 | [PLAN.MD](PLAN.MD) |
| 任务与验收记录 | [TODO.MD](TODO.MD) |
| multiprec 舍入说明 | [docs/ROUNDING_MP.md](docs/ROUNDING_MP.md) |
| Web 蓝图编辑器 | [webapp/README.md](webapp/README.md) |

> 当前开发树约在 `v1.0.0` 之后持续加固（数学 residual、Web Lab、张量）。开源发布请以仓库内最新 `make test` 与文档为准。

---

## 目录

- [为什么做 SuperNumber](#为什么做-supernumber)
- [功能一览](#功能一览)
- [架构要点](#架构要点)
- [快速开始](#快速开始)
- [API 导览](#api-导览)
- [正确性、性能与边界](#正确性性能与边界)
- [构建与测试](#构建与测试)
- [Web Lab（浏览器蓝图）](#web-lab浏览器蓝图)
- [仓库布局](#仓库布局)
- [发布与开源清单](#发布与开源清单)
- [贡献与路线](#贡献与路线)
- [许可证](#许可证)

---

## 为什么做 SuperNumber

常见数值栈往往割裂：

- 宿主 `int64` / `double` 位宽固定；
- 大整数另接 GMP；
- 任意精度浮点另接 MPFR/libbf；
- 密码学模运算、特殊函数、张量又是另一套。

SuperNumber 尝试在 **同一套 ctx / value / 错误码 / 分配器** 下覆盖：

1. **任意位宽整数**（含逻辑/算术右移、科学计数法输入）；
2. **自定义 IEEE 风格浮点格式**（E、M 运行时指定，可关 NaN，总位宽可远超 64）；
3. **C 数学库级超越函数**（窄格式 host 桥接，宽格式 pure soft multiprec）；
4. **密码学常用模运算**（含 Montgomery、`powmod_ct`）；
5. **复数与 2D 张量**（Attention / RMSNorm / RoPE 等积木）；
6. **浏览器蓝图 Lab**（WASM + 节点图，导出 `sn_api` C 代码）。

它不是“再造 GMP/MPFR 的全部工程生态”，而是 **可嵌入的统一数值运行时**。

---

## 功能一览

### 整数与大整数

| 能力 | 说明 |
|------|------|
| 定宽整数 | `i8`–`i64` / `u8`–`u64`，可指定任意 width + 有无符号 |
| 无限位 | `BIGINT` |
| 算术 | add/sub/mul/div/rem/neg/abs |
| 位运算 | and/or/xor/not（负数十进制补码语义，对齐常见大整数习惯） |
| 移位 | `shl`；**`shr` 逻辑右移**；**`sar` 算术右移** |
| 位操作 | `bitlen` / `getbit` / `setbit` / `popcount` / `ctz` |
| 字符串 | 基 2–36；科学计数法整数（`1.5e3` → 1500；非整数则 `SN_ERR_FORMAT`） |
| 数论/密码学 | `gcd` `lcm` `modinv` `mulmod` `powmod` `powmod_ct` `isqrt` |
| Montgomery | 奇数模：`mont_setup` / `mont_from` / `mont_mul` / `mont_to`（`powmod` 自动使用） |

`mulmod` 约定：结果落在 **`[0, m)`**（含负积的符号折叠）。

### 浮点（自定义 E/M）

| 能力 | 说明 |
|------|------|
| 格式 | 运行时 `e_bits` / `m_bits` / `nan_enabled` |
| 上限 | `SN_FLOAT_E_MAX` / `SN_FLOAT_M_MAX` 默认约 `INT_MAX/4`（防恶意超大分配）；**无“只支持到 64 位”的产品硬顶** |
| 窄路径 | 约 `2≤e≤30` 且 `1≤m≤52`：快速编码 + host 桥接 |
| 宽路径 | 其余合法格式走 multiprec soft（多 limb；工作指数使用 `int64`） |
| 算术 | + − × ÷、`fma`（multiprec **fused** 单次最终舍入）、`sqrt` |
| 余数 | `fmod`、`frem`、`remquo`（最近商 **IEEE ties-to-even**） |
| 分类/比较 | classify、signbit、`totalorder`、copysign、fmin/fmax/fdim |
| 舍入族 | floor/ceil/trunc/rint/nearbyint/`fround`（对应 C `round`；命名避开与枚举冲突） |
| 缩放 | frexp/ldexp/ilogb/logb/scalbn/nextafter/modf |
| 字符串 | 科学计数法输入；multiprec 十进制 `to_str` 已做尾数质量整理 |

### 数学库（第四期）

- `m_bits ≤ 52`：host `math.h` 计算后重编码到目标格式  
- `m_bits > 52`：`sn_math_soft` pure software（级数 / AGM / 提升精度 / 恒等变换）

覆盖包括但不限于：

- 三角 / 反三角 / 双曲 / 反双曲  
- `exp` `expm1` `exp2` `log` `log1p` `log2` `log10` `pow` `hypot` `cbrt`  
- `erf` `erfc` `tgamma` `lgamma` `digamma` `trigamma` `polygamma`  
- 不完全 gamma/beta：`igamma` `igammac` `ibeta` `ibetac`  
- 椭圆积分：`ellipk` `ellipe` `ellipf` `ellipeinc` `ellipiinc`  
- Jacobi：`jacobi_sn` / `jacobi_cn` / `jacobi_dn`  
- Bessel：`j0/j1/jn` `y0/y1/yn` `i0/i1/in` `k0/k1/kn`

### 复数

矩形坐标 `sn_cplx`（re/im 共享同一浮点格式）：加减乘除、abs/arg/conj、polar 转换等，经 `api.cplx` 绑定。

### 张量

`sn_tensor`：行主序 `rows×cols` 的 SN 浮点矩阵（元素为 `sn_value`，由 ctx 分配）。

| 算子族 | 例子 |
|--------|------|
| 结构 | create/copy、transpose、reshape、slice、concat、gather、from_doubles/from_str/to_str |
| 线性/逐元 | matmul、add/sub/hadamard/div/scale、unary（neg/exp/tanh/relu/gelu/silu/sqrt/abs…） |
| 归一化/注意力 | softmax_row、rms_norm、layer_norm、rope、sin_pe、attention_sdp |

用于 Web Lab 中 Transformer / Qwen3-toy 蓝图；数值路径优先走 SN，而不是 JS 占位实现。

### Web Lab

浏览器端节点（蓝图）环境：自定义常量、连线、函数定义、控制流、工程 JSON 导入导出、运行预览、导出标准 `sn_api` C 代码。详见 [webapp/README.md](webapp/README.md)。

---

## 架构要点

```text
┌─────────────────────────────────────────────────────────┐
│ sn_api (sn_api_bind)                                    │
│  ctx · value · integer · arith · flt · math · crypto    │
│  cplx · tensor                                          │
└───────────────────────────┬─────────────────────────────┘
                            │
        ┌───────────────────┼───────────────────┐
        ▼                   ▼                   ▼
   sn_ctx (alloc/round/   sn_value            sn_tensor
   flags/rng/soft_cache)  INT/BIGINT/FLOAT    2D float grid
        │                   │
        ▼                   ▼
   sn_alloc_*          sn_int / sn_float(_mp)
                       sn_math(_soft) / sn_crypto / sn_complex
```

### 无全局可变状态

- **没有** 进程级全局默认分配器指针（多实例互不踩踏，崩溃状态不串台）。
- `sn_ctx_init` 将 ctx 绑到 `sn_alloc_default`；可用 `set_alloc` 换成 arena / 计数器。
- `sn_api_bind` 只填充函数指针表，可每线程 / 每模块调用。

### 公共 API vs 扁平符号

| 层 | 头文件 | 用途 |
|----|--------|------|
| 推荐公共 | `sn.h` | 类型、`sn_api`、`sn_api_bind` |
| 内部/测试 | `sn_flat.h` | 扁平 `sn_add` 等；可用 `SN_HIDE_FLAT` + ELF `-fvisibility=hidden` 隐藏 |

### 状态码与 flags

```c
typedef enum sn_status {
    SN_OK = 0,
    SN_ERR_NOMEM,
    SN_ERR_TYPE,
    SN_ERR_RANGE,
    SN_ERR_DOMAIN,
    SN_ERR_FORMAT,
    SN_ERR_DIVZERO,
    SN_ERR_INVALID,
    SN_ERR_ARG
} sn_status;
```

浮点异常类 flags（可累积在 ctx 上）：`SN_FLAG_INVALID`、`DIVZERO`、`OVERFLOW`、`UNDERFLOW`、`INEXACT`。

### 舍入与溢出策略

- 浮点：`SN_ROUND_NTE`（默认，最近偶数）/ `TZ` / `UP` / `DN` / `NA`（最近远离 0）
- 整数溢出：`SN_IOV_WRAP` / `SN_IOV_SATURATE`
- 单次运算可用 `sn_op_opt` 覆盖 ctx 默认

### 堆契约（重要）

凡经某 `sn_ctx` 分配的：

- `sn_value` 堆 limb  
- `to_str` 返回的 `char *`  
- `sn_tensor.data`  

必须用 **同一 ctx** 的 `value.clear` / `str_free` / `tensor.clear` / `ctx.fini` 释放。跨 ctx free 视为未定义行为。

---

## 快速开始

### 依赖

- C99 编译器：GCC / Clang / MinGW-w64  
- GNU Make（Windows 上常用 `mingw32-make`）  
- 链接 `-lm`

### 编译与测试

```bat
cd /d path\to\SuperNumber
mingw32-make CC=gcc
mingw32-make test CC=gcc
mingw32-make example CC=gcc
```

Unix / macOS：

```sh
make
make test
make example
```

期望输出形如：`Result: NNNN passed, 0 failed`（开发树近期全量约 **22146** 项；以你本机为准）。

### 最小示例（推荐 `sn_api`）

```c
#include "sn.h"
#include <stdio.h>

int main(void)
{
    sn_ctx ctx;
    sn_api api;
    sn_value a, b, c;
    char *s = NULL;

    sn_api_bind(&api);
    api.ctx.init(&ctx);
    api.value.init(&a);
    api.value.init(&b);
    api.value.init(&c);

    /* 整数 */
    api.integer.i64(&ctx, &a, 40);
    api.integer.i64(&ctx, &b, 2);
    api.arith.add(&ctx, &c, &a, &b, NULL);

    /* 类似 binary64 外形的浮点 + sin */
    api.flt.from_str(&ctx, &a, "0.5", 11, 52, 1, NULL);
    api.math.sin(&ctx, &c, &a, NULL);

    /* multiprec 示例：e=15, m=80 */
    api.flt.from_str(&ctx, &a, "1.25e-3", 15, 80, 1, NULL);
    api.flt.to_str(&ctx, &s, &a, 10);
    if (s) {
        puts(s);
        api.flt.str_free(&ctx, s);
        s = NULL;
    }

    /* 模幂：2^10 mod 1000 == 24 */
    api.integer.i64(&ctx, &a, 2);
    api.integer.i64(&ctx, &b, 10);
    api.integer.i64(&ctx, &c, 1000);
    api.crypto.powmod(&ctx, &a, &a, &b, &c);
    api.integer.to_str(&ctx, &s, &a, 10);
    if (s) {
        puts(s);
        api.integer.str_free(&ctx, s);
    }

    api.value.clear(&ctx, &a);
    api.value.clear(&ctx, &b);
    api.value.clear(&ctx, &c);
    api.ctx.fini(&ctx);
    return 0;
}
```

扁平 API 示例见 [`examples/basic.c`](examples/basic.c)（`#include "sn_flat.h"`，更偏测试/内部风格）。

链接：

```text
cc -std=c99 -O2 -Iinclude your.c -L. -lsn -lm -o your_app
```

---

## API 导览

### 绑定

```c
sn_api api;
sn_api_bind(&api);   /* 无进程全局写入；可重复调用 */
```

### 表结构

| 字段 | 职责 |
|------|------|
| `api.ctx` | init/fini、set_alloc、set_round、set_rng、flags |
| `api.value` | init/clear/copy |
| `api.integer` | 定宽/大整数构造、字符串、位操作 |
| `api.arith` | 通用加减乘除、位运算、移位、比较 |
| `api.flt` | 浮点构造、算术、cast、字符串、分类 |
| `api.math` | 超越函数与特殊函数 |
| `api.crypto` | gcd/modinv/mulmod/powmod/mont/isqrt/… |
| `api.cplx` | 复数 |
| `api.tensor` | 矩阵与注意力积木 |

完整函数指针列表以 [`include/sn.h`](include/sn.h) 为准（单文件可检索）。

### 浮点格式约定

- `e_bits`：指数域位数（≥ 2）  
- `m_bits`：尾数存储宽度（实现与 pack 约定一致）  
- `nan_enabled = 0`：无效操作按约定偏向 ±Inf + `INVALID`，而不是 NaN  

Web Lab 提供 FP16/32/64 等预设；C 侧直接传 E/M 即可。

### 字符串与科学计数法

- 整数：`api.integer.from_str` / `from_str_bigint`  
- 浮点：`api.flt.from_str(ctx, out, "1.25e-3", e, m, nan_en, opt)`  
- 输出：`to_str` + 对应模块的 `str_free`

---

## 正确性、性能与边界

### 正确性口径

SuperNumber 用 **可复现的 residual / 单元测试门禁** 约束实现，而不是“全部函数已机器证明 0.5 ulp 正确舍入”：

| 门禁 | 对照物 | 开发树近期参考结果 |
|------|--------|--------------------|
| `make test` | 内置单元 / 交叉 / 张量等 | 约 **22146** passed, 0 failed |
| `libbf-mp-res-probe` | Bellard **libbf** | 约 **8347** residual fails=0（需本地 `playground`） |
| `gmp-int-probe` | **mini-gmp** | 约 **9711** fails=0；大整数乘法常达到或优于 mini-gmp（机器相关） |
| `specials-mp-res-probe` | 自提升 / 递推恒等 | lgamma/tgamma/digamma 约 **532**=0 |
| `highorder-mp-res-probe` | 椭圆 / igamma / ibeta / Jacobi / I·K | 约 **1068**=0 |

Residual 判定示意：

```text
|SN - ref| / max(1, |ref|)  <  2^(-(m - slack))
```

`slack` 是允许损失的尾数 bit 余量（截断、提升往返、组合恒等放大）。**进一步收紧 slack** 是持续优化项，不阻塞功能发布。

multiprec 打包与整数舍入决策表见 [docs/ROUNDING_MP.md](docs/ROUNDING_MP.md)。

### 实现边界（请读）

| 主题 | 现状 |
|------|------|
| 默认 RNG | xorshift64*，**不是 CSPRNG**；请 `set_rng` 注入 |
| 普通 `powmod` / 模运算 | 变时 limb 运算，**不保证**侧信道安全 |
| `powmod_ct` | 奇数模：固定宽度扫描 + 始终平方/乘 + 掩码选择；Mont 最终归约减少数据分支；偶数模返回 `SN_ERR_DOMAIN`。堆分配/缓存/setup 仍非完整 CT |
| 窄浮点数学 | 经 host `double`，质量受宿主 libm 影响 |
| 宽浮点数学 | soft 级数/AGM/提升；以 residual 门禁为准，不宣称全域 bit-exact |
| `__int128` / 汇编 | **默认不依赖**，保持可移植 C |
| 超大 E/M 或位宽 | 允许到实现上限；失败返回 `SN_ERR_NOMEM` / `SN_ERR_RANGE` |
| 复数 | 子模块，不是完整 C23 `complex.h` 替代 |
| 张量 | 2D、蓝图/算法实验友好，不是 BLAS 竞品 |

### 性能备注

- 大整数：纯 C limb；相对 mini-gmp 的 ratio 随位数变化（见 gmp probe 输出）。  
- multiprec 初等函数：ctx 内 soft cache（`ln2`/`pi` 等，随 `fini` 释放）。  
- `log` 等路径对 AGM 切换有工程阈值（可用编译期宏标定，如 `SN_SOFT_LOG_AGM_MIN_M`）。  
- Web 路径优先正确性与可交互性；重负载建议原生链接 `libsn`。

---

## 构建与测试

### Make 目标

| 目标 | 说明 |
|------|------|
| `lib` / 默认 `all` | 生成 `libsn.a` |
| `test` | 编译并运行全量 `tests/test_runner` |
| `test-debug` | `-DSN_DEBUG_ALLOC` 重建并测试 |
| `example` | 构建 `examples/basic` |
| `bench` | `bench/bench_sn` |
| `clean` | 删除对象与 `libsn.a` |
| `hide-flat` | ELF 上隐藏扁平符号并跑测试 |
| `shared` | 构建 `libsn.so`（Unix-like） |

### 可选 residual probes

下列目标通常需要本地 **`playground/`** 中的第三方源码（**默认 gitignore，不随仓库发布**）：

| 目标 | 依赖 | 作用 |
|------|------|------|
| `softfp-probe` | libbf softfp | IEEE softfp64 对照 |
| `libbf-arith-probe` / `libbf-math-probe` / `libbf-str-probe` / `libbf-mp-res-probe` | libbf | 浮点算术/超越/字符串 residual |
| `gmp-int-probe` | mini-gmp | 整数正确性与粗性能 |
| `specials-mp-res-probe` / `highorder-mp-res-probe` / `polygamma-mp-res-probe` / `erf-mp-res-probe` | 主要依赖 libsn | 特殊函数与高阶恒等 |
| `agm-thr-probe` | libbf | log AGM 阈值标定 |

示意目录：

```text
playground/libbf/           # Bellard libbf
playground/gmp-6.3.0/mini-gmp/
```

正式 probe 源文件已跟踪，例如：

- `tests/_libbf_mp_res_probe.c`  
- `tests/_gmp_int_probe.c`  
- `tests/_specials_mp_res_probe.c`  
- `tests/_highorder_mp_res_probe.c`  

---

## Web Lab（浏览器蓝图）

### 能力摘要

- 节点化搭建整数 / 浮点 / 复数 / 张量 / 控制流 / 自定义函数  
- 工程 JSON 导入导出；运行预览；导出 `sn_api` C  
- 示例：`webapp/examples/rsa.json`、`transformer_toy.json`、`qwen3_demo.json` 等  

### 一键构建

```bat
cd /d path\to\SuperNumber
build.bat
rem 或
cd webapp
build.bat
```

默认流水线：组装 `app.js` → emcc modular → Node smoke → 生成单页 HTML。  
需要本机 [Emscripten](https://emscripten.org/)（`emcc`）与 Node.js。

| 产物 | 说明 |
|------|------|
| `webapp/snlab.mjs` + `snlab.wasm` | modular（开发 / smoke） |
| `webapp/snlab-single.html` | 单文件页，便于分发 |
| `web/sn_web_bridge.c` | `snw_*` 导出桥 |

操作说明、子命令与快捷键见 **[webapp/README.md](webapp/README.md)**。

---

## 仓库布局

```text
.
├── include/           # sn.h, sn_flat.h
├── src/               # 核心实现
├── tests/             # 单元测试 + 正式 residual probes
├── examples/          # basic 等
├── bench/             # 基准
├── web/               # sn_web_bridge.c
├── webapp/            # Lab 前端、构建脚本、examples
├── docs/              # ROUNDING_MP.md 等
├── PLAN.MD TODO.MD
├── Makefile
├── build.bat build_web.bat build_wasm.bat
└── LICENSE
```

默认 **不** 纳入版本库（见 `.gitignore`）：

- `playground/`（oracle 与本地实验）  
- `*.o`、`*.exe`、`*.a`、`build/`  
- 一次性诊断稿与大量 `*.out` / `*.err`

---


## 贡献与路线

### 欢迎的方向

1. **Residual 收紧**：保持全绿前提下减小 slack，或扩展对照面。  
2. **性能**：大整数与 multiprec 热点（默认可移植路径；可选加速需可关闭）。  
3. **数学**：特殊函数定义域边缘、复数完备性。  
4. **Web Lab**：无障碍交互、更多节点、导出 C 质量。  
5. **工程化**：CI 矩阵、打包、更多示例教程。  

### 建议工作流

1. Fork + feature branch  
2. `make test`（Windows：`mingw32-make test CC=gcc`）  
3. 改 soft 数学 / 整数内核时补充测试或 residual 用例  
4. 改 WASM 导出时跑 `webapp\build.bat` 并确认 smoke  
5. PR 写清动机、API 影响、风险与测试证据  

### 技术约束（贡献时请遵守）

- 不引入库级全局可变状态  
- 不把 asm / `__int128` 变成默认硬依赖  
- 新能力优先挂到 `sn_api` 表  
- 破坏性语义（舍入、模运算区间、关 NaN 策略）必须改文档与测试  

### 路线图（摘要）

已完成主线：整数 → 自定义浮点 → 密码学大整数 → 数学库 → 复数 / 张量 / Web Lab → residual 加固。  

持续项：更紧 residual、更大蓝图与性能、CI/发行、可选更强正确舍入叙述。  

细节以 [PLAN.MD](PLAN.MD) 与 [TODO.MD](TODO.MD) 为准。

---

## 许可证

本项目采用 **MIT License**。

```text
Copyright (c) 2026 SuperNumber contributors
```

libbf、GMP 等 **测试对照物不包含在默认仓库树中**；若你本地拉取，请遵守其各自许可证，并仅用于开发测试。

---

## 致谢

- 开发中以 **Bellard libbf**、**GMP/mini-gmp** 等作为 residual 对照（可选依赖）。  
- Web Lab 使用 **Emscripten** 工具链。  
- 需求侧推动了“统一 `sn_value` + 任意 E/M + 蓝图导出 C”的形态。

---

**一句话**：SuperNumber 是可嵌入的统一数值运行时——从自定义位宽整数/浮点，到超越函数、模运算、张量与浏览器蓝图——用同一套 `sn_ctx` / `sn_api` 串起来，并把正确性尽量建立在可复现的 residual 门禁上。