# 08-module-editor-tooling.md — M8 编辑器与工具层设计

- 日期：2026-09-02
- 状态：设计 v1（编译层模块名 **TcsEditor**，R0 §9；依据裁决 3、D4-1/D4-2/D5-7/D5-9、TCS 12 + orphan-scan 实证）
- 职责一句话：**让三张表（词表行/触发行/链）可被策划安全高效地编辑与验证——校验前置、可视化跟随数据形状、调试可解释。**

## 1. 模块边界

- 消费者：策划（日常）、程序（调试/新类型接入）。
- 依赖：全部模块的数据形状与注册表 API（**editor-only**——UnrealEd/Slate；M1–M5 数据形状为主，§4 消费 TcsDamage 流程黑板、§5 用 DefLibrary 接口（M6/M1）——工具作用于数据形状，不作用于世界接线）。

## 2. 校验器（编辑器期静态模拟）

- **词表引用校验**：链内 `Param()` 键 ∈ 该技能 NumericParameters/BoolSwitches（D2-1/D5-5 注册表桥接）；触发行 EventTag ∈ 事件词汇；`DefTag` ∈ 状态词表。**2026-09-22 tag 化后收窄**：拼写/存在性由 `FGameplayTag::IsValid()` 与 `RequestGameplayTag` 的 ensure 前移覆盖；本校验器剩的是"**已注册却指向不存在实体**"那一层（台账 T-10）。
- **参数源四联矩阵（D5-18 v3 / D5-17 v3 / PV-10 合并交付物）**：`源类型 × 上下文类型 × 可配约定 × 视图兼容`——①可配约定白名单（Literal/表型源可配；`ParamRef`/`AttributeScaled` 禁配）；②**视图兼容 = 视图 `IsCompatible(Probe)` 虚函数**（内置 Series 要求可枚举源；宿主视图自声明需要什么）；③可失败源 Fallback 必填、等级表非空、ParamRef 禁自引用/成环（PV-8）；④上下文扩展 cast 合法性。**错误挂具体配置元素（可点击定位）+ 可操作建议**（如"此键源不支持整表，建议 InstigatorLevelArray"）；编辑器保存期即报。
- **关系表静态检查**：Blocks/Requires 闭环与死锁预检（运行期 D2-3 的 SCC 是兜底，这里是前端）。
- **濒死配置扫描**：定义了但永不可达的行/链/CueId（无事件源、条件恒假——`AttributeCompare` 用编译期已知常量判假）。
- **孤儿扫描（orphan-scan 修复清单，TCS 实证缺陷）**：DefAsset 清理 = ①受管范围**白名单显式声明**（禁递归子目录隐式纳入）；②删除前必须**触发回写确认**；③重复 `DefTag` 判孤儿**加缓冲期**（标记→人工复核→二次确认才删）。
- 校验时机：保存期 + 手动"全量体检"按钮；结果 = 错误（阻断）/警告（放行+线索）。

## 3. 可视化（裁决 3：跟随数据形状，不做通用图编辑器）

- 修饰符/参数 = **表**（NumericParameters/BoolSwitches 双表直编）；状态 = **表 + 关系矩阵**（Blocks/Requires/Priority 矩阵视图）；链 = **管线图**（UEdGraph/SGraphPanel 只读视图先行，受限编辑后续——离散事件图表是 D3-5 留下的未来方向，第一版只读）；重定向栈 = **检查器**（对某技能列出当前生效的重定向链，模拟"这个单位身上谁在改它"）。

## 4. Explain 与调试面板

- **WhyNot 查询**：给定单位+触发行，回放四道门+条件逐项判定结果（`bConditionMissIsSilent=false` 的线索在此消费）。
- **描述槽名交叉扫描**（D5-17 v3，2026-09-15，消费 TcsNotation 扫描接口）：StringTable 原文只含**槽名**（零语法）——文本 `{槽名}` ∈ 该 Def.Views 列表（缺 = 错误，指名 StringTable 条目）；Views 声明未被引用 = 孤儿**警告**；视图兼容性在配置元素上校验（`IsCompatible`，见 §2 四联矩阵）——**扫描器零语法解析，加视图不改扫描器**。
- **链运行追踪**：FChainRun 生命周期事件（起链/每步/挂起/唤醒/结束原因）→ 时间线视图。
- **属性求值过程**：依赖图 + 每键修正链 + clamp 级联的中间值展示——覆盖 **M2 实体属性**（惰性重算回放）与 **TcsDamage 流程属性黑板**（伤害计算每键贡献：BaseDamage→修改器→Final 的逐级解释，开发期开关控制，生产不记——见 09 非目标）。
- 数据源 = M0/M2/M4 已埋的 Explain 钩子（D0-6 日志分类、D2-5 flush、D4-1 静默标记）；面板只是消费者。

