# R3 实施计划一：TcsCore + TcsAttribute（内核与属性系统）

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** 在 remake 分支上从零搭建 TcsCore（句柄/池/总线/时钟）与 TcsAttribute（聚合管线/事务），使属性系统独立可运行、可观测。

**Architecture:** 三个 UE 模块（TcsCore ← TcsAttribute；TcsNotation 平级骨架壳——D5-18 v2 依赖边的编译前提）。模块目录收窄口径：**根仅平铺 Build.cs + Module.h/.cpp**，领域代码 Public/Private 分层 + 领域子目录 PascalCase（Public 为对外 include 根）；池化 struct + `UTickableWorldSubsystem` 门面；零战斗词汇进 Core；属性聚合为 recalc 式管线（Override>Add>PercentAdd>Mul>FlatAdd、组内顺序无关、脏标记、1e-5 epsilon）。

**Tech Stack:** UE 5.8 C++、GameplayTags、UBT；编译验证用 unreal-cpp-compile 技能探测引擎路径（引擎路径不写死）。

## Global Constraints

- **禁 TDD**（用户级最高纪律）：无失败测试步骤；每任务验证 = UBT 编译通过 + 定向人工检查。
- **提交纪律**：任何 `git commit` 仅在用户明确授权后执行；计划不含自动提交步骤，任务边界即"停点待检查"。
- **风格**：创建/修改 C++ 前执行 unreal-cpp-style 技能——Tab 缩进、UTF-8 无 BOM、LF；`.h` 用 region（region 前中文注释、region 内重新声明访问域）、`.cpp` 扁平；单 `.cpp` ≤300 行，超出按 `<Name>_<Feature>.cpp` 拆分；文件头 `// Copyright Tirefly. All Rights Reserved.`。
- **命名**：类型 = 前缀字母 + `Tcs` + 语义名（`UTcsAttributeSubsystem` / `FTcsAttributeName` / `TTcsInstanceHandle`）；枚举值 = 枚举名缩写全大写（`ETcsAttributeBoundMode` → `ABM_None`）；API 导出宏 `TIREFLYCOMBATSYSTEM_API`；动词纪律（Resolve 仅用于句柄/Id→对象）。
- **日志**：归 UE 原生分类制（D0-6 v2）——每模块 `DECLARE_LOG_CATEGORY_EXTERN(LogTcs<模块名>, Log, All)` 于**独立通道文件**（`Public/<模块名>LogChannel.h` 声明 + `Private/<模块名>LogChannel.cpp` 定义——使用日志 include LogChannel 头，不 include Module.h），运行期 `log LogTcsXxx Verbose` 可调；**TcsCore 零自建日志设施**（无门面/注册表/级别系统——但保留自身原生分类 `LogTcsCore` 供内核观测：到期回调/总线 flush）；**TcsCore 零日志设施**（无门面/注册表/级别）；屏显 = 测试装置直调 UE API（插件模块零屏显调用）。
- **编译**：UBT 调用方式按 unreal-cpp-compile 技能执行期探测；每任务以 Development Editor 编译通过为完成门槛。
- **依赖铁律**：TcsCore 依赖 Core/CoreUObject/GameplayTags + **Engine（按需——UDeveloperSettings/UTickableWorldSubsystem/UBlueprintAsyncActionBase 等壳类型）**；禁止反向 include；实例数据结构禁 `TSubclassOf`。
- **记法层（D5-18 v2）**：TcsNotation 仅引擎基础依赖；仅内容定义承载模块依赖之（本计划=TcsAttribute），机制层禁止；账本/聚合运行时零约定逻辑——转换只发生在物化/注册边界。
- **确定性**：时钟封装内禁 `FDateTime::UtcNow`/`FPlatformTime`（review 检查点）；单游戏线程断言（越线 ensure）。
- **数值**：聚合比较 epsilon = 1e-5；clamp 三态边界（None/Static/Dynamic）。
- 设计规格来源：仓库内 `Documents/combat-system-design/`（`01-module-m0-core.md`、`02-module-attributes.md`、`2026-09-10-param-value-source-decision-points.md`）；冲突时以设计文档 + 用户拍板为准。

## File Structure（本计划新建，收窄口径：根仅模块壳三文件，领域代码 Public/Private + 领域子目录 PascalCase）

```
E:\Projects_Dev\LegendAutoChess\Plugins\Tirefly\TireflyCombatSystem\   ← 仓库根（remake 分支，干净基线：仅 .uplugin）
  TireflyCombatSystem.uplugin                     （修改：EngineVersion 5.8；Modules 列表）
  Source/
    TcsCore/
      TcsCore.Build.cs                            （根）
      TcsCoreModule.h                             （根：模块类）
      TcsCoreModule.cpp                           （根：IMPLEMENT_MODULE）
      Public/
        TcsCoreLogChannel.h                       （日志通道 D0-6 v2：DECLARE_LOG_CATEGORY_EXTERN(LogTcsCore, Log, All)）
        Handle/TcsSourceHandle.h                 （归属来源标识 + 来源分配）
        Handle/TcsInstanceHandle.h               （句柄模板：Index+Generation）
        Pool/TcsInstancePool.h                   （池模板，header-only）
        EventBus/TcsEventHandler.h               （共享 Handler 基类，UCLASS，Blueprintable）
        EventBus/TcsEventBus.h                   （总线内核：订阅表/双通道/泛化动态多播）
        EventBus/TcsEventBusSubsystem.h          （门面，UTickableWorldSubsystem）
        EventBus/TcsAsyncAction_ListenForCombatEvent.h（BP/CS 订阅入口：Tag 过滤+PayloadType 类型匹配——**A' 反射面组成件，提前铺设**：R3 纯 C++ 无 BP 消费者，BP 节点面/CS 载荷验证留 CS 接入轮实测）
        Clock/TcsTimeSource.h                    （可注入时间源接口）
        Clock/TcsClock.h                         （唯一取时入口）
        Clock/TcsExpiryHeap.h                    （到期最小堆 + FTcsTimeEntryHandle）
        Clock/TcsClockSubsystem.h                （泵门面，UTickableWorldSubsystem）
        Parameter/TcsParamValue.h               （数值配置载体 PV 系列：{TInstancedStruct<FTcsParamValueSource> Source}，默认 Literal——D2-12 载体被取代）
        Parameter/TcsParamValueSource.h         （抽象基类：virtual double Evaluate(const FTcsParamEvaluateContext&)；FTcsParamEvaluateContext 反射可见 USTRUCT——禁 TFunction 成员）
        Parameter/TcsParamTableReader.h         （UINTerface：TryGetNumericParam(FName, out double)——上下文参数访问口，反射面，宿主/UnrealSharp 可实现）
        Parameter/TcsParamSource_Literal.h      （内置源：Literal{Value}）
        Parameter/TcsParamSource_ParamRef.h     （内置源：ParamRef{Key, Fallback}——链式+编辑期 DAG 去重、限同域）
        TcsDeveloperSettings.h                   （UDeveloperSettings 壳）
      Private/
        TcsCoreLogChannel.cpp                     （DEFINE_LOG_CATEGORY(LogTcsCore)）
        EventBus/TcsEventBusSubsystem.cpp
        EventBus/TcsAsyncAction_ListenForCombatEvent.cpp
        Clock/TcsClockSubsystem.cpp
        TcsDeveloperSettings.cpp
    TcsNotation/
      TcsNotation.Build.cs                        （根）
      TcsNotationModule.h / .cpp                  （根：模块类）
      Public/
        TcsNotationLogChannel.h                   （DECLARE_LOG_CATEGORY_EXTERN(LogTcsNotation, Log, All)）
        TcsValueConvention.h                     （ETcsValueConventionFlag EnumFlags + 写入点转换助手，D5-18）
      Private/
        TcsNotationLogChannel.cpp                 （DEFINE_LOG_CATEGORY(LogTcsNotation)）
    TcsAttribute/
      TcsAttribute.Build.cs                       （根）
      TcsAttributeModule.h / .cpp                 （根：模块类）
      Public/
        TcsAttributeLogChannel.h                  （DECLARE_LOG_CATEGORY_EXTERN(LogTcsAttribute, Log, All)）
        Attribute/TcsParamSource_AttributeScaled.h （PV-3 参数取属性值源：{AttributeName, Coefficient, Fallback}——经扩展上下文/ITcsAttributeProvider Evaluate）
        Attribute/TcsAttributeName.h             （FName 显式包装，D2-1）
        Attribute/TcsAttrModInstance.h         （ETcsAttributeOp 含 TAO_FlatAdd / Bound 三态 / ValueDomain 模式）
        Attribute/TcsAttributeInstance.h         （单属性实例：Base/CachedCurrent/Bounds/ValueDomain/ModifierSlots）
        Attribute/TcsAttributeStore.h            （句柄键控容器）
        Attribute/TcsAttributeProvider.h         （对外唯一契约 UINTerface）
        Attribute/TcsAttrModDef.h                （修正器模板资产 D3-19/D2-13：纯模板+OperandDef+ValueConvention 列）
        TcsAttributeSubsystem.h                  （门面）
        Attribute/TcsAttributePipeline.h         （聚合管线声明——**公开面，2026-09-18 用户拍板移出 Private**）
      Private/
        TcsAttributeLogChannel.cpp                （DEFINE_LOG_CATEGORY(LogTcsAttribute)）
        Attribute/TcsAttributePipeline.cpp / _Dependency.cpp / _Cascade.cpp  （聚合/依赖登记/SCC/事务——实现在 Private）
        Attribute/TcsAttrModDef.cpp
        TcsAttributeSubsystem.cpp
```

