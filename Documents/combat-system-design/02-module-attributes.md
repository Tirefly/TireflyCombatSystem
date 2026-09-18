# 02-module-attributes.md — M2 属性/数值层设计

- 日期：2026-09-02
- 状态：设计 v1（编译层模块名 **TcsAttribute**，R0 §9；依据已拍板决策 D2-1~D2-5、D0-1/D0-2）
- 职责一句话：**纯函数式数值聚合器——给定属性词表与 modifier 集合，计算当前值并管理变更事务。M2 不认识时间、不认识 buff、不认识任何具体属性。**

## 1. 模块边界

- 消费者：M3（buff 挂 modifier）、TcsDamage（瞬时流程的属性读取与临时修正器；**Damage/Heal/ModifyFlow 原语+执行器亦住 TcsDamage——D4-14 归属翻转**）、M5（技能消耗）、M8（表编辑器）。
- 依赖：M0（句柄/总线/时钟）+ **TcsNotation（D5-18 v2：UTcsAttrModDef 约定列与物化转换——账本/聚合运行时零约定逻辑，"账本不做二次猜测"不变）**；**不依赖 M3/M4**——modifier 的来源只是一个句柄。

## 2. 类型词汇（对外）

### 2.1 属性词表（D2-1 终定）
- `FAttributeName`：FName 包装结构体，**explicit 构造**（裸 FName/TEXT 传不进属性 API）；内部缓存稠密 int32 id（注册表世代号校验失效）。
- 项目侧常量：`UE_DECLARE_COMBAT_ATTR / UE_DEFINE_COMBAT_ATTR` 宏（仿 GameplayTag 宏模式）；词表 + DevSettings 注册，**编辑器即时响应**。
- `FAttributeRegistry`：启动注册（重名/非法引用**加载期报错**）；`Resolve(FAttributeName) -> int32` 稠密 id；行名 ↔ 常量约定映射校验。
- **属性定义（2026-09-17 双轨制定案，实现名）**：**表 = 编辑期载体、资产 = 运行期载体**（用户口径：DataTable 便于策划批量编辑，**不作为运行期加载源**；运行期一律走资产——资产制扩展性好，未来给定义加 Fragment 之类只动资产与定义行）：
  - **定义行 `FTcsAttributeDefTableRow : FTableRowBase`（字段形状的唯一声明处）**：`BaseValue` + `Bounds` + `ValueDomain`（D2-6 值域模式挂定义）——项目词表表 `DT_AttributeDefinitions` 的行类型，**身份 = 行名（= 属性名**，D2-1 的"行名 ↔ 项目侧常量"映射），**行内不带 id**（2026-09-17 用户口径：DataTable 的键就是行名，行内再放一份是双真相）；
  - **运行期资产 `UTcsAttributeDef : UPrimaryDataAsset`**：持自身 `DefId` 与**组合持有的一行** `Def`（不复制字段集）；**主资产身份 = `[PrimaryAssetType, DefId]`**（显式声明类型常量、覆写 `GetPrimaryAssetId`——名取 DefId 不取资产名，资产文件可自由改名/挪目录）；`IsDataValid` 只报空 DefId（"资产名与 DefId 同名"不再是要求）；两类型**同住 `TcsAttributeDef.h`**；
  - 两轨一致性由 08 §5 的编辑器同步器维护（资产为权威，M8 工具面）；**运行期零 DataTable 加载路径**。
  - 单位实例由定义行初始化后**自持**这些字段（实例不持定义引用——热路径不回查定义，D2-1/D2-9）；单位侧调用面 `AddAttribute(单位, 属性名)` / `RemoveAttribute(单位, 属性名)`——**定义解析是门面内部流程**（门面持属性定义表，`RegisterAttributeDef` 由宿主/DefLibrary 加载后登记）。
