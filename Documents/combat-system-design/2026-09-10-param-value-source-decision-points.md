# Parameter 值来源策略体系决策点（PV 系列）

- 日期：2026-09-10（方向）/ **2026-09-11（细节全部拍板，本文档为收束版）**
- 状态：**已全部拍板**——待折入模块文档与计划（PV-8 清单）
- 背景输入：技能配置走查（"攻击力×[5/7.5/10/13.5/18] 随技能等级"）暴露三处口径不足——数值配置下放宿主 delegate 过度、FTcsParamScalar 二值载体表达力不足、等级表无处安放；用户提出旧 TCS Parameter 策略模式（Constant/StateLevelArray/InstigatorLevelArray/Formula）应升格为通用 TCS 基建，且模块化归属 = TcsCore 提供基础、扩展模块各自实现。
- 涉及既有裁决：D2-12（FTcsParamScalar 统一载体）→ **被取代**；D3-11（等级→数值表不进引擎）→ **范围修订**；D7-2/D7-5（插件零公式/流程宿主化）→ **范围收窄**；D2-11 拒绝清单 **不波及**（该清单管 M2 属性修正器 Operand 的域不透明，M2 账本 Operand 保持 `{Literal, AttributeScaled}` 封封不动）；D2-13 双形状精神保留（定义侧丰富、账本/快照侧恒为已解析规范值）。

---

## PV-0 前提：策略载体 = InstancedStruct（已拍板——D3-7 v3，另会话折入）

策略形态按 **D3-7 v3**（README 决策日志 2026-09-10 条；可行性调研轮拍板，已折入 10/03/04/R0 §9/m3/m4/plan2/启动提示词/openspec project.md）：**策略 = USTRUCT 反射基类 + C++ 虚函数分派（StateTree `FStateTreeConditionBase::TestCondition` 同构），Def/步骤以 `TInstancedStruct<Base>` 成员持有**；UE5.8 StructUtils 已迁 CoreUObject，编辑器开箱（类型 picker + 内嵌展开）。本 PV 系列是该形态在 Parameter 域的应用，不另立形态裁决。

## PV-1 载体与基类（已拍板 2026-09-11）

**FTcsParamValue{ TInstancedStruct<FTcsParamValueSource> Source }** 为全插件统一"数值配置"载体（取代 D2-12 FTcsParamScalar 在定义侧的地位），默认 Source=Literal。消费面：BuffDef/StateDef 参数行 Base、SkillDef 参数行 Base、UTcsAttrModDef OperandDef.Literal、UTcsSkillModDef Operand、链步骤数值字段、FlowModify Operand、Duration 等一切"策划书写的数值"（PV-6 全量切换）。

- **抽象基类 `FTcsParamValueSource`（USTRUCT，TcsCore）**：**`virtual double Evaluate(const FTcsParamEvaluateContext& Context) const = 0`**——虚函数命名用户拍板 **Evaluate**（动词纪律：Resolve 仅用于句柄/Id→对象，禁止泛化偷懒）；纯函数性纪律（D0-1）：同上下文同结果，随机/时间禁入。**连带更名：解析上下文 `FTcsParamEvaluateContext`**（与动词对齐）。
- **`FTcsParamEvaluateContext`（TcsCore，最小数据面）**：Subject 单位句柄 + 参数表只读访问 + 级别位（EffectiveLevel），零领域词汇。
- **上下文反射面（2026-09-11 用户提出 UnrealSharp 扩展需求后拍板）**：`FTcsParamEvaluateContext` MUST 是**反射可见的 USTRUCT**——参数表访问走 `ITcsParamTableReader`（UINTerface，TcsCore：`TryGetNumericParam(FName, out double)`，miss=false → 源落 Fallback）；**禁止 TFunction/std::function 等不可反射成员出现在上下文**（否则宿主脚本无法读写上下文——扩展通道焊死）。**C# 扩展地图**：①读上下文——反射 USTRUCT 字段 ✓；②自定义参数表——宿主实现 ITcsParamTableReader ✓（UnrealSharp 可实现 UInterface）；③新参数语义——PV-5 delegate/Formula 源（反射可见委托，C# 可绑定）；④新源 USTRUCT 类型——C++ only（D3-7 v3 已接受代价，与 BP 同）。
- **上下文扩展机制（已拍板）**：结构体继承 + checked cast——如 TcsState 定义 `FTcsStateParamEvaluateContext : FTcsParamEvaluateContext` 增持领域数据，源 Evaluate 内 checked cast（失败=配置错误，加载期校验兜底）；InstancedStruct 为备选形态。**源 × 上下文合法性矩阵 = M8 显式交付物**（cast 失败面必须被编辑器校验全覆盖，防运行期地雷——用户认可该形态近旧 TCS「UObjectCDO+Payload」，当前无更优解的接受项）。
- **Fallback（已拍板）**：可失败源 Fallback 必填（除 Literal 恒成功）；**编辑器阶段即校验**（Def 资产 IsDataValid / M8 钩子）——让策划配置时即刻看见错误，不等运行期。
- **增补（2026-09-14，参数折叠与展示轮）**：①`FTcsParamValue` 补 **`Evaluate` 便利转发**（`Source.IsValid()` 兜底 0）；②上下文补齐 **`Subject`（`FCombatEntityHandle`）+ `EffectiveLevel`（int32）**——落地时机 = **随 TcsState 等级源同批**（当前代码只有 `ParamTable`，属已记偏差）；③**`FCombatEntityHandle` 落 TcsCore 的边界让步记录**（Core 唯一持有的战斗实体词汇——plan1"句柄别名由各域定义"的例外）；④源能力族**新增可枚举能力**（PV-10）。详见 `2026-09-14-param-fold-and-display-decision-points.md`。

