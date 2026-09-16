# 11-module-notation.md — TcsNotation 策划记法层设计（v1 定稿）

- 日期：2026-09-02
- 状态：**v1 定稿**——折入 D5-17/D5-18/D5-18 v2 与 D2-12 载体边界；修订记录见文末
- 职责一句话：**策划记法底座——数值书写约定（策划语言值↔规范值）与描述文本绑定（StringTable+占位符）的通用词汇与便利设施；零战斗语义。**

## 1. 模块边界

- 消费者：内容定义承载模块——TcsSkill（SkillDef 参数双表/描述绑定，D5-5/D5-17）、TcsState（FStateDefBase 参数行/描述绑定，D5-18 v2）、TcsAttribute（UTcsAttrModDef 约定列，D5-18 v2）；TcsEditor（占位符扫描，M8）；宿主 UI（反变换与渲染）。
- 依赖：**仅引擎基础**（CoreUObject 等）——**与 TcsCore 平级互不依赖**（D5-18：Core=运行时机制底座，Notation=策划记法底座；宿主非战斗配置可独立复用本模块）。
- **不依赖方（同等重要的边界）**：纯机制层 TcsEffect / TcsDamage / TcsTargeting / TcsCue 运行时**不加边**——运行时只消费规范值；"字面量|Param"数据形状载体 **`FTcsParamValue{TInstancedStruct<FTcsParamValueSource>}` 归 TcsCore**（PV 系列 2026-09-11 取代 D2-12 FTcsParamScalar），本模块只管"值怎么书写"不管"值是字面量还是引用"；**源的可枚举能力基类（PV-10）同样归 Core**（本模块只定义视图策略与纯数据面契约）。
- R3：不在六模块竖切内——**骨架壳**（枚举 + 转换助手，计划一 Task 0，作为 TcsAttribute←Notation 依赖边的编译前提）；功能面随内容模块轮落地。

## 2. 类型词汇（对外）

### 2.1 值约定（D5-18 终定）
- `ETcsValueConventionFlag`：`VCF_None=0（默认）/ VCF_Percent / VCF_OneMinus / VCF_Negate`——EnumFlags，**固定组合顺序 Percent→OneMinus→Negate**。
- 三变换语义（用户 GAS 扩展实战）：Percent（配 85 → 逻辑 0.85）；OneMinus（配 25"减少25%" → 逻辑 0.75）；Negate（配 1000"减少1000生命" → 逻辑 -1000）——**变换改写逻辑消费的值本身，不只是显示格式**。
- 转换助手：`ConvertToCanonical(Raw, Flags)`（写入点调用）；`DecomposeFromCanonical(Canonical, Flags)`（UI 反变换：0.75 → "25"+%样式）。
- **实现现状与纪律**：`ConvertToCanonical` 已落地（`Public/FTcsValueConvention.h`，FORCEINLINE 静态）；`DecomposeFromCanonical` **未实现**——注意它是正向的**逆序**（Negate→OneMinus→×100，因正向固定序为 Percent→OneMinus→Negate），随描述组装器功能面一并落地。反向变换逐项执行：非当前格 = `Decompose(ConvertToCanonical(表值, 约定), 约定)`（与逻辑同源往返，杜绝第二份真相）；当前格 = `Decompose(账本现值, 约定)`。**本模块不引入新的 `Resolve` 用法**（动词纪律：Resolve 仅句柄/Id→对象）。

### 2.2 描述视图体系（D5-17 v3——视图策略化，2026-09-15；v2 文本内嵌词法已被本版取代）

**原则**：**TCS 提供表达形态的选项与各自的数据契约，画什么、怎么画由开发者/文案决定**——插件不规定某个参数该怎么展示。

**核心动作：文本只留"槽名"，机器语义全部进结构化配置**——v2 的文本内嵌词法（`{Param:X}` 等）**废除**：token 悬在共享文案里，编辑器无法把错误定位到配置元素、视图无法携带自身参数（样式键/分隔符），且源与展示形态不可能一一对应（必须允许项目自定义视图）。v3 起 **StringTable 回归纯文案**，占位符只有槽名（FName），全部机器知识住 Def 资产的结构化字段。

