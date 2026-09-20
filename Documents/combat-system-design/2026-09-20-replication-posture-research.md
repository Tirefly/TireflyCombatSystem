# 网络复制专项调研（UE 5.8 源码）：产物快照与 Cue 上下文能否过网

- 日期：2026-09-20
- 状态：**调研结论（只读调研，不含任何代码改动）**——为"结果快照 / Cue 上下文快照过网"通道选型提供源码级事实；**不改变既有裁决**（NET-1/NET-2 只留接口位、D6-4 只定契约不实现）
- 依据（引擎侧）：`E:\UnrealEngine\UE_5.8\Engine\Source\...`，行号锚点随文给出
- 依据（TCS 侧）：`01-module-m0-core.md` §4（NET-1/2 镜像模式接口位）、`06-module-integration.md` §2.3（D6-4 `ICombatReplicationProxy` 契约）、`2026-09-10-param-value-source-decision-points.md` PV-1（`FTcsParamValue{TInstancedStruct}`）、`09-module-damage.md`（FDamageRecord / 流程黑板）
- 阅读约定（本文严格执行）：
  - **【源码可证】** = 有 `文件:行` 锚点，且该锚点为引擎源码原文
  - **【推断】** = 引擎源码未直接给出结论，由既有事实外推；同时写明"依据不足在哪"
- 已知前提（TireflyCombatSystem 产品级目标：先单机、网络姿态已定）：
  - 服务器权威：链与流程只在权威侧全量执行；客户端只跑表现（Cue 类）
  - 过网的是**产物**：结果快照（`FDamageRecord`）与表现用上下文快照（CueId + 快照），**不是流程上下文本身**
  - 运行态上下文现有形态：`AActor*` / `TWeakObjectPtr<AActor>`、`TMap<FName, FTcsParamValue>`（内含 `TInstancedStruct<FTcsParamValueSource>`）、键→折叠 double 的黑板（`TMap<FName, TArray<提交>>`，提交内含 `TFunction OnConsumed`）、`TArray<FGameplayTag>`、`FTcsSourceHandle`（进程内原子发号 uint64）

---

## 零、结论速览（详细依据见 §1–§6，逐条判断见 §7）

| # | TCS 现有数据结构 | 能否"原样"过网 | 需要的形态 | 推荐通道 |
|---|---|---|---|---|
| 1 | `FDamageRecord`（扁平标量 + 身份） | **能**（需按 §3 纪律裁剪） | USTRUCT + 反射 UPROPERTY，无容器嵌套/无回调 | **FastArray**（追加记录流，客户端回调播 Cue） |
| 2 | CueId + 多态快照（`TInstancedStruct`） | **能**（两条复制系统都支持） | 内层 struct 必须是"纯数据 + 反射成员" | 经典：属性 or RPC；Iris：同上（有专用 NetSerializer） |
| 3 | `TMap<FName, X>` 任意形态 | **不能** | 摊平成 `TArray<{Key,Value}>` | 且**不该过网**（黑板是流内中间态） |
| 4 | `TMap<FName, TArray<提交>>` | **不能**（连 UPROPERTY 声明都非法） | 一维数组 + 复合键 | 不过网（回调本地） |
| 5 | `TFunction OnConsumed` | **不能**（UHT 直接报错） | 只能作为**非 UPROPERTY 本地成员**存在 | 永不过网 |
| 6 | `AActor*` / `TObjectPtr<AActor>` | 能 | 直接 `UPROPERTY` | 经典 NetGUID / Iris ObjectReference |
| 7 | `TWeakObjectPtr<AActor>` | 能 | 直接 `UPROPERTY` | 同上（弱引用语义：客户端可能收到 null） |
| 8 | `TArray<FGameplayTag>` | 能 | 直接 `UPROPERTY(Replicated)` | Iris 有专用 serializer / NetToken，带宽更优 |
| 9 | `FTcsSourceHandle`（进程内 uint64 发号） | 值能复制 | 语义上必须补"谁发号" | **不过网**或用稳定 Id 族（见 §7.9） |

---

## 一、`FInstancedStruct` 的复制现状

### 1.1 StructUtils 侧：有 `NetSerialize`（这是"能过网"的根）

**【源码可证】**
- `Engine/Source/Runtime/CoreUObject/Public/StructUtils/InstancedStruct.h:163`：`UE_API bool NetSerialize(FArchive& Ar, UPackageMap* Map, bool& bOutSuccess);`（`FInstancedStruct` 的成员声明）
- `InstancedStruct.h:275-292`：`TStructOpsTypeTraits<FInstancedStruct>` 显式打开 `WithSerializer / WithIdentical / WithAddStructReferencedObjects / WithNetSerializer = true`（`WithNetSerializer` 在 `:286`）
- `Engine/Source/Runtime/CoreUObject/Private/StructUtils/InstancedStruct.cpp:538`：`FInstancedStruct::NetSerialize` 实现；`:546-557` 先写 `bValidData` 位并把 `UScriptStruct*` 作为对象引用序列化（`Ar << SerializedScriptStruct`），`:552-555` 类型变化才重建实例
- `InstancedStruct.cpp:568-580`：内层 struct 的载荷分两条：
  - 内层有原生 `NetSerialize`（`ScriptStruct->StructFlags & STRUCT_NetSerializeNative`）→ 调 `GetCppStructOps()->NetSerialize(...)`
  - 否则 → 调静态委托 `NetSerializeScriptStructDelegate`（`:577-580` 有 `ensureMsgf`：**没有绑定委托就报错**）
- `InstancedStruct.cpp:478-504`：`Identical()` 走 `StructTypePtr->CompareScriptStruct(...)`（深比较），这是经典路径做变更检测的方式
- `Engine/Source/Runtime/CoreUObject/Private/UObject/Class.cpp:3192`：`STRUCT_NetSerializeNative` 由 C++ StructOps traits 在 `PrepareCppStructOps` 阶段置位（即"内层能否用自研 NetSerialize"取决于内层的 traits，不是 `FInstancedStruct` 决定的）