## PV-2 TcsCore 内置源最小集（已拍板 2026-09-11）

- **`FTcsParamSource_Literal{ Value: double }`**——原 FTcsParamScalar.Literal 模式；**命名用户拍板用 Literal 不用 Constant**（与全系统既有词汇一致），恒 Evaluate 成功。
- **`FTcsParamSource_ParamRef{ Key: FName, Fallback: double }`**——引用同参数空间另一键（参数嵌套）；Evaluate 经上下文递归取值。细则（均已拍板）：
  - a) **允许链式引用（A→B→C）**；**编辑器期间杜绝无限递归**——去重机制：自己无法引用自己、引用链上不允许成环（DAG），环 = 编辑期/加载期报错，杜绝运行期死循环；
  - b) **只能引用同域参数表**（StateDef 引用自身参数行 / SkillDef 引用自身参数行）——跨域引用维持 D2-11 拒绝理由（域不透明）；
  - c) Fallback = 引用键不存在/求值失败时的兜底值，必填；
  - d) **引用解析目标（终版钉死 2026-09-11——D5-9/D5-14 修订）**：**EffectiveDefId 概念整体移除**（技能级重定向撤、形态组改多 Entry 组合范式）——ParamRef **无条件解析于 Entry 自身 DefId 的参数表**，零跨 Def 漂移、零解析链；环检测在单 Def 引用图上做。"同槽位队列"议题消解。
- 原 FTcsParamScalar 处置：**删除**（Task 0 壳改造为 FTcsParamValue + Literal 源；plan1 Task 4 未动工，零返工窗口）。

## PV-3 TcsAttribute 属性值源（已拍板 2026-09-11）

**`FTcsParamSource_AttributeScaled{ AttributeName, Coefficient = 1.0, Fallback }`**——参数直接取属性值：Value = Coefficient × Current(Attribute)，住 TcsAttribute（经扩展上下文读属性），Subject=上下文单位。

- **Snapshot（已拍板）**：快照构建时求值一次冻结（属性后续变化不追溯）——R3 唯一路径。
- **Live 模式（已拍板）**：R3 不做，**但为已承诺基建——未来必须实现**（决策记录挂账，非"按需再议"）；Live×AttributeScaled 组合 R3 校验器拒绝。
- **读即登记（已拍板）**：即属性变化 → 依赖参数打脏标记（与刷新 CurrentValue 的 recalc 脏标记同族机制，D2-3）——**随 Live 化一并实现**，R3 不做。
- **快照条目形状（已拍板 2026-09-11）**：已解析规范值 + **源引用位**双字段——Debug 可见取值来源，Live 化（已承诺基建）零结构迁移。

## PV-4 等级表族（已拍板 2026-09-11；评判轮修订同日）

用户裁决（评判轮修订）：**否决"泛化 AttributeIndexedArray 归 TcsAttribute"方案**；**否决六源对称方案——TargetLevel 系暂不提供**；接口收窄命名 **`ITcsEntityLevelProvider`** 且**定义于 TcsState**（不进 TcsCore——避免泛化实体接口越"Core 零战斗词汇"边界；未来要 GetEntityX 再立新接口，接口分离）。

