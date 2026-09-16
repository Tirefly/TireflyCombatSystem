# 参数折叠与展示决策点（D5-5 v3 / D5-17 v2 / D5-18 v3 / PV-10 / PV-1 增补）

- 日期：2026-09-14
- 状态：**已拍板**——用户逐项裁定；本轮同时收束 TcsNotation 复审发现的过期项
- 背景输入：① 走查"攻击力 × [5/7.5/10/13.5/18]% 随技能等级 + tooltip 当前档高亮"暴露三处缺口——参数链折叠顺序敏感、描述绑定只有单值词法、源与文本之间缺"可枚举"能力；② TcsCore Parameter 体系在 2026-09-11 其他会话改造（目录 `Vocabulary/`→`Parameter/`、五文件落地、上下文缩至只有 `ParamTable`），Notation 侧文档需重新对齐；③ 用户三原则——**展示形态由 TCS 提供选项、开发者自行选择**（插件不替项目规定某个参数怎么显示）；**动词纪律**（Resolve 仅句柄/Id→对象，禁泛化使用）；**参数链聚合应与 M2 属性修改器完全同式**（运算符相同则语义相同）。
- 涉及既有裁决：D5-5 → **v3 修订**（参数链折叠语义）；D5-17 → **v2 扩展**（描述绑定词法族）；D5-18 → **v3 增补**（约定可配面）；PV-1 → **增补**（上下文补齐 + 载体便利转发）；PV 系列 → **新增 PV-10**（源的可枚举能力）；D2-10 的"M5 参数链不跟进"口径**作废**；D5-19 CompeteGroup 语义保留、折叠序改带式。

---

## D5-5 v3：参数链 = 带式聚合（已拍板 2026-09-14）

**问题**：D5-5 原口径"参数链 = 有序链式（SortKey，先后覆盖语义——区别于 M2 顺序无关）"制造配置地雷——编辑器里把 Mul 行写在 Add 行之前，`(0×0.1)+120 = 120` 与 `(0+120)×0.1 = 12` 结果相差十倍，且策划无从察觉。

**拍板**：**参数链折叠与 M2 属性聚合完全同式**（运算符相同、带序相同、顺序无关），具体：

- **Op 集合 = M2 同一套五带**：`Add / PercentAdd / Mul / FlatAdd / Override`（原参数链的 `AddPct` 命名废弃——与 M2 `PercentAdd` 失步修正；原集合缺 `FlatAdd` 补齐）。字面值名沿用 M2（`TAO_*`）。
- **折叠公式（与 M2 一致）**：
  - `Override` 组存在 → 取组内**最大值**直接作为结果（`FlatAdd` 一并被覆盖，"最强覆盖生效"不变）。
  - 否则 `Final = ((初值 + ΣAdd) × (1 + ΣPercentAdd)) × ΠMul + ΣFlatAdd`。
- **带权 = M2 同表**：`Override 0 / Add 10 / PercentAdd 15 / Mul 20 / FlatAdd 30`；**SortKey 在参数链中不再承担排序语义**（退化为带权展示位，与 M2 `FTcsAttributeModifier.SortKey` 同义）。
- **折叠初值（新钉）**：**该键参数行的求值结果；该键无参数行则 0**——这是 M2 `Base` 在参数域的对应物（避免了未定义口径）。
- **CompeteGroup（D5-19）语义保留、位置后移**：仍在折叠前按组分桶、组内取值最大者进折叠；选优后的行**按其 Op 落入对应带**（原"先分组选优后按序折叠"→"先分组选优后按带折叠"）。"取优先级最高"从 `Override+SortKey` 组合表达改为**与 M2 一致的 Override 组取最大值**。
- **折叠器单份（复用而非并列）**：`(Base + ΣAdd) × (1+ΣPercentAdd) × ΠMul + ΣFlatAdd` + Override 覆盖语义抽为一个纯函数，**住 TcsAttribute（本模块提供类型与词汇）**，三处共用：M2 属性聚合、M5 参数链、TcsDamage 流程属性容器（02 §2.2a 已承认三处"同一形状、作用域容器不同"）。归属理由：三个消费者的编译边都已含 TcsAttribute（零新边），且不把 Op 枚举与带序词汇搬进零语义的 TcsCore。

**例证走查**：

