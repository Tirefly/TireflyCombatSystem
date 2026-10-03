# TCS 设计文档术语表与编号注册表

- **文档 ID**：`GLOSSARY`
- **类型**：治理（术语与编号权威表）
- **状态**：生效中（2026-09-29 建立）
- **权威范围**：本文是 TCS 设计文档里**全部缩写、编号族与代号**的唯一权威表，同时是**文档 ID → 当前路径**的唯一映射表。规格正文不在本文（见 `INDEX.md`）。
- **最后更新**：2026-09-30

> **怎么用**：读到不认识的三五个字母就先来这里查。查法有两种——① 按首字母看 §2 的族表；② 直接 Ctrl+F 搜编号（如 `D5-17`、`STAT-2`）。

---

## 1. 先读这一节：三个最容易混的地方

### 1.1 `R` 一个字母，三种意思（**本仓库最大的记号陷阱**）

| 写法 | 含义 | 权威定义处 | 个数 | 例子 |
|---|---|---|---|---|
| `R0`–`R8`（**无连字符**） | **实施轮次**（R=Round，"一轮 ≈ 一个模块的一次拍板→文档循环"） | `PLN-R4` 的《轮次路线图》§`R4–R8`（现行）；`DEC-2026-09-02-module-map`（原始定义，已存档） | 9 | `R3` 竖切、`R4` 触发行轮、`R5` 状态层轮 |
| `R-1`–`R-6`（**连字符 + 1 位数字**） | **反射未解决项**（反射可达性专项待办） | `LEDGER-reflection`（`reflection-backlog.md`）的《未解决项总表》 | 6 | `R-1` 参数源族宿主插槽 |
| `DAMAGE-1` / `STAT-2` / …（**两段主题词 + 序号**，2026-09-29 起） | **跨轮遗留输入条目**（台账条目；共 11 个主题词族：`CORE`/`DAMAGE`/`INTEG`/`PRES`/`SCRIPT`/`STAT`/`TOOLS`/`WAIT`…） | `LEDGER-deferred`（`deferred-inputs-ledger.md`） | 37 | `STAT-2` = 参数源可枚举能力（原 `R4-1`） |

> **旧写法已废**：台账条目曾写 `R4-1`、`R5-4`、`R8-6` 等"轮号-序号"形态，**与轮次 `R4`/`R5` 字面撞车**（实例：`R5` 在台账里指 M4a 触发行轮，在路线图里指 M3 状态层轮）。2026-09-29 全部改为主题 ID，映射表见 §5。

> **`R4.5` 是唯一带小数点的 R 编号**（2026-09-30 登记）：它**不是正式轮**，而是 R4 与 R5 之间的一批「脚本通道收口」工作（立此编号时即明文"不占正式轮号"）——权威定义处 = `PLN-R4` 的《R4.5 批次表》。看到 `R4.5` 按「**半轮批次**」读，不要当成路线图里的第 4.5 轮。

### 1.2 `M` 也有两套含义

- `M0`–`M9` = **模块轮次 / 模块号**：M0 内核 → M1 记法 → M2 属性 → M3 状态 → M4 效果 → M5 技能 → M6 集成 → M7 表现 → M8 编辑器与工具 → M9 收尾轮。权威表 = `DEC-00-constitution`（`2026-09-02-r0-rebuild-position-paper.md`）**§9 模块物化规定**（全库新文档引它取"编译层模块名"）。
- **但 `D` 编号里的数字不等于模块号**：`D0/D2/D3/D4/D5/D6` 恰好与 M0/M2/M3/M4/M5/M6 同号是巧合，**从 `D7` 起断裂**——`D7-*` 是 **TcsDamage 模块轮**的决策，而 `M7` 是**表现层（TcsCue）**。

### 1.3 编号的连字符有意义

`R5`（轮次）≠ `R-5`（反射项）≠ 旧 `R5-4`（台账条目）；`S-8` 是编码，而 `S8` / `S1` 在全库**不作为编码存在**（历史上有过脱字符误写，见 `LEDGER-reflection` 里你的原话"R2 与 R1 可以紧邻"——那里指的是反射项 `R-1`/`R-2`）。**书写纪律：所有编号 MUST 带连字符，禁止脱字符写法。**

---

## 2. 编号族总表（十五族）

