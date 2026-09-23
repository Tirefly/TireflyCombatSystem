# Change: 登记表持有对 GC 可见（修 T-8 地雷）

## Why

两张定义登记表用**裸 C++ 容器 + `TUniquePtr`** 持有内容，而 GC 只走 `RefLink` 链、看不见这类容器 —— 于是**登记表里的对象引用不会被保活**，被静默回收后流程/链跑到那一步取到空引用（表现为"公式不生效"而非崩溃，极难排查）。

这不是理论风险，已有两条实证线索：①台账 **T-8**（2026-09-21 登记，源自 plan2 Task 4/6 实测）；②宿主被迫绕过 —— `UTcsDevBootstrap` 用 `UPROPERTY TObjectPtr<UTcsDevDamageFormula>` 强引用公式 delegate，其头注释明写"模板登记表非 UPROPERTY，保不住步骤里的对象引用"。**第一个不知道要绕的项目就会踩静默 GC。**

规格现状也**没有覆盖这一面**：两条需求只要求"地址稳定"（防扩容搬移导致 C++ 引用悬空），没要求"GC 可见"（防对象引用被回收）。两件事正交，须分别写明。

## What Changes

- `damage-flow` 的「流程模板与登记表」需求：登记表持有要求从"地址稳定"扩为**地址稳定 + GC 可见**；补一条 Scenario 钉住"模板步骤里的对象引用在 GC 后仍存活"。
- `effect-chain` 的「链定义登记表」需求：同款扩展（链步骤可放任意宿主自定义 struct —— D4-16 无公共基类、picker 不设限，其中的 `UPROPERTY` 对象引用同样需保活）。
- 实现落点：两个子系统各自覆写 `UObject::AddReferencedObjects`（**静态签名** `static void AddReferencedObjects(UObject* InThis, FReferenceCollector& Collector)`），逐个定义调 `FReferenceCollector::AddPropertyReferencesWithStructARO`。

## 手法依据（引擎事实，已核源码）

- `UDataTable::AddReferencedObjects` 对 `RowMap` 用的**正是这一招**（`Engine/Private/DataTable.cpp:300-318`）—— 同为"非 UPROPERTY 容器持 struct"的场景，是本项目可援引的最近先例。
- `FReferenceCollector::AddPropertyReferencesWithStructARO` 是 **public** API（`UObjectGlobals.h:2929`，`FReferenceCollector` 类起于 `:2530` 的 public 区）。
- 它能**递归进 `FInstancedStruct` 内层实例内存**：`FInstancedStruct` 带 `WithAddStructReferencedObjects`，其 ARO 调 `AddPropertyReferencesWithStructARO`（`InstancedStruct.cpp:506-524`）—— 故 `TArray<FInstancedStruct>` 里任意深度的对象引用都能被保活。
- 静态 ARO 签名经 `GENERATED_BODY()` 自动接入（`UOBJECT_CPPCLASS_STATICFUNCTIONS_FORCLASS` 取 `&TClass::AddReferencedObjects`，`ObjectMacros.h:2087`），无需额外注册。

## 影响面

- **Affected specs**: `damage-flow`（MODIFIED × 1）、`effect-chain`（MODIFIED × 1）
- **Affected code**:
  - `Source/TcsDamage/Public/TcsDamageSubsystem.h` + `Private/TcsDamageSubsystem.cpp`（新增 ARO 覆写）
  - `Source/TcsEffect/Public/TcsEffectSubsystem.h` + `Private/TcsEffectSubsystem.cpp`（新增 ARO 覆写）
- **不改**：登记/注销/查询 API 签名、登记表容器类型（`TUniquePtr` 保留 —— 地址稳定仍需要）、宿主侧（其绕过手段可保留，成为冗余保险而非必需）。

## 非目标

- **不做**模板/链定义的资产化（那是台账 T-8 的另一半，由资产 root 后本问题自然消解；本次只修"代码组装路径"的地雷，两者不冲突 —— 资产化落地后 ARO 覆写仍无害）。
- **不改** `FTcsAttributeStore` 等其余容器：已核其不持对象引用（纯数值 + 句柄），无需处理。
- **不做**全库扫描式改造：本次只处理**已实证的两处**登记表。