### 1.2 经典（非 Iris）复制路径：**可用**，且 RPC 也走同一套

**【源码可证】**
- 委托绑定在引擎启动时完成：`Engine/Source/Runtime/Engine/Private/UnrealEngine.cpp:351-370` —— 从 `UPackageMapClient` 拿 `UNetConnection` → `NetConnection->GetDriver()` → `GetStructRepLayout(内层类型)` → `RepLayout->SerializePropertiesForStruct(...)`。也就是说：**经典路径下"内层 struct 的载荷"是用内层 struct 的 RepLayout 逐属性序列化的（反射粒度），等价于"内层 struct 当普通 struct 复制"**
- `FRepLayout` 按 struct 类型缓存：`Engine/Source/Runtime/Engine/Private/NetDriver.cpp:7904-7915`（`RepLayoutMap`，`FRepLayout::CreateFromStruct`）
- `FRepLayout::InitFromStruct` 只跳过 `CPF_RepSkip`：`Engine/Source/Runtime/Engine/Private/RepLayout.cpp:6567-6570` —— **内层 struct 的每个反射成员（除非显式 `NotReplicated`）都会被纳入**，包括 TMap 这类"其实不支持复制"的类型（见 §3）
- 含 `NetSerialize` 的 struct 属性在 RepLayout 里被压缩成**单条命令**（不再向下递归）：`RepLayout.cpp:5716-5755`
- ReplicationGraph 是**经典系统的复制驱动（ReplicationDriver）**：它只决定"哪些 Actor 发给谁"，属性序列化仍走 `FRepLayout` —— 因此 **ReplicationGraph 不改变 `FInstancedStruct` 的可复制性**（【推断】：ReplicationGraph 插件源码中不出现任何 struct 序列化分支，仅调用 `UNetDriver::GetObjectClassRepLayout`；本轮未逐行核验其全部源码）

### 1.3 Iris 侧：有专用 NetSerializer（但**不是 UCLASS**）

**【源码可证】**
- 头文件：`Engine/Source/Runtime/Net/Iris/Public/Iris/Serialization/InstancedStructNetSerializer.h`
  - `:64-80`：`USTRUCT() struct FInstancedStructNetSerializerConfig : public FNetSerializerConfig`（**配置是 USTRUCT**，含 `UPROPERTY() TArray<TSoftObjectPtr<UScriptStruct>> SupportedTypes` + 私有 `FInstancedStructDescriptorCache`）
  - `:93`：`UE_NET_DECLARE_SERIALIZER(FInstancedStructNetSerializer, IRISCORE_API);`
  - **结论修正：Iris 的 NetSerializer 机制里没有 "UCLASS" 这一层**——serializer 是普通 C++ `struct`（`InstancedStructNetSerializer.cpp:65` `struct FInstancedStructNetSerializer`）+ 宏声明/实现（`Engine/Source/Runtime/Net/Iris/Public/Iris/Serialization/NetSerializer.h:455-467`）。模块名 **IrisCore**（`Engine/Source/Runtime/Net/Iris/IrisCore.Build.cs:5`）
- 默认**已注册**，无需项目再配：`Engine/Source/Runtime/Net/Iris/Private/Iris/Serialization/InstancedStructNetSerializer.cpp:651-655`
  `FInstancedStructPropertyNetSerializerInfo::FInstancedStructPropertyNetSerializerInfo() : FNamedStructPropertyNetSerializerInfo(FName("InstancedStruct"), UE_NET_GET_SERIALIZER(FInstancedStructNetSerializer))`，`:120` 处 `UE_NET_IMPLEMENT_NETSERIALIZER_INFO(...)`。注册键是**属性 struct 的类型名 "InstancedStruct"**，而 `TInstancedStruct<T>` 在 UHT 里就是 `FInstancedStruct` 类型的属性（`Engine/Source/Programs/Shared/EpicGames.UHT/Types/Properties/UhtStructProperty.cs:616-636`：`[UhtPropertyType(Keyword = "TInstancedStruct")]` → `UhtTemplateStructProperty(..., "TInstancedStruct", 模板实参)`，`ScriptStruct` 取 `FInstancedStruct`）
- 任意内层类型默认放行、按需建 ReplicationStateDescriptor 并缓存：`InstancedStructNetSerializer.cpp:618-645`（`InitInstancedStructNetSerializerConfig`，注释明说 "For now let's allow any UScriptStruct"）+ `:33` 的 cvar `InstancedStruct.MaxCachedReplicationStateDescriptors`
- Iris 的内层载荷同样是**反射粒度**（`FReplicationStateOperations::ApplyStruct(...)`，`InstancedStructNetSerializer.cpp:566-576`）

### 1.4 两条路径的关键差异（**这是 TCS 必须先钉的坑**）

**【源码可证】**
- 内层 struct 若**自带 `NetSerialize` 而没注册 NetSerializer**：
  - 经典路径：走内层自己的 `NetSerialize`（`InstancedStruct.cpp:573-575`）
  - Iris：**忽略自定义 `NetSerialize`，退回反射式 StructNetSerializer**，并打警告 —— `Engine/Source/Runtime/Net/Iris/Private/Iris/ReplicationState/ReplicationStateDescriptorBuilder.cpp:2630-2632`：`UE_LOGF(LogIris, Warning, "Generating descriptor for struct %ls that has custom serialization.", ...)`（白名单机制：`BaseEngine.ini:1554+` 的 `SupportsStructNetSerializerList`，读取处 `ReplicationStateDescriptorBuilder.cpp:2279-2291`）
  - `FInstancedStruct` 自己因为**有专属 serializer（名字注册）**所以不受此警告影响（`IsStructWithCustomSerializer` 先查 `FindStructSerializerInfo`，`ReplicationStateDescriptorBuilder.cpp:2266-2277`）

---

## 二、含 `TFunction` 成员的 USTRUCT/UCLASS 做 `UPROPERTY` 会怎样