| 配置（任意书写顺序） | 带式折叠 |
|---|---|
| `{Add, AttributeScaled(AttackPower)}` + `{Mul, ParamRef(DamageRate)}` | `((0+120)×1)×0.10 = 12.0` |
| `{Add, AttributeScaled(AttackPower)}` + `{Mul, ParamRef(DamageRate)}` + `{FlatAdd, ParamRef(DamageAddition)}` | `((0+120)×1)×0.10 + y = 攻击力×x + y` |

**附带收益**：带式天然顺序无关，比"SortKey 相等时依赖稳定遍历序"更贴合 D0-1 确定性纪律。

---

## D5-18 v3：约定可配面（已拍板 2026-09-14）

**问题**：D5-18 只定了"行级约定列 + 写入点转换"，未定**哪些源可以配约定**、以及 `ParamRef` 行的二次转换陷阱。

**拍板**：

- **约定列 = 行级"这一行的书写口径"**，作用域 = **本行自己书写的数值**（不是"引用来的值"、不是"算出来的复合结果"）。
- **可配白名单（M8 校验强制）**：
  - ✅ 可配：`Literal`、表型源（`StateLevelArray/Map`、`InstigatorLevelArray/Map`、未来 `LevelCurve` 等）——其书写值直接成为结果。
  - ❌ 禁配：`ParamRef`（读到的已是规范值，再转 = 二次转换；等级表行 `{5,7.5,10}` 配 Percent 得 `0.10`，引用行再配 Percent 会变 `0.001`）；`AttributeScaled`（结果是 `Coefficient × Current(Attribute)` 乘积，约定作用在系数还是乘积上有歧义；系数需要百分比语义直接写小数）。
- **修正行 Operand 补约定列（已拍板）**：PV-6 全量切换后，链步骤数值字段/FlowModify Operand/时值字段也都是 `FTcsParamValue`——其中**参数修正行 `FTcsNumericParamModifier` 的 Operand 补 `ValueConvention` 列**（否则"冷却 -30%"这类策划书写只能手算 `0.30`）。**链步骤数值字段与纯结构时值（Duration 秒数等）按需再议**（YAGNI：它们换型后同样可挂约定列，需求出现即按同一模式加，不在本轮预建）。
- **`Literal.Value` 定性（消解文档/代码冲突）**：Literal 的 `Value` 是**配置态书写值**；无约定列（`VCF_None`）时它即规范值（恒等）。"账本/快照内恒为规范值"这条不变式的管辖范围是**运行侧**，不是 Def 配置态。→ 需修正 `FTcsParamSource_Literal.h` 的注释措辞（原文"直接返回配置的规范值"/"字面量值（规范值）"读起来像是 Literal 只能写规范值，与约定列的存在矛盾）。
- **M8 校验扩展**：`源类型 × 可配约定` 合法性矩阵（与 PV-8 已有的 `源 × 上下文` 矩阵合并为四联矩阵，见下）。

---

## D5-17 v2：描述绑定的词法族与取值契约（已拍板 2026-09-14；**token 载体已被下文 D5-17 v3 取代——本节保留为历史记录**）

**问题**：D5-17 只有单值占位符 `{Param:X}`，无法表达"整表 + 当前档高亮"；且"某个参数该怎么展示"被当成插件决策——越界。

**原则（用户拍板）**：**TCS 提供表达形态的选项与各自的数据契约，画什么、怎么画由开发者/文案决定。**

### 词法族（对外词汇）

| 词法 | 渲染结果 | 需要的数据契约 | 可用源 |
|---|---|---|---|
| `{Param:X}` | 单个值（当前档 / 账本现值） | `Evaluate` | **全部源** |
| `{ParamSeries:X}` | 全部条目 + 当前档高亮（`5% / 7.5% / ⟨10%⟩ / 13.5% / 18%`） | **可枚举能力**（PV-10） | 表型源（Array/Map） |
| `{ParamRange:X}` | 两端区间（`50~270`） | **零新能力**——在 `LevelBase` 与 `MaxLevel` 两个等级点各求一次值 | **全部源** |
| `{Attr:X}` | 属性值（现值） | 属性词表读取 | 属性词表（既有） |

