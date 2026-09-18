# attribute-store Specification

## Purpose
TBD - created by archiving change add-tcsattribute-types-and-store. Update Purpose after archive.
## Requirements
### Requirement: 属性数据宿主与单位注册

`TcsAttribute` MUST 提供 `UTcsAttributeSubsystem : UWorldSubsystem`（**非 Tickable**——M2 不认识时间，D2-8；零自 tick 依赖）作为 M2 数据宿主的唯一门面，MUST 仅在 Game / PIE / GamePreview 世界实例化（无跨 World 静态状态，PIE 安全），`Deinitialize` MUST 确定性清空全部单位与 Store。

- 单位身份 = `FTcsCombatEntityHandle`（`TcsCore` 词汇，PV-1 边界让步记录）；本门面 `RegisterUnit(FName UnitName)` 发号并建空 Store，`UnitName` 为调试/屏显名；`UnregisterUnit(FTcsCombatEntityHandle)` 释放该单位的 Store（无效/已注销句柄 MUST ensure 拦截，不得静默）；
- Store 布局 MUST 为"每单位一个 `FTcsAttributeStore`"，门面持 `TMap<FTcsCombatEntityHandle, TUniquePtr<FTcsAttributeStore>>`（**外层经 `TUniquePtr` 间接层**），`GetStore(Unit)` 暴露该单位 Store 指针（mutable/const 两版；无效句柄返回 nullptr + ensure）；`FTcsAttributeStore` 内含 `TMap<FTcsAttributeName, FTcsAttributeInstance> Attributes`——键控形状与 02 §2.2a 一致。
- **指针稳定性的真实边界（引擎事实，2026-09-17 实证）**：**容器指针**（`GetStore` 返回值）MUST 在"新增其他单位"后仍有效（由外层 `TUniquePtr` 间接层保证——这正是 02 §2.2a"适配器缓存 Store 指针"的前提）；句柄注销后该指针失效（容器被释放，注销与缓存不可交叉）。**容器内部实例指针 MUST NOT 被跨插入缓存**（`TMap`/`TSet` 元素存在连续缓冲中、扩容即搬移——直接按值存的容器都会搬走指针）。**"热路径不重查定义"（D2-9）由"实例自持定义字段"满足，与指针缓存无关**。

#### Scenario: 注册发号唯一且可解析

- **WHEN** 连续注册两个单位
- **THEN** 得到两个互异且 `IsValid()` 的实体句柄，各自 `GetStore` 非空且互不相同

#### Scenario: 容器指针跨"新增单位"稳定

- **WHEN** 缓存某单位的 `GetStore` 返回值后，再注册若干个**其他**单位
- **THEN** 该缓存指针仍指向同一容器（外层 `TUniquePtr` 间接层保证）；容器内部实例指针不作此保证（按名查询）

#### Scenario: 注销后句柄失效

- **WHEN** 注册后 `UnregisterUnit`，再以同一句柄调用 `GetStore` 或 `UnregisterUnit`
- **THEN** 前者返回 nullptr、后者 ensure 命中（悬空句柄不静默通过）

#### Scenario: 非游戏世界不创建

- **WHEN** 在编辑器预览/检查器世界解析该子系统
- **THEN** 子系统不存在

### Requirement: 属性定义表与单位侧添加移除

门面 MUST 持有**属性定义表**并在内部完成定义解析——**调用面纪律**（2026-09-17 用户口径）：单位侧只按属性名操作，调用方 MUST NOT 传定义数据（定义解析是门面内部执行流程）。