### 2.1 `UPROPERTY() TFunction<...> X;` → **UHT 编译期报错**（不是静默跳过）

**【源码可证】**
- UHT 的成员属性解析入口只认 `UPROPERTY` 宏（`Engine/Source/Programs/Shared/EpicGames.UHT/Parsers/UhtPropertyParser.cs:1076` 的 `UPROPERTYKeyword`）；属性类型的解析里，**未匹配到任何已知模板类型时走默认解析器**：`UhtPropertyParser.cs:1152-1180`（`UhtDefaultPropertyParser.DefaultProperty`，其中 `findOptions = DelegateFunction | Enum | Class | ScriptStruct`，`type == null` → `return null`）
- 解析不到的类型最终报错文案：`Engine/Source/Programs/Shared/EpicGames.UHT/Utils/UhtSession.cs:2645` —— `Unable to find 'class' or 'struct' or 'enum' or 'delegate' with name '{name}'`（`messageSite.LogError` ⇒ **UHT 失败、编译中断**）
- 旁证：UHT 对"不能复制的成员"一向是**硬报错**而非跳过（TMap `UhtMapProperty.cs:274/:289/:323`、TSet `UhtSetProperty.cs:201/:216/:250`、RPC 里的 delegate 参数 `UhtDelegateProperty.cs:175-187`："Replicated functions cannot contain delegate parameters (this would be insecure)"）
- **未找到**：UHT 里不存在 "TFunction" 关键词处理（`grep -i tfunction` 在 UHT C# 源码中除 `UhtFunction` 的字面重合外零命中）

### 2.2 `TFunction` 作为**非 UPROPERTY** 成员 → 被 UHT 完全忽略；能留在结构里，但**不过网**

**【源码可证】** UHT 只处理 `UPROPERTY` 标注的成员（同上 `:1076`）；而复制侧的属性枚举来自**反射**（`TFieldIterator<FProperty>`：`RepLayout.cpp:6567`、`RepLayout.cpp:6103-6127` 的 `ClassReps`、Iris `ReplicationStateDescriptorBuilder.cpp:2637+`）—— 非 UPROPERTY 成员不在反射表内，因此**既不会被序列化也不会被反序列化**。

**【推断】** 客户端侧的"同类型 struct"里 `OnConsumed` 永远是**默认构造（未绑定）**状态：客户端收到的是"被赋值过的成员"，回调域不会被写过。依据不足点：本轮未实测"客户端实例的 TFunction 是不是空"（未跑引擎），但"过网成员集合 = 反射属性集合"这一点由 §1.2/§1.3 的序列化入口决定，**回调无论如何都不会被传输**。

结论：**`OnConsumed` 可以留在被复制的结构里（必须是普通成员、不能 UPROPERTY），但它的语义只能在权威侧成立；把它写进复制负载的期望是错的。**

---

## 三、容器与复制的限制

### 3.1 `TMap` / `TSet`：**不支持复制**，而且有两道防线

**【源码可证】**
- 第一道（UHT，编译期）：
  - 直接 `UPROPERTY(Replicated) TMap<...>`：`UhtMapProperty.cs:320-324` → `Replicated maps are not supported.`
  - RPC 参数里：`UhtMapProperty.cs:289` → `Maps are not supported in an RPC.`
  - 被复制的 struct **成员里**含 map（未标 `NotReplicated`）→ 递归校验报错：`UhtMapProperty.cs:270-277` `Maps are not supported for Replication or RPCs.  Map '<name>' in '<outer>'.  Origin '<referencingProperty>'`；触发路径：`UhtStructProperty.cs:239-248`（属性带 `CPF_Net` 时调 `ValidateScriptStructOkForNet`）→ `UhtSession.cs:2960-3000`（**递归检查父类链与所有成员**）
  - 同样的机制适用于 TSet：`UhtSetProperty.cs:197-202/:250`
  - `RepSkip` 是唯一豁免口：`Engine/Source/Programs/Shared/EpicGames.UHT/Specifiers/UhtPropertyMemberSpecifiers.cs:287-297`（`NotReplicated` 只为 **struct 成员**开放）
- 第二道（运行期兜底，防"绕过 UHT 的路径"如 `FInstancedStruct` 内层）：`Engine/Source/Runtime/CoreUObject/Private/UObject/PropertyMap.cpp:941-945`
  ```cpp
  bool FMapProperty::NetSerializeItem(FArchive& Ar, UPackageMap* Map, TNotNull<void*> Data, TArray<uint8>* MetaData) const
  {
      UE_LOGF( LogProperty, Error, "Replicated TMaps are not supported." );
      return 1;
  }
  ```
  **只打一条 Error、不写任何数据**（即静默丢数据 + 日志噪音）

### 3.2 嵌套容器：**连 UPROPERTY 声明都不合法**

**【源码可证】**
- 数组自身清掉容器能力位：`UhtArrayProperty.cs:58` `PropertyCaps &= ~(CanBeContainerValue | CanBeContainerKey);`；于是 `TArray<TArray<X>>` / `TArray<TMap<...>>` 触发 `UhtArrayProperty.cs:257-259` → `The type '<TArray<...>>' can not be used as a value in a TArray`
- TMap 同样清位：`UhtMapProperty.cs:64` ⇒ `TMap<FName, TArray<X>>` 在 `UhtMapProperty.cs:335-338` 报 `... can not be used as a value in a TMap`；TSet 同理（`UhtSetProperty.cs:51`）
- 自引用数组限制：`UhtArrayProperty.cs:216-220` `'Struct' recursion via arrays is unsupported for properties.`
- 显式分配器限制：`UhtArrayProperty.cs:208-213` `Replicated arrays with MemoryImageAllocators are not yet supported`；RPC 里的 TArray 必须 const 引用：`:191-197` `Replicated TArray parameters must be passed by const reference`

### 3.3 `TArray<FInstancedStruct>`：**可以复制**，粒度是"整元素"