- **候选能力：AttributeSet → 已升为正式裁决（D2-15，2026-09-17）**：见 §2.2a 的"AttributeSet"条与决策文档 `2026-09-17-attribute-set-and-existence-decision-points.md`（形态 B1a+B2+B3a：实体侧引用 / 资产 GameInstance 级 + 施加 World 级 / diff 替换；内容 = `DefIds`，覆写列首版不做；实现随 M6 轮）。
  - 与 `FTcsAttributeStore` 的联动（随 D2-15 一并定）：①**施加/撤离 = roster diff 替换**（旧 Set 独有 → `RemoveAttribute`；新 Set 独有 → `AddAttribute`；共有 → 保留实例）；②**Store 记"当前生效的 Set"**（软引用或 Id）——切换要 diff 谁、屏显"属性来自哪套"均可查；③**实例级溯源不做**（D2-14：动态增删不承诺精确撤销）。
  - **建议的边界（重要）**：**属性 = 结构（单位注册期/模式切换时确定的 roster），修正器 = 动态**——§2.2a 已写"基础值：属性表默认 → 实体注册时初始化"，即属性集合本就在注册期确定；buff/装备/技能一律走 modifier，不动属性集合。守住这条边界，实例不需要来源追踪，M2 保持简单。**反向情形**（"装备授予一条新属性"这类动态属性增删）才需要溯源机制，属更大的决定，值得单独一轮裁决。
- **M2 零词汇**：核心不特殊化任何属性（Health 也不例外）；"血量归零死亡"是上层属性条件规则。

