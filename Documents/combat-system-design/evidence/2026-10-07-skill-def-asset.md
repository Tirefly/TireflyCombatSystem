# EVID-2026-10-07-skill-def-asset（R6 Task 1：`TcsSkill` 模块 + 技能 Def 资产族 + 第五条发现路径）

> **结论一句话**：技能定义资产**真被内容目录发现**（第一次 PIE 会话 `0` 条 ⇒ 重启后 `1` 条，**这是"发现路径真的在读内容目录"的构造性证据**，不是硬编码），可**按 `DefTag` 解析**、**主资产身份**符合 `[PrimaryAssetType, DefTag]`、**继承白拿的字段与施法语义逐项保真**、**合法资产判 `Valid`**（非 `NotValidated`）、**表行往返保真且 `RowName` 与 `DefTag` 不同名**；作者期校验对**六个配置错误类别逐一拦下**（含"空时段表合法"的反向对照）。
>
> **合计 28 PASS / 0 FAIL**（Run 17 + Reject 11）。三条命令各执行**恰好 1 次**，顺序 = `Prepare`（非 PIE）→ 重启 PIE → `Run` → `Reject`（非 PIE）。
>
> **证据纪律**（照 `EVID-2026-10-06-trigger-host-slots` 先例）：区段哈希 + 复算脚本 + **如实边界清单**。每条读数对应日志里的一行；"未覆盖"的面一律进 §5，**不用推断冒充实测**。

> **★ 2026-10-08 取代说明（本包正文一律保留原样，MUST NOT 改写历史读数）**：本包的读数**对应 `bb0e05d`（及此前 `579b6e4`）那个提交**，属当时真实状态。其后 R6 Task 3 提案 `add-cast-run-and-gates` 按用户裁定**删除了技能侧 `AttrCapture` 整套**（`FTcsCastAttrCapture` / `ETcsCastAttrCaptureFrom` 类型 + `FTcsSkillDefData.AttrCaptureList` 字段），**因此：**
> - §5 边界第 1 条里列举的 `AttrCaptureList` **已不存在**（该字段不再参与"能存能取能往返"的读数）；
> - **本包不重跑、不追溯改写**——`AttrCaptureList` 的相关读数在其生效时点是**正确且已取证的**；取代关系按"supersede 取代删除"纪律**就地留痕**（本块即留痕），而非删改原文；
> - **未受影响的面**：模块物化、发现路径、`DefTag` 解析、主资产身份、其余字段保真、六类作者期校验——这些**与 AttrCapture 无关，结论继续有效**。
> - 取代判据与三条删除理由见 `PLN-R6` 第十九轮 + 提案 `add-cast-run-and-gates` 的 What Changes 第 3 条。

---

## 1. 读数（逐条对应日志一行）

### 1.1 `Tcs.Test.SkillDef.Prepare`（非 PIE；创作真资产）

| # | 判据 | 读数 | 日志行 |
|---|---|---|---|
| P1 | 新建真资产作者期校验为 `Valid` | **PASS** | `:2457` |
| P2 | 真技能资产已保存 | **PASS** | `:2460` |

行 `:2456` 的 `Failed to find object 'TcsSkillDef /Game/TcsDev/E2E/DA_SkillDef_E2E...'` 是**预期内的"资产尚不存在"探测结果**——`Prepare` 的幂等设计正是先 `LoadObject` 探测、失败则创建（这不是缺陷；见 §5 边界 4）。`:2458-2459` 的 `LogSavePackage: Moving output files` 是落盘实证。

### 1.2 `Tcs.Test.SkillDef.Run`（PIE 内；17 项）