**【源码可证】** 布局：数组命令 + 内层单条 NetSerialize 命令（`RepLayout.cpp:5679-5712` 数组命令生成 + `:5716-5755` struct 单命令）→ 比较：`RepLayout.cpp:1692-1775`（`CompareProperties_Array_r` 全函数）
- 逐元素与 shadow 比较；**新出现的下标强制视为变化**：该区间内 `bForceFail = bOldForceFail || i >= ShadowArrayNum;`（约 `:1734-1737`）
- 只有在有元素变化时，把变化的元素句柄打包下发（约 `:1751-1765`）；数组变短则只发"新长度"（约 `:1766-1775`）
- 数组长度上限：`RepLayout.cpp:277-293`，`ValidateArraySize` ⇒ **复制数组长度必须 < 65535**，否则 `ensureMsgf` + Error

**成本特征（判据）**
- **经典路径**：以"元素"为最小脏判定单位；元素内部若还有多个属性，变化只发该元素**变化的子属性**（`CompareProperties_r` 递归）；但**元素自身是 `FInstancedStruct`（NetSerialize struct）时，单条命令 ⇒ 元素内任何变化都重发整个元素载荷**，且 `NetSerialize` 每次都会写一次 `UScriptStruct*` 引用（`InstancedStruct.cpp:564-568`，PackageMap 去重后仍有位成本）
- 结构体整体（非数组）属性：`RepLayout.cpp:5716-5755` 单命令 ⇒ **全量重发**该 struct；变更检测靠 `Identical`（`FInstancedStruct` 是深比较，`InstancedStruct.cpp:478-504`）
- 无 PushModel 时**每帧每属性都做比较**（PushModel.h:29-36 原文解释此代价），数组越大比较成本越高

---

## 四、瞬态/事件型数据过网的通行做法对比

### 4.1 `UFUNCTION(Server/Client/Multicast, Reliable)` RPC

**【源码可证】**
- 可靠性只影响"是否进可靠缓冲"：`Engine/Source/Runtime/Engine/Private/NetDriver.cpp:3304-3313`（`if (Function->FunctionFlags & FUNC_NetReliable) Bunch.bReliable = 1;`），且上方有原文警告：`//warning: RPC's might overflow, preventing reliable functions from getting thorough.`
- **可靠缓冲溢出 = 断连**：`NetDriver.cpp:3315-3333`（`Connection->Close(ENetCloseResult::RPCReliableBufferOverflow)`；非可靠时仅 Warning 并丢弃）
- 不可靠 Multicast 被硬性节流：`Engine/Source/Runtime/Engine/Private/DataReplication.cpp:38-42`（cvar `net.MaxRPCPerNetUpdate` 默认 **2**："Maximum number of unreliable multicast RPC calls allowed per net update, additional ones will be dropped"）；执行处 `:2302-2330`（同一函数在一次 net update 内超过阈值直接 `return` 丢弃）
- 单 bunch 位上限：`Engine/Source/Runtime/Engine/Classes/Engine/NetConnection.h:1362-1365`（`GetMaxSingleBunchSizeBits()` = MaxPacket*8 − 头尾开销）
- RPC 参数的类型限制与属性一致（TMap/TSet 不行、delegate 不行、TArray 必须 const 引用）：见 §3.1、§2.1

**适用条件**：一次性、低频、必须"立刻发生"的事件（施法请求、单次结算通知）。**不适合**"高频产生的结果记录"：会撞上 `net.MaxRPCPerNetUpdate`（不可靠）/可靠缓冲（可靠）。粗粒度日志型数据用 RPC 也会失去"按元素增量"。

### 4.2 复制属性（`UPROPERTY(Replicated/ReplicatedUsing)`）

**【源码可证】** 属性由 `FRepLayout` 按 net-update 比较驱动（`RepLayout.cpp` 的 `CompareProperties*`），支持条件（`COND_*`）、RepNotify、优先级/带宽系统；数组元素级增量见 §3.3。

**适用条件**：**有"当前状态"语义**的数据（最新值即真理）。对"不断追加的记录流"不理想：数组越长，比较成本线性上升（Array 的比较是逐元素，`RepLayout.cpp:1726-1740`），且数组重排/中间删除的语义是"影子逐下标对比"（`RepLayout.cpp:1716-1718` 影子数组直接 `Resize` 到新长度），删除中间元素的代价体现为"后面元素全部逐位比较"。

### 4.3 `FFastArraySerializer`（FTR）

**【源码可证】** 头文件 `Engine/Source/Runtime/Net/Core/Classes/Net/Serialization/FastArraySerializer.h`
- 定位与代价（原文，`:49-58`）：适合 **TArrays of UStructs**；大数据集性能好、**任意位置删除最优**、**客户端可拿到 add/remove 事件回调**；代价 = **必须由游戏代码显式标脏**，且**客户端与服务端的列表顺序不保证一致**
- 最小要求（`:59-140` 六步样板）：
  1. item 继承 `FFastArraySerializerItem`（带 `ReplicationID / ReplicationKey / MostRecentArrayReplicationKey`，`:298-310`）
  2. 外层 struct 继承 `FFastArraySerializer`，且**必须有一个名为 `Items` 的 `TArray<Item>`**（`:104-110`）
  3. 实现 `bool NetDeltaSerialize(FNetDeltaSerializeInfo&)` → `FastArrayDeltaSerialize<Item, Array>(Items, Parms, *this)`
  4. `TStructOpsTypeTraits<YourArray>` 打开 `WithNetDeltaSerializer = true`
  5. 变更时 **`MarkItemDirty(item)`**；删除时 **`MarkArrayDirty()`**（`:133-137`）
  6. 客户端回调（可选）：`PreReplicatedRemove / PostReplicatedAdd / PostReplicatedChange`（`:80-84`、`:140-145`）