- **区间端点统一规则**：一端 = `LevelBase`，另一端 = `MaxLevel`；某端求值 miss 落 Fallback（Map 源取最小键 / 最大键）。用户的"曲线表"表达（【造成 50~270（随等级提升）的伤害】）即 `{ParamRange:X}` + 文案原文——**"（随等级提升）"是文案，不是机制**。
- **渲染形态可组合**：同一参数在不同文案里各取所需（面板成长预览用 `{ParamSeries:X}`、句内当前值用 `{Param:X}`、曲线概览用 `{ParamRange:X}`）。

### 绑定链（源与文本解析器的对应关系）

**文本层永远只认"键名"，不认识任何源类型**——这是 Notation 与 Core 互不依赖的机制保证：

```
① StringTable 原文 "…{ParamSeries:DamageRate}…"
   └─ 文本解析器：扫描出 (词法, 键名)，仅此而已
② (Series, "DamageRate")
   └─ 组装器向"取值回调"要数据
③ 内容模块适配器：按键在 Def 参数表找到行 → 取 Row.Base.Source
   └─ 虚分派（能力探测 / Evaluate），零中心 switch
④ 源给出值（单值 / 序列+当前档索引 / 区间两端）
   └─ DecomposeFromCanonical 反变换 + 格式化 + 富文本包裹（Notation）
⑤ FText::Format 替换占位符 → 宿主 RichTextBlock 渲染
```

- **取值回调契约（Notation 侧定义，纯 `double`/`TArray<double>`/`int32`，不含任何 Core 类型）**：`GetValue(Key)` / `GetSeries(Key, Out)` / `GetRange(Key, Out)` / `GetAttribute(AttrKey)`——由**内容模块实现**（TcsSkill/TcsState 是唯一同时认识 Def 形状与 Notation 词汇的地方），宿主 UI 调用。
- **索引口径（两处不同，均须遵守）**：
  - 技能面板 = **Entry 当前 `EffectiveLevel`**（口径 = 下一次施法）；
  - 实例（buff/状态）tooltip = **该实例快照的 Level**（口径 = 本次施加；"运行中升级不追溯"的既定语义——用查询时等级高亮会出现"显示 5 级、实际按 3 级生效"）。
- **当前格取值 = 账本现值（含修正器）**；非当前格 = Def 表内基准值。当前格反变换后若与基准不同，可另附差值提示（宿主政策）。
- **反向变换逐项执行**：非当前格 = `Decompose(ConvertToCanonical(表值, 约定), 约定)`（与逻辑同源的往返，杜绝第二份真相）；当前格 = `Decompose(账本现值, 约定)`。注意 `DecomposeFromCanonical` 是 `ConvertToCanonical` 的**逆序**（`Negate → OneMinus → ×100`），目前**尚未实现**（属本模块待补功能面）。
- **`{Attr:X}` 的口径提醒（写进文档防误修）**：展示读属性**现值**，而计算侧 `AttributeScaled` 是 PV-3 快照语义（施法瞬间冻结）——两者在"施法后属性变化"的窗口里允许不一致，此为 D5-17 既定取舍（与逻辑同源不同时机），非缺陷。

---

## PV-10：源的可枚举能力（已拍板 2026-09-14）

**唯一新增机制**（整表 + 当前档高亮展示的支撑——原 `{ParamSeries:X}` 词法载体，**2026-09-15 随 D5-17 v3 更名 Series 视图**，机制不变）：

- **`FTcsParamEnumerableSource : FTcsParamValueSource`**（可选能力基类，住 **TcsCore**，与值来源策略同族）：`Enumerate(Level, OutValues, OutCurrentIndex)`（枚举条目 + 当前档索引）与 `GetIndexForLevel(Level)`。
- **索引解析唯一真相**：clamp（Array 越界 → 末项）、Map 键查找（miss → 无当前项）、未来曲线采样政策——**全部写在源自己身上**，且 `Evaluate` 内部走同一份解析——这是"tooltip 高亮 10%、实打 7.5%"唯一的防漂移手段。
- **能力探测 = 虚分派/checked cast，不做中心 switch**（D3-7 v3 形态）；不可枚举的源配序列展示 = **M8 配置错误**（v3 起走视图 `IsCompatible` 探针、错误挂配置元素），运行期优雅降级单值。
- **命名纪律**：本能力不引入新的 `Resolve` 用法（Resolve 仅句柄/Id→对象）。
- **曲线源（LevelCurve）的展示策略**：区间表达**零能力即可用**；"整表 + 高亮"对曲线无天然格数，需求出现时由源自定义采样政策（元数据给采样范围），不在本轮预建。
- **落地时机**：随 TcsState 等级源（PV-4 四源）同批进 TcsCore——**不进 plan1 Task 0**（零消费者不预建）。

