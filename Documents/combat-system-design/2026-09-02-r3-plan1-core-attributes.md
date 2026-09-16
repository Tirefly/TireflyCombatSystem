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
        Handle/FTcsSourceHandle.h                 （归属来源标识 + 来源分配）
        Handle/TTcsInstanceHandle.h               （句柄模板：Index+Generation）
        Pool/TTcsInstancePool.h                   （池模板，header-only）
        EventBus/UTcsEventHandler.h               （共享 Handler 基类，UCLASS，Blueprintable）
        EventBus/FTcsEventBus.h                   （总线内核：订阅表/双通道/泛化动态多播）
        EventBus/UTcsEventBusSubsystem.h          （门面，UTickableWorldSubsystem）
        EventBus/UTcsAsyncAction_ListenForCombatEvent.h（BP/CS 订阅入口：Tag 过滤+PayloadType 类型匹配——**A' 反射面组成件，提前铺设**：R3 纯 C++ 无 BP 消费者，BP 节点面/CS 载荷验证留 CS 接入轮实测）
        Clock/ITcsTimeSource.h                    （可注入时间源接口）
        Clock/FTcsClock.h                         （唯一取时入口）
        Clock/FTcsExpiryHeap.h                    （到期最小堆 + FTcsTimeEntryHandle）
        Clock/UTcsClockSubsystem.h                （泵门面，UTickableWorldSubsystem）
        Parameter/FTcsParamValue.h               （数值配置载体 PV 系列：{TInstancedStruct<FTcsParamValueSource> Source}，默认 Literal——D2-12 载体被取代）
        Parameter/FTcsParamValueSource.h         （抽象基类：virtual double Evaluate(const FTcsParamEvaluateContext&)；FTcsParamEvaluateContext 反射可见 USTRUCT——禁 TFunction 成员）
        Parameter/ITcsParamTableReader.h         （UINTerface：TryGetNumericParam(FName, out double)——上下文参数访问口，反射面，宿主/UnrealSharp 可实现）
        Parameter/FTcsParamSource_Literal.h      （内置源：Literal{Value}）
        Parameter/FTcsParamSource_ParamRef.h     （内置源：ParamRef{Key, Fallback}——链式+编辑期 DAG 去重、限同域）
        UTcsDeveloperSettings.h                   （UDeveloperSettings 壳）
      Private/
        TcsCoreLogChannel.cpp                     （DEFINE_LOG_CATEGORY(LogTcsCore)）
        EventBus/UTcsEventBusSubsystem.cpp
        EventBus/UTcsAsyncAction_ListenForCombatEvent.cpp
        Clock/UTcsClockSubsystem.cpp
        UTcsDeveloperSettings.cpp
    TcsNotation/
      TcsNotation.Build.cs                        （根）
      TcsNotationModule.h / .cpp                  （根：模块类）
      Public/
        TcsNotationLogChannel.h                   （DECLARE_LOG_CATEGORY_EXTERN(LogTcsNotation, Log, All)）
        FTcsValueConvention.h                     （ETcsValueConventionFlag EnumFlags + 写入点转换助手，D5-18）
      Private/
        TcsNotationLogChannel.cpp                 （DEFINE_LOG_CATEGORY(LogTcsNotation)）
    TcsAttribute/
      TcsAttribute.Build.cs                       （根）
      TcsAttributeModule.h / .cpp                 （根：模块类）
      Public/
        TcsAttributeLogChannel.h                  （DECLARE_LOG_CATEGORY_EXTERN(LogTcsAttribute, Log, All)）
        Attribute/FTcsParamSource_AttributeScaled.h （PV-3 参数取属性值源：{AttributeName, Coefficient, Fallback}——经扩展上下文/ITcsAttributeProvider Evaluate）
        Attribute/FTcsAttributeName.h             （FName 显式包装，D2-1）
        Attribute/FTcsAttributeModifier.h         （ETcsAttributeOp 含 TAO_FlatAdd / Bound 三态 / ValueDomain 模式）
        Attribute/FTcsAttributeInstance.h         （单属性实例：Base/CachedCurrent/Bounds/ValueDomain/ModifierSlots）
        Attribute/FTcsAttributeStore.h            （句柄键控容器）
        Attribute/ITcsAttributeProvider.h         （对外唯一契约 UINTerface）
        Attribute/UTcsAttrModDef.h                （修正器模板资产 D3-19/D2-13：纯模板+OperandDef+ValueConvention 列）
        UTcsAttributeSubsystem.h                  （门面）
      Private/
        TcsAttributeLogChannel.cpp                （DEFINE_LOG_CATEGORY(LogTcsAttribute)）
        Attribute/FTcsAttributePipeline.h / .cpp  （聚合/依赖登记/SCC/事务——模块内部）
        Attribute/UTcsAttrModDef.cpp
        UTcsAttributeSubsystem.cpp