- **描述配置空间**（**字段归各 Def**——`FStateDefBase.Descriptions`；词汇约定归本模块；实现名见命名批）：

```cpp
TArray<FTcsDescriptionEntry> Descriptions;
// FTcsDescriptionEntry{ DescriptionId(FName——"Tip"/"Codex"/"LevelUpPreview"…多描述入口),
//                       TextKey(FName——StringTable 键),
//                       Views: TArray<FTcsDescriptionViewSlot{ SlotName(FName), TInstancedStruct<FTcsParamView> View }> }
// 文案原文引用槽位："对目标造成 {Rate} 的火焰伤害"——零语法，FText::Format 命名参数白拿（重排/复用/同槽多次引用）
```

- **视图 = 策略形态（D3-7 v3 同构）**：USTRUCT 反射基类 + 虚函数分派 + `TInstancedStruct` 持有（用户认同 Kind+FInstancedStruct——它与策略形态同物，取策略形态：零 Kind 维护、灭"类型与载荷配对"bug 类、编辑器类型 picker 开箱、与全插件策略位同构）；每个视图自带配置参数（读哪个参数键、样式键、分隔符）。
- **内置视图最小集（v2 词法族的转世——语义不变，载体从 token 变类型）**：

| 内置视图 | 展示结果 | 需要的源能力（Probe） | 对应旧词法（已废） |
|---|---|---|---|
| `FTcsParamView_Value` | 单个值（当前档 / 账本现值） | —（`Evaluate`） | `{Param:X}` |
| `FTcsParamView_Series` | 全部条目 + 当前档高亮（`5% / 7.5% / ⟨10%⟩ / 13.5% / 18%`） | **可枚举**（PV-10） | `{ParamSeries:X}` |
| `FTcsParamView_Range` | 两端区间（`50~270`） | **零新能力**——`EvaluateAtLevel` 在 `LevelBase` 与 `MaxLevel` 各求一次值 | `{ParamRange:X}` |
| `FTcsParamView_Attribute` | 属性值（现值） | 属性词表读取 | `{Attr:X}` |

  - **区间端点统一规则**：一端 = `LevelBase`、另一端 = `MaxLevel`；某端求值 miss 落 Fallback（Map 源取最小键/最大键）。"（随等级提升）"这类限定语是**文案**不是机制——曲线源的"50~270"表达 = Range 视图 + 文案原文。
  - **多对多（本设计的落位）**：一个参数源 ← 被多个视图引用（Tip 用 Value / Codex 用 Series / 图鉴用 Range）；一个 Def ← 多条描述入口（Descriptions 数组——同一技能"简单描述=当前档单值、复杂描述=全表+高亮"并存）；一个描述 ← 多个视图槽位（文本按槽名任意组合与重复引用）。

- **绑定链（源与视图的对应关系）**：**文本层只有槽名，永远不认识源类型与视图类型**——这正是本模块与 TcsCore 互不依赖的机制保证：

```
① StringTable 原文 "…{Rate}…"（零语法，只有槽名）
   └─ 组装器：FText::Format 扫描出 (槽名)，仅此而已
② 槽名 → Def.Views 查到视图实例（TInstancedStruct<FTcsParamView>）
③ 视图策略 BuildText(Ctx) —— 虚分派，零中心 switch；Ctx 由内容模块适配器装配
   └─ 视图内：按键取数（GetValue / TryGetSeries / EvaluateAtLevel）→ 反变换 + 格式化 + 富文本包裹（本模块助手）
④ 视图产出 FText 片段 → 组装器以命名参数替换 {Rate}
⑤ 宿主 RichTextBlock 渲染
```

