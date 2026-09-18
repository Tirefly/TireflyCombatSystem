# Change: 落地属性聚合管线与变更事务（attribute-pipeline / attribute-transaction）

## Why
plan1 Task 5（聚合管线）动工前的规格先行提案：Task 4 已把 M2 的**类型词汇与数据宿主**落地（属性定义双轨、实例、容器、门面），但**「值怎么算出来」与「改了怎么记账/通知」还没有规格**——`CachedCurrent` 目前只是定义时的初值，任何 modifier 的挂/摘都不产生数值变化。本提案把 02 §3（聚合管线）与 §4（事务与 flush）的既定设计变成可执行规格，并同时收编三件已定但未落地的行为：

1. **折叠器单份**（D5-5 v3 / 用户拍板"运算符相同则实现必须单份"）：五带折叠公式 + Override 覆盖语义抽为**一个纯函数**，M2 属性聚合 / M5 参数链 / TcsDamage 流程属性三处共用；
2. **属性冻结/解冻**（D2-14，2026-09-18 拍板，规格 delta 草稿已在决策文档 §附录）：`RemoveAttribute` = 冻结整条实例、`AddAttribute` = 解冻优先；
3. **Task 4 遗留的三条边界纪律**：属性增删 MUST 与 modifier 同走一条 store 变更路径与同一事务纪律；`RemoveBySource` 的扫描面 MUST 含暂存区；modifier 的 Target 已无实例时忽略 + 日志（不 ensure）；
4. **覆盖带强弱口径**（2026-09-18 用户拍板，落地期增补）：`OverridePriority`（修正器侧）+ `OverrideTieBreak`（属性定义侧，封闭四值）——数值大小本身不含方向，"取最大值"对"越低越强"的属性会取到最温和的一条。

## What Changes
- 新增能力规格 `attribute-pipeline`（**值怎么算**）：
  - **五带折叠纯函数（单份）**：`FoldTcsAttributeBands(BaseValue, Entries, OverrideTieBreak = OTB_Max)` + `FTcsAttributeBandEntry{Op, Value, OverridePriority}`，**住 Public**（M5/TcsDamage 要复用）——`Final = ((Base + ΣAdd) × (1 + ΣPercentAdd)) × ΠMul + ΣFlatAdd`；**存在 Override 时按"优先级 → 同优先级策略 → 有符号值"三级选出一条替换整个结果（FlatAdd 一并被覆盖）**；带序唯一真相在 Op（SortKey 不参与）；组内与带间顺序无关（覆盖带裁决为全序）；空集返回 Base。
  - **按需重算与脏标记**：`EvaluateCurrent(Unit, Attribute)` —— 干净返缓存、脏则重算（收集 → 折叠 → 值域收口 → 写缓存 → 清脏）；**管线是 `CachedCurrent` 的唯一生产者**。
  - **值域收口**：末端按 `ValueDomain` 收口（Clamp 默认 / Wrap 循环 / **Custom 逃逸位**——值域策略接口不在 R3，命中时 **ensure 提示**并按 Clamp 收口，不静默）；动态边界先按管线求值；自引用已在属性添加期拦截。
  - **依赖登记与环检测**：求值期间**读其他属性的 Current 即登记依赖边**（含 `OPK_AttributeScaled` 的 `Coefficient × Current(Attribute)` → 主属性变化自动把派生属性标脏，零宿主维护）；环检测 = **Tarjan SCC**（成环 ensure + 拒绝该边）；**重算轮数上限与环判定解耦**（修旧 TCS"8 轮误判深链为环"的缺陷）。
  - **变更广播**：提交尾比较新旧值（**epsilon 1e-5**），有实质变化才走总线立即通道发属性变更事件；未变不广播。