---

## PV-1 增补：上下文补齐与载体便利转发（已拍板 2026-09-14）

- **`FTcsParamEvaluateContext` 补齐两字段**（PV-1 原口径：Subject 句柄 + 参数表只读 + 级别位；代码 2026-09-11 只落了 `ParamTable`）：
  - `Subject`：单位句柄 `FCombatEntityHandle`；
  - `EffectiveLevel`：级别位 `int32`。
  - **落地时机 = 随 TcsState 等级源同批**（它们是等级源/属性源的唯一消费者）。
- **`FCombatEntityHandle` 落 TcsCore 的边界记录**：plan1 口径"具体句柄类型由各域定义"，但上下文住 TcsCore 且 Core 不得反向依赖域模块——故实体身份句柄是 **Core 唯一持有的战斗实体词汇**，属"零战斗词汇"边界上的**明示让步**（02 §2.2a / 06 已把它当全插件通用实体键使用）。
- **`FTcsParamValue` 补 `Evaluate` 便利转发**（消解调用点每次 `.Source.Get()` 的噪音）：
  ```cpp
  double Evaluate(const FTcsParamEvaluateContext& Ctx) const
  {
      return Source.IsValid() ? Source.Get().Evaluate(Ctx) : 0.0;   // 空载体兜 0
  }
  ```
- **PV-8 校验矩阵扩展为四联**：`源类型 × 上下文类型 × 可配约定（D5-18 v3）× 可配词法（D5-17 v2；**2026-09-15 该轴随 D5-17 v3 更名"视图兼容"**——视图 `IsCompatible(Probe)`）`——M8 显式交付物。

---

## 命名批（2026-09-14，用户逐项裁定）

判据三条：① **含 Notation 类型的形状不得进 TcsCore**（Core 零 Notation 引用检查点）→ 参数行必须住有 Notation 边的模块；② 谁定义 Def 形状谁持资产类；③ 动词纪律（避开 `Resolved` 这类泛化命名）。

| 概念（文档用词） | 实现名 | 住哪 |
|---|---|---|
| 数值参数行 `{Key, Base, Mode, ValueConvention}` | `FTcsNumericParamRow` | TcsState（FSkillDef 继承白拿） |
| 布尔开关行 | `FTcsBoolSwitchRow` | TcsState |
| 参数行 Mode | `ETcsParamMode{ Snapshot, Live }` | TcsState |
| 参数修正行（原 `FNumericParamModifier`） | `FTcsNumericParamModifier` | TcsSkill |
| Def 资产基类（DefId + IsDataValid 校验挂点） | `UTcsStateDefAsset`（对应 `FStateDefBase` 家族；备选 `UTcsStateDefAssetBase`） | TcsState |
| Buff / 技能 Def 资产 | `UTcsBuffDefAsset` / `UTcsSkillDefAsset` | TcsState / TcsSkill |
| DataTable 双轨行 | `FTcsBuffDefTableRow` / `FTcsSkillDefTableRow{ FName DefId; …Def; }` | 各域 |
| 参数快照条目（规范值 + 源引用位） | `FTcsParamSnapshotEntry`（**不用 `Resolved*`**——动词纪律） | TcsState |
| 参数快照容器 | `FTcsParamSnapshot` | TcsState |
| 源的可枚举能力基类 | `FTcsParamEnumerableSource` | **TcsCore** |
| ~~组装器取值回调 / 序列 / 区间视图~~ | ~~`FTcsDescriptionSource` / `FTcsParamSeriesView` / `FTcsParamRangeView`~~（**已被 D5-17 v3 取代**——见下文 v3 节命名批增补） | TcsNotation |

- **范围澄清**：DefLibrary 管辖的是 **`FStateDefBase` 家族**（Buff + Skill）；修正器模板（`UTcsAttrModDef`/`UTcsSkillModDef`）与属性词表（AttributeDef）是**并列的另一族**，不共用该资产基类——故基类名不用 `UTcsDefAssetBase`（会暗示覆盖全部 Def）。
- **钉法**：设计文档保留概念名，实现名在此表 + 各模块轮的**计划类型清单**里钉（README 惯例"设计文档类型名为概念名，R3 执行以计划为准"）。

---