| 族 | 写法 | 含义 | 权威定义处 | 规模 |
|---|---|---|---|---|
| 裁决 | `裁决1` / `裁决2a` / `裁决2b` / `裁决3` | 2026-08-31 轮次的四项早期裁决（**编号继承，不重议**） | `INDEX` 前身 README 的《已定》表（现行落点见 §4.12） | 4 |
| 模块决策 | `MD-1`–`MD-3` | 模块地图 / 实施顺序 / TCS 旧仓处置 | 仅存在于 README《拍板记录》，正文提案用无 ID 的 1/2/3/4 | 3 |
| 网络需求 | `NET-1` / `NET-2` | 网络姿态**需求**（非决策）：NET-1 本作不联机但插件须可扩展；NET-2 千人战场服务器权威 | README《拍板记录》2026-09-02 条 | 2 |
| 核心决策 | `D0-1`–`D0-6` | M0 内核（句柄/池、载荷、线程、时钟、诊断） | `LOG-00-core`（`2026-09-02-m0min-m2-decision-points.md`） | 6 |
| 属性决策 | `D2-1`–`D2-16`（无 `D2-14`…见下） | M2 属性/数值（身份、Modifier、依赖、clamp、事务、加带、Operand、载体…） | `LOG-00-core`；`D2-14`/`D2-15` 在 `DEC-03-attribute-set` | 14 |
| 状态决策 | `D3-1`–`D3-19`（**无 `D3-8`/`D3-9`**） | M3 状态/Buff（实例存储、就绪、关系表、堆叠、到期、扩展点、物化…） | `LOG-01-states`（`2026-09-02-m3-states-decision-points.md`） | 17 |
| 效果决策 | `D4-1`–`D4-5`、`D4-14`–`D4-17`（**无 `D4-6`–`D4-13`**） | M4 效果执行（触发行、事件分类、原语集、目标选择、条件；注册制分派、模块成立、原语移除、语言无关执行器） | `LOG-02-effects`；**`D4-5` 定义在 `LOG-03-skill` §定案总表首行** | 9 |
| 技能决策 | `D5-1`–`D5-19`（多版本：`D5-5` v2/v3、`D5-17` v2/v3、`D5-18` v2/v3） | M5 技能/施法（查询契约、账本、冷却、Cost、快照、形态组、参数链、描述绑定、值约定…） | `LOG-03-skill`；v2/v3 亦见于 `DEC-02-fold-display` | 19 |
| 集成决策 | `D6-1`–`D6-5`（`D6-3` 有 v3） | M6 集成（StateTree 形态、两级单位、引导时序、复制契约、tick 接线） | `LOG-04-integration` | 5 |
| **伤害流程决策** | `D7-1`–`D7-7` | **TcsDamage 瞬时流程层**（独立模块、原语语义、流程属性、消耗型修正器、流程模板化、修改器唯一通道、四粒度让渡） | `LOG-02-effects` §D7 增补（**注意：数字 7 ≠ 模块 M7**） | 7 |
| 参数值来源 | `PV-0`–`PV-10` | Parameter 值来源策略体系（载体/内置源/属性源/等级表/逃生口/消费面/伤害值来源/校验/链行来源/可枚举） | `DEC-01-pv`（`2026-09-10-param-value-source-decision-points.md`） | 11 |
| 脚本缺口 | `G-1`–`G-4` | C# 脚本化缺口（G-1 上下文反射化、G-2 注册表反射入口、G-3 原语缺口、G-4 策略虚分派） | `RSCH-scripting-ustruct` §7.6；`RSCH-csharp-authoring` §7 | 4 |
| 脚本化条目 | `SCRIPT-1`–`SCRIPT-8`（旧 `S-1`–`S-8`） | 脚本化工作流条目（门面反射化 → 宿主脚本插槽） | `LEDGER-deferred` §脚本化工作流 | 8 |
| 跨轮遗留 | `CORE-n` / `DAMAGE-n` / `INTEG-n` / `PRES-n` / `SCRIPT-n` / `STAT-n` / `TOOLS-n` / `WAIT-n`（**实测 8 个前缀族**，2026-09-29 核） | 台账条目（见 §5） | `LEDGER-deferred` | 37 |
| 触发条件型 | `WAIT-1`–`WAIT-10`（旧 `T-1`–`T-10`） | 不绑轮次、等真实消费者的条目 | `LEDGER-deferred` §触发条件型 | 10 |
| 反射未解决项 | `R-1`–`R-6` | 反射可达性专项待办 | `LEDGER-reflection` | 6 |

> **断档说明（不是遗漏）**：`D1-*` 不存在（M1 无独立决策轮）；`D3-8`/`D3-9`、`D4-6`–`D4-13` 未使用（编号跳段）。

---

## 3. 速查：最常被引用的编号

| 编号 | 一句话含义 | 详情 |
|---|---|---|
| `D0-1` | 确定性 = 轻量纪律 + 网络姿态（服务器权威/headless/观感预测） | `LOG-00-core` |
| `D0-2` | 句柄 = 索引 + 代际的强类型句柄（净新增） | `LOG-00-core` |
| `D0-6` | 诊断日志归 UE 原生分类，TcsCore 零日志设施 | `LOG-00-core` |
| `D2-1` | 属性身份 = `FGameplayTag`（**2026-09-22 有据重开**，取代原 `FName` 词表） | `LOG-00-core` + 提案 `switch-identifiers-to-gameplay-tags` |
| `D2-9` | 热路径零回查 Def（实例持快照/缓存） | `LOG-00-core` |
| `D2-15` | AttributeSet 形态：`UTcsAttributeSet` + 实体侧引用 + diff 替换 | `DEC-03-attribute-set` |
| `D3-1` | 状态实例住中央注册表（per-unit 句柄桶） | `LOG-01-states` |
| `D3-7` | 生命周期扩展点 = 三件套 + Custom 逃逸位；**v3 = 策略载体切 `FInstancedStruct`** | `LOG-01-states` §D3-7 v3 |
| `D3-19` | 修正器模板：独立资产 + 引用行纯引用 + 覆写唯一通道=参数传值 | `LOG-01-states` |
| `D4-1` | 触发行 **10 字段**（v2，原提案为 12） | `LOG-02-effects` |
| `D4-14` | **注册制分派 + 依赖层级反转**（`Core←Attribute←Effect←{Damage,Targeting,State}←Skill`） | `LOG-02-effects` |
| `D4-16` | 原语集收敛为 **15**（SpawnProjectile/SpawnArea 移除） | `LOG-02-effects` |
| `D4-17` | 语言无关执行器：不内嵌脚本引擎，注册双入口 | `LOG-02-effects` |
| `D5-5` | 参数修正 = 任意键 + 修正链；**v3 = 带式聚合，与 M2 完全同式、顺序无关** | `LOG-03-skill` / `DEC-02-fold-display` |
| `D5-17` | 描述文本与数值绑定；**v3 = 视图策略化（`FTcsParamView`），StringTable 回归纯文案** | `DEC-02-fold-display` |
| `D5-18` | `ValueConvention` 值约定（策划书写值 ↔ 规范值） | `DEC-02-fold-display` |
| `D6-3` | 双层引导时序（GameInstance DefLibrary + World 驱动器）；v3 终定 | `LOG-04-integration` |
| `D7-5` | **流程模板化**：TcsDamage = 机制层，标准十阶段 = 步骤库 + 官方默认模板 | `LOG-02-effects` |
| `D7-6` | **伤害修改器 = 触发行 + `ModifyFlow`（唯一通道）** | `LOG-02-effects` |
| `PV-1` | 参数值载体 `FTcsParamValue{ FInstancedStruct Source }`（2026-09-24 换裸形态） | `DEC-01-pv` |
| `PV-7` | 伤害基础值 = 参数账本解算输入，**流程零计算** | `DEC-01-pv` |
| `R-1` | 参数源族宿主插槽（虚分派脚本不可达） | `LEDGER-reflection` |
| `SCRIPT-8`（旧 `S-8`） | **宿主脚本插槽**（UObject 接口 + `BlueprintNativeEvent`），✅ 已落地 | `LEDGER-deferred` |