- **`ITcsEntityLevelProvider`（UINTERFACE，TcsState）**：自旧 TCS `ITcsEntityInterface` 收窄搬入，声明 **`GetEntityLevel()`**——宿主实现；Instigator 运行期判定是否实现接口（宿主可以不经 TcsAttribute 承载实体等级——接口解耦）。
- **等级源（归 TcsState——State 拥有施法/施加上下文引用，TcsSkill 依赖 State 白拿）**：
  - `FTcsParamSource_StateLevelArray{ Values[], Fallback }` / `StateLevelMap{ Map: TMap<int32,double>, Fallback }`——状态自身 EffectiveLevel（上下文级别位；clamp 越界 / miss 取 Fallback）；
  - `FTcsParamSource_InstigatorLevelArray/Map{...}`——经上下文实体引用判 `ITcsEntityLevelProvider` 取级。
- D3-11 修订记录：等级→数值表以 def 数据形式进引擎（本体论收编 TCS），宿主升级逻辑照旧（运行中升级不追溯——D3-11 既有语义不变）。

## PV-5 Delegate/Formula 逃生口（已拍板 2026-09-11）

R3 不实现，**但为已承诺基建——未来必须实现**。且用途不限于 TcsDamage：凡"条件驱动的公式值"都需要（例：buff 效果"当条件 X 触发时，增加角色 {formula} 的移动速度"——即属性修正器 Operand 侧的公式需求）。形态 = delegate/接口插槽（宿主 C++ 绑定），受无表达式宪法约束（R0 §8：禁数据表达式——加载期校验/网络重放/Explain 失效）。

## PV-6 消费面切换清单（已拍板 2026-09-11：全量切换）

| 消费点 | 现状 | 切换后 |
|---|---|---|
| StateDef/BuffDef 参数行 Base | double/字面量 | FTcsParamValue（StateLevelArray/Map 落点——"buff 每级+10HP"直接配置） |
| SkillDef 参数行 Base | double/字面量 | FTcsParamValue（等级表/属性源落点） |
| SkillDef/StateDef 时值字段（DurationTime、冷却等，D2-12 原消费者） | FTcsParamScalar | FTcsParamValue |
| UTcsAttrModDef / UTcsSkillModDef 的 OperandDef.Literal（D2-13 定义侧） | FTcsParamScalar | FTcsParamValue（模板默认值可等级化；物化上下文含级别位） |
| 链步骤数值字段 / FlowModify Operand | Param() 引用 | FTcsParamValue；FlowModify 的"黑板键引用"保留为流程域自身 Operand 选项（不进通用参数源族——流程域词汇） |
| M2 账本运行侧（FTcsAttrModOperand.Literal） | double | **不动**（账本零膨胀，D2-13 不变式） |

## PV-7 伤害值来源（已拍板 2026-09-11：用户否决全部三案，裁定参数解算制）

**用户裁定：基础伤害值由 StateParam（参数账本）解算，最终结果输入伤害流程——伤害流程自己不算基础伤害。** 复合运算（攻击力×倍率）由**参数账本链**承载（既有语义：封闭带式运算——**D5-5 v3 修订 2026-09-14：与 M2 同五带 `Add/PercentAdd/Mul/FlatAdd/Override`、顺序无关**）：

```
SkillDef:
  参数行: { Key: "DamageRate", Base: FTcsParamValue{InstigatorLevelArray[5,7.5,10,13.5,18]} }
  参数链初始行（**机制待裁决——PV-9**，示意）:
    - { ParamKey: "DamageBase", Op: Add, Operand: FTcsParamValue{AttributeScaled(AttackPower)} }
    - { ParamKey: "DamageBase", Op: Mul, Operand: FTcsParamValue{ParamRef("DamageRate")} }
  → 激活折叠（带式，任意书写顺序，D5-5 v3）：DamageBase = (0 + AttackPower) × DamageRate
链步骤: FTcsStepDamage.DamageBase = FTcsParamValue{ParamRef("DamageBase")} → 输入伤害流程
```