- **数据面契约**（本模块定义，**不含任何 Core 类型**）：
  - `FTcsViewBuildContext`——视图构建上下文，由**内容模块适配器装配**（TcsSkill/TcsState 是唯一同时认识 Def 形状与本模块词汇的地方）：`GetValue(Key)`（规范值现值）/ `TryGetSeries(Key, Out 值表, Out 当前档索引)`（可枚举源）/ `EvaluateAtLevel(Key, Level, Out)`（任意等级点求值——Range 视图的全部能力来源）/ `TryGetConvention(Key, Out 行约定)`（反变换输入）。**索引口径在适配器装配处落实**（见下条）。**瞬时对象、非反射、不序列化**——PV-1 的反射禁令只约束"进 Def/求值上下文序列化面"的成员，本上下文只活在 C++ 调用栈（StateTree `TestCondition(FStateTreeExecutionContext&)` 同款先例）。
  - `FTcsViewProbe`——**纯数据能力探针**（编辑器校验用）：`bEnumerable` 等能力位，由内容模块适配器对源实例探测后填充；视图用 `IsCompatible(Probe)` 声明自己需要什么。
- **索引口径（两处不同，均须遵守）**：技能面板 = Entry 当前 `EffectiveLevel`（口径 = 下一次施法）；实例（buff/状态）tooltip = **该实例快照的 Level**（口径 = 本次施加——"运行中升级不追溯"的既定语义，用查询时等级高亮会出现"显示 5 级、实际按 3 级生效"）。**当前格取账本现值（含修正器）**，非当前格取 Def 表内基准值。
- **Attribute 视图口径提醒（防误修）**：展示读属性**现值**，而计算侧 `AttributeScaled` 是 PV-3 快照语义（施法瞬间冻结）——两者在"施法后属性变化"的窗口里允许不一致，此为 D5-17 既定取舍（与逻辑同源不同时机），非缺陷。
- 样式与约定分工（v3）：**样式键随视图走**（视图配置参数——`CurrentStyleKey` 等，视图自己包富文本样式，翻译者不碰标签）；**约定仍随参数行走**（行级，反变换输入）；RichTextBlock 渲染归宿主 UI。
- **编辑器预警四层（M8 消费）**：
  1. **预防**：Def 资产 Details 面板——Views 数组类型 picker（只列视图类型）+ 参数行**源能力徽章**（"可展示：单值/整表/区间"，由内容模块适配器对源实例探测派生）；
  2. **拦截（主防线）**：Def `IsDataValid` / M8 保存期——`View.ParamKey` ∈ 参数键集、**`IsCompatible(Probe)`**（内置 Series 对不可枚举源报错）、SlotName 重复/为空——**错误挂具体配置元素（可点击定位）+ 可操作建议**（"此键源不支持整表，建议 InstigatorLevelArray"）；
  3. **文案交叉扫描**：文本 `{槽名}` ∈ Views 列表（缺 = 错误，指名 StringTable 条目）；Views 声明未被引用 = 孤儿**警告**——扫描器**零语法解析**；
  4. **保命**：运行期**只降级不报错**（`TryGetSeries` 失败 → 该槽退化单值渲染）。
  - **不变式：编辑期必须报错、运行期只许降级**——编辑期静默降级会让策划永远不知道配错；运行期报错是把配置错误变成线上事故。
- **新增视图操作手册（宿主扩展 = 一步）**：定义 `FMyParamView : FTcsParamView`（项目/插件 C++，消费 BuildContext 查询面；需要新能力判定时重载 `IsCompatible`）→ 在任何 Def 的 Views 列表选它。**零公共代码改动**：不动扫描器（文本零语法）、不动组装器、无注册表、M8 矩阵代码零改（校验走视图自身虚函数）。新增**源能力**（罕见路径）才动公共面：加能力基类（PV-10 模式）+ Probe 能力位 + 适配器实现——成本按"能力"计、多视图分摊。

### 2.3 边界声明（防越界）
- **FTcsParamValue 不在本模块**（原 D2-12 拍板 FTcsParamScalar 归 Core；PV 系列 2026-09-11 取代载体）：它是数据形状（值源策略 TInstancedStruct<FTcsParamValueSource> + Evaluate 求值），住 TcsCore——放 Notation 会给 TcsEffect/TcsState 强加记法层编译边（它们的依赖线没有 Notation，D5-18 v2 审计结论）。**源的可枚举能力（PV-10 `FTcsParamEnumerableSource`）同样不在本模块**——它住 Core 与值来源策略同族；本模块只定义**视图策略与纯数据面契约**（视图配置类型 + BuildContext/Probe，均不含 Core 类型），保证零反向依赖。
- **显示排版与值约定正交并存**：小数位/千分位/样式包裹另列（M8/宿主）。
- **参数键注册表不做**：键空间 = 各域参数表（词汇归项目，D5-5）——本模块不认识任何具体键。