---

## 4. 逐族注册表

> 表中「落点」= 该编号的正式规格归属（`SPEC-xx` 文档）；**空 = 悬空**（设计有决策、规格无归属，属待修项）。

### 4.1 `D0-*` 核心（6 项）— 定义 `LOG-00-core`

| 编号 | 含义 | 落点 |
|---|---|---|
| `D0-1` | 确定性纪律（轻量纪律 + 网络姿态四条） | `SPEC-00-core` §4 |
| `D0-2` | 句柄形态 = 索引+代际强类型句柄 | `SPEC-00-core` §2.1 |
| `D0-3` | 事件载荷类型安全 = 具体 FStruct + `FInstancedStruct` 扩展位 | `SPEC-00-core` §2.2 |
| `D0-4` | 池与线程模型 = 单游戏线程 + 断言 | `SPEC-00-core` §2.1 |
| `D0-5` | 时钟泵 = 单一 TickableWorldSubsystem + 可注入时间源 | `SPEC-00-core` §2.3 |
| `D0-6` | 诊断：日志归 UE 原生分类；**TcsCore 零日志设施**；屏显归测试装置 | `SPEC-00-core` §2.4 |

### 4.2 `D2-*` 属性/数值（14 项）— 定义 `LOG-00-core`（`D2-14/15` 在 `DEC-03-attribute-set`）

| 编号 | 含义 | 落点 |
|---|---|---|
| `D2-1` | 属性身份载体（**现为 `FGameplayTag`**，2026-09-22 修订） | `SPEC-01-attributes` §2.1 |
| `D2-2` | Modifier 生命周期 = 来源句柄注销 → 级联移除 | `SPEC-01-attributes` §2.2 |
| `D2-3` | 属性间依赖 = 读即登记 + 环检测 | `SPEC-01-attributes` §3 |
| `D2-4` | clamp 位置 = 管线末端统一，边界三态 | `SPEC-01-attributes` §3 |
| `D2-5` | 事务与 flush = 单次管线事务 + 提交尾行内 flush | `SPEC-01-attributes` §4 |
| `D2-6`–`D2-9` | 六至九项（Operand 收敛/实例化禁令/热路径零回查等） | `SPEC-01-attributes` |
| `D2-10` | `FlatAdd` 加带（乘后平坦加，带权 30） | `SPEC-01-attributes` §2.2 |
| `D2-11` | `AttributeModOperand`：主属性→派生属性载体 | `SPEC-01-attributes` §2.2 |
| `D2-12` | 统一数值载体（**载体已被 PV 系列 `FTcsParamValue` 取代**） | `SPEC-00-core` §2.1（现载体） |
| `D2-13` | Operand 双形状（定义侧可吃参数键 / 运行侧已解析） | `SPEC-01-attributes` §2.2 |
| `D2-14` | 属性存在性：框架允许动态增删，推荐用法=结构；不做溯源 | `SPEC-01-attributes` §2.2a |
| `D2-15` | AttributeSet 形态（B1a+B2+B3a） | `SPEC-01-attributes` §2.2a / `SPEC-05-integration` |
| `D2-16` | 被否方案与理由表（留痕，避免后议重来） | `SPEC-01-attributes` §7 |

### 4.3 `D3-*` 状态/Buff（17 项）— 定义 `LOG-01-states`

| 编号 | 含义 | 落点 |
|---|---|---|
| `D3-1` | 状态实例存储 = 中央注册表 | `SPEC-02-states` §3.1 |
| `D3-2` | 定义就绪语义 = 显式状态机（Ready 须全量成功） | `SPEC-02-states` §3.7 |
| `D3-3` | 关系表执行时机 = apply 拒绝 + 移除级联重评 | `SPEC-02-states` §3.4 |
| `D3-4` | 堆叠/刷新 = 五轴 `FStateStackPolicy` + Custom 逃逸位 | `SPEC-02-states` §3.2 |
| `D3-5` | buff 行为载体 = 原语链（弃每实例内嵌 StateTree） | `SPEC-02-states` §5 |
| `D3-6` | 到期驱动 = 集中到期最小堆 | `SPEC-02-states` §3.3 |
| `D3-7` | 生命周期扩展点三件套 + Custom 逃逸位；**v3 = `FInstancedStruct` 载体** | `SPEC-02-states` §3.5 |
| `D3-10` | Def 类层级终定（StateDef/BuffDef/SkillDef） | `SPEC-02-states` §2 |
| `D3-11` | Level 终定（默认参数 LevelBase/MaxLevel；增长归项目） | `SPEC-02-states` §3.5 |
| `D3-12` | 双维度快照（参数 Mode 列 + 属性读默认 Live、`AttrCapture` 捕获后读快照） | `SPEC-02-states` §3.6 |
| `D3-13` | `EDurationPolicy{Finite,Infinite}` + `PeriodRefresh{Keep/Reset/Immediate}` | `SPEC-02-states` §3.3 |
| `D3-14` | Overflow 用事件组合表达，不内建 | `SPEC-02-states` |
| `D3-15` | 生命周期操作（`ExtendDuration`/`SetRemaining`；`MaxStacks` 运行期只读） | `SPEC-02-states` |
| `D3-16` | 关系表/槽位表组织（并存、DataTable 行、解析栈、引用走 tag） | `SPEC-02-states` §3.4 |
| `D3-17` | 堆叠轴命名与分组枚举 | `SPEC-02-states` §3.2 |
| `D3-18` | 实例-定义引用规范（权威 = 身份 tag；`GetDef()` 缓存；运行期零回查） | `SPEC-02-states` §3.1 |
| `D3-19` | 修正器模板与引用行（独立资产 + `ModifierRows` 纯引用 + 参数传值覆写） | `SPEC-02-states` §3 |