### 2.2 Modifier 与来源（D2-2）
- `FAttributeModifier`：`Attribute(FAttributeName) / Op(EAttributeOp) / Operand(FTcsAttrModOperand) / Source(句柄) / SortKey / SourceTag`——**权威完整形状** {Handle, Op, Operand, Source, SortKey, Tag}（实例侧见 §2.2a）。
- **`SortKey` 的定位（2026-09-18 补记，此前本文档只列了字段未写用途）**：它是一个 **int32 排序/裁决键**，但在**属性聚合里是零语义的展示/审计位**——D5-5 v3 把折叠改成"顺序无关、带序只由 `Op` 决定"之后，折叠器**不接收**该字段（规格有专门场景钉住），保留它是为了审计、调试与 UI 排序。**它只在别处有语义**：09-module-damage 里"免疫/减伤裁决选一"与"消耗型流程步骤裁决选一"靠 `SortKey` 决定谁生效。两者作用域不同，**不得**用它承担覆盖带强弱（那会让"带序唯一真相在 Op"与"折叠不依赖 SortKey"两条既有纪律出现第二真相）。
- **覆盖带强弱键（2026-09-18 新增，与 SortKey 各管一段）**：`OverridePriority`（修正器侧，`int32`，**仅 `TAO_Override` 读**，大者胜）+ `OverrideTieBreak`（属性定义侧的四值策略，同优先级时比较数值）——这才是"多条 Override 里谁生效"的唯一依据，口径见 §3 第 2 步。
- **FTcsAttrModOperand（D2-11，主属性→派生属性载体）**：`{Kind: OPK_Literal(默认)|OPK_AttributeScaled, Literal, Attribute, Coefficient}`——OPK_AttributeScaled 时 Operand = Coefficient × Current(Attribute)，聚合收集时求值并**读即登记依赖边**（D2-3 现成机制）。"1 力量=2 攻击力" = AttackPower 常驻修正器 `{TAO_Add, AttributeScaled(Strength, 2.0)}`（Source=系统常驻句柄约定）；纯派生属性 = Base 0 + 仅此条。取证：AbilityKit `MagnitudeSourceType`（ContextFloat/TimeDecay）同构【源码：modifiers 包】；其表达式引擎（AttributeExpressionFormula 667 行 DSL）**不采纳**——封闭公式原则（无表达式语言）；非线性派生走判定树①上游算/宿主命令式。流程属性黑板同构适用、按需引入。**仅此两种 Kind**——已考察拒绝的候选（ContextFloat/TimeDecay/随机/跨域引用/Custom Fragment）及三条不变式依据见 D2-11 拒绝清单。
- **载体与双形状（D2-12/D2-13；载体被 PV 系列取代 2026-09-11）**：定义侧（模板/Def 配置）`FTcsAttrModOperandDef`——Literal 为 **FTcsParamValue{TInstancedStruct<FTcsParamValueSource>}**（原 FTcsParamScalar；可配 Literal/ParamRef/等级表/AttributeScaled 等源，模板默认值可等级化）；运行侧（M2 账本 ModifierSlots）`FTcsAttrModOperand`——Literal 恒为已解析 double（物化器单点转换，miss 兜底默认；账本不做二次猜测）。OPK_AttributeScaled 两侧同形（live 求值不物化）。**M2 账本 Operand 保持 `{Literal, AttributeScaled}` 封闭不动**（D2-11 拒绝清单不波及——参数源体系住 M2 之上）。
- **修正器模板 UTcsAttrModDef（D3-19）**：独立资产（纯模板=默认值+ParamKey 声明+ValueConvention 可选列——模板默认 Literal 策划语言书写、物化时转换）；FStateDefBase.ModifierRows 纯引用（**行仅存 TemplateId FName——不具名任何模板类型，TcsState 零 TcsSkill/TcsAttribute 类型边**）；物化执行器住 TcsState（本模块提供类型）；**UTcsSkillModDef 住 TcsSkill**（技能参数域词汇）。**基类 = `UPrimaryDataAsset`（2026-09-17 定，Def 资产族统一约定）**——Def 的引用语义是"FName Id + 注册表/DefLibrary 解析"，主资产身份让该解析与按类型发现/加载归引擎；未来 `UTcsStateDef` 家族与 `UTcsSkillModDef` 同此基类（族内不混用两套基类）。**同款双轨组织（2026-09-17 三次复评）**：定义字段的唯一声明处是表行 `FTcsAttrModDefTableRow : FTableRowBase`（模板字段；**身份 = 行名 = 模板 Id，行内不带 id**），资产 `UTcsAttrModDef` 组合持有一行 `Def`（不复制字段集）且**主资产身份 = `[PrimaryAssetType, TemplateId]`**（覆写 `GetPrimaryAssetId`，名不取资产名）——表供策划编辑、运行期走资产；**局限在案**：模板行含 `FTcsParamValue`（`TInstancedStruct`）列，CSV/Excel 往返丢该列，只支持编辑器内表格编辑。
- 优先级带（已定裁决 + D2-10 增带）：`Override(0) > Add(10) > PercentAdd(15) > Mul(20) > FlatAdd(30)`；组内顺序无关公式。**参数链同用此五带与同一折叠器（D5-5 v3，2026-09-14）**——M2 属性聚合 / M5 参数链 / TcsDamage 流程属性三处共用一份纯函数折叠器，语义单一份。
- **M2 无计时器**：modifier 生死完全绑定来源句柄；`RemoveBySource(unit, Source)` 级联撤销；无来源裸 modifier = 系统级常驻来源句柄约定。
- `EAttributeOp` **纯封闭五带（D2-7 砍 Custom op、D2-10 增 FlatAdd，M9 收尾轮定稿；实现枚举 `ETcsAttributeOp`，值名 `TAO_*`）**：`Add(0) / Override(1) / PercentAdd(2) / Mul(3) / FlatAdd(4)`——无 Custom 位（计算在上游求值传入终值，聚合无任何代码插点，D0-1 论证彻底干净）；**新运算 = 末尾追加枚举值**（判定树②加带路径，防旧配置失效）。~~带 Custom 逃逸位（值 1）~~为残留旧文已修正。

### 2.2a 属性数据宿主（AttributeStore / AttributeInstance，2026-09-02 补——用户指出的缺失节）