- 若要"struct delta"级别的增量（只发改动字段），额外硬条件（`:718-730`）：items 数组必须是 FTR 顶层 `UPROPERTY`、**不能标 RepSkip**、**必须是唯一被复制的 items 数组**、不能被塞进静态数组、FTR 不能被嵌套在静态数组里
- 复制布局侧确认同样的假设：`RepLayout.cpp:6373-6380`（"looks for an array property whose inner type is an FFastArraySerializerItem" + 注释承认这些是"约定"而非强约束）
- FFastArraySerializer 在经典与 Iris 下都有支持；Iris 另有原生实现与逐元素变更掩码：`Engine/Source/Runtime/Net/Iris/Private/Iris/ReplicationState/IrisFastArraySerializer.cpp:9-90`、cvar `net.Iris.UseNativeFastArray`（`ReplicationStateDescriptorBuilder.cpp:61-62`）、`net.Iris.UseChangeMaskForTArray`（`:64-65`）

**适用条件小结**：**追加型、可标脏、需要客户端事件回调的列表** —— 正是"高频产生的结果记录"的形态。代价是必须自己维护 `MarkItemDirty` 的调用纪律（漏标 = 那条记录永不复制）。

### 4.4 三种通道的取舍（TCS 关心的维度）

| 维度 | Reliable RPC | 复制属性 | FastArray |
|---|---|---|---|
| 语义 | 事件（一次性） | 状态（最新值即真理） | **事件流 + 状态** |
| 增量能力 | 无（整次调用全量） | 属性/元素级（无 PushModel 则每帧比较） | 元素级 + 显式标脏 |
| 客户端回调 | 函数体本身 | RepNotify（整属性） | **逐条 add/change/remove 回调** |
| 高频代价 | 触发节流/可靠缓冲溢出风险 | 比较成本随数组长度增长 | 只检查已标脏项 |
| 容器限制 | TMap/TSet/delegate 禁 | 同 | 同（items 内层） |
| 可靠性风险 | 可靠 = 溢出断连 / 不可靠 = 可丢 | 由属性条件决定 | 自定义 delta 自带打包/重发逻辑 |

---

## 五、PushModel（推送式脏标记）

### 5.1 宏与开关（5.8 现状）

**【源码可证】**
- 宏定义：`Engine/Source/Runtime/Net/Core/Public/Net/Core/PushModel/PushModel.h:423-460`
  - `IS_PUSH_MODEL_ENABLED()`、`MARK_PROPERTY_DIRTY(Object, Property)`（会校验属性是否 `CPF_Net`）、`MARK_PROPERTY_DIRTY_FROM_NAME(ClassName, PropertyName, Object)`（用 `ClassName::ENetFields_Private::PropertyName` 的 RepIndex，**类/属性名写错则编译失败**）、静态数组变体、`COMPARE_ASSIGN_AND_MARK_PROPERTY_DIRTY`（`:461-472`）
  - 关闭时（`WITH_PUSH_MODEL == 0`）这些宏全是空展开：`:482-495`
- 编译期总开关：`Engine/Source/Runtime/Net/Core/Public/Net/Core/PushModel/PushModelMacros.h:5-7` `#ifndef WITH_PUSH_MODEL / #define WITH_PUSH_MODEL 0`（**默认 0**）
- 由 UBT 按 Target 规则写定义：`Engine/Source/Programs/UnrealBuildTool/Configuration/UEBuildTarget.cs:6566-6574`（`Rules.bWithPushModel ? "WITH_PUSH_MODEL=1" : "WITH_PUSH_MODEL=0"`）
- **Target 规则默认值 = 只有 Editor 打开**：`Engine/Source/Programs/UnrealBuildTool/Configuration/Rules/TargetRules.cs:1522-1527`
  ```csharp
  public bool bWithPushModel { get => bWithPushModelOverride ?? (Type == TargetType.Editor); set => ... }
  ```
  ⇒ **打包出的 Game/Client/Server 默认没有 PushModel 编译开关**（要开必须在 `Target.cs` 里显式 `bWithPushModel = true`）
- 运行期开关：`Engine/Source/Runtime/Net/Core/Private/Net/Core/PushModel/PushModel.cpp:434-440`，cvar **`Net.IsPushModelEnabled`，默认 false**；另有 `Net.MakeBpPropertiesPushModel`（默认 true，`:442-448`）

### 5.2 适用条件（原文摘要）

**【源码可证】** `PushModel.h:9-13`（Epic 自述"实际收益不如预期"）、`:29-36`（传统模式必须每帧比较所有属性）、`:87-95`（**改了值就必须自己调 `MARK_PROPERTY_DIRTY*`**；蓝图按引用传参的自动打脏"仅在从蓝图调用时成立"）、`:103-112`
- **只支持对象顶层属性（RepLayout 的 Parent Commands）** ⇒ 改动 struct / 容器 / 其中嵌套属性时，**必须把宿主 struct 或容器属性整体标脏**
- **标脏不等于跳过比较**：脏属性仍要比较以确定"到底哪变了"

### 5.3 属性如何"选择"推送模式

**【源码可证】** 属性级 opt-in 在生命周期参数上：`Engine/Source/Runtime/Engine/Public/Net/UnrealNetwork.h:134-151`
```cpp
struct FDoRepLifetimeParams { ELifetimeCondition Condition; ELifetimeRepNotifyCondition RepNotifyCondition; bool bIsPushBased = false; ... };
```
配合 `DOREPLIFETIME_WITH_PARAMS_FAST(ClassName, Property, Params)`（`:231-239`）；布局侧读取该位：`RepLayout.cpp:6341`、`:6414`（`bIsPushModelEnabled && LifetimeProp.bIsPushBased`），查询口 `Engine/Source/Runtime/Engine/Public/Net/RepLayout.h:1532` `IsPushModelProperty(Index)`。
⇒ **没有 `UPROPERTY(PushModel)` 这样的 UHT 说明符**：`UPROPERTY(Replicated)` 只完成"可复制"，"推送式"必须写在 `GetLifetimeReplicatedProps` 的 params 里。

### 5.4 与 Iris 的关系