> 可见性口径（2026-09-18 用户拍板）：**类的声明在 `Public/`、实现在 `Private/`**（本仓 TBNS 既有同款——`Public/Pathfinder/TbnsPathfinder.h` + `Private/Pathfinder/TbnsPathfinder.cpp`；内部细节另有 `_Internal.h` 留 Private）。搬出理由：确认未来会有**跨模块消费者**直调管线驱动求值/事务（不改内部执行逻辑）；推荐路径仍是门面（唯一入口），直调为逃生口。代价与纪律写在该头注释：有 out-of-line 成员必须带 `TCSATTRIBUTE_API`；`private:` 段不属消费契约；**实例由门面拥有、MUST NOT 跨帧持有**（门面销毁后即悬空）。

> 拆分规则：`.cpp` 超 300 行按功能拆 `TcsAttributePipeline_Batch.cpp` 式命名（unreal-cpp-style implementation.md）。头文件 include：公开头以 Public 为根（如 `#include "Attribute/TcsAttributeName.h"`）；日志分类 include `Tcs<名>LogChannel.h`（Public 根相对）——Module.h 不对外引用。

---

### Task 0: 仓库基线确认与三模块骨架

**Files:**
- Modify: `TireflyCombatSystem.uplugin`（EngineVersion → 5.8；Modules 列表写入 TcsCore/TcsNotation/TcsAttribute）
- Create: `Source/TcsCore/TcsCore.Build.cs`、`TcsCoreModule.h/.cpp`（根）、`Public/TcsCoreLogChannel.h` + `Private/TcsCoreLogChannel.cpp`（日志通道）、`Public/TcsDeveloperSettings.h`、`Private/TcsDeveloperSettings.cpp`、`Public/Parameter/TcsParamValue.h` + `Public/Parameter/TcsParamValueSource.h` + `Public/Parameter/TcsParamTableReader.h` + `Public/Parameter/TcsParamSource_Literal.h` + `Public/Parameter/TcsParamSource_ParamRef.h`（PV 系列 2026-09-11）；`Source/TcsNotation/TcsNotation.Build.cs`、`TcsNotationModule.h/.cpp`（根）、`Public/TcsNotationLogChannel.h` + `Private/TcsNotationLogChannel.cpp`、`Public/TcsValueConvention.h`；`Source/TcsAttribute/TcsAttribute.Build.cs`、`TcsAttributeModule.h/.cpp`（根）、`Public/TcsAttributeLogChannel.h` + `Private/TcsAttributeLogChannel.cpp`

**Interfaces:**
- Produces: 可编译的三模块空壳（TcsCore/TcsNotation/TcsAttribute）；日志通道文件 `Public/Tcs<名>LogChannel.h`（DECLARE）+ `Private/Tcs<名>LogChannel.cpp`（DEFINE）——Module.h/Module.cpp 只含模块类。

- [x] **Step 1: 基线确认**

`E:\Projects_Dev\LegendAutoChess\Plugins\Tirefly\TireflyCombatSystem\` 已存在（用户已清理：remake 分支仅剩 .uplugin 与 .git）。确认 `git status` 干净、`git pull origin remake` 无落后。

- [x] **Step 2: .uplugin 更新**

EngineVersion → "5.8"；Modules 按依赖序写入：TcsCore（Runtime）、TcsNotation（Runtime）、TcsAttribute（Runtime）。

- [x] **Step 3: 三模块骨架**

Build.cs 按 unreal-cpp-style Build.cs 章节格式（`PublicDependencyModuleNames.AddRange` 数组式）；TcsCore 依赖 Core/CoreUObject/GameplayTags；TcsNotation 依赖 Core/CoreUObject（模块类 + `Public/TcsValueConvention.h`：`ETcsValueConventionFlag{VCF_None=0, VCF_Percent, VCF_OneMinus, VCF_Negate}` EnumFlags + 静态转换助手 ConvertToCanonical——D5-18）；TcsAttribute 依赖 Core/CoreUObject/Engine/GameplayTags + TcsCore + **TcsNotation**（D5-18 v2：UTcsAttrModDef 约定列与物化转换）。目录按收窄口径：**根仅 Build.cs/Module.h/Module.cpp**，其余 Public/Private 分层——`TcsCore/Public/Parameter/` 五文件（PV 系列 2026-09-11 取代 D2-12 FTcsParamScalar）：`TcsParamValue.h`（`{TInstancedStruct<FTcsParamValueSource> Source}`，默认 Literal）+ `TcsParamValueSource.h`（抽象基类 `virtual double Evaluate(const FTcsParamEvaluateContext&) const = 0`——D3-7 v3 虚分派；**命名 Evaluate**，Resolve 仅句柄/Id→对象；上下文 = **反射可见 USTRUCT，禁 TFunction 成员**）+ `TcsParamTableReader.h`（UINTerface 参数访问口——反射面，宿主/UnrealSharp 可实现）+ `TcsParamSource_Literal.h` + `TcsParamSource_ParamRef.h`（链式+编辑期 DAG 去重、限同域、Fallback 必填）。**等级表源与 `ITcsEntityLevelProvider` 归 TcsState（评判轮修订），不进 TcsCore。**模块类 `FTcsCoreModule : IModuleInterface` + `IMPLEMENT_MODULE`；**日志分类独立通道**（D0-6 v2）：`Public/Tcs<名>LogChannel.h`（DECLARE_LOG_CATEGORY_EXTERN(LogTcsCore/LogTcsNotation/LogTcsAttribute, Log, All)）+ `Private/Tcs<名>LogChannel.cpp`（DEFINE）——Module.h/Module.cpp 只含模块类，使用日志 include LogChannel 头。`UTcsDeveloperSettings : UDeveloperSettings` 空壳（Config 分类名 `Tcs`）。

- [x] **Step 4: 冒烟编译**

按 unreal-cpp-compile 技能探测引擎与 UBT，对 LAC 项目（`E:\Projects_Dev\LegendAutoChess\` 下 .uproject）执行 Development Editor 编译。
Expected: 编译通过。

> **2026-09-11 补充收尾改造完成**（PV 系列）：FTcsParamScalar 删除（零消费者），`Parameter/` 五文件 FTcsParamValue 载体体系落地（原规划目录名 `Vocabulary/`，同日用户拍板更名 Parameter——领域命名消除二义性；FTcsParamValue / FTcsParamValueSource / FTcsParamEvaluateContext / ITcsParamTableReader / FTcsParamSource_Literal / FTcsParamSource_ParamRef），冒烟编译通过。规格偏差待追认：基类 Evaluate 未用纯虚 (=0)——UHT 为 USTRUCT 无条件生成 TCppStructOps 需可默认构造，纯虚报 C2259；改用 StateTree FStateTreeConditionBase 同款默认体 + `meta=(Hidden)`。
> 2026-09-15：FTcsParamValue 补 Evaluate 便利转发（PV-1 增补已拍板，见 2026-09-14 决策文档）

---

### Task 1: 句柄与池（TTcsInstanceHandle / TTcsInstancePool / FTcsSourceHandle）

**Files:**
- Create: `Handle/TcsInstanceHandle.h`、`Handle/TcsSourceHandle.h`、`Pool/TcsInstancePool.h`（全 header-only 模板）

**Interfaces:**
- Produces:
```cpp
template <typename TTagType>
struct TTcsInstanceHandle
{
	static constexpr uint32 InvalidIndex = 0xFFFFFFFF;

	uint32 Index = InvalidIndex;
	uint32 Generation = 0;

	// 句柄有效性
	bool IsValid() const { return Index != InvalidIndex; }
};

struct TIREFLYCOMBATSYSTEM_API FTcsSourceHandle
{
	uint64 Id = 0;
	// 来源有效性
	bool IsValid() const { return Id != 0; }
};

// 来源分配：进程内原子递增（FTcsSourceHandleRegistry::Allocate / Release 由需要方驱动）
template <typename TInstanceType, typename TTagType>
class TTcsInstancePool
{
public:
	// 分配槽位（复用 FreeList；Free 时 Generation + 1）
	TTcsInstanceHandle<TTagType> Allocate();
	// 释放槽位
	void Free(TTcsInstanceHandle<TTagType> Handle);
	// 句柄解析：悬空 ensure（Development）/nullptr（Shipping）
	TInstanceType* Resolve(TTcsInstanceHandle<TTagType> Handle);
	// 句柄有效性（不取对象）
	bool IsValid(TTcsInstanceHandle<TTagType> Handle) const;
	// 稳定序遍历（Index 升序）
	void ForEach(TFunctionRef<void(TInstanceType&)> Fn);

private:
	TArray<TInstanceType> Instances;
	TArray<uint32> Generations;
	TArray<uint32> FreeList;
};
```
- 消费者：总线订阅句柄、到期堆条目、属性实例寻址、计划二全部句柄。

- [x] **Step 1: 三个头文件**（Generation 校验：`Generations[Index] == Handle.Generation`；Allocate/Free/ForEach 进入 `ensure(IsInGameThread())`；池占用统计 CVar `Tcs.Core.PoolSlots`）

> 2026-09-11 实施注记：CVar 需独立翻译单元注册，增设 `Public/TcsCoreStats.h` + `Private/TcsCoreStats.cpp`（`FTcsCorePoolStats` 维护口 + `Tcs.Core.PoolSlots`）；池模板以显式实例化锚点（TcsCoreStats.cpp 内 Anchor 命名空间）确保全成员编译检查，Task 2 起由真实消费者取代。代际奇偶约定（奇=已分配/偶=空闲）供 ForEach 跳空闲槽。

- [x] **Step 2: 编译验证**（Result: Succeeded，2026-09-11）
- [x] **Step 3: 人工检查（检查点 4 提前验）**

PIE 临时测试：分配→Free→旧句柄 Resolve → **ensure 命中**（Development）。

> 2026-09-11：CLI 环境无编辑器，本检查点待用户 PIE 验证（或并入 Task 6 测试装置复验）；代码路径已由显式实例化编译覆盖。
> **验证入口（2026-09-11 增设临时测试装置）**：`Source/TcsCore/Private/Testing/TcsCoreTestRig.{h,cpp}`（临时——Task 6 正式装置落地后删除；纯日志、无屏显，遵守 D0-6 插件模块零屏显纪律）。PIE 控制台执行 `Tcs.Test.Pool`（8 项：分配/解析/释放/复用/遍历/统计 CVar）与 `Tcs.Test.Pool.Dangling`（悬空 Resolve——预期 ensure 命中 + 返回 nullptr）；结果以 `[TcsTest]` 前缀写入 `LogTcsCore`。
> **2026-09-11 用户 PIE 验证通过**：两条命令全项 PASS，检查点 4 提前验闭环。

---

### Task 2: 事件总线（FTcsEventBus + UTcsEventHandler + 双通道）

**Files:**
- Create: `Public/EventBus/TcsEventHandler.h`、`Public/EventBus/TcsEventBus.h`、`Public/EventBus/TcsEventBusSubsystem.h` + `Private/EventBus/TcsEventBusSubsystem.cpp`、`Public/EventBus/TcsAsyncAction_ListenForCombatEvent.h` + `Private/EventBus/TcsAsyncAction_ListenForCombatEvent.cpp`

**Interfaces:**
- Consumes: Task 1 句柄池。
- Produces:
```cpp
UCLASS(Abstract, Blueprintable)
class UTcsEventHandler : public UObject
{
	GENERATED_BODY()

public:
	// 事件处理入口（共享 Handler：事件类型 → Handler CDO；BlueprintNativeEvent——BP 子类可覆写）
	// 2026-09-11 用户拍板补入 EventTag：同一 Handler 订阅多 Tag 不失真，且与 A' 多播 (EventTag, Payload) 形状对齐
	UFUNCTION(BlueprintNativeEvent)
	void HandleEvent(FGameplayTag EventTag, const FInstancedStruct& Payload);
};