## D5-17 v3：描述视图策略化（2026-09-15 补充轮——用户重设计）

**用户立场（四点）**：①View 的配置**在文本描述侧、有单独的配置空间**（不能配在 ParamSource 上——同一参数在不同文案要用不同形态）；②认同 Kind+FInstancedStruct / 策略形态；③ParamSource 与 ParamView **不可能完全匹配**，且必须允许**项目自定义专属视图** → 文本内嵌 token（v2 词法）**无法在编辑器预警**（错误悬在共享文案里、无法定位到配置元素）——废；④Source×View 是**多对多**（同一技能简单描述 = 当前档单值、复杂描述 = 全表 + 高亮，并存）。

**拍板**：

- **StringTable 回归纯文案**：占位符只有**槽名**（FName）——零语法；v2 词法 `{Param:X}`/`{ParamSeries:X}`/`{ParamRange:X}`/`{Attr:X}` 废除。
- **视图 = 策略形态**（D3-7 v3 同构）：`FTcsParamView` USTRUCT 基类（`BuildText(Ctx)` + `IsCompatible(Probe)` 虚函数）+ `TInstancedStruct` 持有——Kind+FInstancedStruct 与策略形态同物，取策略形态（零 Kind 维护、灭"类型与载荷配对"bug 类、picker 开箱、与全插件策略位同构）；装饰器组合 YAGNI。
- **配置空间**：`Def.Descriptions: TArray<FTcsDescriptionEntry{DescriptionId, TextKey, Views: TArray<FTcsDescriptionViewSlot{SlotName, View}>}>`——取代单字段 DescriptionTextKey；多描述入口（Tip/Codex/LevelUpPreview）承载多对多。
- **内置视图最小集**（v2 词法转世，语义不变）：`Value` / `Series` / `Range` / `Attribute`；视图自带配置参数（ParamKey/样式键/分隔符）。
- **数据面**：`FTcsViewBuildContext`（内容模块适配器装配；`GetValue`/`TryGetSeries`/`EvaluateAtLevel`/`TryGetConvention`；**瞬时非反射**——PV-1 反射禁令只约束序列化面，StateTree `TestCondition` 同款先例）+ `FTcsViewProbe`（纯数据能力探针）。
- **编辑器预警四层**：源能力徽章（预防）→ `IsCompatible` 保存期拦截（**错误挂配置元素 + 可操作建议**）→ 槽名交叉扫描（缺=错误 / 孤儿=警告，扫描器零语法解析）→ 运行期只降级。不变式：**编辑期必须报错、运行期只许降级**。
- **扩展成本**：宿主自定义视图 = **一步**（定义 struct → Def 里选它），零公共代码（扫描器/组装器/注册表/M8 矩阵全不动）；新增源能力（罕见）才动公共面，成本按"能力"计、多视图分摊。
- **取代清单**：v2 的 `GetView(Key, Kind, Out)` 泛化回调、`FTcsDescriptionSource` 四方法回调、`FTcsParamSeriesView`/`FTcsParamRangeView`、词法注册表方案。**保留**：索引口径双处、当前格账本现值、Decompose 逆序往返、约定白名单（D5-18 v3）、降级不变式、展示政策归开发者。

**命名批增补（v3，提案名待用户终审）**：

| 概念 | 实现名 | 住哪 |
|---|---|---|
| 视图策略基类 | `FTcsParamView`（`BuildText` / `IsCompatible`） | TcsNotation |
| 内置视图 | `FTcsParamView_Value` / `_Series` / `_Range` / `_Attribute` | TcsNotation |
| 描述视图槽位 / 配置条目 | `FTcsDescriptionViewSlot` / `FTcsDescriptionEntry` | TcsState（Def 形状所在） |
| 视图构建上下文 / 兼容探针 | `FTcsViewBuildContext` / `FTcsViewProbe` | TcsNotation |

**折入清单（v3 补充轮，2026-09-15）**：11（§1/§2.2 重写/§2.3/§3/§4/§6/§7/§8/§9）、05（§2 + 修订记录）、03（§2/§3.6 + 修订记录）、08（§2/§4/§7）、01（§2.1 四联矩阵措辞）、m5（D5-17 v3 行）、PV 文档（PV-8/PV-10/拍板汇总）、README（文档清单 + 决策日志 v3 条）、openspec/project.md、本文档（本节 + 命名批注记 + 拍板汇总）。