**【源码可证】** `Engine/Source/Runtime/Engine/Private/NetDriver.cpp:1932-1935`：
```cpp
// Disable PushModel globally when the GameNetDriver is running with Iris.
// Temp until we fully integrate Iris PushModel support.
const bool bAllowPushModelHandles = !IsUsingIrisReplication();
UEPushModelPrivate::SetHandleCreationAllowed(bAllowPushModelHandles);
```
另有一组给 Iris 用的转发委托 `UE_NET_SET_IRIS_MARK_PROPERTY_DIRTY_DELEGATE`（`PushModel.h:426-427/:474-475`），且 Iris 自己的"完全推送"由配置面表达：`[/Script/IrisCore.ReplicationStateDescriptorConfig] bEnsureAllClassesAreFullyPushModel`（`BaseEngine.ini:1535-1536`，声明 `Engine/Source/Runtime/Net/Iris/Public/Iris/ReplicationState/ReplicationStateDescriptorConfig.h:59-62`）。
⇒ **5.8 下经典系统与 Iris 的 PushModel 不是同一条路；跑到 Iris 时经典 PushModel 句柄被全局禁用。**

---

## 六、Iris 与 ReplicationGraph 在 5.8 的关系

### 6.1 默认走哪个：**默认经典（Generic），Iris 要显式开**

**【源码可证】**
- 总开关 cvar **`net.Iris.UseIrisReplication`，默认 0**：`Engine/Source/Runtime/Net/Iris/Private/Iris/IrisConfig.cpp:15-16`；判定函数 `:23-26`（`GUseIrisReplication > 0`）；命令行 `-UseIrisReplication=` 解析 `:33-40`
- NetDriver 是否用 Iris 的合成逻辑：`Engine/Source/Runtime/Engine/Private/UnrealEngine.cpp:14798-14840`（`UEngine::WillNetDriverUseIris`）
  - 配置面：`IrisConfig->bCanUseIris`（ini `Engine/Config/BaseEngine.ini:346` `+IrisNetDriverConfigs=(NetDriverDefinition=GameNetDriver, bCanUseIris=true)`，`:347` DemoNetDriver=false）
  - 引擎默认：`bConfigCanUseIris && UE::Net::ShouldUseIrisReplication()`（即 cvar 打开）
  - 覆盖点：`IrisConfig->PreferredReplicationSystem`（`EReplicationSystem::Iris/Generic`）与 `GameInstance->GetDesiredReplicationSystem(...)`（`UnrealEngine.cpp:14823-14840`）
- 结果位：`Engine/Source/Runtime/Engine/Classes/Engine/NetDriver.h:2013-2016` `IsUsingIrisReplication()` → `bIsUsingIris`（`NetDriver.cpp:6604-6606`）

### 6.2 二者不能叠：**ReplicationGraph 与 Iris 互斥**

**【源码可证】**
- `Engine/Source/Runtime/Engine/Private/NetDriver.cpp:7354-7360`
  ```cpp
  if (IsUsingIrisReplication() && NewReplicationDriver)
  {
      checkf(false, TEXT("ReplicationDriver (%s) are not supported with a NetDriver (%s) configured to use Iris"), ...);
      NewReplicationDriver->MarkAsGarbage();
      return;
  }
  ```
- ReplicationGraph 是**独立插件、默认关闭且标记 Beta**：`Engine/Plugins/Runtime/ReplicationGraph/ReplicationGraph.uplugin`（`"EnabledByDefault": false`、`"IsBetaVersion": true`，模块类型 Runtime / LoadingPhase PreDefault）。它实现的是经典体系的 `UReplicationDriver`

### 6.3 哪些能力只在 Iris 下存在（与 TCS 相关）

**【源码可证】**（均在 `Runtime/Net/Iris` 或 `Engine` 的 Iris 分支）
- **`FInstancedStructNetSerializer`**（含描述符缓存、量化数据、delta/IsEqual/CloneDynamicState 全套）：`Iris/Serialization/InstancedStructNetSerializer.cpp:65-128`；经典路径没有这个（经典靠 §1.2 的 RepLayout 委托）
- **结构体自定义 NetSerializer / 命名 struct serializer 注册表**：`Iris/ReplicationState/PropertyNetSerializerInfoRegistry.h:195-203`、`DefaultPropertyNetSerializerInfos.cpp:103-137`（如 `FGameplayTag`→`FGameplayTagNetSerializer`：`Runtime/GameplayTags/Private/GameplayTagNetSerializer.cpp:267`，且有 NetToken 数据流 `FGameplayTagTokenStore`，`NetDriver.cpp:1917`）
- **逐元素变更掩码**（数组非原子，丢包下可能出现"从未同时存在的状态"）：cvar `net.Iris.UseChangeMaskForTArray`，`ReplicationStateDescriptorBuilder.cpp:64-65`
- **过滤/优先级/NetBlob/DataStream 等配置族**：`Engine/Config/BaseEngine.ini:1481-1501`
- 反向：**经典专属**的 `ReplicationGraph`、`PushModel`（`NetDriver.cpp:1934`）、`FastArray 的 DeltaFlags/StructDelta` 路径（Iris 有原生 FTR，见 `IrisFastArraySerializer.cpp:9-90`，但"派生 struct delta"走的是另一套）

---

## 七、与 TCS 的结合结论（逐条：能不能直接复制 / 要改成什么形态才可复制 / 推荐通道）

> 本节每条先给**判断**，再标 **【源码可证】**（依据 §1–§6 锚点）或 **【推断】**（并写清依据不足处）。总体纪律出自既有裁决：流程只在权威侧跑，过网的是产物。

### 7.1 `FDamageRecord`（结果快照：谁打谁/元素/Hit/Crit/数值/Kill/时刻）