### 4.4 `D4-*` 效果执行（9 项）— 定义 `LOG-02-effects`（`D4-5` 在 `LOG-03-skill`）

| 编号 | 含义 | 落点 |
|---|---|---|
| `D4-1` | 触发行 **10 字段**（v2；含 `Source` 语义、条件、Cues 引用制） | `SPEC-03-effects` §2.2 |
| `D4-2` | **事件分类默认表**（通道由事件类型定，不给策划选） | `SPEC-10-presentation`（消费处：§3 派发通道 + §5 依据行）——**2026-09-29 结案**：原记"悬空"系在 `SPEC-03-effects` 中查不到该编号所致，实为落点错记模块（消费方是表现层，非效果层） |
| `D4-3` | `FEffectStep` 原语集 = **15 原语** | `SPEC-03-effects` §2.1 |
| `D4-4` | 目标选择数据化；**v2 = 战斗步骤不内嵌 selector，消费 `Context.Targets`** | `SPEC-03-effects` §2.3 / `SPEC-06-targeting` |
| `D4-5` | 条件最小集（`HasAllTags`/`AttributeCompare`/`VariableCompare`/`GateCheck`/`Chance` + Custom） | `SPEC-03-effects` §2.2 |
| `D4-14` | 注册制分派 + 依赖层级反转 | `SPEC-03-effects` §1 |
| `D4-15` | TcsTargeting 模块成立；v2 = 策略模式 + `RadiusArea` 后置 | `SPEC-06-targeting` |
| `D4-16` | `SpawnProjectile`/`SpawnArea` 移除（原语 17→15） | `SPEC-03-effects` §2.1 |
| `D4-17` | 语言无关执行器（不内嵌脚本引擎；注册双入口） | `SPEC-03-effects` §2.1/§5b |

### 4.5 `D5-*` 技能/施法（19 项）— 定义 `LOG-03-skill` / `DEC-02-fold-display`

| 编号 | 含义 | 落点 |
|---|---|---|
| `D5-1` | 施法阶段 = 查询契约（`IsInterruptibleNow`/`CanMoveNow`）+ 可选时段表 | `SPEC-04-skill` §3.3 |
| `D5-2` | 账本全 struct 化（`FLearnedSkillEntry` + `FCastRun`） | `SPEC-04-skill` §3.1 |
| `D5-3` | 冷却策略 × 时机双枚举（**被 `D5-15` 泛化为多轨道**） | `SPEC-04-skill` §3.5 |
| `D5-4` | 打断不返还 Cost（返还是政策）；其余继承 | `SPEC-04-skill` |
| `D5-5` | 参数修正 = 任意键 + 修正链（封闭的是运算）；**v3 = 带式聚合** | `SPEC-04-skill` §3.4 |
| `D5-6` | `FEntrySelector{All/ByTag/ById/Custom}` | `SPEC-04-skill` §3.4 |
| `D5-7` | 起链解析 = 让渡点三实现 | `SPEC-04-skill` §3.4 |
| `D5-8` | 链参数注入（`Param()`；链启动时快照 + 可 opt-in 实时） | `SPEC-04-skill` §3.4 |
| `D5-9` | **技能级重定向 —— 2026-09-11 已移除**（改为"换成新 Skill"） | `SPEC-04-skill` §3.4（撤除声明） |
| `D5-10` | Cost 策略 × 时机终定 | `SPEC-04-skill` §3.6 |
| `D5-11` | `ECastInstancing{InstancePerExecution/InstancePerEntity}` | `SPEC-04-skill` §3.1 |
| `D5-12` | Skill 侧 `FCastRun.ParamSnapshot`；v2 = 双维度快照 | `SPEC-04-skill` §3.1 |
| `D5-13` | 实例-定义引用规范（Entry 权威 = Def tag；执行期零回查） | `SPEC-04-skill` §3.1 |
| `D5-14` | **技能形态组 —— 2026-09-11 已移除**（改为"多个 SkillDef 都学习"+ 组合范式） | `SPEC-04-skill`（撤除声明） |
| `D5-15` | 冷却多轨道泛化（`FCooldownPolicy{Tracks}`，真 GCD = 纯数据） | `SPEC-04-skill` §3.5 |
| `D5-16` | 冷却三事件 + `OnCostApplied` + `Adjust/ResetCooldown` | `SPEC-04-skill` §3.5 |
| `D5-17` | 描述文本与数值绑定；**v3 = 视图策略化** | `SPEC-07-notation` §2.2 |
| `D5-18` | `ValueConvention` 值约定；v3 = 约定可配白名单 | `SPEC-07-notation` §2.1 |
| `D5-19` | 技能侧修正器模板 + `CompeteGroup`（激活期挂载） | `SPEC-04-skill` §3.4 |

### 4.6 `D6-*` 集成（5 项）— 定义 `LOG-04-integration`

| 编号 | 含义 | 落点 |
|---|---|---|
| `D6-1` | StateTree 集成形态（只做决策/编排） | `SPEC-05-integration` |
| `D6-2` | 两级单位注册协议 | `SPEC-05-integration` |
| `D6-3` | 双层引导时序状态机（v3 终定） | `SPEC-05-integration` §2.1 |
| `D6-4` | 复制代理契约（**仅契约不实现**） | `SPEC-05-integration` §2.3 |
| `D6-5` | tick 接线 | `SPEC-05-integration` |

