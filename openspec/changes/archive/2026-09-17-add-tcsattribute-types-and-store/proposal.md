# Change: 落地 TcsAttribute 类型词汇与属性存储（attribute-types / attribute-store）

## Why
plan1 Task 4（TcsAttribute 类型与存储）动工前的规格先行提案：M2 属性层的**全部对外词汇**（属性名、五带运算、操作数双形状、边界三态、值域模式、账本修正器、属性实例）与**数据宿主**（单位注册 + 单位键控 Store + 门面 API + 读侧契约）在此一次性落地，是 plan1 Task 5 聚合管线与 plan2 Damage 读 Armor/Attack、扣血写 Health 的编译前提。同时需要收口 2026-09-14/15 两轮拍板中落在本任务的实现缺口：D5-18 v3 约定列白名单的可执行落点、PV-3 属性值参数源（`AttributeScaled`）与 PV-1 的实体身份句柄边界记录。

## What Changes
- 新增能力规格 `attribute-types`（M2 类型词汇）：
  - **属性定义双轨制**（2026-09-17 用户口径）：载荷 `FTcsAttributeDef`（`BaseValue` / `Bounds` / `ValueDomain`）+ 运行期资产 `UTcsAttributeDef : UPrimaryDataAsset`（`DefId` = 属性名 = 解析锚点）+ 编辑期表行 `FTcsAttributeDefTableRow : FTableRowBase`（`DefId` + 载荷，行名即属性名）——**DataTable 供策划批量编辑、运行期一律走资产**（运行期零 DataTable 加载路径；两轨同步器属 M8 工具面）；
  - `FTcsAttributeName`（FName 显式包装 USTRUCT + 哈希/相等——裸 FName/TEXT 传不进属性 API，D2-1）；
  - `ETcsAttributeOp` 封闭五带（`TAO_Add` 0 默认 / `TAO_Override` 1 / `TAO_PercentAdd` 2 / `TAO_Mul` 3 / `TAO_FlatAdd` 4；无 Custom 位，新运算 = 末尾加值）+ 带权助手（Override 0 / Add 10 / PercentAdd 15 / Mul 20 / FlatAdd 30——**带序唯一真相在 Op，SortKey 仅展示位**）；
  - 操作数双形状（D2-13/PV-6）：定义侧 `FTcsAttrModOperandDef`（Literal 为 `FTcsParamValue`，可等级化）+ 运行侧 `FTcsAttrModOperand`（Literal 恒为已解析 double，账本零膨胀）；`ETcsOperandKind{OPK_Literal, OPK_AttributeScaled}` 仅此两种（D2-11 拒绝清单不破）；
  - 边界三态 `FTcsAttributeBound`（None/Static/Dynamic(FName)，Min/Max 各自独立，D2-4）+ `FTcsAttributeBounds`；值域模式 `ETcsAttributeValueDomain{AVD_Clamp=0, AVD_Custom=1(逃逸位), AVD_Wrap=2}`（D2-6；**策略接口 `IValueDomainPolicy` 不在本任务**）；
  - 账本修正器 `FTcsAttrModInstance{Target, Op, Operand, Source(级联锚点), SortKey, Tag}` + 单属性实例 `FTcsAttributeInstance{BaseValue, CachedCurrent, bDirty, Bounds, ValueDomain, ModifierSlots}`；
  - 修正器模板资产 `UTcsAttrModDef : UPrimaryDataAsset`（D3-19 纯模板；基类取 `UPrimaryDataAsset` 是 **Def 资产族统一约定**——2026-09-17 复评拍板）+ **`IsDataValid` 落地 D5-18 v3 约定列白名单**；
  - 参数源 `FTcsParamSource_AttributeScaled`（PV-3）+ 扩展求值上下文 `FTcsAttributeEvaluateContext`。