struct FTcsEventSubscriptionHandle
{
	TTcsInstanceHandle<struct FTcsEventSubTag> Inner;
};

class FTcsEventBus
{
	// Subscribe(Tag, UTcsEventHandler*, ETcsDispatch::Immediate|FrameEnd) -> FTcsEventSubscriptionHandle
	// Unsubscribe(Handle)（池配对清理）
	// PublishImmediate(Tag, FInstancedStruct)（同步遍历同 Tag 订阅 → Handler CDO）
	// PublishFrameEnd(...)（入队，帧末 flush，稳定序=入队序）
};
UCLASS() class UTcsEventBusSubsystem : public UTickableWorldSubsystem { ... };  // 门面 + 帧末队列 flush 注册到时钟泵

// BP/CS 动态监听层（A'，Lyra GMS 形态）：
DECLARE_DYNAMIC_MULTICAST_DELEGATE_TwoParams(FTcsOnCombatEvent, FGameplayTag, EventTag, FInstancedStruct, Payload);
// UTcsEventBusSubsystem 上：UPROPERTY(BlueprintAssignable) FTcsOnCombatEvent OnEvent;
// （PublishImmediate 同步喂绑定者；帧末通道 flush 时喂——绑定者收全量事件流，按 Tag 自行过滤）

UCLASS() class UTcsAsyncAction_ListenForCombatEvent : public UBlueprintAsyncActionBase
{
	// ListenForCombatEvent(WorldContext, FGameplayTag Filter, UScriptStruct* PayloadType, EMatchType 精确/部分)
	//   -> 绑定门面 OnEvent，类型匹配过滤（消息 struct 与 PayloadType 兼容才触发自身 OnEvent）
	// UPROPERTY(BlueprintAssignable) FTcsOnCombatEvent OnEvent;（子集流）
	// Activate 绑定 / 结束退订
};
```
- 载荷规约：核心词汇事件 = 具体 FStruct 包进 FInstancedStruct；核心不 include 域头。
- 消费者：计划二收集事件协议、Damage.Record、属性变更广播；BP/CS 反射面（UnrealSharp 绑定动态多播）。

- [x] **Step 1: 订阅表 + 句柄配对清理**（TMultiMap + Task 1 池）
- [x] **Step 2: 双通道发布**（立即同步；帧末入队，泵 flush）
- [x] **Step 3: 泛化动态多播 + UTcsAsyncAction_ListenForCombatEvent**（Publish 同步喂/flush 喂；类型匹配过滤；Activate 绑定、结束退订）
- [x] **Step 4: 编译验证**（Result: Succeeded，2026-09-11）
- [x] **Step 5: 人工检查**：PIE 注册测试 Handler → Publish 立即通道同步到达；帧末通道下一帧到达；Unsubscribe 后不再到达（`LogTcsCore` 日志可见）；**动态多播冒烟**——C++ 测试装置绑定 OnEvent 收到 Publish 事件（BP/CS 绑定通路验证；InstancedStruct 节点面/CS 载荷读取两条验证项留 CS 接入轮实测）

> **2026-09-11 用户 PIE 验证通过**：`Tcs.Test.Bus` 全项 PASS（首轮 2 项 FAIL 经查明为测试装置时序假设错误——`SetTimerForNextTick` 非"下一帧"，改逐 tick 轮询后通过；总线实现本身无缺陷）。

> 2026-09-11 实施注记：
> - 增设 `Private/EventBus/TcsEventHandler.cpp`（BlueprintNativeEvent 的 `_Implementation` 需实现体，计划 File 列表未列）。
> - **帧末冲洗时机**：tickable 子系统的 Tick 位于 `UWorld::Tick` 尾部 `TickObjects`（晚于全部 Actor tick 组）——本帧游戏逻辑期间入队的事件在**本帧末**（非下一帧）派发；"下一帧到达"语义由 Task 3 时钟泵接管实现（PrePhysics：时钟推进 → 总线冲洗 → 到期堆）；**Task 3 接线时须停用本子系统自 tick**（`SetTickableTickType(ETickableTickType::Never)`），否则帧末残留 tick 会抢先冲洗。
> - 派发健壮性：快照同 Tag 订阅句柄 + 代际校验（派发中订阅/退订不影响本轮）；Handler 弱引用失效时惰性摘除（防僵尸订阅）。
> - `LogTcsCore` Verbose 日志覆盖订阅/退订/发布/冲洗四点，供本检查点观测。
> - **Handler 签名补 EventTag**（2026-09-11 用户拍板，原规格仅 Payload）：解决"同一 Handler 订阅多 Tag 无法区分"的缺口，并与 A' 动态多播参数形状对齐；零消费者期落地，无迁移成本。**命名同日二次拍板：ActualTag → EventTag**（用户："更符合直觉"；C++ 精确订阅语义下 Actual 无对立面，EventTag 直接描述"事件的 Tag"）——全词汇面已清零旧名（代码 6 文件 + 本计划 + README 决策日志）。
> - **CLI 环境无编辑器，Step 5 待用户 PIE 验证**。
> - **验证入口（2026-09-11 增设临时测试装置）**：PIE 控制台执行 `Tcs.Test.Bus`——检查同 Tag 双通道订阅/立即通道同步到达与保真/帧末通道不当场派发+队列深度/退订后不再到达/动态多播绑定与解绑；帧末到达校验由**逐 tick 轮询**（上限 10 tick）在冲洗发生后自动完成并输出汇总，同时打印"发布帧 vs 到达帧"供观察实际到达时机（当前实现=同帧末；Task 3 时钟泵接管后应变为下一帧）。想看逐笔细节时先执行 `log LogTcsCore Verbose`；`Tcs.Test.Bus.Flush` 可手动冲洗队列（隔离验证冲洗逻辑本身 vs 冲洗驱动时机）。
> - **轮询而非单次延迟的原因（引擎时序事实，2026-09-11 实测）**：`SetTimerForNextTick` 的"下一 tick"可能仍落在**本帧** TimerManager tick（`ExpireTime = InternalTime` + 严格大于判到期），而该 tick 早于 `TickObjects`（帧末冲洗点）——单次延迟会在冲洗前校验而误判 FAIL。此事实已入 `unreal-development-workflow` 引擎机制事实节。

---

### Task 3: 时钟与到期堆（UTcsClockSubsystem / FTcsClock / ITcsTimeSource / FTcsExpiryHeap）

**Files:**
- Create: `Public/Clock/TcsTimeSource.h`、`Public/Clock/TcsClock.h`、`Public/Clock/TcsExpiryHeap.h`、`Public/Clock/TcsClockSubsystem.h` + `Private/Clock/TcsClockSubsystem.cpp`

**Interfaces:**
- Consumes: Task 2 帧末 flush 注册。
- Produces:
```cpp
struct FTcsClock
{
	uint64 Frame = 0;
	double Elapsed = 0.0;      // 累计 ScaledDt
	double DeltaSeconds = 0.0;
};

struct FTcsTimeEntryHandle
{
	TTcsInstanceHandle<struct FTcsExpiryTag> Inner;
};

class FTcsExpiryHeap
{
public:
	// 入堆（到期时刻排序最小堆；条目含 Generation 防悬空回调）
	FTcsTimeEntryHandle Push(double DueTime, uint64 OwnerId, TFunction<void(uint64)> OnDue);
	// 惰性取消（标记无效，出队跳过）
	void Cancel(FTcsTimeEntryHandle Handle);
	// 推进：弹出全部到期项并回调（到期时刻升序）
	void AdvanceTo(double Now);
};