- **宿主**：`UCombatAttributeSubsystem : UWorldSubsystem`（M2 拥有）→ `FAttributeStore`（per entity，`FCombatEntityHandle` 键控）。与 D3-1 **同型不同类**：中央句柄键控 + 适配器缓存 Store 指针——按域各自实现（TcsAttribute 独立模块不把桶类型放他域）；句柄与代际机制统一在 TcsCore。**"缓存 Store 指针"的引擎前提（2026-09-17 实测）**：`TMap`/`TSet` 元素存在连续缓冲里，**扩容即搬移**——按值存容器会让缓存指针悬空，故外层注册表 MUST 经间接层（`TMap<句柄, TUniquePtr<Store>>`）；容器**内部**实例指针不保证跨插入稳定（按名查询），"热路径不重查定义"（D2-9）由实例自持定义字段满足。
- **FAttributeInstance（每属性一条）**：`{Attr, BaseValue, CachedCurrent, bDirty, Min/Max 边界, ModifierSlots}`。词表 schema 由 `FAttributeName` 稠密序号缓存覆盖（D2-1）——**实例无需 Def 缓存**；实例-定义引用规范同 D2-9（权威=键，热路径走 schema 缓存）。
- **基础值**：由该单位的 **AttributeSet 在注册期初始化**（见下"属性集合"条）；**等级成长 = 宿主升级事务 `SetBaseValue`**（等级→数值映射归项目，引擎不内置）。
- **属性集合（D2-14 裁决，2026-09-17——能力与推荐用法分开书写）**：
  - **推荐用法 = 结构**：roster 由 AttributeSet 在**初始化/重置**时确定；运行期只动 modifier（挂/移除/改基值/来源级联）——属性集合的变更只有一种形式 = **重设 Set**。
  - **框架能力 = 允许**：`AddAttribute` / `RemoveAttribute` 是常驻能力，引擎**不禁止**运行期动态增删（不替使用者设限）。
  - **不做溯源**：实例不加来源字段 ⇒ **不承诺动态增删的精确撤销**（不记"谁加的"）。**代价与兜底（2026-09-18 修订：加冻/解兜底）**：
    1. **撤销粒度仍只能到"整条属性"**：框架不知道有几方在要求某属性存在——两个来源都要 `Health` 时，其中一个撤销只能整条删。**但后果已降级**：属性被**冻结**（见下）而非销毁，数值效果与基础值都留着，属性被加回即恢复；不再是"数值永久丢失"。要精确到"来源级"仍只能由宿主层自行记账。
    2. ~~`RemoveAttribute` 丢弃槽位内修正器~~ → **已由冻结/解冻消除**（见下"属性冻结暂存区"）。
    3. ~~重新加回 = 全新实例、基础值回定义默认~~ → **已由冻结/解冻消除**（整条实例进出暂存区，基础值保留）。
  - **属性冻结暂存区（2026-09-18 用户拍板；设计定稿，代码待落地，见下方实施状态）**：`RemoveAttribute` = **冻结**——把**整条属性实例**（基础值 / 边界 / 值域模式 / 修正器槽位）从单位容器搬进暂存区，不销毁、不丢数据，输出日志；`AddAttribute` = **解冻优先**——暂存区有同名实例则整条搬回（基础值取回冻结前的值、槽位原样保留），否则按定义行新建，两者都输出日志。**双态约束**：同名实例不得同时存在于容器与暂存区（搬移语义保证）。**与来源撤销的共存（硬规则）**：`RemoveBySource`（聚合管线轮落地）**必须同时扫描容器与暂存区**——否则来源在属性被冻结期间结束、其修正器永久滞留，属性恢复时**凭空多出数值**（比丢数值更难查）。**不做**：条目上限、保质期清理、解冻时校验来源存活性（框架判断不了来源死活——`FTcsSourceHandle` 只是编号、无存活性登记，属既有设计）。
  - **实施状态（2026-09-18 更新）**：本条与"冻结暂存区"**已随 plan1 Task 5 落地**——`RemoveAttribute` = 冻结整条实例（`FTcsAttributeStore::FrozenAttributes`）、`AddAttribute` = 解冻优先、`RemoveBySource` 扫描面含暂存区、双态约束在装置中有检查。规格落点：`attribute-store`（MODIFIED + ADDED）随提案 `add-tcsattribute-pipeline-and-transaction` 归档。
  - **AttributeSet 与运行期动态集合分离**：Set 的职责只有**初始化/重置**，不是"运行期属性集合的唯一入口"，也不承担动态增删语义。