- **伤害流程零基础值计算**：默认模板 BaseDamage 阶段语义改为"接收输入值"（09:38 条款修订）；`ICombatDamageFlowDelegate.CalculateBaseDamage` 从"★主公式"**降级为逃生口**（D7-2 收窄落点）。
- **流程中途加入公式贡献 = 伤害修改器自己实现**（触发行订阅收集 + ModifyFlow 通道，D7-6）——不在 BaseDamage 阶段开公式口。
- 排除记录：a) FlowModify 黑板管线拼复合（用户否决——基础值属参数域不属流程域）；b) 表达式源（违宪）；c) 步骤双字段特设（破坏通用性）。

## PV-9 已拍板：参数链初始行的来源机制（PV-7 的必要前置；2026-09-11 用户采纳 ParamChainRows 提案）

**问题**：PV-7 要成立（DamageBase = 攻击力×倍率），必须有机制把复合运算的修正器行放进**本条目自己的**参数链——账本链行不会凭空出现。既有链行来源只有两个：外部注入（装备/宿主 `ApplyParamModifiers`）与 `ModifierRows` 模板引用（D3-19/D5-19，**FEntrySelector 作用于外部目标条目**——正如用户指正：EntrySelector 专属于外部施加场景，不该发明 Self-Entry）。"Def 修饰自己"是缺失的第三种来源。

**拍板（采纳提案）**：SkillDef 增加**自带参数修正行 `ParamChainRows`**——独立于 `ModifierRows` 的新字段：声明作用域恒为本条目自身的 `FTcsNumericParamModifier` 形状行（可内联或引用 UTcsSkillModDef 模板；原概念名 `FNumericParamModifier`，实现名统一 Tcs 前缀），**无 FEntrySelector**，激活期物化进本条目参数链，Source=施法运行句柄，结算/打断级联摘除（与 ModifierRows 同生命周期语义）；外部注入（FEntrySelector 场景）语义完全不动。**落域范围注记**：SkillDef 先行（M5 Entry 账本有参数链落点）；State/Buff 侧参数为快照单值、无账本链，暂不需要对应机制——复合需求出现再议（防预建设施）。

**被否备选**：a) 复用 ModifierRows + 约定"恒作用于自身"——同字段两种目标语义，语义混载；b) 拒绝 Def 自带链行、复合全走宿主命令式——策划体验倒退，与 PV-7 动机冲突。

## PV-10 源的可枚举能力（已拍板 2026-09-14——参数折叠与展示轮）

**问题**：描述视图体系中的 Series 视图（整表 + 当前档高亮——D5-17 v3）需要"源枚举出全部条目 + 指出当前档"的能力，PV-2~PV-4 的源族没有这个面。

**拍板**：

- **`FTcsParamEnumerableSource : FTcsParamValueSource`**（可选能力基类，住 **TcsCore**，与值来源策略同族）：`Enumerate(Level, OutValues, OutCurrentIndex)`（枚举条目 + 当前档索引）与 `GetIndexForLevel(Level)`。
- **索引解析唯一真相**：clamp（Array 越界 → 末项）、Map 键查找（miss → 无当前项）、曲线采样政策——**全部写在源自己身上**，且 `Evaluate` 内部走同一份解析——这是"tooltip 高亮 10%、实打 7.5%"唯一的防漂移手段。
- **能力探测 = 虚分派 / checked cast，不做中心 switch**（D3-7 v3 形态）；不可枚举源配 Series 视图 = **M8 配置错误**（视图 `IsCompatible` 探针，保存期即报、错误挂配置元素），运行期优雅降级单值。
- **命名纪律**：本能力**不引入新的 `Resolve` 用法**（Resolve 仅句柄/Id→对象）。
- **曲线源（LevelCurve）的展示策略**：区间表达（Range 视图——原 `{ParamRange:X}` 词法，D5-17 v3 载体更名）**零能力即可用**（在 `LevelBase` 与 `MaxLevel` 两点各求一次值）；"整表 + 高亮"对曲线无天然格数，需求出现时由源自定义采样政策（元数据给采样范围）——不在本轮预建。
- **落地时机**：随 TcsState 等级源（PV-4 四源）同批进 TcsCore——**不进 plan1 Task 0**（零消费者不预建）。

详见 `2026-09-14-param-fold-and-display-decision-points.md`。

## PV-8 校验与折入安排（已拍板，本轮执行）