UCLASS() class UTcsClockSubsystem : public UTickableWorldSubsystem
{
	// 泵顺序（PrePhysics）：ITcsTimeSource 取 DeltaSeconds（默认 World DeltaSeconds × TimeDilation）
	//   → FTcsClock 推进 → 总线帧末 flush → FTcsExpiryHeap.AdvanceTo
};
```
- 禁 wall-clock：时钟封装内不出现 FDateTime/FPlatformTime（review 检查点）。
- 消费者：计划二 WaitDelay、（未来）冷却/状态到期。

- [x] **Step 1: ITcsTimeSource 默认实现 + FTcsClock 推进**
- [x] **Step 2: FTcsExpiryHeap**（手写堆或 TSortedMap；Generation 校验；堆深度统计 CVar）
- [x] **Step 3: 泵顺序接线**（含 Task 2 帧末 flush；到期回调打 `LogTcsCore` Log 级日志——含到期时刻，供检查点 7 观测）
- [x] **Step 4: 编译验证**（Result: Succeeded，2026-09-16）
- [x] **Step 5: 人工检查（检查点 7 提前验）**

PIE 设 `slomo 0.1`：Output Log 中到期回调的时刻增量随游戏时间减速；`pause` 时冻结（`LogTcsCore` 时间戳对照）。

> **2026-09-16 检查点 7 验收状态**：**slomo 半边已实测确认**（用户双跑对照——slomo 0.1 vs slomo 1：游戏时间增量恒 ≈0.50s，实时帧数按 1/TimeDilation 从 54 拉长至 400+，ScaledDt 语义成立；slomo 1 全项 14/14 PASS、迟滞 0.005s）。**`pause` 冻结项未单独回报**（观测方式：暂停期轮询随 TimerManager 停摆 + 到期回调静默，解除后补触发）——如需补验：执行 `Tcs.Test.Clock` 后在 0.5s 窗口内 `pause` 观察。

> **2026-09-16 实施注记（OpenSpec 提案 add-tcscore-clock-expiry-heap 规格先行）**：
> - **泵点机制修正（引擎事实，源码核实）**：Tickable 自 tick 位于 `UWorld::Tick` 尾部 `FTickableGameObject::TickObjects`（LevelTick.cpp:1821，晚于全部 tick 组与 TimerManager）——无法承担 PrePhysics 泵点；改由 **`FWorldDelegates::OnWorldTickStart`**（`UWorld::Tick` 头部广播，LevelTick.cpp:1522）驱动。`UTickableWorldSubsystem` 壳类型保留（01 §2.3 口径）但自 tick 全程停用：**重写 `GetTickableTickType()` 返回 Never**——引擎在 `Initialize` 内以 `GetTickableTickType()` 返回值注册自 tick（WorldSubsystem.cpp:101），构造期 `SetTickableTickType(Never)` 会被重注册覆盖，重写才是全程停用的正确机制。总线子系统同款处理。
> - **引擎信号核实**：暂停判定 = `UWorld::IsPaused()`（LevelTick.cpp:1565 同口径，暂停帧步长兜 0）；TimeDilation 读 `WorldSettings->TimeDilation`（slomo 改写目标，WorldSettings.h:742；无 WorldSettings 世界按 1.0 兜底）。时间源 = 抽象 C++ 接口 `ITcsTimeSource`（非 UINTerface——引擎管道设施非 Def 配置数据）+ `FTcsTimeSource_Default`（Raw × TimeDilation），宿主经 `SetTimeSource` 注入替换。
> - **FTcsExpiryHeap**：条目池化（TTcsInstancePool 代际校验）；**Cancel 即刻回收槽位（代际 +1），堆内残留条目出队时凭代际失配跳过**；AdvanceTo **先换出当前到期集再逐项回调**（回调内新入堆归下一拍——与总线冲洗换出同纪律）；稳定序 = (DueTime 升序，同刻按入堆 Sequence)；回调先拷出再触发（回调内入堆可致池扩容/槽位复用，原条目指针失效）；深度统计 CVar `Tcs.Core.ExpiryHeapDepth`（`FTcsCoreHeapStats`，对齐 PoolStats 无参对签名纪律）。
> - **子系统增补消费者入口**：`PushExpiry` / `CancelExpiry` 转发内部到期堆（对齐总线门面"转发而非暴露内核"模式），`AdvanceTo` 保持泵私有——plan1 File 列表未列，OpenSpec 提案 R3 已同步。
> - **帧末通道派发时机变化（BREAKING）**：总线自 tick 停用后，帧末事件于下一帧泵点（早于该帧游戏逻辑）派发——event-bus 能力规格由本提案 MODIFIED；临时测试装置 `Tcs.Test.Bus` 新增"到达帧 = 发布帧 + 1"检查。
> - **验证入口（临时装置增补，不入库）**：PIE 控制台执行 `Tcs.Test.Clock`——A 段本地堆机制 5 项（泵已推进/到期升序+同刻入堆序/取消不回调/回调内入堆归下一拍/深度统计增减）+ B 段泵集成轮询 4 项（0.5s 条目泵点触发/触发时刻与时机/深度回落，末行打印入堆帧 vs 触发帧供 slomo/pause 对照）；**故意 ensure 的悬空 Cancel 检查拆为独立命令 `Tcs.Test.Clock.Dangling`**（对齐 `Tcs.Test.Pool.Dangling` 先例）。
> - **"首次运行红字刷屏 + 断点式卡顿"的真因（2026-09-16 用户实测反馈，引擎机制）**：ensure **每站点每进程只上报一次**（站点持 `static std::atomic<uint8> bExecuted`，触发时 `exchange(GEnsureResetState)`——已等于重置态则静默返回）——故编辑器会话内首次跑到该站点才有完整输出（Error 级约 11 行：ensure 消息 + 4 行 callstack + `EnsureFailed` 重复块），且 `core.EnsureBreakEnabled`（默认 true）令附加调试器时断点捕获（即卡顿来源）；重跑同一路径完全静默。`core.ResetEnsureState` 可重置以复现。**实践结论已入 `unreal-development-workflow` 引擎机制事实节**：故意 ensure 必须独立 opt-in 命令。
> - **装置三轮校准（2026-09-16，用户 PIE 实测驱动）**：①首轮"触发迟滞 ≤1s"判据被 PIE 单帧 dt 尖刺（实测 +3.5s）误判——迟滞由 `AdvanceTo(Elapsed)` 结构性保证（迟滞不可能超过一个泵点），降级为日志观测项；②次轮泵集成 30 tick 预算在 60fps 下差 1~2 帧未及 Due；③轮次三 slomo 0.1 下空图跑至约 80fps，0.5 游戏秒需约 402 帧，400 tick 预算被精确击穿——**tick 预算改实时 20 秒**（fps 无关）+ 中间轮询静默；④控制台命令在编辑器侧执行、时机可能早于同外帧泵点，导致发布帧与冲洗帧重合（Bus"到达帧=发布帧"FAIL）——**帧末发布/入堆改由 TimerManager 回调执行**（UWorld::Tick 内序恒为 泵点→tick 组→TimerManager，回调必然晚于同帧泵点），帧号判定转为确定性，"到达帧 > 发布帧"判据保留 `>=` 口径。轮询随暂停冻结（TimerManager 暂停期不 tick）——pause 观测语义：轮询停摆 + 到期回调静默 = 冻结确认。
> - **检查点 7 slomo 半边已验（2026-09-16，用户双跑对照）**：slomo 0.1 vs slomo 1——游戏时间增量恒 ≈0.50s（49.355−48.855 / 54.004−53.499），实时时长约 400 帧 vs 54 帧（按 1/TimeDilation 拉长）——ScaledDt 语义确认；slomo 1 全项 14/14 PASS、迟滞 0.005s。

---

### Task 4: TcsAttribute 类型与存储（FTcsAttributeName / Modifier / Bounds / Instance / Store / Provider；**骨架已在 Task 0 创建——本任务只补类型文件**）

**Files:**
- Create: `Source/TcsAttribute/TcsAttribute.Build.cs`（Task 0 骨架已建，本任务补依赖与内容）、`Public/Attribute/TcsAttributeDef.h` + `Private/Attribute/TcsAttributeDef.cpp`（定义行 + 运行期资产，同文件）、`Public/Attribute/TcsAttributeBounds.h`（2026-09-17 四次复评拆出的值域词汇：边界三态 + 值域模式）、`Public/Attribute/TcsAttributeName.h`、`Public/Attribute/TcsAttrModInstance.h`、`Public/Attribute/TcsAttributeInstance.h`、`Public/Attribute/TcsAttributeStore.h`、`Public/Attribute/TcsAttributeProvider.h`、`Public/Attribute/TcsAttrModDef.h` + `Private/Attribute/TcsAttrModDef.cpp`、`Public/TcsAttributeSubsystem.h` + `Private/TcsAttributeSubsystem.cpp`

**Interfaces:**
- Consumes: TcsCore 句柄/FTcsSourceHandle。
- Produces:
```cpp
struct FTcsAttributeName
{
	FName Name;
	// 显式包装：裸 FName/TEXT 传不进属性 API（D2-1）；内部缓存稠密 int32 id
	explicit FTcsAttributeName(FName InName);
};

UENUM()
enum class ETcsAttributeOp : uint8
{
	TAO_Add = 0,          // 默认（值 0）
	TAO_Override = 1,
	TAO_PercentAdd = 2,
	TAO_Mul = 3,
	TAO_FlatAdd = 4       // D2-10：乘后平坦加，不受 PercentAdd/Mul 缩放（GAS FixedAdd 借鉴）
	// 纯封闭五带，无 Custom（D2-7：计算在上游传入终值）；新运算 = 末尾加值（判定树②，D2-10 首例）
};

UENUM()
enum class ETcsAttributeValueDomain : uint8
{
	AVD_Clamp = 0,        // 默认（值 0）
	AVD_Custom = 1,       // 逃逸位：IValueDomainPolicy 只接管值域函数，时序/级联/事务由引擎守护
	AVD_Wrap = 2          // 循环值域
};

UENUM()
enum class ETcsAttributeBoundMode : uint8 { ABM_None, ABM_Static, ABM_Dynamic };

struct FTcsAttributeBound              // 每侧一个 Bound（Min/Max 各自三态，D2-4）
{
	ETcsAttributeBoundMode Mode = ETcsAttributeBoundMode::ABM_None;
	double StaticValue = 0.0;          // ABM_Static
	FTcsAttributeName DynamicAttribute; // ABM_Dynamic：先按管线求值（HP≤MaxHP 形态）；自引用禁止
};

struct FTcsAttributeBounds
{
	FTcsAttributeBound Min;
	FTcsAttributeBound Max;
};