### 4.7 `D7-*` 伤害流程（7 项）— 定义 `LOG-02-effects` §D7 增补（**数字 7 ≠ M7**）

| 编号 | 含义 | 落点 |
|---|---|---|
| `D7-1` | TcsDamage 独立模块 + 瞬时流程骨架 | `SPEC-08-damage` §1 |
| `D7-2` | `Damage`/`Heal` 原语 = 流程发起器；公式归项目 delegate | `SPEC-08-damage` §2 |
| `D7-3` | 流程属性复用 M2 语义（键 + 修正链，作用域=流程） | `SPEC-08-damage` §2.1 |
| `D7-4` | 消耗型修正器 `{MaxUses/Cooldown/SortKey}`（原 `OnConsumed` 闭包 **2026-09-30 改事件语义**）；收集≠消费，成功才消费 | `SPEC-08-damage` §2.2 |
| `D7-5` | **流程模板化**（标准十阶段 = 步骤库 + 官方默认模板） | `SPEC-08-damage` §2.2 |
| `D7-6` | **伤害修改器 = 触发行 + `ModifyFlow`（唯一通道）** | `SPEC-08-damage` §2.3 |
| `D7-7` | `FFlowRedirect` 模板重定向（让渡模式升四粒度） | `SPEC-08-damage` §2.2 |

### 4.8 `PV-*` 参数值来源（11 项）— 定义 `DEC-01-pv`

| 编号 | 含义 | 落点 |
|---|---|---|
| `PV-0` | 前提：策略载体 = `FInstancedStruct`（`D3-7` v3） | `SPEC-00-core` §2.1 |
| `PV-1` | 载体与基类（`FTcsParamValue` + `FTcsParamValueSource::Evaluate`；2026-09-14 增补上下文） | `SPEC-00-core` §2.1 |
| `PV-2` | TcsCore 内置源最小集（`Literal`/`ParamRef`） | `SPEC-00-core` §2.1 |
| `PV-3` | TcsAttribute 属性值源（`AttributeScaled`，仅 Snapshot） | `SPEC-01-attributes` |
| `PV-4` | 等级表族（四源归 TcsState；`ITcsEntityLevelProvider`） | `SPEC-02-states` §3.5 |
| `PV-5` | Delegate/Formula 逃生口 | `SPEC-08-damage` §2 |
| `PV-6` | 消费面全量切换清单 | 各 `SPEC` |
| `PV-7` | 伤害值来源 = **参数账本解算**，流程零计算 | `SPEC-08-damage` §2 |
| `PV-8` | 四联校验矩阵（源 × 上下文 × 可配约定 × 视图兼容） | `SPEC-09-editor` §2 |
| `PV-9` | 参数链初始行来源机制（`ParamChainRows`） | `SPEC-04-skill` §3.4 |
| `PV-10` | 源的可枚举能力（`FTcsParamEnumerableSource`；索引解析唯一真相在源） | `SPEC-00-core` §2.1 |

### 4.9 `MD-*` / `NET-*` / `裁决N`

| 编号 | 含义 | 状态 |
|---|---|---|
| `MD-1` | 模块地图 M0–M8 认可（M4 拆 a/b/c） | 已拍板；**其地图已被 `R0 §9` 取代**（§9 = 现行模块表） |
| `MD-2` | 实施顺序 = C 叶先行 + 竖切验收 | 已拍板（现行顺序见 `PLN-R4` 路线图） |
| `MD-3` | TCS 旧仓冻结 + 两个悬空提案冻结，Fragment 思路迁 M3 重审 | 已拍板 |
| `NET-1` | LAC 本作不联机；插件须可扩展至网络同步 | 需求（非决策）；落点 = 各模块《网络姿态落点》节 |
| `NET-2` | 千人战场 = 联机、服务器权威、士兵层不做本地预测 | 同上 |
| `裁决1` | 状态约束 = Tag 关系表 `FUnitStatusRule{Status,Blocks,Requires,Priority}` | 已定不重议 → `SPEC-02-states` §3.4 |
| `裁决2a` | 事件 = 总线 + 订阅表 + 共享 Handler UClass；struct 不自绑 delegate | 已定不重议 → `SPEC-00-core` §2.2 |
| `裁决2b` | tick = TickableWorldSubsystem 泵 `ScaledDt` | 已定不重议 → `SPEC-00-core` §2.3 |
| `裁决3` | 可视化跟随数据形状（表/矩阵/管线图，不做通用图编辑器） | 已定不重议 → `SPEC-09-editor` §3 |

### 4.10 `G-*` 脚本缺口（4 项）— 定义 `RSCH-scripting-ustruct` §7.6

| 编号 | 含义 | 状态 |
|---|---|---|
| `G-1` | 上下文/运行态反射化（`FTcsEffectContext` 可做；`FTcsDamageFlowContext` 物理不可） | 降为可选（`SCRIPT-8` 插槽路线绕开） |
| `G-2` | 三张注册表的反射注册入口（`TFunction` 形参） | 部分被 `SCRIPT-8` 替代；余见 `R-2` |
| `G-3` | 效果链原语缺口（12/15） | 归 `DAMAGE-2` 等台账条目 |
| `G-4` | 4 个策略基类虚分派（C# 不可达） | 可选架构改造；**用户未拍板** |

### 4.11 `SCRIPT-*` 脚本化工作流（8 项，旧 `S-1`–`S-8`）— 定义 `LEDGER-deferred`