- **加载期/编辑期校验（M8 钩子）**：可失败源缺 Fallback / ParamRef 自引用与环 / 等级表空表 / Live×不可 Live 源组合 / 上下文扩展 cast 失败——**编辑器阶段即报**（策划即配即见）。**2026-09-14 升级为四联矩阵**：`源类型 × 上下文类型 × 可配约定（D5-18 v3 白名单）× 视图兼容（D5-17 v3：视图 IsCompatible 探针）`。
- **折入清单（本轮执行）**：01（FTcsParamValue/基类/Literal+ParamRef 源/上下文/ITcsEntityInterface）、02（OperandDef.Literal 换型 + D2-12 取代记录）、03（参数行 + StateLevel 源 + D3-11 修订）、05（参数行 + Instigator/Target 级源 + 参数链复合示例）、09（D7-2 收窄：基础值=参数解算输入、delegate=逃生口）、11（FTcsParamScalar 条目替换，载体归 01）、R0 §9（TcsCore 行载体更名）、README 决策日志、m0min（D2-12/D2-13 修订注记）；plan1（Task 0 产物改造 + Task 4 类型清单 + 命名）、plan2（StepDamage.DamageBase + 消费面字段）。

---

## 拍板汇总

| 项 | 裁决（2026-09-10 方向 / 09-11 细节 / 09-11 评判轮修订） |
|---|---|
| 策略形态全局 | D3-7 v3：USTRUCT 反射基类 + 虚函数分派 + TInstancedStruct<Base>（另会话拍板并折入） |
| D2-12 | FTcsParamValue{TInstancedStruct<FTcsParamValueSource>} 取代 FTcsParamScalar；基类虚函数 **Evaluate**（上下文随名 FTcsParamEvaluateContext）；**Literal**/ParamRef 进 TcsCore（Constant 更名 Literal——用户拍板） |
| ParamRef | 链式允许 + 编辑期 DAG 去重（禁自引用/成环）；**限同域**；Fallback 必填 + 编辑器即报 |
| D3-11 | 等级表参数进引擎：**StateLevel/InstigatorLevel × Array/Map 四源归 TcsState**（评判轮：TargetLevel 系暂不提供；`ITcsEntityLevelProvider::GetEntityLevel` 定义于 TcsState——用户否决 TcsSkill 归属与 AttributeIndexedArray 泛化两案） |
| D7-2/D7-5 | 收窄：**基础伤害值 = StateParam 参数账本解算输入伤害流程，流程零计算**（用户否决流程域拼装三案）；伤害修改器自实现中途公式贡献；delegate=逃生口 |
| TcsAttribute | AttributeScaled 属性值源；Snapshot-only；**Live 与读即登记（脏标记）为已承诺基建，随 Live 化实现**；快照条目留源引用位（Debug+Live 预留） |
| Formula 逃生口 | R3 不做、**未来必做**（用途不限于 Damage，例：条件 buff {formula} 移速） |
| 消费面 | 全量切换 FTcsParamValue；M2 账本运行侧不动；上下文合法性矩阵 = M8 显式交付物 |
| **PV-9（已拍板）** | 参数链初始行来源机制——采纳 `SkillDef.ParamChainRows`（自带参数修正行：无 FEntrySelector，恒作用于本条目，激活物化进本参数链；State/Buff 侧暂不需要）；见 PV-9 节 |
| **PV-10（已拍板 2026-09-14）** | 源的可枚举能力——`FTcsParamEnumerableSource`（Core，可选能力基类）+ `Enumerate`/`GetIndexForLevel`；索引解析唯一真相在源、Evaluate 同源；曲线源区间零能力可用、序列表待需求；见 PV-10 节 |
| **PV-1 增补（已拍板 2026-09-14）** | `FTcsParamValue.Evaluate` 便利转发；上下文补 `Subject`/`EffectiveLevel`（随 TcsState 等级源同批）；`FCombatEntityHandle` 进 Core 的边界让步记录 |
| **折叠与展示（已拍板 2026-09-14/15）** | D5-5 v3（参数链 = 带式聚合，五带同 M2、顺序无关、折叠器单份住 TcsAttribute 三处共用）/ D5-17 v2→**v3**（描述视图策略化：视图=结构化策略配置 `FTcsParamView`、StringTable 只含槽名、`IsCompatible` 探针校验、展示政策归开发者——v2 token 词法已废）/ D5-18 v3（约定可配白名单）——详见 `2026-09-14-param-fold-and-display-decision-points.md` |