UENUM()
enum class ETcsOperandKind : uint8
{
	OPK_Literal = 0,          // 默认（值 0）
	OPK_AttributeScaled = 1   // Operand = Coefficient × Current(Attribute)，收集时求值+读即登记（D2-11/D2-3）
};

// —— 定义侧（模板/Def 配置，D2-13 B 形状；载体 PV 系列换型 FTcsParamValue）——
struct FTcsAttrModOperandDef
{
	ETcsOperandKind Kind = ETcsOperandKind::OPK_Literal;
	FTcsParamValue Literal;                // PV 系列：TInstancedStruct<FTcsParamValueSource>（Literal/ParamRef/等级表/AttributeScaled 源）——物化时 Evaluate 求值
	FTcsAttributeName Attribute;           // OPK_AttributeScaled
	double Coefficient = 1.0;              // 留 double（参数化缩放系数未见需求，判定树候补）
};

// PV-3：参数取属性值源（住 TcsAttribute；Evaluate 经扩展上下文/ITcsAttributeProvider 读属性——Subject=上下文单位；Snapshot 求值一次冻结）
USTRUCT() struct FTcsParamSource_AttributeScaled { FTcsAttributeName Attribute; double Coefficient = 1.0; double Fallback = 0.0; };

// —— 运行侧（M2 账本 ModifierSlots，物化后）：Literal 恒为已解析规范值（D2-12 账本纯净、零膨胀） ——
struct FTcsAttrModOperand              // D2-11：主属性→派生属性载体（D2-4 动态边界同构）
{
	ETcsOperandKind Kind = ETcsOperandKind::OPK_Literal;
	double Literal = 0.0;                  // 已解析规范值（物化器单点转换保证——D2-13）
	FTcsAttributeName Attribute;           // OPK_AttributeScaled
	double Coefficient = 1.0;              // Operand = Coefficient × Current(Attribute)，收集时求值+读即登记
};

UCLASS(BlueprintType)
class UTcsAttrModDef : public UDataAsset    // D3-19：修正器模板（纯模板=默认值；引用处零字段覆写）
{
	// FTcsAttributeName Target; ETcsAttributeOp Op;
	// FTcsAttrModOperandDef Operand;       // D2-13 定义侧形状
	// ETcsValueConventionFlag ValueConvention;  // D5-18 v2：默认 Literal 书写约定（物化时 ConvertToCanonical）
	// int32 SortKey = 0; FName Tag;
};

struct FTcsAttrModInstance
{
	FTcsAttributeName Target;
	ETcsAttributeOp Op;
	FTcsAttrModOperand Operand;   // D2-11：支持属性引用（"1 力量=2 攻击力"= 常驻修正器声明）
	FTcsSourceHandle Source;           // 级联撤销锚点（D2-2）
	int32 SortKey = 0;                 // 优先级带权（Override 0 / Add 10 / PercentAdd 15 / Mul 20 / FlatAdd 30）
	FName Tag;                         // 可选：同来源内分组
};

struct FTcsAttributeInstance
{
	FTcsAttributeName Attr;
	double BaseValue = 0.0;
	double CachedCurrent = 0.0;        // 派生缓存非权威：管线唯一生产者，惰性重算（D2-8）
	bool bDirty = false;
	FTcsAttributeBounds Bounds;
	ETcsAttributeValueDomain ValueDomain = ETcsAttributeValueDomain::AVD_Clamp;
	TArray<FTcsAttrModInstance> ModifierSlots;
};

// FTcsAttributeDef（定义载荷，2026-09-17 复评补入）：USTRUCT —— { BaseValue, Bounds, ValueDomain }
// 双轨制：运行期载体 = UTcsAttributeDef（PrimaryDataAsset，DefId = 属性名 = 解析锚点）
//         编辑期载体 = FTcsAttributeDefTableRow（DataTable 行：DefId + 载荷，行名即属性名；仅供策划编辑）
// 运行期零 DataTable 加载路径；两轨同步器属 M8 工具面

// FTcsAttributeStore：单位句柄键控（→ TMap<FTcsAttributeName, FTcsAttributeInstance>）
UCLASS() class UTcsAttributeSubsystem : public UWorldSubsystem    // M2 不认识时间——非 Tickable（D2-8）
{
	// RegisterUnit(FName UnitName) -> 单位句柄；AddAttribute(单位, FTcsAttributeName, const FTcsAttributeDef&)
	// GetStore(...)（管线与计划二经门面访问）
};

// ITcsAttributeProvider（UINTerface，M2 对外唯一契约——M6 军官组件/Mass 桶适配器都实现它）
UINTERFACE(MinimalAPI)
class UTcsAttributeProvider;
class ITcsAttributeProvider
{
	// GetBaseValue / GetCurrentValue(FTcsAttributeName) / PeekPending
};
```
- 消费者：Task 5 管线、ITcsAttributeProvider 的实现方、计划二 Damage 读 Armor/Attack、扣血写 Health；UTcsAttrModDef 仅落类型与资产类——物化执行器住 TcsState（状态模块轮），R3 竖切经 ApplyModifier API 直挂（Task 6）。

- [x] **Step 1: Build.cs 补依赖（TcsCore + TcsNotation——骨架已建）**（现状确认：Task 0 已就位，本任务未改）
- [x] **Step 2: 类型头文件**（如上；反射 struct 带 GENERATED_BODY 与各模块 API 宏——UBT 按模块名派生：TcsCore 用 `TCSCORE_API`、TcsAttribute 用 `TCSATTRIBUTE_API` 等，2026-09-10 编译实证修正；**全内联值类型不加导出宏**——2026-09-16 链接实证）
- [x] **Step 3: UTcsAttributeSubsystem 门面**（RegisterUnit/AddAttribute/GetStore）+ ITcsAttributeProvider 声明
- [x] **Step 4: 编译验证**

> **2026-09-16 落地实施注记**（提案：`openspec/changes/add-tcsattribute-types-and-store`，已归档；12 delta / 4 能力）：
> - **产物**：TcsCore 侧 `Public/Handle/TcsCombatEntityHandle.h`（实体身份句柄 + 发号器，PV-1 边界让步落地）与两处公共面增补（`FTcsParamValueSource::AllowsValueConvention()` 能力位、`FTcsParamEvaluateContext::GetScriptStruct()` 类型标识虚函数）；TcsAttribute 侧 `Public/Attribute/` 八文件（FTcsAttributeName / FTcsAttrModInstance（五带+操作数双形状+边界三态+值域）/ FTcsAttributeInstance / FTcsAttributeStore / ITcsAttributeProvider / UTcsAttrModDef(+.cpp 的 IsDataValid = D5-18 v3 约定白名单) / FTcsParamSource_AttributeScaled（PV-3））+ `UTcsAttributeSubsystem` 门面。
> - **偏差 1（上下文不持 `Subject`）**：`FTcsCombatEntityHandle` 是纯 C++ 账本面值类型，**不能作反射 USTRUCT 的 UPROPERTY**；而 PV-1 已把 `Subject`/`EffectiveLevel` 的落地时机定为"随 TcsState 等级源同批"。故 `FTcsAttributeEvaluateContext` 只持 `Provider`（单位由读口实现者绑定——02 §2.3 本就是"计算器不关心单位载体"）。已回写提案规格与 design.md。
> - **偏差 2（Core 上下文 +1 虚函数）**：PV-1 的"结构体继承 + 源内 checked cast"缺载体（USTRUCT 无内建类型查询，已核源码）；按 GAS `FGameplayEffectContext::GetScriptStruct` 同款机制补类型标识虚函数，源侧以 `IsChildOf` 判定（支持多层派生）。属落地期增补，已记入提案钉名表与规格。
> - **机制发现 1（导出宏边界，Task 1 潜伏缺陷）**：全内联值类型加模块导出宏会让消费方**导入**不存在的符号——MSVC 只为"本模块自己用到的类型"生成导出符号，`FTcsSourceHandle` 的隐式构造与 `FTcsCombatEntityHandleRegistry::Allocate` 在链接期以 LNK2019 炸开。已修为：宏只给"有 out-of-line 成员（TcsCoreStats）或反射符号（USTRUCT/子系统类）"的类型；同时修掉 `FTcsEventSubscriptionHandle` 同类潜伏项。后续模块新增值类型一律照此。
> - **机制发现 2（UHT 枚举注释）**：枚举值上方的 `//` 注释被 UHT 当作该值的 ToolTip 元数据，与同值 `UMETA(... ToolTip=...)` 并存报 `Metadata key 'ToolTip' first seen ... then ...` 并致编译失败——前缀注记已移入枚举 doc 块（引擎事实，已记入 `unreal-development-workflow` 技能）。
> - **临时装置**：`Source/TcsAttribute/Private/Testing/`（不入库、清单随 Task 6 删除/并入）`Tcs.Test.Attribute`（15 项正向：属性名语义与哈希 / 带权五带 / 约定能力位 / 属性值源三路径 / 模板校验五例 / 单位注册 / 定义与初值 / 空查询 / 值域回读 / 容器地址稳定性 / 注销清记录）与 `Tcs.Test.Attribute.Dangling`（5 项拒绝面，**故意 ensure 独立 opt-in**——沿用 MEM-20260916-01 纪律）。
> - **编译**：Development Editor 通过（零警告）。**用户 PIE 实测待跑**（`Tcs.Test.Attribute`；`.Dangling` 按需）。
> - **给 Task 5 的输入**：折叠器按 Op 分桶（`SortKey` 仅展示位，带权唯一真相在 `GetTcsAttributeBandWeight`）；`AVD_Custom` 收口点须 ensure 提示（值域策略接口 R3 未建）；`GetCurrentValue` 惰性重算口与 `PeekPending` 语义随管线补入 Store/门面。