| 编号 | 旧号 | 含义 | 状态 |
|---|---|---|---|
| `SCRIPT-1` | `S-1` | 门面 API 反射化（A 类加宏 + B 类形参反射化） | ✅ 已消费（2026-09-24） |
| `SCRIPT-2` | `S-2` | 三张注册表的反射注册入口 | 待触发（定位降级，低于 `SCRIPT-8`） |
| `SCRIPT-3` | `S-3` | 上下文/运行态反射化 | 降为可选 |
| `SCRIPT-4` | `S-4` | `ITcsDamageFlowDelegate` 反射化 | ✅ 已消费（并入 `SCRIPT-8`） |
| `SCRIPT-5` | `S-5` | `ITcsEntityQuery` 反射化（脚本读世界） | **仍未落地** |
| `SCRIPT-6` | `S-6` | `TInstancedStruct<T>` 字段的 C# 导出缺口 | ✅ 已消费（换裸 `FInstancedStruct`） |
| `SCRIPT-7` | `S-7` | 脚本层正常竞态的红字噪音 | ✅ 已修（口径先例） |
| `SCRIPT-8` | `S-8` | **★ 宿主脚本插槽**（UObject 接口 + `BlueprintNativeEvent`） | ✅ 已消费（2026-09-27；GC 面见 `R-2`） |

### 4.12 `R-*` 反射未解决项（6 项）— 定义 `LEDGER-reflection`

| 编号 | 含义 | 状态 |
|---|---|---|
| `R-1` | 参数源族宿主插槽（`FTcsParamValueSource` 虚分派脚本不可达） | ✅ 已调研并拍板（2026-09-24），待落地 |
| `R-2` | 条件求值器/载荷读取器注册表反射入口 + **跨世界寿命缺陷** | ✅ 已调研并拍板（2026-09-27），待落地 |
| `R-3` | `ITcsEntityQuery` 反射化 | 待调研（= `SCRIPT-5`） |
| `R-4` | 上下文整体反射化 | 待调研（前置 = `R-6`） |
| `R-5` | `FTcsSourceHandle` 反射化 + `FindChain` 裸指针 | 待调研 |
| `R-6` | `OnConsumed` / 消耗语义形状 | **✅ 形状已落地（2026-09-30）**；消费动作待台账 `DAMAGE-4` |

---

## 5. 文档 ID 映射表（ID → 当前路径）与台账改名映射

### 5.1 文档 ID → 路径

> **2026-09-29 目录重构后**：文档按类型分入 8 个子目录（`spec/` `decisions/` `log/` `plans/` `research/` `evidence/` `ledger/` `HISTORICAL/`）。
> **本表是"ID → 当前路径"的唯一出处**——引用写 ID，文件移动只改本表。