| # | 判据 | 读数 |
|---|---|---|
| D1 | 定义库已就绪 | PASS |
| D2 | 失败清单为空 | PASS |
| D3 | 技能定义发现计数 = 1 | PASS |
| D4 | 验收身份词 `SkillDef.Check.SkillLayer` 已声明 | PASS |
| D5 | 技能定义按 `DefTag` 可解析 | PASS |
| D6 | 真技能资产按路径可加载 | PASS |
| D7 | 资产 `DefTag` 与验收身份一致 | PASS |
| D8 | 主资产身份 = `[PrimaryAssetType(TcsSkillDef), DefTag.GetTagName()]` | PASS |
| D9 | 继承字段就位（`StatusTag` 有效 + `Params` 字面量 = 17） | PASS |
| D10 | 布尔开关表就位 | PASS |
| D11 | 瞬发以空时段表表达 | PASS |
| D12 | 查询档与实例化保持默认档 | PASS |
| D13 | 主链身份有效且启动时点 = 施法开始 | PASS |
| D14 | 描述条目就位 | PASS |
| D15 | 空身份查询返回 `nullptr`（正常未命中，不 ensure） | PASS |
| D16 | 合法真资产的作者期校验为 `Valid`（**不是 `NotValidated`**） | PASS |
| D17 | 表行往返保真，且 `RowName` ≠ `DefTag` | PASS |

**汇总行**（`:2619`）：`SUMMARY Run: Passed=17 Failed=0 SkillDefCount=1 失败清单=0`

### 1.3 `Tcs.Test.SkillDef.Reject`（非 PIE；11 项）

| # | 注入的缺陷 | 读数 |
|---|---|---|
| R0 | 基线合法资产 → **判 `Valid`**（六类注入的对照面） | PASS |
| R1a | `Params` 键重复 | PASS |
| R1b | `BoolSwitches` 键重复 | PASS |
| R2 | `ModifierRows` 空引用 | PASS |
| R3 | `Phases` 内 `Duration` 来源为空 | PASS |
| R4a | `Triggers` 内 `EventTag` 无效 | PASS |
| R4b | `Triggers` 内 `EffectChainId` 无效 | PASS |
| R5 | `CastChainId` 无效 | PASS |
| R6a | 描述条目缺 `DescriptionId` | PASS |
| R6b | 描述条目缺 `TextKey` | PASS |
| R7 | **反向对照**：空时段表**不**构成错误（瞬发合法） | PASS |

**汇总行**（`:2650`）：`SUMMARY Reject: Passed=11 Failed=0 案例数=11`

**R0 与 R7 是本清单里最有价值的两条**：只有它们能证明"`Invalid` 是**被注入的那一条规则**拦下的"——没有 R0，基线自己先 `Invalid` 就没有对照面；没有 R7，"拦下一切"的假实现也能让 R1a~R6b 全绿。

---

## 2. ★ 本轮最强的一条证据：发现是**真在读内容目录**

同一进程内两次 PIE 会话的第五计数：

| PIE 会话 | `发现技能定义资产 N 个` | 就绪日志 | 日志行 |
|---|---|---|---|
| 第 1 次（`Prepare` **之前**） | **0 个** | `…修正器模板 1 条，技能定义 0 条，失败 0 条` | `:2346` / `:2347` |
| 第 2 次（`Prepare` **之后**，重启 PIE） | **1 个** | `…修正器模板 1 条，技能定义 1 条，失败 0 条` | `:2515` / `:2516` |

**为什么这条强**：`0 → 1` 的**唯一变量是磁盘上多了一个 `.uasset`**（mtime `2026/10/7 13:43:06`，落在两次会话之间：会话 1 在 `05:42:14`、`Prepare` 在 `05:43:06`、会话 2 在 `05:46:31`）。若是硬编码、若发现路径没真跑、若缓存跨会话复用，这个数**不会**随文件出现而变。⇒ 它同时证明了 ① 按类发现路径可达；② `.uasset` 真被扫到；③ 定义库在 GameInstance 初始化时发现（故新资产须下一次 PIE 才可见，与 `Prepare` 的提示语一致）。

**并且它解释了"为什么必须重启 PIE"**：这不是装置的缺陷，而是**定义库的既定生命周期**——`DiscoverSkillDefs` 在 `Initialize` 跑一次。

---

## 3. 环境与产物身份