## 5. DataTable 工具

- 双轨同步继承 TCS 03 模式（Def 资产 ↔ 表行，Row struct 整体赋值）；热重载 → DefLibrary 增量校验（M6/M1 接口）。
- **双轨制的职责分工（2026-09-17 用户口径，全 Def 族适用）**：**表 = 编辑期载体、资产 = 运行期载体**——DataTable 只服务策划批量编辑（表格/Excel 往返）与编辑器即时响应，**不作为运行期加载源**；运行期一律按 `DefTag` 解析资产（资产制扩展性好：给定义加 Fragment 之类只动资产与载荷，消费面不动）。故同步器是**编辑器侧**工具（保存资产 → 刷新行 / 导入表 → 更新资产，冲突以资产为权威）；运行期零 DataTable 加载路径。
- **Def 资产族统一基类 = `UPrimaryDataAsset`（2026-09-17 定）**：`UTcsStateDef` 家族 / `UTcsAttrModDef` / `UTcsSkillModDef` / `UTcsAttributeDef`——双轨的"资产轨"以主资产身份登记（`PrimaryAssetType` 对应族语义，`GetPrimaryAssetId` 给出"**身份 tag ↔ 资产**"的解析锚点）。**表行轨同款组织（2026-09-17 四次复评；身份 2026-09-22 tag 化）**：定义字段的唯一声明处是表行，资产组合持有它（字段集单份，不复制）——属性词表行 `FTcsAttributeDefTableRow`（`DefTag + Def`）与修正器模板行 `FTcsAttrModDefTableRow`（`TemplateTag + 模板字段`）；**行内身份字段（`DefTag` / `TemplateTag`）才是内容身份，`RowName` 降为编辑期定位**（`FTableRowBase` 的键类型是引擎硬约束的 `FName`），二者 **MUST NOT 被要求同名**——同步器维护的是"两轨指向同一身份"，不再是"行名 ↔ 常量映射"；对应资产的身份 = `[PrimaryAssetType, DefTag.GetTagName()]`（显式声明类型常量 + 覆写 `GetPrimaryAssetId`——**资产文件可自由改名/挪目录而不失联**）。**表格编辑局限在案**：修正器模板行含 `FTcsParamValue`（`TInstancedStruct`）列，CSV/Excel 往返不保留该列（引擎 CSV 导入无法表达多态实例结构）——模板行只支持编辑器内表格编辑；标量列仍可表格批量编辑。词表/Def 的装载与注册（含 `PrimaryAssetTypes` 注册）属 M6/M8 轮。**AttributeSet 的编辑面（D2-15，2026-09-17 裁决）**：`UTcsAttributeSetAsset` 是 Def 族新成员（`<族>Def` 命名标准 + `PrimaryAssetType` + 覆写 `GetPrimaryAssetId` 一体适用；命名待定，可作 `UTcsAttributeSetAsset` 或并入 **`TcsAttributeSet`**——按"`<族>Def`=定义资产"标准，Set 不是"Def"而是"组合声明"，故保留 `Set` 词根）；策划在资产里配 `TArray<FGameplayTag> DefTags`（Def 资产主身份解析；2026-09-22 由 `TArray<FName> DefIds` 改 tag），**覆写列首版不做**；"情景 → Set"的对应关系住**实体侧配置**（组件引用），编辑器面只需能跳转/校验 `DefTag` 有效性（与词表同款校验：未登记的 tag = 保存期报错）。

## 6. 非目标

不做 K2；不做通用图编辑器框架；不做 Timeline 编辑器；不做策划权限/审批流。

## 7. 依据

裁决 3（可视化跟随数据形状）；D4-1/D4-2（引用制/帧末 → WhyNot 与合并可观测）；D5-7/D5-9（重定向栈可视化需求）；TCS 12 报告（编辑器现状）+ orphan-scan 实证（11/ZZ:97）；**D5-18 v3 / PV-10（2026-09-14：参数源四联矩阵）+ D5-17 v3（2026-09-15：视图策略化——探针校验与槽名交叉扫描）**。

## 8. 验收钩子

濒死配置扫描抓到一个真实死行；WhyNot 回放解释一次"触发行为什么没跑"；孤儿扫描在注入的诱饵资产上报警且不误删白名单外资产。