- **AttributeSet（D2-15 裁决，2026-09-17；实现随 M6 轮）**：`UTcsAttributeSetAsset : UPrimaryDataAsset`——"某情景下该单位用哪些属性"的宿主可配资产，内容 = `TArray<FName> DefIds`（由 DefLibrary 按 `[PrimaryAssetType, DefId]` 解析；**覆写列首版不做**，同属性不同情景不同基础值走宿主 `SetBaseValue`）。
  - **粒度 = 实体侧引用**（B1a）：引用点在实体 Def / Actor BP 的组件配置上；**引擎不认识"游戏模式"轴**——换情景 = 宿主换实体身上的 Set 引用（或换实体）。
  - **生命周期与施加**（B2）：Set 资产 = **GameInstance 级 Const 内容**（归 DefLibrary 管辖面）；"某单位当前用哪套 Set" = **World 级**可变状态；施加入口在 `RegisterEntity` 之后、穿过 DefLibrary `IsRuntimeReady()` 门禁之后（06 §2.1/§2.2）。
  - **切换语义 = diff 替换**（B3a）：`ApplyAttributeSet(单位, Set)` = 把该单位的属性集合置为这套（旧 Set 独有 → 移除、新独有 → 添加、**共有保留实例**——不打断在飞 modifier、不丢 `SetBaseValue`）；`ClearAttributeSet(单位)` = 整组清空（供重生成单位用，在飞 modifier 随之失效）。
  - 决策文档：`2026-09-17-attribute-set-and-existence-decision-points.md`（含未采纳方案 A2/A3/B1b 的取舍与"动态增删未决语义"记录）。
- **当前值**：`CachedCurrent` = 派生缓存非权威——聚合管线唯一生产者，惰性重算（脏则算）；永远可由 Base+修正器+公式重建（操作复制+客户端重算的地基）。
- **FAttributeModifierInstance**：挂在被修饰属性的 **ModifierSlots**（槽位自由链表/数组+freelist，M0 池机制复用；TCS/AbilityKit 同款）：`{Handle, Op, Operand, Source, SortKey/Tag}`；Source 级联移除；聚合收集按属性遍历（零查找）。
- **三种修正器存放地总表**：实体属性修正器→FAttributeStore 属性槽（Source 级联）；技能参数修正→FLearnedSkillEntry 参数链集（M5）；流程属性修正→FDamageFlowContext 流程属性容器（TcsDamage，流程结束即弃）——同一形状（Source 级联），作用域容器不同。
- **一致性待办**：03 文档 `UCombatStateRegistry` 与 06 文档 `UCombatWorldRegistrySubsystem` 命名统一（M6 拥有世界子系统壳；M2/M3/M5 域 Store 挂载方式实施期定）。

### 2.3 读侧契约
- `ICombatAttributeProvider`（UINTerface，M2 对外唯一契约）：`GetBaseValue / GetCurrentValue(FAttributeName)`、`PeekPending`。军官组件与 Mass 存储桶适配器都实现它（计算器不关心单位载体——M6 的适配在此收敛）。
- `GetCurrentValue` = 脏则惰性重算，永远返回聚合+clamp 后值；事务期读旧值，`PeekPending` 供表现层预览（D2-5）。

## 3. 聚合管线（核心规格）