- 新增能力规格 `attribute-store`（M2 数据宿主）：`UTcsAttributeSubsystem : UWorldSubsystem`（非 Tickable——M2 不认识时间，D2-8）门面 `RegisterUnit` / `UnregisterUnit` / `AddAttribute(Unit, Name, Def)` / `GetStore`；`FTcsAttributeStore`（每单位一个，名字键控 `TMap<FTcsAttributeName, FTcsAttributeInstance>`）；读侧契约 `ITcsAttributeProvider`（UINTerface，M2 对外唯一契约——**本任务只声明，首个实现在 plan2 Task 5 的战斗实体组件**）。
- 修订 `instance-handle-pool` 能力：新增 Core 第三套句柄词汇 `FTcsCombatEntityHandle`（uint64 Id，0 无效）+ `FTcsCombatEntityHandleRegistry`（原子递增发号，进程内永不复用）——PV-1 §"边界让步"已拍板该类型落 TcsCore（Core 唯一持有的战斗实体词汇），本任务是其首个消费者。
- 修订 `param-value` 能力：`FTcsParamValueSource` 新增能力位虚函数 `AllowsValueConvention()`（默认 true；`ParamRef` / `AttributeScaled` 覆写为 false）——**约定列白名单的唯一真相在源自身**（对齐 PV-10"索引解析唯一真相在源"与 D3-7 v3"能力探测 = 虚分派，不做中心 switch"）；`FTcsParamEvaluateContext` 新增类型标识虚函数 `GetScriptStruct()`——PV-1"结构体继承 + 源内 checked cast"的锚点（USTRUCT 无内建类型查询；GAS 同款机制，落地期定案见 design.md 实施定案 1）。
- 未触及：聚合公式与 Override 覆盖语义、脏标记惰性重算、事务 flush、读即登记依赖边、SCC 环检测——**全部归 Task 5**；本任务只落"形状与宿主"，`CachedCurrent` 在定义时初值化，ModifierSlots 恒空。

## Impact
- Affected specs: `attribute-types`（新建能力）、`attribute-store`（新建能力）、`instance-handle-pool`（MODIFIED：新增战斗实体身份句柄需求）、`param-value`（MODIFIED：值来源策略基类新增约定能力位）。
- Affected code:
  - 新建 `Source/TcsCore/Public/Handle/TcsCombatEntityHandle.h`（header-only）；
  - 修改 `Source/TcsCore/Public/Parameter/TcsParamValueSource.h`（+1 虚函数）、`TcsParamSource_ParamRef.h`（覆写）；
  - 新建 `Source/TcsAttribute/Public/Attribute/`（`TcsAttributeName.h` / `TcsAttrModInstance.h` / `TcsAttributeInstance.h` / `TcsAttributeStore.h` / `TcsAttributeProvider.h` / `TcsAttrModDef.h` / `TcsParamSource_AttributeScaled.h`）+ `Public/TcsAttributeSubsystem.h` + `Private/Attribute/TcsAttrModDef.cpp` + `Private/TcsAttributeSubsystem.cpp`；
  - 临时测试装置（`Source/TcsAttribute/Private/Testing/`，**不入库、不进规格**——随 Task 6 并入验收装置或删除）：`Tcs.Test.Attribute` 命令复核定义期拒绝面与约定白名单。
- `TcsAttribute.Build.cs` 依赖（Core/CoreUObject/Engine/GameplayTags + TcsCore + TcsNotation）在 Task 0 已就位，本任务**不改**（plan1 Task 4 Step 1 记为"现状确认"）。
- 决策依据：02 §2.1/§2.2/§2.2a/§2.3/§3、PV-1/PV-3（2026-09-11）、D5-18 v3（2026-09-14）、plan1 Task 4（含 File Structure 的类型清单）、plan2 Task 5（消费者形状）。
- 风险与迁移：无外部消费者、无静态数据迁移面（`TcsAttribute` 至今零类型）；`FTcsCombatEntityHandle` 的**发号方**在 R3 是 M2 门面，M6 世界注册表落地后移交发号（类型不变，消费者签名不连锁改动——见 design.md D1）。