## 3. 写入点与消费点全图（核心规格）

**写入点（策划语言→规范值；转换责任在配置域，本模块提供转换助手）：**

| # | 写入点 | 宿主模块 |
|---|---|---|
| 1 | SkillDef 参数行 Base → Entry 账本注册 | TcsSkill（M5） |
| 2 | FStateDefBase 参数行 Base → ParamSnapshot 构建 | TcsState（M3） |
| 3 | UTcsAttrModDef / UTcsSkillModDef 模板默认 Literal → 物化 | TcsState / TcsSkill 物化器 |
| 4 | 宿主装备 / 命令式 ApplyParamModifiers | 宿主配置域（账本不做二次猜测） |
| 5 | 参数链修正行 Operand（D5-18 v3 补约定列；链步骤数值字段/时值字段按需再议） | TcsSkill |

**约定可配白名单（D5-18 v3，M8 强制）**：✅ 可配 = `Literal` 与表型源（书写值直接成为结果）；❌ 禁配 = `ParamRef`（读到的已是规范值，再转 = 二次转换）与 `AttributeScaled`（结果是乘积，约定作用面歧义）。**约定列是行级"本行书写口径"**，作用域 = 本行自己书写的数值。

**消费点（永远规范值）**：M2 账本/聚合、M5 参数链、FStateInstance.ParamSnapshot / FCastRun.ParamSnapshot、Explain 回放、TcsDamage 流程。
**反变换点**：UI 显示（组装器/宿主，视图按行约定反变换+包样式）。

## 4. 入口服务

- `ConvertToCanonical / DecomposeFromCanonical`（静态助手，无状态）。
- **组装器（便利设施）**：`BuildDescription(const FTcsDescriptionEntry&, const FTcsViewBuildContext&) -> FText`——按槽名逐个调 `View.BuildText(Ctx)` 产出富文本片段，`FText::Format` 命名参数替换；取值按 Entry 自身 Def 的账本**现值**（EffectiveDefId 已整体移除——D5-9/D5-14 重构 2026-09-11；与逻辑同源不同时机：逻辑读 ParamSnapshot，UI 读账本——D5-17）。组装器**不查表、不认识源类型、不认识任何参数键、零语法解析**——只编排槽名与 FText。
- **占位符扫描接口**（M8 消费）：槽名交叉校验（文本 `{槽名}` ∈ Views、孤儿视图 = 警告）——见 §2.2 预警四层。
- **落地状态**：本节四项（`DecomposeFromCanonical`/组装器/视图体系/扫描接口）为**已规格化未实现**——随内容模块轮（M3/M5/M8）落地，当前代码只有 `ConvertToCanonical`。

## 5. 网络姿态落点（NET-1/2）

- 规范值过线：账本内已是规范值，操作复制/客户端重算无约定参与——**本模块零网络面**；UI 反变换纯本地（客户端拿规范值自行反变换显示）。

## 6. 非目标

不做表达式/公式（封闭公式宪法）；不做本地化流程本体（FText/StringTable 平台能力白拿）；不做 RichTextBlock 渲染（宿主 UI）；不做显示排版小数位（正交另列）；不做参数键注册表（键空间归各域）；不做宿主装备书写约定（宿主配置域）；**不做展示形态的政策**——只提供视图选项与数据契约，某个参数在游戏中怎么展示由开发者/文案决定（D5-17 v2/v3）；不做视图装饰器组合（视图套视图——需求出现再议，YAGNI）。

## 7. 依据