> **术语分层（2026-09-18 用户拍板保持）**：**"聚合"指本模块与管线整体**（聚合管线 / `FTcsAttributePipeline`），**"折叠"指那份共用的带式计算核**（折叠器 / `FoldTcsAttributeBands`）——两层各有其名，不混用。**不与 GAS 的"Aggregator"对齐命名**（已核 GAS 源码）：`FAggregator` 是**有状态容器**（持基础值 + 按运算分桶的修正器 + 挂摘 API，对应我们的属性实例 + 门面挂摘），真正算公式的是 `FAggregatorModChannel::EvaluateWithBase`（`GameplayEffectAggregator.cpp:74-84`）——它不叫聚合器；且 GAS 的 Override 取**首个合格项**（`return Mod.EvaluatedMagnitude` 直接返回、无任何大小比较，依赖遍历顺序），我们用**优先级 → 策略 → 有符号值的全序**（顺序无关，有意改进）。把"聚合器"借给纯函数会与"聚合管线"撞名，并暗示它带状态。

> **管线可见性与调用纪律（2026-09-18 用户拍板）**：`FTcsAttributePipeline` 的**声明住 `Public/Attribute/`**（2026-09-18 从 Private 移出——确认未来有跨模块消费者直调它驱动某单位的求值/事务，但**不改内部执行逻辑**），实现在 `Private/`。**推荐路径仍是门面** `UTcsAttributeSubsystem`（唯一入口，游戏逻辑一律经它）；直调管线是**逃生口**（离屏工具、无门面引用等场景），两条路径操作同一份状态、MUST NOT 混用成两条记账路径。机制面（重算内核/依赖登记/求值栈）属 `private:`，**不构成消费契约**；实例由门面拥有，调用方 **MUST NOT 跨帧持有**（门面销毁后即悬空）。同仓既有同款形态见 TBNS（`Public/Pathfinder/TbnsPathfinder.h` + `Private/Pathfinder/TbnsPathfinder.cpp`）。

1. **收集**：目标属性的 modifier 按优先级带分桶。
2. **聚合**（顺序无关公式，D2-10 五带）：`Final = ((Base + ΣAdd) × (1 + ΣPercentAdd)) × ΠMul + ΣFlatAdd`（FlatAdd = 乘后平坦加，不受 PercentAdd/Mul 缩放——GAS FixedAdd 借鉴，M9 收尾轮拍板）；存在 Override 时按**覆盖带强弱口径**选出一条作为结果（**FlatAdd 一并被覆盖**，"最强覆盖生效"）。**覆盖带强弱口径（2026-09-18 用户拍板，落地于 plan1 Task 5）**——原口径"取 Override 组最大值"隐含"数值大 = 更强"，而**数值本身不含方向**：对护甲（越大越强）对，对承伤倍率/冷却（越小越强）就取到最温和的一条——同一套规则不可能两边都对。故定为三级阶梯：①`OverridePriority`（**修正器侧**，大者胜、**唯一第一裁决键**；可表达"数值更小的反而该赢"，如变身 B 护甲 30 覆盖变身 A 的 50）；②**同优先级策略** `OverrideTieBreak`（**属性定义侧**声明本属性的语义方向：取最大/取最小/绝对值最大/绝对值最小，封闭四值、**不开放自定义**——热路径比较函数必须全域且确定）；③有符号值（补齐全序：`OTB_MaxAbs` 下 ±5 这类"策略下打平"必须有确定答案，否则赢家取决于遍历顺序）。**策略住属性定义而非修正器**：放修正器上会变成"两个来源各说各话"，等于又需要一条规则来裁决规则。**框架不定义任何其它"谁盖谁"的规则**（用户口径：跨来源协调完全交给 Priority，否则属二次规则、"到底以哪个为准"对使用者有争议）；默认值 = 历史行为（全 0 + 取最大值 ≡ 旧的组内最大值；组内多 Override 不再是"配置冗余"，而是可解释的排座次）。**运算枚举纯封闭五带（2026-09-02：Custom op 砍除 D2-7、FlatAdd 加带 D2-10）**——计算在上游（施加链/伤害流程/项目代码）求值并传入终值，聚合无任何代码插点（D0-1 论证彻底干净）。**折叠器单份（D5-5 v3，2026-09-14）**：本式 + Override 覆盖语义抽为一个纯函数**住本模块**，M5 参数链与 TcsDamage 流程属性容器**共用**（§2.2a 三处"同一形状、作用域容器不同"的复用落点；参数链的折叠初值 = 该键参数行的求值结果，M2 侧对应 `BaseValue`）——归属依据：三个消费者的编译边都已含 TcsAttribute（零新边），且不把 Op 枚举与带序词汇搬进零语义的 TcsCore。**`SortKey` 在本式里是零语义字段**（定位见 §2.2 的字段说明）。
3. **clamp（D2-4）**：末端统一 clamp；`FAttributeBound` 三态 `None / Static / Dynamic(FAttributeName)`；**AttributeDef 值域模式（D2-6，2026-09-02 采纳）**：`ValueDomain{Clamp(默认)/Wrap/CustomFragment(IValueDomainPolicy 只接管值域函数，时序/级联/事务由引擎守护)}`——用户自定义 clamp 策略类失败的修正引入（钳制反应走事件订阅，管线不换行为）；Dynamic 边界先按管线求值（HP≤MaxHP≤Level 链，TCS 05:158 已验证形态）；**clamp 引起的 CurrentValue 变化继续级联标脏**（ongoing clamp 行为规格继承）；自引用禁止（注册期报错）。
4. **比较**：变化阈值 1e-5（已定裁决），无实质变化不广播。
5. **依赖图（D2-3）**：属性粒度；求值期间"读即登记"（`FAttributeEvalContext` 记录边）；拓扑重算；Kahn + Tarjan SCC 环检测（skip 剔除重试 / 严格报错）；**重算轮数上限与环判定解耦**（修 TCS 8 轮误判深链为环的缺陷）；图每轮重建不缓存（脏标记已限流，热了再优化）。