- **判断：能直接复制，前提是"扁平 + 反射成员"**。要求：成员只用 `FName / int32 / float / double / bool / FGameplayTag / TObjectPtr<AActor> / 稳定 Id`；**不得**含 TMap/TSet/嵌套容器/TFunction；数组元素建议 < 65535。【源码可证】UHT 容器限制（§3.1/§3.2）+ RepLayout 数组上限（§3.3）
- **推荐通道：`FFastArraySerializer`**（`TArray<FDamageRecord> Records` 包在 `FDamageRecordsContainer : FFastArraySerializer` 里，服务器 `MarkItemDirty`，客户端 `PostReplicatedAdd` → 播 Cue / 飘字）。理由【源码可证】：客户端逐条 add 回调（FastArraySerializer.h:80-84/:140-145）、任意位置删除最优（`:52-55`）、代价是需要人工标脏（`:133-137`）
- **次选：Reliable Multicast RPC**（只在"低频 + 必须立刻"时）。**不要**用不可靠 Multicast 承载记录流：`net.MaxRPCPerNetUpdate` 默认 2，超出直接丢（DataReplication.cpp:38-42/:2324-2330）【源码可证】
- **"时刻"字段**：别传裸 `FDateTime` 语义期望两端可比；引擎通行做法是服务器时间由 `AGameStateBase::ReplicatedWorldTimeSecondsDouble`（`GameStateBase.h:149-151`）同步、客户端用 `GetServerWorldTimeSeconds()`（`:72`）换算。【推断】：TCS 若自建时钟（M0 时钟），应显式定义"记录时刻"是服务器权威时间还是本地呈现时间；依据不足 = 未核验 TCS 侧时基设计（`01-module-m0-core.md` 的时钟章节本轮未逐行复核）

### 7.2 表现用上下文快照（CueId + 快照）

- **判断：CueId 用 `FGameplayTag`/`FName` 直接可复制；快照本体若需要多态，`FInstancedStruct`/`TInstancedStruct` 两条复制路径都支持**。【源码可证】§1.1/§1.2/§1.3
- **"要改成什么形态才可复制"**（重要）：内层被实例化的 struct 必须是**反射纯数据**——
  - 经典路径下它的**全部反射成员**都会被纳入序列化（只跳过 `RepSkip`），**含 TMap 就会走 `FMapProperty::NetSerializeItem` → 只打 Error 不写数据**（`RepLayout.cpp:6567`、`PropertyMap.cpp:941-945`）
  - Iris 路径下同理走反射描述符；若内层 struct 自带 `NetSerialize` 而没注册 serializer，**Iris 会忽略它并打警告**（`ReplicationStateDescriptorBuilder.cpp:2630-2632`）
  - **UHT 无法在编译期替你检查内层类型**（`TInstancedStruct` 只把外层的 `ScriptStruct` 报成 `FInstancedStruct`，`UhtStructProperty.cs:616-636`）⇒ **必须有 TCS 侧校验（M8 校验器）来兜"哪些源类型允许出现在可复制的快照里"**（【推断】自动化校验的落点属 TCS 侧设计，引擎侧不存在该能力）
- **推荐通道**：Cue 上下文如果只是"少量标量 + 枚举/标签"，**首选扁平 USTRUCT 属性**（最省事、最稳）；只有确实需要"一族异质上下文"时才付出 `FInstancedStruct` 的代价（每次发送都要写一次 struct 引用，`InstancedStruct.cpp:564-568`；且每条记录都建/查描述符）【源码可证 + 推断（成本判断为推断）】

### 7.3 `TMap<FName, FTcsParamValue>`（含 `TInstancedStruct`）与 `TMap<FName, double>` 黑板

- **判断：不能直接复制，且"改形态"也救不回它的语义**。【源码可证】`UPROPERTY(Replicated) TMap` 直接报错（UhtMapProperty.cs:323）；放进被复制 struct 里递归报错（:270-277 + UhtSession.cs:2960-3000）；即使绕过（塞进 `FInstancedStruct` 内层）运行期也只打 Error 丢数据（PropertyMap.cpp:941-945）
- **要过网必须改成**：`TArray<FTcsKeyValue{FName Key; double Value;}>`（一维数组、可复制、元素级增量）。**嵌套形态 `TMap<FName, TArray<X>>` 连 UPROPERTY 都声明不了**（UhtArrayProperty.cs:58/:256-259、UhtMapProperty.cs:64）→ 只能摊平成一维数组 + 复合键（如 `FName("Attr.Health")`）
- **TCS 建议**：黑板是**流执行中间态**，与"只过产物"的既定姿态一致 ⇒ **黑板整体不过网**；如果未来确实要"服务器黑板 → 客户端表现"，应产出**专门的快照 DTO**（扁平数组 + 版本号/序号），而不是复制黑板本体。【推断】依据不足点：TCS 的"结果快照"字段清单未在本轮核对，无法判定快照里是否已隐含需要黑板内容

### 7.4 `TMap<FName, TArray<提交>>`（键→折叠 double 的黑板 + 提交队列）

- **判断：不能复制（UHT 层面非法）**，且提交队列里的 `OnConsumed` 回调**永不过网**。【源码可证】§3.2 嵌套容器限制；§2.1/§2.2 `TFunction`
- **要过网必须改成**：按 §7.3 摊平；把"提交"里的回调剥掉，只留数据字段（回调必须在本地由接收方自己按"事件类型"重建）——【推断】：引擎不存在"回调过网"的能力，接收方要重放语义只能靠**事件种类 + 数据**重新绑定本地回调；依据不足点：TCS 的提交类型是否带"事件种类"字段本轮未核对

### 7.5 `TFunction OnConsumed`（及任何回调成员）

- **判断：`UPROPERTY` → UHT 报错；非 UPROPERTY → 合法但不复制，客户端实例上为未绑定状态。** 【源码可证】§2.1/§2.2
- **结论**：**可以留在被复制的结构里**（作为普通成员），但必须明确写下"客户端侧该字段无效"的契约；不要在复制负载的读取路径上依赖它。【推断】"客户端侧为未绑定"由"序列化成员集合 = 反射属性集合"外推，未实机验证

### 7.6 `AActor*` / `TObjectPtr<AActor>`