> **2026-09-17 复评补正**（用户三条疑问驱动；提案 `design.md`「复评定案」5-7 条）：
> - **补词表行类型**：本 Task 4 的类型清单里漏了"属性定义"这一件——定义数据被内联成 `DefineAttribute(单位, 属性名, BaseValue, Bounds, ValueDomain)` 的 5 个参数，而设计口径是"词表本体 = 项目 DataTable"（02 §2.1；竖切剧本"属性表 4 行 = FName 词表 + 显式包装结构 + DataTable 行"；plan2 Task 6"FName 键 + Base + Bounds"）。已补 `Public/Attribute/TcsAttributeDef.h`（`FTableRowBase` 派生 = DataTable 行类型：`BaseValue` / `Bounds` / `ValueDomain`，**行名即属性名**——名称不进结构体以免双真相），门面改为 **`AddAttribute(Unit, Name, Def)`**。
> - **命名改 `AddAttribute`**（原 `DefineAttribute`）：动词纪律下 "Define" 易读成"定义词表"（那是 M8 的事），本 API 的动作是"往单位加一条属性实例"。
> - **`UTcsAttrModDef` 基类 `UDataAsset` → `UPrimaryDataAsset`**（**Def 资产族统一约定**）：查全部 11 篇设计文档 + 决策点文档，`PrimaryDataAsset` 零命中——原基类只是本计划写下、从未被论证；而 Def 的引用语义本就是 "FName Id + 注册表/DefLibrary 解析"（03 §2 命名批 / 06 `ResolveDef`），主资产身份让该解析与按类型发现/加载归引擎。族级约定已回写 **02 §2.2 / 03 §2 / 08 / project.md**；未来 `UTcsStateDef` 家族与 `UTcsSkillDefMod` 同此基类（族内混用两套基类会让 DefLibrary 发现逻辑分叉）。`PrimaryAssetTypes` 注册属 M6 DefLibrary 轮。
> - **词表装载/注册仍归 M8**（`FAttributeRegistry`：`Resolve(FName) → 稠密 id`、重名/非法引用加载期报错、行名 ↔ 常量映射校验、DevSettings 指路 DataTable）——零消费者不预建；R3 由调用方显式传定义行（plan2 Task 6 建的那张属性 DataTable 在 R3 只验"能被反射承载"）。
> - 装置增至 **16 项**（新增"词表行 DataTable 承载往返"）；编译再次通过（零警告）；PIE 实测待跑。

> **2026-09-17 二次复评补正：文件名去类型前缀（UE 规范）**：原命名（`UTcsAttrModDef.h` / `FTcsAttributeName.h` / `TTcsInstancePool.h` / `ITcsTimeSource.h` 等）违反 UE「文件名 = 类型名去前缀字母」规范，且**是本计划与 plan2 的 File Structure 系统性写下的**——存量违规含 Task 0-3 既有文件，共 **35 个文件**全量改名（`Public`/`Private` 成对同改）。同步范围：全库 `#include`（含 `.generated.h`——UHT 按头文件名生成产物）、本计划与 plan2 的 File Structure 与各 Task 文件清单（**含尚未创建的文件**，否则后续任务照旧名再犯）、活动规格/设计文档的路径引用；归档提案 `openspec/changes/archive/` 保持原貌（冻结历史）。规范落点：`unreal-cpp-style`（structure.md 新增「文件命名」节 + 检查清单项）与 `openspec/specs/cpp-module-structure`（本提案 ADDED 需求「文件名去类型前缀」）。全量编译通过（零警告），UHT 产物按新名重生。

> **2026-09-17 三次复评补正：属性定义改双轨制（Def 族统一语义）**：用户澄清既定策略——**DataTable 供策划编辑（编辑器阶段），运行期一律用 DataAsset**（资产制扩展性好：未来给 AttributeDef 加 Fragment 之类只动资产与载荷）。故属性的"定义"从单一 `FTcsAttributeDef : FTableRowBase` 拆成三件套：**载荷 `FTcsAttributeDef`（纯 USTRUCT）** + **运行期资产 `UTcsAttributeDef : UPrimaryDataAsset`**（`DefId` = 属性名 = DefLibrary 解析锚点；`IsDataValid` 报空 DefId、提示资产名与 DefId 不一致）+ **编辑期表行 `FTcsAttributeDefTableRow : FTableRowBase`**（`DefId` + 载荷，行名即属性名）。**两轨一致性由编辑器侧同步器维护（资产为权威），运行期零 DataTable 加载路径**；同步器与词表装载属 M8。门面保持只认载荷（`AddAttribute(单位, 属性名, 载荷)`，不依赖资产类型）。族级语义已回写 **02 §2.1 / 03 §2 / 08 §5**（全 Def 族适用，含 `FTcsBuffDefTableRow` 家族）。装置经后续两轮补正增至 **32 项**（PIE 中应 32 项全 PASS——新增运行期资产承载与资产校验，表行检查改 `FTcsAttributeDefTableRow`）；编译通过（零警告）。

> **2026-09-17 四次复评补正（用户五条口径）**：①**形态收口**——载荷层 `FTcsAttributeDef` 删除：`FTcsAttributeDefTableRow`（`DefId` + `BaseValue` + `Bounds` + `ValueDomain`）成为**字段形状唯一声明处**，`UTcsAttributeDef` 组合持有一行（不复制字段集）；两类型同住 `TcsAttributeDef.h`（不拆文件）。②**修正器模板同款**（用户要求，已实现）——新增 `FTcsAttrModDefTableRow : FTableRowBase`（`TemplateId` + 模板字段），`UTcsAttrModDef` 改为 `TemplateId` + 行；**局限在案**：模板行含 `FTcsParamValue`（`TInstancedStruct`）列，CSV/Excel 往返丢该列，只支持编辑器内表格编辑。③**调用面收口**——单位侧只认属性名：`AddAttribute(单位, 属性名)` / `RemoveAttribute(单位, 属性名)`；定义解析移入门面内部（新增**属性定义表** + `RegisterAttributeDef(行)` / `FindAttributeDef`，宿主/DefLibrary 加载定义资产后登记）；`RemoveAttribute` 语义 = 整条属性下线（实例连同槽位内容丢弃；来源级联撤销仍走 `RemoveBySource`，二者不互相替代）。④**AttributeSet（2026-09-17 已裁决为 D2-15，随 M6 轮落地）**——不同情景下同一 CombatEntity 类需要不同属性集合；**形态 B1a+B2+B3a**（实体侧引用 / 资产 GameInstance 级 + 施加 World 级 / diff 替换），内容 `TArray<FName> DefIds`（覆写列首版不做）。裁决与取舍详见决策文档 `2026-09-17-attribute-set-and-existence-decision-points.md`。⑤**文件职责整理与命名归位**——`TcsAttributeModifier.h` 按用户提议改名 `TcsAttrModInstance.h`（族内类型统一短前缀：`FTcsAttrModInstance` / `FTcsAttrModOperand(Def)`），并拆出值域词汇：新增 `TcsAttributeBounds.h`（边界三态 + 值域模式，复评后合并为一个文件），原文件只留运算带 / 运算数双形状 / 账本修正器。⑥ **Def 命名标准**：定义资产 = `<族>Def`（**去 Asset 后缀**）、表行 = `<族>DefTableRow`——`UTcsAttributeDefAsset` → `UTcsAttributeDef`（文件同名 `TcsAttributeDef.h`/`.cpp`），文档侧未实现的 `UTcsStateDefAsset`/`UTcsBuffDefAsset`/`UTcsSkillDefAsset` 同步为 `UTcsStateDef`/`UTcsBuffDef`/`UTcsSkillDef`；标准条文落 `openspec/project.md`。装置改为按名添加/移除并覆盖"定义未登记/重复登记/移除未持有"拒绝面；编译通过（零警告）。

---

### Task 5: 聚合管线（recalc + 依赖登记 SCC + clamp + 事务）——**已完成并归档（2026-09-18）**

**Files:**
- Create: `Public/Attribute/TcsAttributePipeline.h`（**声明在 Public**——2026-09-18 用户拍板：确认未来有跨模块消费者）+ `Private/Attribute/TcsAttributePipeline.cpp`（实现留 Private；超 300 行按 `TcsAttributePipeline_Batch.cpp` 拆分）；`UTcsAttributeSubsystem` 暴露入口

**Interfaces:**
- Consumes: Task 4 类型。
- Produces:
```cpp
class FTcsAttributePipeline
{
public:
	// 读：按需 recalc（脏标记）——求值语义（非 Resolve：不从句柄取对象）
	double EvaluateCurrent(单位句柄, FTcsAttributeName);
	// 写：挂修正器 / 按来源移除（D2-2 级联）
	void ApplyModifier(单位句柄, FTcsAttrModInstance);
	void RemoveBySource(单位句柄, FTcsSourceHandle);
	// 事务：BeginBatch / Commit（提交尾行内 flush：单帧多次变更只算一次，逐属性广播一次）
	//       PeekPending（不落账读预览）
	// 派生依赖：读即登记（管线内读其他属性 → 记边）；环检测 = Tarjan SCC；成环 ensure + 拒绝该边
	// 聚合：五带顺序无关——((Base+ΣAdd)×(1+ΣPercentAdd))×ΠMul + ΣFlatAdd 后按 ValueDomain 收口（Clamp/Wrap/Custom）；
	//       Operand 求值挂接（D2-11）：OPK_AttributeScaled 的 Operand = Coefficient × Current(Attribute)——
	//       求值走读即登记（D2-3），主属性变化自动把派生属性标脏（"1 力量=2 攻击力"零宿主维护）；
	//       Override 存在时按"优先级（OverridePriority，大者胜）→ 同优先级策略（OverrideTieBreak，属性定义侧四值）
	//       → 有符号值"选出一条替换整个结果（FlatAdd 一并被覆盖，"最强覆盖生效"语义不变；默认全 0 + 取最大 ≡ 旧口径）；
	//       带权 Override 0 / Add 10 / PercentAdd 15 / Mul 20 / FlatAdd 30（D2-10）
	// 广播：变更 → 总线立即通道（比较 epsilon 1e-5，未变不广播）
};
```
- 消费者：计划二 Damage 公式与扣血、屏显验收信号。