**新需求判定树（2026-09-02，"解释权出处唯一"的操作手册）**：
① 只是数值/幅度不同？→ 上游算好传终值（Param/修正器），公式零改动；
② 是新的"运算位置/种类"？→ 加带（唯一改公式的方式：引擎 C++，Authoring 第三层，需论证与既有带的交换性；**D2-10 FlatAdd 即首例**——交换性论证：FlatAdd 在全部缩放之后、clamp 之前，与既有带无交换歧义，与 Override 交互=被覆盖）；
③ 是值域语义不同？→ 值域模式（Wrap/CustomFragment，D2-6）；
④ 是"何时生效/对谁生效"的条件问题？→ 上游条件门控（触发行/施加条件），公式零改动；
⑤ 根本不是"持续量"？→ 不该是 M2 属性——瞬时量走 TcsDamage，计算产物走链/项目代码。

**锐化规则**：加带是最后一步且有门槛；**项目侧计算的是"修改量"永远不是"终值"**——终值只能由 M2 聚合器产出（绕过管线直写终值 = 第二解释权出处，评审盯防形态）。

## 4. 事务与 flush（D2-5）

- 单次管线事务：变更（挂/移除/改基值）在事务内完成候选集计算（求值→聚合→clamp 全在内存候选值上），**唯一提交点写回，失败零写入**。（**D2-14 追加，2026-09-17**：`AddAttribute`/`RemoveAttribute` 作为框架常驻能力，MUST 走同一 store 变更路径与同一事务纪律——批内加属性与批内挂 modifier 行为一致；其事务化随聚合管线轮落地，R3 的 Task 4 为直接路径。）
- **提交尾行内 flush** 广播变更事件（TCS 同款，核验纠正后确认）；帧末安全点（TG_PostUpdateWork）机制保留、**默认关**——承接跨调用栈脏项与广播期延迟补发。
- 事务事件：`FAttributeChangedEvent { Unit, Attribute, Old, New, Reason }`——核心词汇 FStruct，走总线（D0-3）。
  - **`Reason` 的实施状态：未落地（2026-09-18 拍板"先不加"）**。事件语义 = **当前值（对外可读值）变了**（结果事件，非操作事件）；因此"是 base 还是 current"这个二选一不存在——广播前提就是当前值变了，`Reason` 能表达的只是"**本次变化的原因里包含什么**"。候选取值域 = **位掩码** `None=0 / Base=1<<0 / Modifier=1<<1 / Dependency=1<<2`（位 0 = None，与"Custom 固定值 1"的逃逸位约定同族）——MUST 用位掩码而非布尔/单值枚举，因为"基值与当前值同时变"是常态（`SetBaseValue` 在无边界时两者同变；批内改基值 + 挂修正器会合并成一次广播）。**已知缺口并接受**：基值改了却被值域收口吃掉（满血时提上限）→ 当前值没变 → 不发事件；被否的补法有二：①基值变更无条件发事件（事件退化为操作事件，与"未变不广播"冲突）；②另立 `BaseChanged` 事件（多一条订阅面）——多数漏报已由边界属性自己的 ValueChanged 覆盖（HP 被 MaxHP 钳住时 MaxHP 会发自己的事件）。**排期**：等第一个真实消费者（血条飘字区分成长/buff、成长数字动画）出现时落地，届时按需求钉死取值域。