```

> 拆分规则：`.cpp` 超 300 行按功能拆 `FTcsAttributePipeline_Batch.cpp` 式命名（unreal-cpp-style implementation.md）。头文件 include：公开头以 Public 为根（如 `#include "Attribute/FTcsAttributeName.h"`）；日志分类 include `Tcs<名>LogChannel.h`（Public 根相对）——Module.h 不对外引用。

---

### Task 0: 仓库基线确认与三模块骨架

**Files:**
- Modify: `TireflyCombatSystem.uplugin`（EngineVersion → 5.8；Modules 列表写入 TcsCore/TcsNotation/TcsAttribute）
- Create: `Source/TcsCore/TcsCore.Build.cs`、`TcsCoreModule.h/.cpp`（根）、`Public/TcsCoreLogChannel.h` + `Private/TcsCoreLogChannel.cpp`（日志通道）、`Public/UTcsDeveloperSettings.h`、`Private/UTcsDeveloperSettings.cpp`、`Public/Parameter/FTcsParamValue.h` + `Public/Parameter/FTcsParamValueSource.h` + `Public/Parameter/ITcsParamTableReader.h` + `Public/Parameter/FTcsParamSource_Literal.h` + `Public/Parameter/FTcsParamSource_ParamRef.h`（PV 系列 2026-09-11）；`Source/TcsNotation/TcsNotation.Build.cs`、`TcsNotationModule.h/.cpp`（根）、`Public/TcsNotationLogChannel.h` + `Private/TcsNotationLogChannel.cpp`、`Public/FTcsValueConvention.h`；`Source/TcsAttribute/TcsAttribute.Build.cs`、`TcsAttributeModule.h/.cpp`（根）、`Public/TcsAttributeLogChannel.h` + `Private/TcsAttributeLogChannel.cpp`

**Interfaces:**
- Produces: 可编译的三模块空壳（TcsCore/TcsNotation/TcsAttribute）；日志通道文件 `Public/Tcs<名>LogChannel.h`（DECLARE）+ `Private/Tcs<名>LogChannel.cpp`（DEFINE）——Module.h/Module.cpp 只含模块类。

- [x] **Step 1: 基线确认**