- `RegisterAttributeDef(const FTcsAttributeName& Attribute, const FTcsAttributeDefTableRow& DefRow)`：宿主 / DefLibrary 加载定义资产后登记（`UTcsAttributeDef::Def` 即行、`DefId` 即属性名）；属性名由调用方显式给出（行内不带 id）。拒绝面（ensure + false）：属性名为空、同属性名重复登记（D2-1：词表重名 = 加载期错误，不得静默覆写）。
- `FindAttributeDef(FTcsAttributeName)`：查回已登记定义（未登记返回 nullptr，**正常查询路径不 ensure**）。
- `AddAttribute(Unit, FTcsAttributeName)`：按名在该单位容器内建 `FTcsAttributeInstance`，定义字段自内部表展开；初值 `BaseValue`、`CachedCurrent = BaseValue`、`bDirty = false`、`ModifierSlots` 空。拒绝面（ensure + false）：单位未注册、属性名为空、同单位重复添加、**定义未登记**、定义行的动态边界自引用（D2-4）。
- `RemoveAttribute(Unit, FTcsAttributeName)`：整条属性下线——销毁实例及其修正器槽位内容。语义：槽位内修正器随实例一并丢弃；来源方（buff/装备等）的级联撤销仍按其自身生命周期走 `RemoveBySource`（聚合管线轮），二者不互相替代。拒绝面（ensure + false）：单位未注册、属性名为空、该单位未持有此属性。

实例 MUST NOT 持有定义行或资产的引用（热路径不回查定义）。定义数据的**资产 → 载荷**解析归 DefLibrary（M6）/ 词表（M8）——本能力无 DataTable / 资产加载路径。

#### Scenario: 定义先登记后按名添加

- **WHEN** 先 `RegisterAttributeDef`（BaseValue 100、静态 0..100、Clamp）再 `AddAttribute(Unit, Health)`
- **THEN** 实例基础值与缓存值均为 100、`bDirty` 为假、边界与值域来自定义行；调用方未传任何定义数据

#### Scenario: 定义未登记被拦截

- **WHEN** 对未登记过的属性名调用 `AddAttribute`
- **THEN** ensure 命中且返回 false（不建实例）

#### Scenario: 重复登记与重复添加被拦截

- **WHEN** 同 `DefId` 再次 `RegisterAttributeDef`，或对同一单位同一属性再次 `AddAttribute`
- **THEN** 各自 ensure 命中且返回 false（不覆写既有定义／实例）

#### Scenario: 动态边界自引用被拦截

- **WHEN** 登记的定义行其 `Bounds.Max.DynamicAttribute` 为自身，并对其调用 `AddAttribute`
- **THEN** ensure 命中且不建实例

#### Scenario: 移除属性

- **WHEN** 对已持有的属性调用 `RemoveAttribute`
- **THEN** 返回 true 且该实例不再可查；未持有的属性调用时 ensure 命中且返回 false

### Requirement: 读侧契约

`TcsAttribute` MUST 提供 `ITcsAttributeProvider`（`UINTERFACE(MinimalAPI)`，M2 对外**唯一**契约，02 §2.3）：`GetBaseValue(FTcsAttributeName)` / `GetCurrentValue(FTcsAttributeName)` / `PeekPending(FTcsAttributeName)`，三者为反射可见事件（宿主/适配器可实现）。**单位由实现者自身绑定**（军官组件 / Mass 存储桶适配器各绑自己的单位）——契约签名不含单位参数是刻意的：计算器不关心单位载体，适配在 M6 收敛。本任务**只声明契约**：M2 内部以 `TScriptInterface<ITcsAttributeProvider>` 持有（属性值参数源的扩展上下文），首个实现在 plan2 Task 5 的战斗实体组件。`GetCurrentValue` 的"脏则惰性重算"与 `PeekPending` 的"未提交候选值"语义（D2-5）MUST 由实现者与 Task 5 管线共同保证——本任务不得提供"看起来会重算、实际读脏缓存"的 Store 取值口（Task 4 的 Store 只给 `FindInstance` / `GetBaseValue`）。

#### Scenario: 契约可被宿主实现

- **WHEN** 宿主（C++/UnrealSharp/BP）实现 `ITcsAttributeProvider` 并绑定自己的单位
- **THEN** 三个读取方法均可在 C++ 与反射面覆写（单位绑定留在实现者内部）

#### Scenario: 本任务无假重算口

- **WHEN** 检查 Task 4 交付的 Store 公开口
- **THEN** 只有实例查找与基础值读取；惰性重算的当前值读取口随 Task 5 管线一并出现（避免缓存值被误当权威）