## 5. 网络姿态落点（NET-1/2）——**操作复制 + 客户端重算**

- 聚合是纯函数（D0-1 轻量纪律保证可重算），因此 M2 的网络策略定为：**服务器权威、复制 modifier 操作流（挂/移除/来源），客户端镜像侧重放操作 + 本地重算**——不复制数值。
- **姿态补充（D2-14，2026-09-17）**：属性**集合**的变更（`AddAttribute`/`RemoveAttribute`/Set 施加）在本期**不在复制面内**——原因是推荐用法下它只发生在初始化/重置（注册路径，两端各自确定性重建即可）；若项目在运行期动态增删属性且要联网，需要把"属性增删"作为**可选操作类型**加进操作流与 D6-4 契约载荷表（**留位不实现**——本期如实声明，不含混过去）。
- 收益：带宽极小；回滚只需重放操作；客户端值与服务端值天然一致（同一纯函数）。
- 姿态接口位：镜像侧 store 只接受操作流注入（同 M0 镜像模式）；本期不实现。
- 标注：此为**设计声明**（新推导，依据 D0-1 + D2-5 的纯函数性），非继承结论。

## 6. 非目标

不认识具体属性/死亡；不做计时与时长；不做 GAS 兼容层；不做值复制；不做多线程。

## 7. 依据

- 拍板：D2-1~D2-5、D0-1/D0-2（2026-09-02）；裁决（聚合公式、脏标记、1e-5）；**D2-10 FlatAdd 加带（M9 收尾轮：GAS FixedAdd 借鉴，命名用户定 TAO_FlatAdd）**；**D2-12 FTcsParamScalar / D2-13 Operand 双形状（R3 计划审阅轮 4/5；D2-12 载体后被 PV 系列 FTcsParamValue 取代，2026-09-11）**；**D5-18 v2 Notation 边（TcsAttribute←Notation：模板约定列）**；**D5-5 v3 折叠器单份（2026-09-14：M2/M5/TcsDamage 流程属性共用同一带式折叠纯函数，住本模块）**。
- 证据：TCS 04/05 核验口径（事务 7 项提交动作、提交尾行内 flush、ongoing clamp、读即登记、句柄级联移除）；AbilityKit attributes 取证（`AttributeGroup/Slot{BaseValue,Cached,Dirty}` 并行数组同构）。
- 用户贡献：FName 词表直觉（D2-1 两次质疑被采纳）；伤害计算器用例代码已在前轮对话确认。

## 8. 验收钩子

竖切剧本第 1（数值正确）、2（单次重算）、5（依赖铁律 grep）、6（数据驱动线：**新增纯数据链资产，零 C++**——M9 收尾轮后 buff 验证改测试 Source/链数据）项落在 M2。