`E:\Projects_Dev\LegendAutoChess\Plugins\Tirefly\TireflyCombatSystem\` 已存在（用户已清理：remake 分支仅剩 .uplugin 与 .git）。确认 `git status` 干净、`git pull origin remake` 无落后。

- [x] **Step 2: .uplugin 更新**

EngineVersion → "5.8"；Modules 按依赖序写入：TcsCore（Runtime）、TcsNotation（Runtime）、TcsAttribute（Runtime）。

- [x] **Step 3: 三模块骨架**

Build.cs 按 unreal-cpp-style Build.cs 章节格式（`PublicDependencyModuleNames.AddRange` 数组式）；TcsCore 依赖 Core/CoreUObject/GameplayTags；TcsNotation 依赖 Core/CoreUObject（模块类 + `Public/FTcsValueConvention.h`：`ETcsValueConventionFlag{VCF_None=0, VCF_Percent, VCF_OneMinus, VCF_Negate}` EnumFlags + 静态转换助手 ConvertToCanonical——D5-18）；TcsAttribute 依赖 Core/CoreUObject/Engine/GameplayTags + TcsCore + **TcsNotation**（D5-18 v2：UTcsAttrModDef 约定列与物化转换）。目录按收窄口径：**根仅 Build.cs/Module.h/Module.cpp**，其余 Public/Private 分层——`TcsCore/Public/Parameter/` 五文件（PV 系列 2026-09-11 取代 D2-12 FTcsParamScalar）：`FTcsParamValue.h`（`{TInstancedStruct<FTcsParamValueSource> Source}`，默认 Literal）+ `FTcsParamValueSource.h`（抽象基类 `virtual double Evaluate(const FTcsParamEvaluateContext&) const = 0`——D3-7 v3 虚分派；**命名 Evaluate**，Resolve 仅句柄/Id→对象；上下文 = **反射可见 USTRUCT，禁 TFunction 成员**）+ `ITcsParamTableReader.h`（UINTerface 参数访问口——反射面，宿主/UnrealSharp 可实现）+ `FTcsParamSource_Literal.h` + `FTcsParamSource_ParamRef.h`（链式+编辑期 DAG 去重、限同域、Fallback 必填）。**等级表源与 `ITcsEntityLevelProvider` 归 TcsState（评判轮修订），不进 TcsCore。**模块类 `FTcsCoreModule : IModuleInterface` + `IMPLEMENT_MODULE`；**日志分类独立通道**（D0-6 v2）：`Public/Tcs<名>LogChannel.h`（DECLARE_LOG_CATEGORY_EXTERN(LogTcsCore/LogTcsNotation/LogTcsAttribute, Log, All)）+ `Private/Tcs<名>LogChannel.cpp`（DEFINE）——Module.h/Module.cpp 只含模块类，使用日志 include LogChannel 头。`UTcsDeveloperSettings : UDeveloperSettings` 空壳（Config 分类名 `Tcs`）。

- [x] **Step 4: 冒烟编译**

按 unreal-cpp-compile 技能探测引擎与 UBT，对 LAC 项目（`E:\Projects_Dev\LegendAutoChess\` 下 .uproject）执行 Development Editor 编译。
Expected: 编译通过。

> **2026-09-11 补充收尾改造完成**（PV 系列）：FTcsParamScalar 删除（零消费者），`Parameter/` 五文件 FTcsParamValue 载体体系落地（原规划目录名 `Vocabulary/`，同日用户拍板更名 Parameter——领域命名消除二义性；FTcsParamValue / FTcsParamValueSource / FTcsParamEvaluateContext / ITcsParamTableReader / FTcsParamSource_Literal / FTcsParamSource_ParamRef），冒烟编译通过。规格偏差待追认：基类 Evaluate 未用纯虚 (=0)——UHT 为 USTRUCT 无条件生成 TCppStructOps 需可默认构造，纯虚报 C2259；改用 StateTree FStateTreeConditionBase 同款默认体 + `meta=(Hidden)`。
> 2026-09-15：FTcsParamValue 补 Evaluate 便利转发（PV-1 增补已拍板，见 2026-09-14 决策文档）

---

### Task 1: 句柄与池（TTcsInstanceHandle / TTcsInstancePool / FTcsSourceHandle）

**Files:**
- Create: `Handle/TTcsInstanceHandle.h`、`Handle/FTcsSourceHandle.h`、`Pool/TTcsInstancePool.h`（全 header-only 模板）

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
- Create: `Public/EventBus/UTcsEventHandler.h`、`Public/EventBus/FTcsEventBus.h`、`Public/EventBus/UTcsEventBusSubsystem.h` + `Private/EventBus/UTcsEventBusSubsystem.cpp`、`Public/EventBus/UTcsAsyncAction_ListenForCombatEvent.h` + `Private/EventBus/UTcsAsyncAction_ListenForCombatEvent.cpp`

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
> - 增设 `Private/EventBus/UTcsEventHandler.cpp`（BlueprintNativeEvent 的 `_Implementation` 需实现体，计划 File 列表未列）。
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
- Create: `Public/Clock/ITcsTimeSource.h`、`Public/Clock/FTcsClock.h`、`Public/Clock/FTcsExpiryHeap.h`、`Public/Clock/UTcsClockSubsystem.h` + `Private/Clock/UTcsClockSubsystem.cpp`

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

- [ ] **Step 1: ITcsTimeSource 默认实现 + FTcsClock 推进**
- [ ] **Step 2: FTcsExpiryHeap**（手写堆或 TSortedMap；Generation 校验；堆深度统计 CVar）
- [ ] **Step 3: 泵顺序接线**（含 Task 2 帧末 flush；到期回调打 `LogTcsCore` Log 级日志——含到期时刻，供检查点 7 观测）
- [ ] **Step 4: 编译验证**
- [ ] **Step 5: 人工检查（检查点 7 提前验）**

PIE 设 `slomo 0.1`：Output Log 中到期回调的时刻增量随游戏时间减速；`pause` 时冻结（`LogTcsCore` 时间戳对照）。

---

### Task 4: TcsAttribute 类型与存储（FTcsAttributeName / Modifier / Bounds / Instance / Store / Provider；**骨架已在 Task 0 创建——本任务只补类型文件**）

**Files:**
- Create: `Source/TcsAttribute/TcsAttribute.Build.cs`（Task 0 骨架已建，本任务补依赖与内容）、`Public/Attribute/FTcsAttributeName.h`、`Public/Attribute/FTcsAttributeModifier.h`、`Public/Attribute/FTcsAttributeInstance.h`、`Public/Attribute/FTcsAttributeStore.h`、`Public/Attribute/ITcsAttributeProvider.h`、`Public/Attribute/UTcsAttrModDef.h` + `Private/Attribute/UTcsAttrModDef.cpp`、`Public/UTcsAttributeSubsystem.h` + `Private/UTcsAttributeSubsystem.cpp`

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
struct FTcsAttributeModOperandDef
{
	ETcsOperandKind Kind = ETcsOperandKind::OPK_Literal;
	FTcsParamValue Literal;                // PV 系列：TInstancedStruct<FTcsParamValueSource>（Literal/ParamRef/等级表/AttributeScaled 源）——物化时 Evaluate 求值
	FTcsAttributeName Attribute;           // OPK_AttributeScaled
	double Coefficient = 1.0;              // 留 double（参数化缩放系数未见需求，判定树候补）
};

// PV-3：参数取属性值源（住 TcsAttribute；Evaluate 经扩展上下文/ITcsAttributeProvider 读属性——Subject=上下文单位；Snapshot 求值一次冻结）
USTRUCT() struct FTcsParamSource_AttributeScaled { FTcsAttributeName Attribute; double Coefficient = 1.0; double Fallback = 0.0; };

// —— 运行侧（M2 账本 ModifierSlots，物化后）：Literal 恒为已解析规范值（D2-12 账本纯净、零膨胀） ——
struct FTcsAttributeModOperand              // D2-11：主属性→派生属性载体（D2-4 动态边界同构）
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
	// FTcsAttributeModOperandDef Operand;       // D2-13 定义侧形状
	// ETcsValueConventionFlag ValueConvention;  // D5-18 v2：默认 Literal 书写约定（物化时 ConvertToCanonical）
	// int32 SortKey = 0; FName Tag;
};

struct FTcsAttributeModifier
{
	FTcsAttributeName Target;
	ETcsAttributeOp Op;
	FTcsAttributeModOperand Operand;   // D2-11：支持属性引用（"1 力量=2 攻击力"= 常驻修正器声明）
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
	TArray<FTcsAttributeModifier> ModifierSlots;
};

// FTcsAttributeStore：单位句柄键控（→ TMap<FTcsAttributeName, FTcsAttributeInstance>）
UCLASS() class UTcsAttributeSubsystem : public UWorldSubsystem    // M2 不认识时间——非 Tickable（D2-8）
{
	// RegisterUnit(FName UnitName) -> 单位句柄；DefineAttribute(单位, FTcsAttributeName, BaseValue, Bounds, ValueDomain)
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

- [ ] **Step 1: Build.cs 补依赖（TcsCore + TcsNotation——骨架已建）**
- [ ] **Step 2: 类型头文件**（如上；反射 struct 带 GENERATED_BODY 与各模块 API 宏——UBT 按模块名派生：TcsCore 用 `TCSCORE_API`、TcsAttribute 用 `TCSATTRIBUTE_API` 等，2026-09-10 编译实证修正）
- [ ] **Step 3: UTcsAttributeSubsystem 门面**（RegisterUnit/DefineAttribute/GetStore）+ ITcsAttributeProvider 声明
- [ ] **Step 4: 编译验证**

---

### Task 5: 聚合管线（recalc + 依赖登记 SCC + clamp + 事务）

**Files:**
- Create: `Private/Attribute/FTcsAttributePipeline.h/.cpp`（超 300 行按 `FTcsAttributePipeline_Batch.cpp` 拆分）；`UTcsAttributeSubsystem` 暴露入口

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
	void ApplyModifier(单位句柄, FTcsAttributeModifier);
	void RemoveBySource(单位句柄, FTcsSourceHandle);
	// 事务：BeginBatch / Commit（提交尾行内 flush：单帧多次变更只算一次，逐属性广播一次）
	//       PeekPending（不落账读预览）
	// 派生依赖：读即登记（管线内读其他属性 → 记边）；环检测 = Tarjan SCC；成环 ensure + 拒绝该边
	// 聚合：五带顺序无关——((Base+ΣAdd)×(1+ΣPercentAdd))×ΠMul + ΣFlatAdd 后按 ValueDomain 收口（Clamp/Wrap/Custom）；
	//       Operand 求值挂接（D2-11）：OPK_AttributeScaled 的 Operand = Coefficient × Current(Attribute)——
	//       求值走读即登记（D2-3），主属性变化自动把派生属性标脏（"1 力量=2 攻击力"零宿主维护）；
	//       Override 存在时取 Override 组最大值替换整个结果（FlatAdd 一并被覆盖，"最强覆盖生效"语义不变）；
	//       带权 Override 0 / Add 10 / PercentAdd 15 / Mul 20 / FlatAdd 30（D2-10）
	// 广播：变更 → 总线立即通道（比较 epsilon 1e-5，未变不广播）
};
```
- 消费者：计划二 Damage 公式与扣血、屏显验收信号。