- [x] **Step 1: recalc 聚合**（单属性：Modifiers 四桶 → 公式 → clamp → CachedCurrent）
- [x] **Step 1b（2026-09-18 增补）: 属性冻结暂存区**（规格 delta 草稿见决策文档 §附录，写本任务提案时复制进 `specs/attribute-store/spec.md`）——`RemoveAttribute` 改"冻结整条实例"（搬进暂存区、不销毁、日志）、`AddAttribute` 解冻优先（整条搬回）、双态约束、单位注销释放暂存区；`RemoveBySource` 扫描面含暂存区（同批实现与验证）
- [x] **Step 2: 事务与行内 flush**（Batch 计数；Commit 尾对 bDirty 属性逐个 recalc+广播）
- [x] **Step 3: 依赖登记 + Tarjan SCC 环检测**（R3 无派生属性数据，机制先立、有派生数据时复验）
- [x] **Step 4: RemoveBySource**（按 Source 过滤移除 → bDirty → recalc）
- [x] **Step 5: 编译验证**

> **2026-09-17/18 输入增补（D2-14 裁决折入）**：①`AddAttribute` / `RemoveAttribute` MUST 走与 modifier 同一 store 变更路径与同一事务纪律（批内加属性与批内挂 modifier 行为一致——02 §4 已写明）；②对"modifier 的 Target 已无实例"给出**确定行为**（忽略 + 日志，不 ensure）；③**属性冻结暂存区（2026-09-18 设计定稿，本任务落地）**——`RemoveAttribute` 改为**冻结整条实例**（搬进暂存区、不销毁、输出日志）、`AddAttribute` **解冻优先**（整条搬回，基础值取回冻结前的值）、双态约束（同名不同时存在于容器与暂存区）；④**`RemoveBySource` 的扫描面 MUST 含暂存区**——否则来源在冻结期间结束、其修正器永久滞留、属性恢复后凭空多出数值（本条与本轮的 `RemoveBySource` 同批实现与验证）；⑤装置补检查：逐字段保真 / 解冻取回冻结前的值 / 双态互斥 / 单位注销释放暂存区。⑥AttributeSet（D2-15）不在本任务范围，随 M6 轮。

---

### Task 6: 计划一验收（检查点 2/3/4 + 屏显信号）——**已完成（2026-09-18）**

**Files:**
- Create: 测试装置最小代码（PIE 测试 GameMode/测试 Actor——C++ 测试装置，非内容资产；**落点 = `Source/TcsIntegration/Testing/` 临时代码目录**——随 plan2 Task 5/6 并入 TcsIntegration 或验收后删除，不进插件正式模块面）

- [x] **Step 1: 测试装置**：PIE 生成 2 单位；单位 A 定义 Health(0..MaxHealth)/Attack/Armor；验收信号 = 测试装置订阅属性变更广播 → **直调 `GEngine->AddOnScreenDebugMessage`**（屏显）
- [x] **Step 2: 检查点 2**：一个测试 Source 挂 2 modifier（Attack+10 / Armor×1.5）→ 屏显确认 **1 次重算 + 1 次广播**；RemoveBySource 后数值还原
- [x] **Step 3: 检查点 3**：Batch 内两次改同属性 → 提交尾只 1 次广播；PeekPending 返回未提交值
- [x] **Step 4: 检查点 4**：句柄悬空 ensure（Task 1 已验，此处复验于真实单位数据）
- [x] **Step 5: 全量编译 + 停点待用户检查**（提交经用户授权）——编译零警告；用户 PIE 两轮（首轮 3/1 → 修夹具 → 次轮 **4/0 全 PASS**，2026-09-18）

> **2026-09-18 落地实施注记**（提案：`openspec/changes/add-tcsattribute-pipeline-and-transaction`，18 delta / 2 新能力 + 3 修订）：
> - **产物**：`Public/Attribute/TcsAttributeBandFold.h`（折叠纯函数 + `FTcsAttributeBandEntry`，**Public**——M5/TcsDamage 复用；plan1 原清单只列了 Private 的管线文件，规范扫描时补正）、`Public/Attribute/TcsAttributeChangedEvent.h/.cpp`（变更事件 USTRUCT + 原生 Tag `Tcs.Event.Attribute.ValueChanged`——命名公约 `Tcs.Event.<域>.<事件名>`，2026-09-18 用户拍板；由事件所属模块原生声明，TcsCore 不持战斗域词汇）、**`Public/Attribute/TcsAttributePipeline.h`**（管线类声明，2026-09-18 用户拍板从 Private 移出——确认未来有跨模块消费者；PIMPL 前向声明仍由门面持有）+ `Private/Attribute/TcsAttributePipeline.cpp`（recalc/收口/事务/PeekPending/广播/求值栈）、`TcsAttributePipeline_Dependency.cpp`（Tarjan SCC + 读即登记 + 脏传播）、`TcsAttributePipeline_Cascade.cpp`（RemoveBySource + SetBaseValue）；`TcsAttributeStore` 增冻结暂存区 / 依赖边 / 批深度；门面转发六个管线入口 + `SetBaseValue`，`RemoveAttribute` 改冻结、`AddAttribute` 改解冻优先。
> - **偏差 1（实体句柄升格为反射 USTRUCT）**：事件载荷走总线（`FInstancedStruct`）MUST 反射可见，而 `FTcsCombatEntityHandle` 原为纯 C++ 值类型（Task 4 的 D4 分界）→ **升格为 `USTRUCT(BlueprintType)` + 导出宏**（反射类型必须带宏），且 `Id` 由 `uint64` 改 **`int64`**（UHT 不支持 uint64 作为属性类型；句柄恒为正，无实际差异）。副作用正面：PV-1 规划的上下文 `Subject`（需 UPROPERTY）随之解禁。已回写 `instance-handle-pool` 规格（MODIFIED）。
> - **偏差 2（新增 `SetBaseValue`）**：规格的"改基值"写操作类需要落点（02 §2.2a 的"等级成长 = 宿主升级事务改基值"），plan1 Task 5 接口清单原本没列 → 补 `UTcsAttributeSubsystem::SetBaseValue(单位, 属性名, 值)`（标脏 + 按事务纪律重算/广播，批外立即生效）。
> - **偏差 3（成环语义按设计原文）**：设计写"成环 ensure + **拒绝该边**"（而非整条属性零写入）——实现为：登记边时跑 Tarjan SCC，成环则撤销刚登记的边 + ensure，读者用被读者的上一缓存值，求值有限不递归。提案 delta 的场景措辞已按此对齐。重算轮数上限（64 轮）只作收敛安全网，**与环判定解耦**（修旧 TCS 8 轮误判深链的缺陷）。
> - **偏差 4（每单位运行时状态住容器）**：批深度与依赖边作为 `FTcsAttributeStore` 字段（随单位注销一并释放，无跨单位残留）——提案 `attribute-transaction` 需求已同步。
> - **偏差 5（`AddAttribute` 新建即脏，2026-09-18 首轮 PIE 实测暴露）**：规格 delta 原写 `bDirty = false`——"从定义行抄来的缓存初值"被当成已结算值，**值域收口被整段跳过**（基础值 150、边界 0..100、Clamp 的属性会一直读到 150，直到某个后续写入把它标脏；装置断言"收口 Clamp 越界钳到上边界"FAIL 即此）。改为**新建即脏**：初值在"批外读取或提交"这一刻由管线结算（那一刻单位上的属性图通常已搭好，**定义/添加顺序不影响结果**）；不在 `AddAttribute` 内立即结算的理由——动态边界会读到尚未添加的引用属性（求值 0）→ 把原值钳成 0 且无人再标脏。代价：首次结算可能广播一次（旧值 = 原始基础值），"结算前的缓存值不对外承诺"。delta 已改写（含"越界初值经结算收口"新场景）。
> - **偏差 6（`RemoveBySource` 摘冻结区后标脏，同轮暴露）**：从冻结暂存区摘除修正器后 MUST 标脏该冻结实例——否则来源在冻结期结束后，解冻会把"已被撤销来源的旧缓存值"带回（装置断言"扫描面含冻结暂存区"FAIL 即此：期待 77、实得 110）。
> - **语义澄清 7（Wrap 跨度成立条件，同轮暴露）**：`AVD_Wrap` 只在**两侧边界齐备且 `Max > Min`** 时回卷；跨度未成立（任一侧 `ABM_None`，或 `Max ≤ Min`）时**不回卷、返回聚合原值**（确定、不 ensure——"Wrap 却没给跨度"是作者侧配置错误，热路径不拦，留给 M8 定义校验矩阵）。首轮 FAIL"收口：Wrap 按跨度回卷"根因是**装置夹具**给的是只有上界的边界，而规格场景前提是"值域 0..100"；已修夹具并加一条退化解检查把该分支钉住，规格补对应场景。
> - **增补 8（2026-09-18 用户拍板，落地期）：覆盖带强弱口径**——原"Override 组取最大值"隐含"数值大 = 更强"，而数值本身不含方向（承伤倍率/冷却这类"越低越强"的属性会被取到最温和的一条）。定为三级阶梯：①`OverridePriority`（修正器侧，大者胜，唯一第一裁决键）→ ②`OverrideTieBreak`（属性定义侧，封闭四值：取最大/取最小/绝对值最大/绝对值最小，**不开放自定义策略**）→ ③有符号值（补齐全序，"策略下打平"如 `OTB_MaxAbs` 的 ±5 必须有确定答案）。**策略住属性定义而非修正器**（放修正器上会变成"两个来源各说各话"，等于又需要一条规则来裁决规则）；**框架不定义任何其它"谁盖谁"的规则**（用户口径：跨来源协调完全交给 Priority，否则属二次规则）。默认值 = 历史行为（全 0 + 取最大 ≡ 旧口径），已有检查全部保持绿。数据驱动路径（模板行）同步持 `OverridePriority`，`IsDataValid` 对"非覆盖带填了它"给警告。**`SortKey` 与本机制无关**：它在属性折叠里始终是零语义展示位（09-module-damage 里才有"选一"语义），02 §2.2 已补定位说明。
> - **装置**：`Tcs.Test.Attribute` 增补管线段（折叠四例 / 隐式批广播计数 / 干净零重算 / Clamp·Wrap·动态边界 / AttributeScaled 读即登记传播 / 批内读旧值 + PeekPending 预览 + 提交单次广播 / 来源级联两属性 / 冻结解冻往返与双态约束 / 注销释放暂存区 / **覆盖带六例 + 覆盖带端到端四例**）+ 订阅 Handler（`UTcsTestAttributeChangeHandler`）；拒绝面命令增三条故意 ensure（`AVD_Custom` 回落 / 依赖成环拒边 / 无批提交）+ 模板校验两条（非覆盖带填优先级的警告 / 覆盖带无警告）。装置自身修三处：来源发号器改**单一实例**（原为临时对象，每次构造计数器归零 → 四个来源同 Id → 一次 `RemoveBySource` 摘光全部来源，断言形同虚设）、定义登记改幂等（定义表跨命令调用存活——二次运行同一命令不得重复登记）、管线段加"预热结算"循环（新建属性先读一遍落账，广播计数才只反映真正的变更）。
> - **编译**：Development Editor 通过（零警告）。**用户 PIE 实测三轮：首轮 46 通过/6 失败、次轮 51/1（Wrap 夹具）、第三轮 `Tcs.Test.Attribute` 67 条全 PASS（2026-09-18）→ 提案已归档为 `openspec/changes/archive/2026-09-18-add-tcsattribute-pipeline-and-transaction`，18 delta 已并入 `openspec/specs/`（11 条规格全部 `validate --strict` 通过）。下一站：Task 6（验收装置：检查点 2/3/4 屏显信号）。**