| 文档 ID | 当前路径 | 类型 |
|---|---|---|
| `INDEX` | `INDEX.md` | 治理（入口） |
| `GLOSSARY` | `GLOSSARY.md` | 治理（术语与编号） |
| `CONVENTION` | `docs-convention.md` | 治理（文档规范） |
| `README` | `README.md` | 治理（导航 + 历史前言） |
| `SPEC-00-core` | `spec/01-module-m0-core.md` | 模块规格（TcsCore / M0） |
| `SPEC-01-attributes` | `spec/02-module-attributes.md` | 模块规格（TcsAttribute / M2） |
| `SPEC-02-states` | `spec/03-module-states.md` | 模块规格（TcsState / M3） |
| `SPEC-03-effects` | `spec/04-module-effects.md` | 模块规格（TcsEffect / M4） |
| `SPEC-04-skill` | `spec/05-module-skill.md` | 模块规格（TcsSkill / M5） |
| `SPEC-05-integration` | `spec/06-module-integration.md` | 模块规格（TcsIntegration / M6） |
| `SPEC-06-targeting` | `spec/10-module-targeting.md` | 模块规格（TcsTargeting） |
| `SPEC-07-notation` | `spec/11-module-notation.md` | 模块规格（TcsNotation） |
| `SPEC-08-damage` | `spec/09-module-damage.md` | 模块规格（TcsDamage） |
| `SPEC-09-editor` | `spec/08-module-editor-tooling.md` | 模块规格（TcsEditor / M8） |
| `SPEC-10-presentation` | `spec/07-module-presentation.md` | 模块规格（TcsCue / M7） |
| `SPEC-TRACE` | `spec/tcs-contract-traceability.md` | 模块规格（契约追踪矩阵） |
| `DEC-00-constitution` | `decisions/dec-00-constitution.md` | 决策记录（立场书 / 分离宪法 / 模块物化） |
| `DEC-2026-08-31-state-relation` | `decisions/dec-2026-08-31-state-relation.md` | 决策记录（裁决 1） |
| `DEC-2026-08-31-event-bus` | `decisions/dec-2026-08-31-event-bus.md` | 决策记录（裁决 2a） |
| `DEC-2026-08-31-tick-pump` | `decisions/dec-2026-08-31-tick-pump.md` | 决策记录（裁决 2b） |
| `DEC-2026-08-31-visualization` | `decisions/dec-2026-08-31-visualization.md` | 决策记录（裁决 3） |
| `DEC-2026-09-01-abilitykit` | `decisions/dec-2026-09-01-abilitykit.md` | 决策记录（对照综合与原语集提案 v1，**提案值已被取代**） |
| `DEC-01-pv` | `decisions/dec-01-pv-param-value-source.md` | 决策记录（PV 系列） |
| `DEC-02-fold-display` | `decisions/dec-02-fold-display.md` | 决策记录（参数折叠与展示） |
| `DEC-03-attribute-set` | `decisions/dec-03-attribute-set.md` | 决策记录（属性存在性与 AttributeSet） |
| `DEC-04-callback-carriers` | `decisions/dec-04-callback-carriers.md` | 决策记录（回调载体：`TFunction` 去留与替换边界；**决策已拍板 2026-09-29，实现部分落地**） |
| `LOG-00-core` | `log/log-00-core-m0m2.md` | 决策日志（M0-min + M2；`D0-*`/`D2-*`/`MD-*`/`NET-*`） |
| `LOG-01-states` | `log/log-01-states-m3.md` | 决策日志（M3） |
| `LOG-02-effects` | `log/log-02-effects-m4-d7.md` | 决策日志（M4 + `D7-*` 增补） |
| `LOG-03-skill` | `log/log-03-skill-m5.md` | 决策日志（M5 + `D4-5`） |
| `LOG-04-integration` | `log/log-04-integration-m6.md` | 决策日志（M6） |
| `LOG-99-misc` | `log/log-99-misc-mass-layout.md` | 决策日志（Mass 备忘，未拍板） |
| `LOG-DECISIONS` | `log/decisions-log.md` | 决策日志（历史裁决 + 模块物化 + 拍板记录流水） |
| `LOG-IMPLEMENTATION` | `log/implementation-log.md` | 实施记录（逐 Task 实施与验收 + 检查点状态） |
| `PLN-R3-vertical-slice` | `plans/plan-r3-vertical-slice.md` | 实施计划（R3 验收剧本） |
| `PLN-R3-1` | `plans/plan-r3-1-core-attributes.md` | 实施计划（R3 计划一） |
| `PLN-R3-2` | `plans/plan-r3-2-damage-chain.md` | 实施计划（R3 计划二） |
| `PLN-R4` | `plans/plan-r4-trigger-row.md` | 实施计划（R4 计划三，**含 R4–R8 路线图**） |
| `PLN-R3-content-guide` | `plans/plan-r3-content-guide.md` | 实施计划（竖切内容资产指南） |
| `RSCH-abilitykit-editor` | `research/abilitykit-extract-editor-determinism.md` | 调研（AbilityKit C 路） |
| `RSCH-abilitykit-pipeline` | `research/abilitykit-extract-skill-pipeline.md` | 调研（AbilityKit A 路） |
| `RSCH-abilitykit-trigger` | `research/abilitykit-extract-triggering-attributes.md` | 调研（AbilityKit B 路） |
| `RSCH-replication` | `research/replication-posture.md` | 调研（网络复制姿态） |
| `RSCH-scripting-ustruct` | `research/scripting-language-ustruct.md` | 调研（脚本方案选型，`G-*` 定义处） |
| `RSCH-csharp-authoring` | `research/csharp-authoring.md` | 调研（C# 编写 TCS 逻辑） |
| `RSCH-targeting-absorption` | `research/targeting-abilitykit-absorption.md` | 调研（TcsTargeting 演进：AbilityKit 目标查找吸收 + 形状族/指示器切片；**待落地**） |
| `RSCH-boundary-audit` | `research/boundary-audit.md` | 调研（框架本体边界审计：违规清单 + 32 项"不许删名单" + 真缺陷 R-1~R-3；**冻结**） |
| `EVID-2026-09-28-scripting-e2e` | `evidence/2026-09-28-host-scripting-e2e-pie.md` | 证据（宿主脚本插槽 E2E） |
| `EVID-2026-09-28-selector-ref` | `evidence/2026-09-28-selector-ref-pie.md` | 证据（selector `ref` 容器） |
| `EVID-2026-09-29-registry-lifetime` | `evidence/2026-09-29-registry-lifetime-pie.md` | 证据（注册表跨世界寿命：两连 PIE） |
| `EVID-2026-09-30-modifyflow-acceptance` | `evidence/2026-09-30-modifyflow-primitive-acceptance-pie.md` | 证据（`ModifyFlow` 提交侧验收：单次 PIE，注册可达 + 降级路径） |
| `EVID-2026-10-03-scripting-gc-survival` | `evidence/2026-10-03-scripting-gc-survival-probe-pie.md` | 证据（宿主脚本四槽位的 GC 存活观测：跑前预注册预测 + 负对照；**状态 PENDING**，实跑输出待回填） |
| `LEDGER-deferred` | `ledger/deferred-inputs-ledger.md` | 台账（跨轮遗留输入，活文档） |
| `LEDGER-reflection` | `ledger/reflection-backlog.md` | 台账（反射未解决项，活文档） |
| `LEDGER-terminology` | `ledger/reflection-terminology.md` | 台账（反射术语规约，活文档） |
| `DEC-2026-09-02-module-map` | `HISTORICAL/dec-2026-09-02-module-map-proposal.md` | **历史存档**（模块地图提案 v1） |

| 文档 ID | 当前路径 | 类型 |

### 5.2 台账条目改名映射（2026-09-29，旧 → 新）

命名规则：`<主题词>-<序号>`。主题词按"该条目真正归属的轮次/领域"取，**不再使用轮号**——这样才能在轮次顺序调整时保持稳定。