## 提案内钉名（plan / 设计文档未钉，供审阅否决）

| 项 | 钉法 | 依据 |
|---|---|---|
| Core 实体句柄 | `FTcsCombatEntityHandle`（+ `FTcsCombatEntityHandleRegistry`），住 `Handle/`（与 `FTcsSourceHandle` 同目录同性质） | PV-1 记录概念名 `FCombatEntityHandle`；实现名按 `Tcs` 中缀纪律 |
| 约定能力位 | `virtual bool AllowsValueConvention() const { return true; }` | D5-18 v3 白名单；语义 = "本源书写的数值即结果"（引用/换算源禁配） |
| 带权助手 | `GetTcsAttributeBandWeight(ETcsAttributeOp)`（header-only 自由函数，住 `TcsAttrModInstance.h`） | 带序唯一真相在 Op；Task 5 折叠器直接消费，避免与 SortKey 双真相 |
| 单位注销 | `UnregisterUnit(FTcsCombatEntityHandle)`（plan 只列 `RegisterUnit`） | 注册/注销对称；同会话内 PIE 反复注册不致 Store 堆积 |
| Store 取值口 | Task 4 只给 `FindInstance` / `GetBaseValue`；**惰性重算的 `GetCurrentValue` 随 Task 5 管线落地** | 避免"看起来会重算、实际读脏缓存"的假 API |
| 属性名稠密 id 缓存 | **本任务不落字段**（D2-1 的 id 缓存依赖 M8 词表注册表；R3 以 `TMap` + `GetTypeHash(FName)` 键控） | 零消费者不预建（PV-10 同纪律）；D2-1 的"注册表世代号校验"属 M8 |
| 上下文类型标识 | `virtual const UScriptStruct* GetScriptStruct() const`（Core 上下文；派生覆写自身、源侧 `IsChildOf` 判定） | PV-1 checked cast 的载体；GAS `FGameplayEffectContext` 同款（落地期定案，见 design.md 实施定案 1） |
| 导出宏边界 | 全内联值类型**不加**模块导出宏（`FTcsSourceHandle`/`FTcsCombatEntityHandle` 家族与 M2 账本四类型）；宏只给"有 out-of-line 成员或反射符号"的类型 | 链接期实证：加了会在消费方 LNK2019（design.md 实施定案 3；顺带修掉 `FTcsEventSubscriptionHandle` 同类潜伏项） |
| 属性定义载体 | **双轨制**：载荷 `FTcsAttributeDef` + 运行期资产 `UTcsAttributeDef`（`DefId` 锚点）+ 编辑期表行 `FTcsAttributeDefTableRow`（行名即属性名）；门面只认载荷 | 2026-09-17 三次复评选定：用户既定策略是"DataTable 供策划编辑、运行期一律 DataAsset"（资产制扩展性更好，加 Fragment 只动资产与载荷）；推翻前一轮的纯行形态 |
| Def 资产族基类 | 统一 `UPrimaryDataAsset`（`UTcsAttrModDef` / `UTcsAttributeDef` 起，未来 `UTcsStateDef` 家族与 `UTcsSkillModDef` 同） | 2026-09-17 复评选定：原 `UDataAsset` 仅由 plan1 写下、设计文档无表态；Def 引用本是 FName Id + DefLibrary 解析语义，主资产身份让该解析与按类型发现/加载归引擎 |
| 文件名 | 去类型前缀（`UTcsAttrModDef` → `TcsAttrModDef.h`），35 个文件全量改名 | 2026-09-17 二次复评选定：UE 规范「文件名 = 类型名去前缀字母」；违规源自 plan1/plan2 的 File Structure，规范落 `unreal-cpp-style` |

## 检查点

本提案对应 plan1 Task 4，落点验收 = UBT Development Editor 编译通过 + 临时装置定向检查（定义期拒绝面 + 约定白名单）；plan1 检查点 2/3/4 属 Task 6 验收装置，不在本提案范围。