---

## 折入清单（本轮执行）

| 文件 | 改动 |
|---|---|
| `01-module-m0-core.md` | §2.1 补：Evaluate 便利转发、上下文补齐（含落地时机与当前偏差）、`FTcsParamEnumerableSource`、`FCombatEntityHandle` 边界记录；§6 依据 |
| `02-module-attributes.md` | §2.2 优先级带注明参数链同用；§3 聚合补"折叠器单份三处共用"；§7 依据 |
| `03-module-states.md` | §2 参数行补 Mode 列与实现名；§3.1/§3.6 快照类型名 + buff tooltip 索引口径；修订记录 |
| `05-module-skill.md` | §3.4 参数链改带式（D5-5 v3）+ 实现名；§3.4 PV-9 措辞同步；修订记录 |
| `08-module-editor-tooling.md` | §2 校验器补两矩阵；§4 占位符扫描扩为词法族 |
| `11-module-notation.md` | §1 载体名过期修正；§2.2 词法族 + 绑定链；§2.3 边界补充；§3 写入点补链行/时值；§4 组装器契约；§6 非目标；§7/§8/§9 |
| `2026-09-10-param-value-source-decision-points.md` | PV-1 增补注记、新增 PV-10 节、PV-7 措辞同步、拍板汇总 |
| `2026-09-02-m5-skill-decision-points.md` | D5-1 `AddPct`→`PercentAdd`；D5-5/D5-17/D5-18/D5-19 修订注记 + 新增 v3/v2/v3 行 |
| `2026-09-02-m0min-m2-decision-points.md` | D2-10 条目"M5 参数链不跟进"口径作废 |
| `README.md` | 文档清单 + 决策日志新增条目 |
| `openspec/project.md` | 架构模式段补一行 |
| `Source/TcsCore/Public/Parameter/FTcsParamSource_Literal.h` | 注释措辞修正（书写值定性） |

---

## 拍板汇总

| 项 | 裁决 |
|---|---|
| D5-5 v3 | 参数链 = **带式聚合**（五带同 M2、顺序无关）；SortKey 退化为带权；折叠初值 = 参数行求值结果否则 0；CompeteGroup 选优后进带；**折叠器单份住 TcsAttribute，M2/M5/Damage 流程属性三处共用** |
| D5-17 v2 | **词法族**：`{Param:X}` 单值 / `{ParamSeries:X}` 整表+当前档高亮 / `{ParamRange:X}` 区间（零新能力）/ `{Attr:X}`；**绑定靠键名 + 内容模块适配器 + 源虚分派**（文本层不认识源）；取值回调契约住 Notation（纯数据面）；索引口径技能=EffectiveLevel、实例=快照 Level；当前格取账本现值；**展示政策归开发者，TCS 只提供选项**（**token 载体 2026-09-15 被 v3 取代**——语义转世为内置视图，见下） |
| **D5-17 v3（2026-09-15）** | **描述视图策略化**：StringTable 只含槽名（零语法）；视图 = `FTcsParamView` 策略基类 + `TInstancedStruct` 持有；配置空间 = `Def.Descriptions`（多描述入口 × 视图槽位，多对多）；内置 Value/Series/Range/Attribute；编辑器预警四层（探针拦截挂元素 / 槽名交叉扫描 / 运行期降级）；宿主自定义视图一步接入；见 D5-17 v3 节 |
| D5-18 v3 | 约定列 = 行级书写口径；**可配白名单**（Literal/表型源可配，ParamRef/AttributeScaled 禁配）；链行 Operand 补约定列；`Literal.Value` 定性为配置态书写值；M8 校验 |
| PV-10 | `FTcsParamEnumerableSource`（Core，可选能力基类）+ `Enumerate`/`GetIndexForLevel`；**索引解析唯一真相在源**；不可枚举源配序列展示 = M8 报错（v3 起走视图 IsCompatible 探针）；曲线源区间可用、序列待需求 |
| PV-1 增补 | 上下文补 `Subject`/`EffectiveLevel`（随等级源同批）；`FCombatEntityHandle` 进 Core 的边界让步记录；`FTcsParamValue.Evaluate` 转发；校验矩阵升级为四联 |
| 命名批 | 见上表（`FTcsNumericParamRow`/`UTcsStateDefAsset`/`FTcsParamSnapshotEntry`/`FTcsParamEnumerableSource` 等） |