- [ ] **Step 1: recalc 聚合**（单属性：Modifiers 四桶 → 公式 → clamp → CachedCurrent）
- [ ] **Step 2: 事务与行内 flush**（Batch 计数；Commit 尾对 bDirty 属性逐个 recalc+广播）
- [ ] **Step 3: 依赖登记 + Tarjan SCC 环检测**（R3 无派生属性数据，机制先立、有派生数据时复验）
- [ ] **Step 4: RemoveBySource**（按 Source 过滤移除 → bDirty → recalc）
- [ ] **Step 5: 编译验证**

---

### Task 6: 计划一验收（检查点 2/3/4 + 屏显信号）

**Files:**
- Create: 测试装置最小代码（PIE 测试 GameMode/测试 Actor——C++ 测试装置，非内容资产；**落点 = `Source/TcsIntegration/Testing/` 临时代码目录**——随 plan2 Task 5/6 并入 TcsIntegration 或验收后删除，不进插件正式模块面）

- [ ] **Step 1: 测试装置**：PIE 生成 2 单位；单位 A 定义 Health(0..MaxHealth)/Attack/Armor；验收信号 = 测试装置订阅属性变更广播 → **直调 `GEngine->AddOnScreenDebugMessage`**（屏显）
- [ ] **Step 2: 检查点 2**：一个测试 Source 挂 2 modifier（Attack+10 / Armor×1.5）→ 屏显确认 **1 次重算 + 1 次广播**；RemoveBySource 后数值还原
- [ ] **Step 3: 检查点 3**：Batch 内两次改同属性 → 提交尾只 1 次广播；PeekPending 返回未提交值
- [ ] **Step 4: 检查点 4**：句柄悬空 ensure（Task 1 已验，此处复验于真实单位数据）
- [ ] **Step 5: 全量编译 + 停点待用户检查**（提交经用户授权）