| 项 | 值 |
|---|---|
| 日志 | `Saved/Logs/LegendAutoChess.log`（快照 `Saved/Task1/snapshot.log`） |
| 日志 SHA256 | `85C39BDD26E0167FC8959045E68AEB911746178FAF5B10C69B436CAD524494EB` |
| 日志规模 | 356,780 B / 2650 行（`split('\n')` 口径 2651） |
| 日志编码 | UTF-8 **带 BOM**、**CRLF**（引擎自身写法；**判据读数取自该文件本体**，非控制台回显） |
| 双配置构建 | Editor Development + Game Shipping **均 `Result: Succeeded`，0 warning / 0 error** |
| 验收资产 | `Content/TcsDev/E2E/DA_SkillDef_E2E.uasset`，3,790 B，mtime `2026/10/7 13:43:06` |

**产物身份（尺寸 + mtime；全部早于 PIE 运行时间 `13:42–13:47` ⇒ "产物晚于源码、读数对应本次构建"这条证据属性成立）**：

| 产物 | 尺寸 (B) | mtime |
|---|---|---|
| `Plugins/.../Binaries/Win64/UnrealEditor-TcsSkill.dll`（**新模块**） | 119,296 | `2026/10/7 8:48:23` |
| `Plugins/.../Binaries/Win64/UnrealEditor-TcsIntegration.dll`（含第五条发现路径） | 236,032 | `2026/10/7 8:48:27` |
| `Binaries/Win64/UnrealEditor-TcsDev.dll`（**宿主装置**） | 510,464 | `2026/10/7 8:51:55` |
| `Binaries/Win64/LegendAutoChess-Win64-Shipping.exe` | 169,667,584 | `2026/10/7 8:52:19` |

> `UnrealEditor-TcsSkill.dll` 存在本身即"模块真被链接进编辑器"的产物级证据（对照 `:1707` / `:1772` 的加载日志）。

**模块加载实证**：`:1707` `InternalLoadLibrary: 'TcsSkill'`、`:1772` `Successfully loaded plugin: 'TcsSkill'`。

---

## 4. 区段哈希（复算脚本见 §4.1）

口径：**`split('\n')` 后按 1-based 闭区间取行、以 `\n` 连接、UTF-8 编码、不含尾换行**。

| 区段 | 行范围 | SHA256 |
|---|---|---|
| Prepare | 2454–2460 | `918108fbe531c5d4da17528591f2df8e08a83809eed273362ac90d5e68b652f0` |
| Run | 2601–2619 | `1b37ae60a9fab4e6228bb44931d96a832e74d974348a0f14753b04593e9a8468` |
| Reject | 2638–2650 | `b41518c8f3a7f5bb7c4f3576b4e50bac1fb0235d1f27619ad9e9df533d0dabbf` |
| 就绪段（会话 1，0 条） | 2346–2347 | `7bbf971dbfc62cf88075386a8e0e950286c4e943babc24ff71589b1b1bfa4038` |
| 就绪段（会话 2，1 条） | 2515–2516 | `5f26a50b609a4e622a59f17ca1640fb1d587fe92b9244f54e27a64222caed21e` |

### 4.1 复算脚本

```python
import hashlib, io
path = r"...\Saved\Logs\LegendAutoChess.log"
with io.open(path, 'r', encoding='utf-8-sig', newline='') as f:
    lines = f.read().split('\n')
def seg_hash(a, b):                     # 1-based 闭区间
    return hashlib.sha256('\n'.join(lines[a-1:b]).encode('utf-8')).hexdigest()
print(seg_hash(2601, 2619))             # Run 段 → 1b37ae60…
```

> **⚠ 行号复现纪律**：重启 PIE / 重跑命令后**行号会漂移**。定位 MUST 用**内容正则**（如 `SUMMARY Run: Passed=`），**MUST NOT** 用行号算术偏移。本表的行号只对上述 SHA256 的这份日志快照有效。

---