---

> **2026-09-18 Task 6 落地实施注记**（装置：`Source/TcsAttribute/Private/Testing/TcsAcceptanceRig.h/.cpp`，63 + 457 = 520 行；命令 `Tcs.Test.Acceptance.Plan1`，PIE 中运行）：
> - **Step 1 装置**：屏显订阅者 `UTcsTestScreenChangeHandler`（订阅 `Tcs.Event.Attribute.ValueChanged` 立即通道 → 到达即直调 `GEngine->AddOnScreenDebugMessage` + 计数/留存载荷）+ 三个检查点段 + 屏显固定行号排版（`FScreenSection`：行键固定 → 重跑覆盖同批行，保留 600 秒）。夹具 = 单位 A（Health 上界动态取 MaxHealth / MaxHealth / Attack / Armor）、单位 B（隔离对照）、单位 C（悬空检查专用）；新建实例"预热结算"后广播计数才只反映真正的变更。
> - **落点偏差 1**：计划写 `Source/TcsIntegration/Testing/`，实际落 `Source/TcsAttribute/Private/Testing/`（与既有两个临时装置同目录）。理由：①`TcsIntegration` 模块归**计划二 Task 0** 建立，Task 6 时点不存在；②本装置只用 M0（总线）+ M2（属性门面）能力，**不是跨模块装置**；③装置属临时件（用毕即删），为它提前建一个零消费者的模块壳与用户既有口径（"零消费者不预建"，`01-module-m0-core.md:22 ④`）冲突。计划二 Task 6 的 C++ 测试装置不受影响——其落点 `Source/TcsIntegration/Testing/`（`plan2:259`，注明"plan1 Task 6 同款约定"）届时已由计划二 Task 0 建好模块；"同款约定"按"**装置住其所属模块的 Testing 目录**"理解，本装置归属 TcsAttribute（它只用 M0 总线 + M2 门面能力），两处约定不冲突。
> - **语义澄清 2（检查点 2 的"1 次广播"）**：计划文本"一个测试 Source 挂 2 个 modifier（Attack+10 / Armor×1.5）→ 1 次重算 + 1 次广播"中，两个 modifier 落在**不同属性**上，而广播是**逐属性**的（`attribute-pipeline` 规格："同一提交内同一属性最多广播一次"）——故本条按"**每受影响属性各 1 次重算 + 1 次广播**"落地（两属性 = 本批 2 条），装置屏显直接写明该口径。"同属性两次变更只 1 条"由检查点 3 承担，两处合起来才是完整的"不会重复重算/重复广播"证明。
> - **装置增补 3**：计划只写"2 单位"，装置用 3 个——单位 C 仅供检查点 4 的"真实单位数据上复验悬空"（注册 → 挂属性 → 注销 → 旧句柄访问）；单位 B 作隔离对照（A 的修正器全程不影响 B），把"2 单位"用成真检查而非装饰。
> - **实施提醒 4（对计划二同样适用）**：**unity build 会把同模块的多个 `.cpp` 合并成一个翻译单元**——匿名命名空间里的通用符号名会跨文件相撞。本次首编译即中招：装置内的 `MakeLiteralModifier` / `IsClose` 与 Task 5 装置的 **同名函数** 冲突（`error C2084: 已有主体`），报错落在**旧装置**文件里、极易误判方向。修法 = 装置内符号统一加装置前缀（本次 `Acceptance` 前缀），并把该纪律写进装置源文件头。计划二的装置（TcsEffect/TcsDamage/TcsIntegration）落地时同样适用。
> - **屏显与日志**：屏显 = 人工验收信号（TcsCore 零屏显调用纪律保持——屏显只在装置内）；同批写 `LogTcsAttribute`（Display 汇总行 + Verbose 逐条广播到达）留痕。
> - **风格例外 5**：装置 `.cpp` 457 行，超"单 `.cpp` ≤300 行、超出按 `<Name>_<Feature>.cpp` 拆分"风格线。不拆的理由：拆分需为一个用毕即删的临时装置引入内部头 + 跨 TU 声明（拆分成本 > 收益），且既有两个临时装置（1023 / 726 行）同款未拆——该风格线的拆分口径按模块正式代码理解。
> - **编译**：Development Editor 通过（**零警告**）。
> - **首轮 PIE 实测（2026-09-18，用户执行）**：**通过 3 / 失败 1**——检查点 2（Attack 20→30、Armor 10→15、逐属性各 1 条广播、来源撤销还原）、检查点 3（PeekPending=50、提交尾 1 条广播）、检查点 4（悬空 `AddAttribute` 返回 false + 命中 `Store != nullptr` ensure @ `TcsAttributeSubsystem.cpp:107`，与装置提示一致）全部 PASS；失败项 = 装置自加的"单位隔离性"对照：`单位 B 的 Health=0.0`。**根因 = 装置夹具缺陷，非实现问题**：属性定义是**按属性名全局共享的一行**（词表语义），单位 A 登记的 `Health` 行带 `Max = Dynamic(MaxHealth)`，单位 B 复用同一行却**没有 `MaxHealth`** → 动态上界解析读到 0 → `Clamp(100, 无下界, 0)` = 0。该行为正是 **plan1 偏差 5** 写明的既定口径（"动态边界会读到尚未添加的引用属性（求值 0）→ 把原值钳成 0"，也正是"不在 `AddAttribute` 内立即结算"的理由）。修法：夹具给单位 B 补 `MaxHealth`（+ 隔离断言与屏显同步加该属性）；**记为待复跑项**。
> - **实测副产品（已登记台账）**：该失败把一个真实风险照出来了——**同一属性名在内容里被多个单位/多套集合共用时，动态边界引用的属性若不在本单位，值会静默钳到 0**（真实内容里就是静默错值，不是夹具问题）。已扩写台账 **R8-2**（定义校验矩阵第二类案例，附本次实测现场）与 **R7-1**（AttributeSet 花名册一致性：含动态边界引用的属性须与被引用属性同集）。
> - **次轮复跑（2026-09-18，用户执行，夹具修复后）**：**通过 4 / 失败 0**——检查点 2（批内 0 条广播 / 提交后 2 条逐属性各 1 / 撤销还原 20·10）、检查点 3（PeekPending=50 / 提交尾 1 条）、检查点 4（悬空写入 false + ensure 命中 + 读取 0）、隔离性（单位 B Attack=20.0 / Health=100.0 / MaxHealth=100.0）全绿。**Task 6 完成，计划一（Task 0–6）全部交付**。
> - **待办**：Step 5 停点——**已于 2026-09-18 由用户两轮 PIE 完成**；两个既有临时装置目录（`Source/TcsAttribute/Private/Testing/`、`Source/TcsCore/Private/Testing/`，含本装置）在计划二 Task 6 的内容资产版装置就位后一并退役删除。
> - **后续修正 6（2026-09-18，用户指正后落地）：故意 ensure 的检查已拆出主命令** ——用户指出"四个都通过了但仍有报错"，复核确认：报错是检查点 4 的**故意** ensure（实现无问题），但**位置错了**——它被混进常规验收命令，于是每次跑验收都付出：Error 日志约 11 行 + 栈回溯 ~0.8s + **错误报告上传 ~1.2s**（`SendNewReport`）+ 附加调试器时的断点停顿。这违反了既有纪律：`unreal-development-workflow` 技能「引擎机制事实」明确写着"测试装置里'故意触发 ensure'的检查 MUST 独立成 opt-in 命令，不混入常规检查命令"（2026-09-16 Task 3 用户实测沉淀），既有装置也是这么分的（`Tcs.Test.Attribute` 干净 / `.Dangling` 单独）——**本装置没照办**。修法：拆为两条命令——主命令 `Tcs.Test.Acceptance.Plan1`（检查点 2/3 + 单位隔离性，**零故意 ensure**，检查点 4 的夹具单位一并移除）+ opt-in 命令 `Tcs.Test.Acceptance.Plan1.Dangling`（自带夹具：注册 → 挂属性 → 预热结算 → 注销 → 旧句柄访问；屏显先声明"本命令会故意命中 ensure"）。编译零警告。**复验待办**：用户复跑两条命令（主命令应 `通过 3 / 失败 0` 且日志无 Error；`.Dangling` 应命中 1 条 ensure 且 `检查点 4 结束：通过`）。