- 新增能力规格 `attribute-transaction`（**改了怎么记账/通知**）：
  - **批与提交**：`BeginBatch` / `Commit` —— 单帧多次变更（挂/摘/改基值/**属性增删**）只算一次重算、每属性最多广播一次；
  - **唯一提交点与失败零写入**：候选值在内存上算完，唯一提交点写回，失败零写入；
  - **`PeekPending`**：不落账读预览（批内候选值；无进行中批时 = 当前值）；
  - **`RemoveBySource`**：按来源句柄全量摘除其修正器 → 标脏 → 重算，**扫描面 MUST 含实例槽位与冻结暂存区**（否则来源在属性被冻结期间结束、其修正器永久滞留，属性恢复后凭空多出数值）。
- **修订 `attribute-store` 能力**（MODIFIED + ADDED，取自决策文档 §附录）：`RemoveAttribute` = **冻结整条实例**进 `FrozenAttributes`（不销毁、不丢槽位、输出日志）；`AddAttribute` = **解冻优先**（整条搬回、基础值取回冻结前的值，否则按定义新建，**新建即脏**）；**双态约束**（同名不同时存在于两处）；**属性增删 MUST 走与 modifier 同一 store 变更路径与同一事务纪律**；modifier 施加到已无实例的属性 = 忽略 + 日志；暂存区随单位注销释放；不引入上限/TTL/来源存活性校验。
- **修订 `attribute-types` 能力**（MODIFIED × 4 + ADDED × 1，2026-09-18 增补）：`TAO_Override` 带的强弱口径落成词汇——`FTcsAttrModInstance::OverridePriority`（修正器侧，仅覆盖带读）、`FTcsAttributeDefTableRow::OverrideTieBreak`（属性定义侧，随实例展开）、封闭四值枚举 `ETcsAttrOverrideTieBreak`（取最大/取最小/绝对值最大/绝对值最小，**不开放自定义策略**）；模板行同步持有 `OverridePriority`，`IsDataValid` 对"非覆盖带填了优先级"给**警告**（不静默，不报错）；账本实例形状与"运算带封闭枚举"需求同步补正（`SortKey` 依旧零语义）。
- **不做的**（非目标，保持 R3 边界）：不做 `IValueDomainPolicy` 值域策略接口（Custom 只留逃逸位 + ensure 提示）；不做自定义覆盖策略；不做多线程（D0-4 单游戏线程假设）；不做网络复制实现（姿态已在 02 §5 声明）；不做 M5 参数链 / TcsDamage 流程属性容器的接入（本任务只把折叠器做成公用件，接入随各自模块轮）。

## Impact
- Affected specs: `attribute-pipeline`（新建能力）、`attribute-transaction`（新建能力）、`attribute-store`（MODIFIED 添加/移除语义 + ADDED 冻结暂存区）、`attribute-types`（MODIFIED 运算带/账本/属性定义/模板校验 + ADDED 覆盖带优先级与同优先级策略）、`instance-handle-pool`（MODIFIED 反射句柄）。
- Affected code:
  - 新建 `Source/TcsAttribute/Public/Attribute/TcsAttributeBandFold.h`（折叠纯函数 + 条目结构，**Public**——复用件）；
  - 新建 `Source/TcsAttribute/Public/Attribute/TcsAttributePipeline.h`（管线类**声明**——2026-09-18 移出 Private，供跨模块消费者直调）+ `Source/TcsAttribute/Private/Attribute/TcsAttributePipeline.cpp` / `_Dependency.cpp` / `_Cascade.cpp`（**实现留 Private**；超 300 行按 `TcsAttributePipeline_<面>.cpp` 拆分）；
  - 修改 `Source/TcsAttribute/Public/TcsAttributeSubsystem.h` + `Private/TcsAttributeSubsystem.cpp`（暴露管线入口；`RemoveAttribute` 改冻结、`AddAttribute` 改解冻优先；`RemoveBySource` 落位）；
  - 修改 `Source/TcsAttribute/Public/Attribute/TcsAttributeStore.h`（暂存区 `FrozenAttributes` + `FindFrozenInstance`）；
  - 修改 `Source/TcsAttribute/Public/Attribute/TcsAttrModInstance.h`（`ETcsAttrOverrideTieBreak` + `FTcsAttrModInstance::OverridePriority`）、`TcsAttributeDef.h` / `TcsAttributeInstance.h`（`OverrideTieBreak` 定义侧与展开侧）、`TcsAttrModDef.h/.cpp`（模板行字段 + 适用范围警告）、`Private/Attribute/TcsAttributePipeline.cpp`（收集期带优先级、折叠传策略）；
  - 临时测试装置（`Private/Testing/`，**不入库**）增补：折叠公式与 Override、覆盖带优先级/四策略/顺序无关、脏标记重算、值域收口（含 Wrap 退化解）、依赖登记与成环拒绝、事务批与广播计数、`PeekPending`、`RemoveBySource`（含扫描暂存区）、冻结/解冻、覆盖带端到端（策略 + 优先级 + 递补）。
- 决策依据：02 §3（聚合管线）/§4（事务与 flush）/§2.2a（属性集合与冻结）、D2-2/D2-3/D2-4/D2-5/D2-6/D2-10/D2-11、D5-5 v3（折叠器单份）、D2-14/D2-15（2026-09-18 冻结兜底，决策文档 §附录）；覆盖带强弱口径为落地期增补（2026-09-18 用户拍板，MEM-20260902-20"读侧选优"的延伸）。

## 提案内钉名（plan / 设计文档未钉，供审阅否决）

| 项 | 钉法 | 依据 |
|---|---|---|
| 折叠纯函数 | `FoldTcsAttributeBands(double BaseValue, TConstArrayView<FTcsAttributeBandEntry> Entries, ETcsAttrOverrideTieBreak OverrideTieBreak = OTB_Max)`；条目 `FTcsAttributeBandEntry{ ETcsAttributeOp Op; double Value; int32 OverridePriority; }`（`Public/Attribute/TcsAttributeBandFold.h`） | 设计只说"抽为一个纯函数、三个消费者共用"未钉名；命名与既有自由函数 `GetTcsAttributeBandWeight` 同族；**扁平 (Op, Value) 列表**是因为三处消费者的容器形状各异，摊平后单一签名即够；第三参带默认值使 M5/TcsDamage 可两参调用（无"同优先级策略"概念时不下推概念）。**术语分层（2026-09-18 用户拍板保持）："聚合"指本模块/管线（02 §3 聚合管线 / `FTcsAttributePipeline`），"折叠"指这份**共用计算核（折叠器）**（D5-5 v3 原话"折叠器单份…三处共用"）。**不跟 GAS 的 "Aggregator" 命名对齐**——已核 GAS 源码：`FAggregator` 是**有状态容器**（持基础值 + 按运算分桶的修正器 + 挂摘 API，对应我们的实例 + 门面挂摘），真正算公式的是 `FAggregatorModChannel::EvaluateWithBase`（`GameplayEffectAggregator.cpp:74-84`），它并不叫聚合器；且 GAS 的 Override 取**首个合格项**（`return Mod.EvaluatedMagnitude` 直接返回，无大小比较，依赖遍历顺序），我们用**优先级 → 策略 → 有符号值的全序**（顺序无关，有意改进） |
| 覆盖带强弱口径 | 第一裁决键 `FTcsAttrModInstance::OverridePriority`（修正器侧，仅覆盖带读）；第二级 `FTcsAttributeDefTableRow::OverrideTieBreak`（属性定义侧，随实例展开）；第三级有符号值（补齐全序）；枚举 `ETcsAttrOverrideTieBreak` = `OTB_Max/OTB_Min/OTB_MaxAbs/OTB_MinAbs`，无 Custom 值 | 数值大小不含方向（"取最大"对越低越强的属性取到最温和的一条）；策略住**属性定义**而非修正器——放修正器上会变成"两个来源各说各话"，等于又需要一条规则来裁决规则；不开放自定义：热路径比较函数必须全域且确定（与 `AVD_Custom` 不同源——后者是值域语义且另有 Clamp 回落）。默认值 = 历史行为（全 0 + `OTB_Max` 逐位等价于旧的"组内最大值"） |
| 管线命名 | `FTcsAttributePipeline`：**声明住 `Public/Attribute/`（2026-09-18 用户拍板移出 Private）**、实现住 `Private/Attribute/`（`.cpp` 三份）+ 门面转发（`EvaluateCurrent`/`ApplyModifier`/`RemoveBySource`/`BeginBatch`/`Commit`/`PeekPending`） | 沿用 plan1 类型名与动词纪律（`Evaluate` 非 `Resolve`）。搬出 Public 的依据：**确认未来有跨模块消费者**直调管线驱动求值/事务（不改内部执行逻辑）；同仓 TBNS 既有同款形态（`Public/Pathfinder/TbnsPathfinder.h` + `Private/Pathfinder/TbnsPathfinder.cpp`，另有 `_Internal.h` 留 Private）。纪律写进该头注释：**推荐路径仍是门面**（唯一入口）／直调为逃生口；有 out-of-line 成员必须带 `TCSATTRIBUTE_API`；`private:` 段不属消费契约；**实例由门面拥有、MUST NOT 跨帧持有**（门面销毁后即悬空） |
| 广播事件 | `FTcsAttributeChangedEvent{ Unit, Attribute, OldValue, NewValue }`（核心词汇 FStruct，走总线立即通道）+ 原生 Tag `Tcs.Event.Attribute.ValueChanged`（常量 `Tag_TcsEvent_Attribute_ValueChanged`，本模块声明） | 02 §4 已定事件形状（含 `Reason` 字段——R3 只落前后值，`Reason` 待有消费者再加）。**事件语义 = 当前值（对外可读值）变了**，不是"某次写操作发生了"——改基值/挂摘修正器/依赖连带变化都只在当前值确实动了时发这一条，故它 MUST NOT 当"基础值变更日志"（基础值改了被收口吃掉 = 对外没变 = 不广播）。Tag 口径（2026-09-18 用户拍板）：命名公约 **`Tcs.Event.<域>.<事件名>`**（域段是真层级节点）；**原生声明**而非项目 Tag 表（插件自足、项目漏配不会静默丢事件）、且不住 TcsCore（Core 不持战斗域词汇，01 §2.2）；原生订阅当前为精确匹配，父标签订阅待"原生层级匹配"落地（已记入 01 §2.2 总线输入）。被否：项目侧声明／改门面委托（会推翻 02 §4"变更 → 总线立即通道"）／base 与 current 各发一条事件（一次写操作变两条事件，见 tasks.md 增补 9） |
| 门面暴露方式 | 管线功能经 `UTcsAttributeSubsystem` **转发**暴露（不在门面内联实现），门面仍是唯一入口 | 保持"消费者只认识门面"与 Task 4 已定调用面纪律 |
| 冻结日志 | 冻结/解冻各一条 `Log` 级（含属性名 + 槽位数） | 决策文档 §附录已钉（正常业务流程，不用 Warning） |

## 检查点

落点验收 = UBT 编译通过（零警告）+ 临时装置定向检查（折叠语义 / 重算与脏标记 / 收口 / 依赖与成环 / 事务批与广播计数 / 冻结解冻）+ 用户 PIE 实测；plan1 检查点 2/3/4（屏显信号）归 Task 6 验收装置，不在本提案。