| 旧编号 | 新编号 | 事项 | 归属轮次（现行路线图） |
|---|---|---|---|
| `R5-1` | `DAMAGE-1` | Context 默认目标初始化 = 事件目标 | R4 |
| `R4-2` + `R5-2` | `DAMAGE-2`（**合并**，原为同一事项的两条登记） | TcsEffect 剩余 8 原语整套 | R4/R5 |
| `R5-3` | `DAMAGE-3` | TcsDamage 的 `Heal`/`ModifyFlow` 执行器 | R4/R5 |
| `R5-4` | `DAMAGE-4` | ★ 消耗语义落地（收集≠消费） | R5 |
| `R4-1` | `STAT-2` | `FTcsParamEnumerableSource` 落地 | R5（随等级源） |
| `R4-3` | `STAT-3` | `FFlowRedirect` 模板重定向栈 | R5 |
| （新） | `DAMAGE-5` | `FTcsParamEvaluateContext` 补 `Subject` + `EffectiveLevel` | R5（随等级源） |
| `R6-1` | `STAT-1` | 参数链接入共享折叠器 | R6 |
| `M0-1` | `CORE-1` | 原生侧层级匹配（父标签订阅） | R1 后续 |
| `R7-1` | `INTEG-1` | AttributeSet 全套 | R7 |
| `R7-2` | `INTEG-2` | AttributeSet 资产命名统一 | R7 |
| `R7-3` | `INTEG-3` | `PrimaryAssetTypesToScan` + AssetManager 发现 + 加载层三策略 | R7 |
| `R8-1` | `TOOLS-1` | PV-8 四联校验矩阵 | R8 |
| `R8-2` | `TOOLS-2` | 定义校验矩阵兜作者侧配置错误 | R8 |
| `R8-3` | `TOOLS-3` | Def 双轨同步器 + 词表装载 | R8 |
| `R8-4` | `TOOLS-4` | K2 强类型引脚节点 | R8 |
| `R8-5` | `TOOLS-5` | 总线动态层两条验证项 | R8 |
| `R8-6` | `TOOLS-6` | Explain 调试面板 + 钩子 | R8 |
| （新） | `PRES-1` | TcsCue 本体（此前无任何条目认领） | R8 |
| `S-1`…`S-8` | `SCRIPT-1`…`SCRIPT-8` | 脚本化工作流条目 | 见 §4.11 |
| `T-1`…`T-10` | `WAIT-1`…`WAIT-10` | 触发条件型条目 | 见 `LEDGER-deferred` |

> **历史留痕说明**：`LEDGER-deferred` 的《变更记录》中 **2026-09-29 之前**的条目保留当时的旧编号原文（改了就成了伪造记录）；编号对照以本表与该文档 2026-09-29 增补条为准。`openspec/changes/archive/` 下的归档提案同样保留旧编号，见 `CONVENTION` §7。

> **稳定性承诺**：主题 ID 只在"条目被删除/合并"时变化；轮次顺序调整、模块重排、路线图偏离都不再影响编号。

---

## 6. 已知编号问题（待修清单）

| # | 问题 | 处置 |
|---|---|---|
| 1 | `D4-2`（事件分类默认表）"无规格落点" | **✅ 已结案（2026-09-29）**：落点 = `SPEC-10-presentation`（原判"悬空"是把落点错记到 `SPEC-03-effects` 所致） |
| 2 | `D5-9`/`D5-14` 已移除，但 `SPEC-09-editor` 仍列为设计依据 | **✅ 已改（2026-09-29）**：依据行改指 `SPEC-04-skill` §3.4/§3.5（撤除声明）+ `DEC-01-pv` PV-2.d |
| 3 | `D2-12` 载体已被 PV 系列取代，正文仍称"拍板" | 加取代指针（`SPEC-07-notation`） |
| 4 | `MD-1` 的模块地图已被 `R0 §9` 取代，编号未废 | 保留编号作历史，标注"地图部分已被 §9 取代" |
| 5 | `NET-1/2` 落点不齐（`SPEC-09-editor`/`SPEC-10-presentation` 无《网络姿态落点》节，其余模块规格均有） | **本轮只修编号歧义**：原写作裸号 `SPEC-09`/`SPEC-10`，按本表会读成"伤害/表现"两个模块；已改回带主题词的 `SPEC-09-editor`/`SPEC-10-presentation`。**落点不齐本身仍待办**（视需要补节或显式声明"不涉及"） |
| 7 | **裸编号引用有歧义**（`SPEC-09`、`SPEC-10` 等不带主题词的写法） | **✅ 已修（2026-09-29）**：`NET-1/2` 行两处裸号已改带主题词；并新增纪律——**引用 `SPEC-*` MUST 写全「编号-主题词」**（如 `SPEC-09-editor`），禁止裸号（`SPEC-09` 会被读成"第 9 篇编号"，而 `SPEC-09` 的主题词是 `editor`、文件却是 `spec/09-module-damage.md`——三种口径互不相同，见下条） |
| 8 | **编号与文件名/篇序三轨错位**（本册 §2 的十五族总表与 `INDEX.md` §4 已按编号列，但读者仍易按文件序号理解） | **✅ 已在本文 §2 与 `GLOSSARY` §5.1 双表并列**（ID ↔ 路径）——`SPEC-09` 主题词 = `editor`（文件 `spec/08-…`）、`SPEC-10` 主题词 = `presentation`（文件 `spec/07-…`）、`SPEC-08` 主题词 = `damage`（文件 `spec/09-…`）。**不改编号**：ID 是稳定键，动它要同步 5 份 openspec 生效规格（见 `CONVENTION` §5）。 |
| 9 | **§2 表头计数与表体不符**（写"十二族"，实际 15 行） | **✅ 已修（2026-09-29）**：表头改"十五族"；`INDEX.md` §2 的交叉引用同步。**并修正台账族的清单**——原列 `TRIG-n`/`SKILL-n`/`TARGET-n` 三种前缀**在台账中不存在**（实测 8 个前缀族），已按实测改写 |
| 6 | 台账旧编号残留引用 | 批次 1 已全库替换；验证见重构报告 |
| 10 | **引用库外文档系列**（`TCS 报告 01–13 + ZZ`：2026-09 上旬对**旧 TCS 代码库**的勘察报告，含 orphan-scan / 诊断与时间语义 / 编辑器现状 等实证来源） | **✅ 已处置（2026-10-01）**：该系列在 LAC 工作区与 TCS git 历史中**均不存在**（已全盘搜索）。`SPEC`/`LEDGER` 内 10 处引用已改写为「`TCS 报告 NN`（库外，已不在库内）」的**追溯注记**，不再声称可复核。**若日后补齐入库，MUST 回头把注记改为真实路径 + 节锚点**（见 `CONVENTION` §6.1） |

---

## 7. 相关文档

- 入口：[INDEX.md](INDEX.md)
- 文档规范：[docs-convention.md](docs-convention.md)
- 现状勘察报告：`docs/2026-09-29-tcs-docs-recon.md`（仓库根 `docs/` 目录）