- 拍板：D5-17（描述绑定——用户提案 + 三修正：原生占位符语法/组装器包样式/M8 扫描）、D5-18（值约定——用户 GAS 扩展实战经验；升格第十一模块与 Core 平级）、D5-18 v2（面补全 + TcsState/TcsAttribute 依赖边）、D2-12（载体边界——FTcsParamScalar 归 Core；**载体已被 PV 系列取代**）。**2026-09-14 增补**：D5-17 v2（词法族 + 绑定链 + 取值回调契约 + 索引口径）、D5-18 v3（约定可配白名单 + 链行约定列）、PV-10（源的可枚举能力归 Core）；**2026-09-15 增补**：D5-17 v3（描述视图策略化——token 词法废改为视图策略配置）——均见 `2026-09-14-param-fold-and-display-decision-points.md`。
- 证据：GAS 5.5 FixedAdd（shipped 先例，MEM-20260902-17 方法）；用户 GAS 扩展实战三变换；MEM-20260902-18（书写约定层 judgment 卡）。
- R0 §9 模块表（依赖仅引擎基础）；计划一 Task 0 骨架口径。

## 8. 修订记录

- v1（2026-09-02）：初版折入（R3 补文档轮）。
- v1 增补（2026-09-11，PV 系列）：§2.3 边界声明载体换型——FTcsParamScalar → **FTcsParamValue{TInstancedStruct<FTcsParamValueSource>}**（D2-12 被取代，载体归 TcsCore；Evaluate 虚分派）；写入点语义不变（参数行 Base/模板 Literal 求值后经 ValueConvention 转规范值）。
- v1 增补 2（2026-09-11，D5-9/D5-14 重构）：组装器取值口径——"EffectiveDefId 解析后" → **Entry 自身 Def 的账本现值**（EffectiveDefId 概念整体移除）。
- v1 增补 3（2026-09-14，参数折叠与展示轮）：折入 **D5-17 v2**（§2.2 词法族 `{Param:X}`/`{ParamSeries:X}`/`{ParamRange:X}`/`{Attr:X}`、绑定链"文本只认键名"、取值回调四方法契约、索引口径双处、当前格取账本现值、展示政策归开发者；§4 组装器契约与落地状态）、**D5-18 v3**（§3 写入点补第 5 行 + 约定可配白名单；§2.1 反变换逆序与实现现状）、**PV-10**（§1/§2.3 边界声明：可枚举能力基类归 Core）、§2.3 `{Attr:X}` 现值口径提醒、§9 验收钩子扩展。
- v1 增补 4（2026-09-15，D5-17 v3 描述视图策略化——用户重设计）：§2.2 全节重写——**文本内嵌词法族（v2）废改为视图策略体系**：视图 = `FTcsParamView` USTRUCT 策略基类（`BuildText`/`IsCompatible`）+ `TInstancedStruct` 持有；配置空间 = `Def.Descriptions`（`FTcsDescriptionEntry{DescriptionId, TextKey, Views[SlotName, View]}`，取代单字段 DescriptionTextKey）；StringTable 回归纯文案（只含槽名）；数据面 = `FTcsViewBuildContext`（瞬时非反射）+ `FTcsViewProbe`；编辑器预警四层（源能力徽章 / 探针拦截挂元素 / 槽名交叉扫描 / 运行期降级）；新增视图 = 宿主一步、零公共代码；v2 的四方法回调与序列/区间视图 struct 作废（索引口径/账本现值/降级不变式保留）；§4 组装器签名同步；§6 非目标补视图装饰器组合。

## 9. 验收钩子

计划一 Task 0 骨架编译；功能面验收随内容模块轮人工检查：SkillDef 参数行约定转换（5 → 0.05）、BuffDef 参数行约定转换、模板默认 Literal 物化转换（miss 兜底）、描述组装占位符替换与富文本包裹、M8 占位符扫描报错。**2026-09-14/15 增补验收项**：Series 视图整表渲染 + 当前档高亮（技能面板按 EffectiveLevel、实例 tooltip 按快照 Level，两处口径各验一次）；Range 视图区间渲染（曲线源与表源各一例）；Series 视图挂不可枚举源 = 保存期报错（**错误挂配置元素**）；槽名缺失/视图孤儿 = 扫描报错/警告；宿主自定义视图端到端（C++ 定义 → Def 配置 → 渲染）；约定禁配组合（ParamRef/AttributeScaled 勾约定）= 保存期报错。