- **判断：能直接复制**。经典路径：对象引用走 PackageMap/NetGUID（`RepLayout.cpp:5511-5522` 的 `FObjectPropertyBase` 分支 → `PropertyObject` 命令）；Iris：`FObjectPtrPropertyNetSerializerInfo`（`DefaultPropertyNetSerializerInfos.cpp:101` 一带）
- **注意**：客户端**相关性（relevancy）/加载时序**会导致收到 null（这是引擎既有的"未映射引用"语义）。**【推断】** 依据不足点：本轮未核验"actor 不相关时是否一定为 null"的具体代码路径
- **TCS 建议**：结果快照里"谁打谁"用**稳定 Id（实体句柄 + 宿主注册表 Id）**而非 Actor 指针，避免相关性造成的空值与"客户端根本不认识该 Actor"；Actor 引用只作为表现层的可选增量（拿到就播、拿不到就退化为 Id 驱动）。**【推断】**（属设计取舍，不是引擎事实）

### 7.7 `TWeakObjectPtr<AActor>`

- **判断：能复制**。UHT 支持 `TWeakObjectPtr`（`UhtWeakObjectPtrProperty.cs:135-140` 的 `[UhtPropertyType(Keyword = "TWeakObjectPtr")]` → `FWeakObjectProperty`）；经典 RepLayout 有 `PropertyWeakObject`（`RepLayout.cpp:5519-5521`、`:700`、`:842`、`:5284`）与专门比较（`:633-636`）；Iris 有 `FWeakObjectNetSerializer`（`DefaultPropertyNetSerializerInfos.cpp:103-130`）
- **注意**：弱引用过网后"对方端对象不存在/未映射"即为无效 ⇒ 收到 null 是**预期行为而非错误**。【推断】契约层含义，无单一源码锚点

### 7.8 `TArray<FGameplayTag>`

- **判断：能直接复制**（`FGameplayTag` 是含 `FName` 的 USTRUCT；数组元素级增量见 §3.3）
- **Iris 下更优**：有专用 serializer（`GameplayTagNetSerializer.cpp:267` 的 `UE_NET_IMPLEMENT_NAMED_STRUCT_NETSERIALIZER_INFO(..., FGameplayTagNetSerializer)`）与 `FGameplayTagTokenStore`（`NetDriver.cpp:1917`），走 NetToken 去重。【源码可证】

### 7.9 `FTcsSourceHandle`（进程内原子发号 uint64）

- **判断（值层面）**：若它是 USTRUCT + `UPROPERTY` 的 uint64 成员，**能复制**（经典 `PropertyUInt64`，`RepLayout.h:745`；Iris 有 uint64 serializer）。**【源码可证】**
- **判断（语义层面）**：**"进程内发号"跨端不成立** —— 服务器与客户端各自发号会产生"同号不同源"。要么(a) 不过网；要么(b) 只在服务器发号、客户端只读（变成"服务器分配的 Id"）；要么(c) 过网的产物里改用**双端都能解析的身份**（DefId / 实体注册 Id / 稳定的源标识）。**【推断】** 依据不足点：TCS 的 `FTcsSourceHandle` 具体形态（是否只在本进程内用于索引池）未在本轮核对源码；引擎侧不存在"跨进程句柄复制"的现成机制可引

### 7.10 跨端一致性纪律（给实现轮的清单）

1. **DTO 白名单**：可复制结构里只允许 `数值/布尔/FName/FGameplayTag/枚举/稳定 Id/ObjectPtr`；**禁** TMap/TSet/嵌套容器/TFunction/裸函数指针（§2、§3）
2. **多态只用 `FInstancedStruct` 且内层必须是纯反射数据**；内层若自带 `NetSerialize` 要在"经典 ↔ Iris"之间做选择（Iris 默认忽略它，§1.4）
3. **高频记录用 FastArray**，且必须实现"标脏"纪律（`MarkItemDirty/MarkArrayDirty`，§4.3），漏标 = 数据不复制（静默失败）
4. **PushModel 不要作为"默认前提"**：打包 Target 默认不带 `WITH_PUSH_MODEL`（TargetRules.cs:1524），且它是属性级 opt-in + 手动打脏（§5.3）；需要时先改 Target 规则再加参数
5. **Iris/经典的选择要早钉**：默认经典（`net.Iris.UseIrisReplication=0`，§6.1）；一旦用 Iris，`ReplicationGraph` 与经典 PushModel 都不可用（§5.4/§6.2）

---

## 八、未找到 / 未核验项（诚实清单）

- **未找到**：UHT 中任何 "TFunction" 专门处理（结论来自"未知类型 → `Unable to find 'class' or 'struct' or 'enum' or 'delegate' with name ...`"，§2.1）
- **未核验**：ReplicationGraph 插件全部源码（仅确认其与 Iris 互斥的断言 + 其为经典驱动插件；§1.2 的"不改变 struct 可复制性"标注为推断）
- **未核验**：`FInstancedStruct` 在**真实联机**下的吞吐/带宽实测（本轮纯静态源码调研，未编译、未跑引擎）
- **未核验**：TCS 侧 `FDamageRecord` / `FTcsParamValue` / 提交类型的确切字段清单（本文按题面给出的运行态上下文形态作答）
- **未核验**：`FRepLayout::SerializePropertiesForStruct` 在"内层 struct 含对象引用但未跟踪"时的 Shadow State 细节（`RepLayout.cpp:5814-5824` 有相关警告，未展开）

## 九、与既有裁决的关系

- **不推翻任何裁决**：NET-1/NET-2（只留镜像接口位）、D6-4（`ICombatReplicationProxy` 只定契约）保持不变；本文只补"真要做时，哪些数据结构天然可过网、哪些必须先改形态"的源码级事实
- **新增待办建议（供后续轮次入册）**：若恢复联网议题，需新增两条输入——①"可复制 DTO 白名单"（含 M8 校验器兜内层类型）；②"记录通道选型：FastArray vs Reliable RPC"的最终拍板