## 5. 边界（如实登记；MUST NOT 读成"已全覆盖"）

1. **技能运行态零验证**：本轮**只证定义形状**（可配 / 可发现 / 可解析 / 可校验）。**激活、时段推进、参数链、主链起链全部归后续 Task**——`FTcsSkillDefData` 里 `Phases` / ~~`AttrCaptureList`~~（**2026-10-08 已删除**，见文首取代说明）/ `MainChainStart` 等字段**只验了"能存能取能往返"**，**无一被真正消费**。`CastChainId` 的 D13 也只验"身份有效"，**没有起过链**。
2. **`GateCheck` 求值器不存在（R6.5-e）**：`BoolSwitches` 今天的消费者只有"账本读取面（Task 2）"与"作者期校验"。本轮 **D10 验的是字段形状**，不是门控行为。
3. **`ParamChainRows` 按计划未声明**（Task 4 交付）：`UPROPERTY TArray<T>` 要求元素类型完整，而 `FTcsNumericParamModifier` 本轮不存在 ⇒ **本表不含该字段的任何读数**，这是**计划内的缺项**而非遗漏。
4. ~~**`Prepare` 的幂等路径只走了"创建"半**：`:2456` 证明"不存在则创建"生效；"**已存在则只核对、不覆写**"那半**本轮未触发**（第二次运行 `Prepare` 才能验）。⇒ 该半边的行为**零实测**。~~ → **✅ 已闭合（2026-10-08，跨轮补验）**：用户在 R6 Task 2 取证时**跑了两次 `Prepare`**，第二次两个资产都走到幂等半边——见 `EVID-2026-10-08-skill-registry` §9.4（`真资产已存在——只核对，未覆写`）。**本条边界就此关闭**（原文保留，不删——留痕纪律）。
   - **连带一处判据订正（2026-10-08）**：本包的 **`D3` 判据原为 `SkillDefCount == 1`**，加第二个验收资产后**假失败**——那是"当时只有 1 个资产"的**巧合**，而规格明确允许**多个**技能定义资产。**已改为 `>= 1`**（见 Task 2 证据 §9.1）。⇒ 本包 §1.2 的 `D3` 读数"发现计数 = 1"**是当时的历史读数，保持原样**；它与"判据已放宽"不冲突。
5. **`Reject` 只覆盖 6 类 / 11 例**：`IsDataValid` 的规则面就是计划审定的六类；**未做穷举注入**（例如"多项同时出错时是否全部报出"未验）。
6. **单次运行，无二次复现**：三条命令各跑 **1 次**（非像 Task 0 那样的"两轮逐字比对"）。⇒ 本轮的"可复现"只有**一次运行的内部一致性**（`Reject` 的 11 例互不干扰地各自判对），**没有**独立第二次运行的对照。**装置本身可重跑**（三条命令都是幂等/瞬态，无 `RegisterStepExecutor` 那类不可撤销登记），故需要时可直接再跑一轮。
7. **`IsEntityReady`（Q-11）尚未落地**：归 Task 2，本表零涉及。
8. **跨世界面零涉及**：技能定义**不参与世界装配**（`OnPostWorldInitialization` 未为它建每世界结构），故 `WAIT-11` 那类跨世界缺口**在本 Task 上不存在**。

---

## 6. 本轮顺带订正的一处机制表述

`implementation-log` 与 `decisions-log` 原写 "`ensure` 的同一调用点**每会话**只打印一次"。**订正为"每进程 / 每调用点"**：机制是 `::bGEnsureHasExecuted<FileLineHashForEnsure(__FILE__, __LINE__)>`（`AssertionMacros.h:403-404`，按"文件名 djb2 高 32 位 + 行号低 32 位"散列），与全局 `GEnsureResetState` 比较（`AssertionMacros.cpp:946-961`），**只有 `core.ResetEnsureState` 能重置**。⇒ **`StopPIE` → `StartPIE` 不重置**。（引擎源码注释用的词是 "session"，但那是措辞，不是生命周期边界。）
