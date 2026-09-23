# C# 脚本方案选型调研：UnrealSharp vs UnrealCSharp —— 以 TCS 的 USTRUCT 依赖为硬约束

> 日期：2026-09-23
> 调研对象：UnrealSharp（`UnrealSharp/UnrealSharp`，上游 main @ 46e9c2c，2026-09-22）、UnrealCSharp（`crazytuzi/UnrealCSharp`，main @ 08c6a76，2026-09-14）
> 约束来源：TCS（TireflyCombatSystem）对 USTRUCT 的实际依赖
> 环境：LAC 使用 UE 5.8（`E:/UnrealEngine/UE_5.8`，uproject `EngineAssociation={9316D468-4494-7A4B-1D70-9492A88F1116}`）
> 方法：两库上游源码浅克隆 + UE 5.8 引擎源码 + TCS 本仓源码交叉考古；无运行时实证（待验证项见 §11）

---

## 1. 结论摘要

**用户提出的硬性要求（"脚本语言要允许 USTRUCT 继承"）在字面上无法由任何一个候选满足——但这不是选型问题，是要求本身需要被重构。**

三条承重结论：

1. **C# 语言层不允许 `struct` 继承**（CLR 规则，与 UE 无关）。这决定了任何 C# 方案的起点：要么放弃"结构体"这个载体，要么用 `class` 伪装。
2. **两个库走了完全相反的路，且都不通到 TCS 需要的地方**：
   - UnrealSharp：`[UStruct]` 只能标在 C# `struct` 上 → 生成 `record struct` → 运行时落地为 `UUserDefinedStruct`（蓝图结构体）→ **引擎层面零继承**，且导出器把继承链**展平**。
   - UnrealCSharp：`[UStruct]` 标在 C# `class` 上 → 生成可继承的 C# `class` → 运行时落地为 `UDynamicScriptStruct : UScriptStruct` 并**真调 `SetSuperStruct`** → **引擎层面有真继承**，但**全仓零 `FInstancedStruct` 支持**，且没有 C++ vtable。
3. **TCS 真正依赖的不是"继承"，而是"结构体基类 + C++ 虚函数分派"**（`TInstancedStruct<FTcsParamValueSource>` 持有 + `Source.Get().Evaluate(Ctx)` 虚调用）。虚分派要求实例内存里有 **C++ vtable 指针**，而两个库都不为 C# 类型生成任何 C++ 代码 —— **这一层两库都跨不过去**。

**并且**：TCS 设计文档在 2026-09-10 已明确记录并接受了"新源 USTRUCT 类型 = C++ only"这一代价（`2026-09-10-param-value-source-decision-points.md:20` 的"C# 扩展地图"第 ④ 条）。

### 针对用户实际需求的修正结论（2026-09-23 追加，见 §7）

用户在调研过程中澄清目标为**在 LAC 侧写游戏逻辑**，且明确包含**英雄技能逻辑、羁绊技能效果逻辑、Buff 效果逻辑**。据此深入 TCS 效果链源码后，结论需要修正：

**这三类逻辑的核心载体（链数据 / 触发行 / 步骤 struct）都不需要 USTRUCT 继承**——步骤类型**有意无公共基类**（D4-16），全是纯数据 struct。用户提出的"硬性要求"对应不上真实需求。

真正的卡点在别处，且**全部在 TCS 侧**（§7.6）：

| # | 缺口 | 性质 |
|---|---|---|
| G-1 | 执行器签名 `TFunction` + 非反射的 `FTcsEffectContext`/`FTcsChainRun`/`FTcsDamageFlowContext` | 反射面 |
| G-2 | `Register` 无 `UFUNCTION` 包装（D4-17 承诺的"反射面"未落地） | 反射面 |
| G-3 | **效果链 15 原语今天只落了 3 个执行器**（缺 WaitEvent/Branch/ApplyState/ModifyAttribute/Repeat/Parallel/RunSubChain…） | **功能缺口** |
| G-4 | 参数源/选择器/过滤器走 C++ 虚分派 | 架构（可选，不影响技能逻辑） |

**就"写技能/Buff 逻辑"而言，两个库目前都不足以支撑**——但原因不是库的缺陷，而是 TCS 的执行器通道尚不可反射、且原语步骤库未补齐。补完 G-1/G-2/G-3 后，**UnrealSharp 能支撑**（`FInstancedStruct` 开箱 + 可定义纯数据步骤 struct + 可实现 UInterface）；**UnrealCSharp 仍不行**（零 `FInstancedStruct` 支持）。

**若最终仍需选一个脚本方案**（为关卡/流程/UI/存档等 LAC 游戏逻辑），**推荐 UnrealSharp**：社区规模 2.4×、`FInstancedStruct` 开箱、无逐次访问堆分配、UE 5.6–5.8 明确支持、用户已有 fork。

---

## 2. 前提校正：用户推理链的断点

用户原话（转述）："硬性要求是脚本语言允许 USTRUCT 的继承，因为 TCS 非常依赖 USTRUCT。"

这条推理链有两处需要分开：

### 2.1 "依赖 USTRUCT" ≠ "依赖 USTRUCT 继承"

TCS 确实重度使用 USTRUCT，但用法分两类，脚本友好度截然不同：

| 用法 | 涉及面 | 脚本友好度 |
|---|---|---|
| **数据载体**（载荷、配置、句柄、上下文） | 全系统（`FInstancedStruct` 事件载荷、`FTcsDamageRecord`、`FTcsCombatEntityHandle`…） | ✅ 只要反射可见，C# 可读写 |
| **多态策略**（基类 + 虚函数覆写） | 参数源、目标选择器、目标过滤器 | ❌ 见 §5，C# 跨不过去 |

真正卡住脚本的是**第二类**，而第二类在 TCS 里的数量是**有限且可枚举的**（§4.1 给出清单）。

### 2.2 TCS 设计已就此事拍过板

`Documents/combat-system-design/2026-09-10-param-value-source-decision-points.md:20` 记录了 2026-09-11 的拍板：

> **C# 扩展地图**：①读上下文——反射 USTRUCT 字段 ✓；②自定义参数表——宿主实现 `ITcsParamTableReader` ✓（UnrealSharp 可实现 UInterface）；③新参数语义——PV-5 delegate/Formula 源（反射可见委托，C# 可绑定）；④**新源 USTRUCT 类型——C++ only（D3-7 v3 已接受代价，与 BP 同）**。

同日在记忆层落卡（`MEM-20260911-01`）固化为"C# 四问"：

> ①C# 读得到吗（反射字段）；②C# 实现得了吗（UInterface 可实现）；③C# 绑定得了吗（委托/动态多播）；④类型能新增吗（**源 USTRUCT 类型 C++ only 是已接受代价**，用③的 delegate 通道补偿）。

**即：用户当时已经知道并接受了"C# 不能新增源 USTRUCT 类型"。** 现在提出的"必须支持 USTRUCT 继承"实际上是在**推翻 ④ 这条已接受的代价**。

这不是错误，但必须显式裁决 —— 因为 ④ 被推翻会连带影响 §6 的通道设计（若 C# 能定义源类型，就不必再建 delegate 补偿通道）。

---

## 3. TCS 对 USTRUCT 的实际依赖面（本仓源码实证）

### 3.1 多态基类清单（C++ 虚分派）

全仓扫描"被其他 struct 继承的 USTRUCT 基类"，TCS 自有 4 个（另加 1 个引擎基类）：

| 基类 | 定义位置 | 虚函数 | 派生类 |
|---|---|---|---|
| `FTcsParamValueSource` | `TcsCore/Public/Parameter/TcsParamValueSource.h` | `Evaluate` / `AllowsValueConvention` | `FTcsParamSource_Literal`、`FTcsParamSource_ParamRef`、`FTcsParamSource_AttributeScaled` |
| `FTcsParamEvaluateContext` | 同上 | `GetScriptStruct` | `FTcsAttributeEvaluateContext` |
| `FTcsTargetSelectorStrategy` | `TcsTargeting/Public/Targeting/TcsTargetSelectorStrategy.h` | `Resolve` | `FTcsSelSelf` |
| `FTcsTargetFilterStrategy` | `TcsTargeting/Public/Targeting/TcsTargetFilterStrategy.h` | `Pass` | （待补） |
| `FTableRowBase` | 引擎（`Engine/DataTable.h`） | — | 表格行（纯数据，无虚分派，脚本无关） |

基类声明形状（`TcsParamValueSource.h`）：

```cpp
USTRUCT(meta = (Hidden))
struct TCSCORE_API FTcsParamValueSource
{
    GENERATED_BODY()
    virtual double Evaluate(const FTcsParamEvaluateContext& Context) const
    {
        return 0.0;   // 默认实现（UHT 要求可默认构造，故非纯虚）
    }
    virtual bool AllowsValueConvention() const { return true; }
};
```

持有与调用（`TcsCore/Public/Parameter/TcsParamValue.h`）：

```cpp
// 第 37 行
TInstancedStruct<FTcsParamValueSource> Source;

// 第 48 行 —— 虚调用点
return Source.IsValid() ? Source.Get().Evaluate(Ctx) : 0.0;
```

### 3.2 关键：`TInstancedStruct::Get()` 是裸指针重解释

`TInstancedStruct<T>::Get()` → `UE::StructUtils::GetStructPtr<T>(ScriptStruct, StructMemory)` → 把 `StructMemory`（`uint8*`）**直接 reinterpret_cast 成 `T*`**，然后调用虚函数。

这意味着：**实例内存的第一个机器字必须是 C++ vtable 指针**，否则 `Evaluate()` 就是一次野函数指针调用。这条要求由 `TCppStructOps<FTcsParamValueSource>` 在构造时写入 —— 只有 C++ 类型才有。

### 3.3 TCS 的扩展通道现状盘点

| 通道 | 载体 | 反射面 | C# 可用性 |
|---|---|---|---|
| 参数表访问 | `ITcsParamTableReader`（UINTERFACE，`TcsParamTableReader.h:38` `UFUNCTION(BlueprintNativeEvent)`） | ✅ | ✅ 可实现 |
| 战斗事件接收 | `UTcsEventHandler`（`TcsEventHandler.h:36` `UFUNCTION(BlueprintNativeEvent)`） | ✅ | ✅ 可继承 |
| 属性提供 | `UTcsAttributeProvider`（3 个 `BlueprintNativeEvent`） | ✅ | ✅ 可继承 |
| 伤害流程委托 | `ITcsDamageFlowDelegate`（UINTERFACE，全虚函数带中性默认实现） | ✅ | ✅ 可实现 |
| **步骤执行器注册** | `FTcsFlowStepExecutorRegistry::Register(const UScriptStruct*, TFunction<...>)` | ❌ **无 UFUNCTION 包装** | ❌ **不可达** |
| **参数源类型新增** | `TInstancedStruct<FTcsParamValueSource>` 的具体类型 | ❌ C++ only | ❌ 不可达 |

全仓 `UFUNCTION` 仅 5 处（`TcsAttributeProvider.h` ×3、`TcsEventHandler.h` ×1、`TcsAsyncAction_ListenForCombatEvent.h` ×2），**没有任何一处通向执行器注册表**。

> 这与既有认知一致：TCS R3 的"空转清单"指出执行器注册的"反射可达动态委托入口"是设计意图但**尚未落地**。

---

## 4. 两库的 USTRUCT 能力（上游源码实证）

### 4.1 C# 语言层约束（两库共同起点）

C# 的 `struct` 是**值类型，语言层面不支持继承**（只能实现接口）。因此"让 C# 定义可继承的结构体"只有两条路：

- **A 路**：坚持用 `struct` → 继承不可能，只能把父类字段**展平**进子类型。
- **B 路**：改用 `class`（引用类型，可继承）→ 得到 C# 侧继承，但对象语义/内存布局与 UE `UScriptStruct` 不再同构。

两个库恰好各选了一路。

### 4.2 UnrealSharp —— A 路（展平，无继承）

**属性只允许标在 C# `struct` 上：**

```csharp
// Managed/UnrealSharp/UnrealSharp.Core/Attributes/UStructAttribute.cs:9
[AttributeUsage(AttributeTargets.Struct)]
public sealed class UStructAttribute : Attribute;
```

Analyzer 硬校验（`UnrealSharp.Analyzers/UnrealTypeAnalyzer.cs:68`）：

```csharp
if (symbol.TypeKind == TypeKind.Struct && AnalyzerStatics.HasAttribute(symbol, AnalyzerStatics.UStructAttribute))
```

→ **C# `class` 上写 `[UStruct]` 直接报错**，A 路被锁死。

**导出器把继承链展平**（`Source/UnrealSharpManagedGlue/Exporters/StructExporter.cs:14-33`）：

```csharp
List<UhtStruct> inheritanceHierarchy = new();
UhtStruct? currentStruct = structObj;
while (currentStruct is not null)
{
    inheritanceHierarchy.Add(currentStruct);
    currentStruct = currentStruct.SuperStruct;   // 向上走链
}
inheritanceHierarchy.Reverse();
foreach (UhtStruct inheritance in inheritanceHierarchy)
{
    inheritance.GetExportedProperties(exportedProperties, getSetBackedProperties);  // 全部并成一个平表
}
// 之后还有同名属性去重逻辑
```

注意：**遍历 `SuperStruct` 是为了"取全部字段"，不是为了"建立继承关系"**。

**类型声明不带基类**（`StructExporter.cs:89`）：

```csharp
stringBuilder.DeclareType(structObj, "record struct", structName,
                          csInterfaces: csInterfaces, modifiers: isReadOnly ? " readonly" : null);
//                           ↑ 无 baseType 实参
```

对比 `ClassExporter.cs:55`（类**有**基类）：

```csharp
stringBuilder.DeclareType(classObj, "class", classObj.GetStructName(), superClassName, ...);
//                                                                     ↑ 传了基类名
```

**运行时落地为蓝图结构体**（`Source/UnrealSharpCore/Public/Types/CSScriptStruct.h`）：

```cpp
UCLASS()
class UCSScriptStruct : public UUserDefinedStruct, public ICSManagedTypeInterface
```

而引擎对蓝图结构体的继承是**明令不支持**的（UE 5.8 `Editor/StructUtilsEditor/Private/SInstancedStructPicker.cpp:132`，原文注释）：

```cpp
bool FInstancedStructFilter::IsUnloadedStructAllowed(...)
{
    // User Defined Structs don't support inheritance, so only include them requested
    return bAllowUserDefinedStructs;
}
```

同文件 `:299`：

```cpp
StructFilter->bAllowUserDefinedStructs = BaseScriptStruct == nullptr; // Only allow user defined structs when BaseStruct is not set.
```

→ **一旦属性上声明了 `BaseStruct`，蓝图结构体（= UnrealSharp 的 C# 结构体）就被 picker 直接排除。**

**托管侧反射数据无基类字段**（`Source/UnrealSharpCore/Public/ReflectionData/CSStructReflectionData.h`）：

```cpp
struct FCSStructReflectionData : FCSTypeReferenceReflectionData
{
    TArray<FCSPropertyReflectionData> Properties;   // 只有字段，没有 SuperStruct
};
```

对比 `FCSClassReflectionData` 走的是 `CSManagedClassCompiler` 的 `TryRedirectSuperClass` + `SetSuperStruct` 路径。**结构体编译器完全没有对应逻辑**。

**且结构体编译前会清空基类**（`Source/UnrealSharpUtilities/Private/UnrealSharpUtils.cpp:74`）：

```cpp
void FCSUnrealSharpUtils::PurgeStruct(UStruct* Struct)
{
    ...
    Struct->SetSuperStruct(nullptr);   // 抹掉
    Struct->DestroyChildPropertiesAndResetPropertyLinks();
    ...
}
```

`CSManagedStructCompiler::Compile` 第一步就是 `PurgeStruct(Struct)`。

**结构体不导出任何函数**（`StructExporter.cs:114/117/129/132` 全部传空表）：

```csharp
StaticConstructorUtilities.ExportStaticConstructor(stringBuilder, structObj,
    new List<UhtFunction>(),      // ← 空
    new Dictionary<string, GetterSetterPair>(),
    new Dictionary<string, GetterSetterPair>(),
    new List<UhtFunction>());     // ← 空
```

对比 `ClassExporter.cs` 中 `UhtFunction` 出现 13 次。→ **C# 结构体上写的方法不会被导出，更不可能覆写 C++ 虚函数。**

**`FInstancedStruct` 支持完善**（这是 UnrealSharp 的强项）：

- 原生绑定：`Source/UnrealSharpCore/Private/Binds/Bind_FInstancedStruct.cpp`（`GetNativeStruct` / `InitializeAs` / `GetMemory` / `NativeCopy` / `NativeDestroy`）
- 托管封装：`Managed/UnrealSharp/UnrealSharp/Extensions/CoreUObject/InstancedStruct.cs`（`Make<T>()` / `Make<T>(T)` / `IsA<T>()` / `TryGet<T>()` / `Get<T>()`）

**但约束与缺陷**：

```csharp
// InstancedStruct.cs:101
public static FInstancedStruct Make<T>() where T : struct, MarshalledStruct<T>
//                                            ↑ 再次锁死在 C# 值类型

// InstancedStruct.cs（IsA 实现）
private bool IsA(IntPtr scriptStruct)
{
    IntPtr nativeStruct = Bind_FInstancedStruct.CallGetNativeStruct(ref _manager.StructData);
    return nativeStruct == scriptStruct;   // ← 精确相等，不是 IsChildOf
}
```

→ `IsA<T>()` 做的是**精确类型匹配**，不是子类型判定。这从侧面再次确认：UnrealSharp 的 C# 结构体世界里没有"子类型"概念。

### 4.3 UnrealCSharp —— B 路（真继承，但缺 FInstancedStruct）

**属性只允许标在 C# `class` 上：**

```csharp
// Script/UE/Dynamic/Struct/UStructAttribute.cs:7
[AttributeUsage(AttributeTargets.Class)]
public class UStructAttribute : OverrideAttribute { }
```

**生成可继承的 C# 类**（`Source/ScriptCodeGenerator/Private/FStructGenerator.cpp`）：

```cpp
// :110 —— 保留基类
SuperStructContent = FString::Printf(TEXT(" : %s, IStaticStruct"),
                                     *FUnrealCSharpFunctionLibrary::GetFullClass(SuperStruct));

// :315 —— 类型声明模板
"\tpublic partial class %s%s\n"     // ← class，不是 struct
```

且生成器明确从引擎侧读基类（`:100`）：

```cpp
auto SuperStruct = Cast<UScriptStruct>(InScriptStruct->GetSuperStruct());
if (SuperStruct != nullptr) { /* 生成 : FBase, IStaticStruct */ }
else                        { /* 生成 : IStaticStruct，含 Register/UnRegister */ }
```

**运行时落地为真 `UScriptStruct` 并真设基类**（`Source/UnrealCSharpCore/Public/Dynamic/DynamicScriptStruct.h:17`）：

```cpp
UCLASS()
class UNREALCSHARPCORE_API UDynamicScriptStruct : public UScriptStruct
{
    GENERATED_BODY()
public:
    FGuid Guid;
    virtual FGuid GetCustomGuid() const override;
};
```

基类设置（`Source/UnrealCSharpCore/Private/Dynamic/FDynamicStructGenerator.cpp`）：

```cpp
void FDynamicStructGenerator::BeginGenerator(UDynamicScriptStruct* InScriptStruct,
                                             UScriptStruct* InParentScriptStruct)
{
    if (InParentScriptStruct != nullptr)
    {
        InScriptStruct->SetSuperStruct(InParentScriptStruct);   // ← 真继承
    }
}

void FDynamicStructGenerator::EndGenerator(UDynamicScriptStruct* InScriptStruct)
{
    InScriptStruct->Bind();
    InScriptStruct->StaticLink(true);
    ...
    InScriptStruct->StructFlags = STRUCT_Native;
}
```

父类型解析（同文件 `Generator`）：`LoadObject<UScriptStruct>(nullptr, *ParentPathName)` —— 支持继承**引擎侧原生 USTRUCT**。

**→ 这是两库唯一在引擎层面建立了真 USTRUCT 继承链的实现。**

**但三个硬伤：**

1. **全仓零 `FInstancedStruct`**（`grep -rin "instancedstruct"` 覆盖 `Source/`、`Script/`，零命中）。`Script/UE/CoreUObject/` 下也无 `FInstancedStruct.cs`、无 `FTableRowBase.cs`。而 TCS 的**事件载荷、链步骤、条件数组、参数源**全部由 `FInstancedStruct` 承载 —— 这是**最致命的一条**。
2. **不导出结构体方法**：`FStructGenerator.cpp` 只生成属性 getter/setter，无方法生成逻辑；对比 `FClassGenerator.cpp` 有 244 处方法相关代码。
3. **不生成任何 C++ 代码**：`ScriptCodeGenerator` 只写 `.cs` 文件（`FStructGenerator.cpp:350` `SaveStringToFile`）。→ **没有 C++ vtable 的可能**（§5）。

### 4.4 对比总表

| 维度 | UnrealSharp | UnrealCSharp |
|---|---|---|
| `[UStruct]` 目标 | C# `struct`（Analyzer 硬校验） | C# `class` |
| C# 侧类型 | `record struct`（值类型，**不可继承**） | `partial class`（引用类型，**可继承**） |
| 引擎侧运行时类型 | `UCSScriptStruct : UUserDefinedStruct` | `UDynamicScriptStruct : UScriptStruct` |
| **引擎侧 USTRUCT 继承** | ❌ 展平 + `SetSuperStruct(nullptr)` | ✅ `SetSuperStruct(Parent)` |
| 继承链可否来自 C++ 原生 USTRUCT | ❌ | ✅ `LoadObject<UScriptStruct>` |
| **`FInstancedStruct` 支持** | ✅ 完整（Binds + C# 扩展） | ❌ **零** |
| 结构体成员函数导出 | ❌（传空 `UhtFunction` 表） | ❌（无方法生成逻辑） |
| 参与 C++ 虚分派 | ❌ | ❌（无 C++ 代码生成） |
| 可 `new` 语义 | 值类型，无 GC 压力 | `class` + 句柄，issue #532 报告 GC 分配开销 |
| UE 版本支持（README） | 5.6 – 5.8 | 5.0 – 5.8 |
| LAC 引擎 5.8 兼容 | ✅ | ✅ |
| Stars / Forks | 1930 / 214 | 794 / 120 |
| Open issues | 20 | 6 |
| 最后推送 | 2026-09-22 | 2026-09-22 |
| License | MIT | MIT |
| 创建时间 | 2023-11-01 | 2022-10-05 |

---

## 5. 决定性障碍：C++ 虚分派不可跨越

这是本次调研最重要的技术结论。

### 5.1 引擎侧机制

`FTcsParamValueSource` 带 C++ 虚函数 → 它是**多态类型** → 其 C++ 布局第一个机器字是 **vtable 指针**。

这个 vtable 由谁写？由 `TCppStructOps<FTcsParamValueSource>`（UHT 为每个 `USTRUCT` 生成）：

```cpp
// Engine/Source/Runtime/CoreUObject/Public/UObject/Class.h:2265
template <typename CPPSTRUCT>
struct TAutoCppStructOps
{
    TAutoCppStructOps(FTopLevelAssetPath InName)
    {
        DeferCppStructOps(Name, new TCppStructOps<CPPSTRUCT>);
    }
};
```

引擎在 `UScriptStruct::PrepareCppStructOps()` 里从**延迟注册表**取回（`Class.cpp:3120-3144`）：

```cpp
void UScriptStruct::PrepareCppStructOps()
{
    if (!CppStructOps)
    {
        CppStructOps = GetDeferredCppStructOps().FindRef(GetFlattenedStructPathName());
        if (!CppStructOps)
        {
            if (StructFlags & STRUCT_Native)
            {
                UE_LOGF(LogClass, Fatal, "Couldn't bind to native struct %ls. ...");
            }
            bPrepareCppStructOpsCompleted = true;
            return;      // ← 非原生结构体：CppStructOps = nullptr
        }
        ...
    }
}
```

而结构体尺寸来源（`Class.cpp:944-949`）：

```cpp
if (UScriptStruct::ICppStructOps* CppStructOps = ScriptStruct.GetCppStructOps())
{
    MinAlignment   = IntCastChecked<int16>(CppStructOps->GetAlignment());
    PropertiesSize = CppStructOps->GetSize();      // ← 来自 C++ sizeof
    bHandledWithCppStructOps = true;
}
```

### 5.2 为什么 C# 跨不过去

1. **C# 定义的 USTRUCT 没有 C++ 类型** → UHT 从未为它生成 `TCppStructOps` → `DeferCppStructOps` 表里没有它 → `GetCppStructOps()` 返回 `nullptr`。
2. **没有 CppStructOps 就没有构造函数** → 实例内存由引擎 `FMemory::Memzero` 起步（`UScriptStruct::InitializeStruct` 开头即 `Memzero`），**vtable 指针位为 0**。
3. **调用 `Source.Get().Evaluate(Ctx)`** → `GetStructPtr<T>` 把内存 reinterpret 成 `FTcsParamValueSource*` → 读偏移 0 的"vtable 指针"（= 0 或垃圾）→ **野调用**。
4. 两个库都**不为 C# 类型生成 C++ 代码**（UnrealCSharp 只写 `.cs`；UnrealSharp 只写 `.cs` glue），所以**不存在"给 C# 类型补一个 C++ shim 结构体"的路径**。
5. 两个库都**不导出结构体成员函数**，所以连"用反射函数替代虚函数"的退路也没有。

### 5.3 证据强度声明

- **有证据**：两库的类型落地方式、继承处理、函数导出、C++ 代码生成 —— 全部为源码直读，见 §4。
- **有证据**：虚分派依赖 vtable、vtable 来自 `TCppStructOps`、非原生结构体 `CppStructOps == nullptr` —— 引擎源码直读，见 §5.1。
- **当前假设（强推断，未实测）**：C# 定义的结构体作为 `TInstancedStruct<FTcsParamValueSource>` 具体类型被虚调用时会崩溃。推断链完整（§5.2），但**未做运行时实证**。验证方式见 §11。

---

## 6. 一个被忽略的关键事实：TCS 内部已存在"脚本友好"的分派模式

对比 TCS 的两套机制：

**步骤系统（`TcsEffect/Public/Chain/TcsEffectStep.h`）—— 反射键注册表分派，脚本友好：**

> **无公共基类（D4-16 有意）**：步骤类型之间不存在继承关系，故链的步骤数组不设 `BaseStruct` 编辑器限定——编辑器 picker 可挂任意 struct，**类型合法性由执行器注册表在执行期判定**。

```cpp
// TcsDamage/Public/Flow/TcsFlowStepExecutor.h
using FTcsFlowStepExecute = TFunction<bool(const FInstancedStruct& StepData, FTcsDamageFlowContext& Context)>;

struct FTcsFlowStepExecutorEntry
{
    UScriptStruct* (*GetStepStruct)() = nullptr;
    FTcsFlowStepExecute Executor;
};

class FTcsFlowStepExecutorRegistry
{
    void AddPending(FTcsFlowStepExecutorEntry Entry);                              // 静态自注册
    void Register(const UScriptStruct* StepStruct, FTcsFlowStepExecute Executor);  // 动态注册
    const FTcsFlowStepExecute* Find(const UScriptStruct* StepStruct);
};
```

**参数源系统（`TcsCore/Public/Parameter/TcsParamValueSource.h`）—— 虚分派，脚本敌意：**

```cpp
USTRUCT(meta = (Hidden))
struct FTcsParamValueSource
{
    virtual double Evaluate(const FTcsParamEvaluateContext& Context) const { return 0.0; }
    virtual bool AllowsValueConvention() const { return true; }
};
```

**结论：TCS 内部对同一类问题（"按类型分派到不同实现"）用了两种截然不同的机制。**

- 步骤系统已经用注册表分派 → **只要补一个反射可达的注册入口，C# 就能注册执行器**（`TFunction` 本身不可反射，需要包一层 `UFUNCTION` + 动态委托，或把签名换成反射可见的委托类型）。
- 参数源/选择器/过滤器用虚分派 → **无论换哪个脚本方案都不可达**。

**这给出了一个明确的、比"换脚本语言"更直接的路径**：把参数源、选择器、过滤器也迁移到注册表分派（与步骤系统同构），即可让 C# 通过注册表参与 —— 不需要 USTRUCT 继承，也不需要 C++ vtable。

> 注：这是一项**架构变更**（触及 D3-7 v3 "USTRUCT 反射基类 + C++ 虚函数分派"的核心裁决），需要独立提案与用户拍板，不在本次调研范围内。此处仅指出可行性。

---

## 7. 针对用户实际需求的落点分析（2026-09-23 追加）

用户在本次调研中澄清：引入脚本的目标是**在 LAC 侧写游戏逻辑**，且明确包含——

> 关卡、流程、UI、存档，**以及 LAC 中具体英雄的技能逻辑、羁绊技能效果逻辑、各种 Buff 的效果逻辑**。

后三项直接命中 TCS 的效果链系统。本节重新审视：这三项逻辑在 TCS 里究竟住在哪一层，以及**"USTRUCT 继承"到底是不是它们的必要条件**。

### 7.1 技能 / 羁绊 / Buff 逻辑的三个落点

TCS 的设计哲学（`05-module-skill.md` 开篇）：

> **技能是原语链的编排者，不是执行者（执行在 M4）。**

据此，"技能逻辑"被拆成两层，落在不同地方：

| 落点 | 载体 | 是什么 | 反射面 |
|---|---|---|---|
| ① **编排**（这个技能 = 选目标 → 造成伤害 → 等 0.5s → 再造成伤害） | `FTcsEffectChain.Steps: TArray<FInstancedStruct>` | **数据** | ✅ 反射可见 |
| ② **挂载**（Buff 在什么条件下触发哪些效果） | `FTcsTriggerRow`（10 字段，含 `Conditions` / `Effects`） | **数据** | ✅ 反射可见 |
| ③ **执行**（"造成伤害"具体怎么算、"冲刺到目标"怎么位移） | `FTcsStepExecute`（步骤执行器） | **C++ 函数** | ❌ 不可反射 |

### 7.2 关键发现 A：步骤类型**没有公共基类**——新增步骤不需要继承

`TcsEffect/Public/Chain/TcsEffectStep.h` 的设计注释原文：

> **无公共基类（D4-16 有意）**：步骤类型之间不存在继承关系，故链的步骤数组不设 `BaseStruct` 编辑器限定——编辑器 picker 可挂任意 struct，**类型合法性由执行器注册表在执行期判定**。

实测：全部步骤 struct 均为**纯数据、零虚函数**（`FTcsFlowCollectStart`、`FTcsStepDamage`、`FTcsStepWaitDelay`、`FTcsStepSelectTargets` … 逐一核对，无一含 `virtual`）。

**→ 用户提出的"必须支持 USTRUCT 继承"这条硬性要求，对技能/Buff 逻辑的核心载体（步骤类型）并不适用。** 新增一个步骤类型只需要一个**纯数据 USTRUCT**，而 UnrealSharp 的 `[UStruct] record struct` 正好能表达纯数据 USTRUCT（`Make<T>()` 的 `where T : struct` 约束也满足，见 §4.4 / V-3）。

**→ 更进一步**：`FTcsEffectChain.Steps` 是 `TArray<FInstancedStruct>` 且**无 `BaseStruct` 限定**，引擎 picker 在无 `BaseStruct` 时**允许用户自定义结构体**（§4.2 引用的 `SInstancedStructPicker.cpp:299`）。这意味着 **C# 定义的步骤 struct 可以直接被挂进链的 Steps 数组**。

### 7.3 关键发现 B：真正的卡点是执行器通道，不是继承

`TcsEffect/Public/Chain/TcsEffectStepExecutor.h:18`：

```cpp
using FTcsStepExecute = TFunction<ETcsStepResult(
    const FInstancedStruct& StepData,
    FTcsEffectContext& Context,      // ← 非反射纯 C++ struct
    FTcsChainRun& Run)>;             // ← 非反射纯 C++ struct
```

三重不可反射叠加：

1. **`TFunction`** —— 无反射面，C# 无法构造/传递（与 `MEM-20260911-01` 记录的"不可反射成员把扩展期焊死"同源）。
2. **`FTcsEffectContext`** —— 声明为"效果链黑板……**纯运行态结构，非反射数据**"（`TcsEffectContext.h` 原文）。
3. **`FTcsChainRun`** —— 同样非反射（含 `TTcsInstanceHandle` 池句柄、`TWeakObjectPtr` 等）。

对照 `TcsDamage/Public/Flow/TcsFlowStepExecutor.h:16`，伤害流程侧同款问题：

```cpp
using FTcsFlowStepExecute = TFunction<bool(
    const FInstancedStruct& StepData,
    FTcsDamageFlowContext& Context)>;   // ← "纯运行态结构（非反射）"（该头文件原文）
```

**→ C# 能写"链数据"，但写不了"步骤逻辑"。** 步骤在运行期会被判为"未注册类型"→ 按设计**断链 + Error 日志**（`FTcsEffectStepExecutorRegistry::Find` 注释："未登记返回 nullptr，不 ensure——'未知步骤类型'由解释器按断链处置"）。

### 7.4 关键发现 C：设计意图与实现的不一致（D4-17 双入口未落地）

`TcsEffectStepExecutor.h` 对 `Register` 的注释宣称：

> **动态注册入口（D4-17 双入口之反射面：脚本层 / 测试装置 / 运行期补登）。**

但实测：

```cpp
void Register(const UScriptStruct* StepStruct, FTcsStepExecute Executor);
//   ↑ 无 UFUNCTION 包装，且 Executor 是 TFunction
```

- 全仓 `UFUNCTION` **仅 7 处**，无一处通向执行器注册表（§3.3）。
- `Register` 的签名本身不可反射。

**→ 注释里的"反射面"目前是设计意图，尚未成为事实。** 这与既有认知（TCS R3"空转清单"：执行器的反射可达动态委托入口未落地）一致。

### 7.5 关键发现 D：效果链的 15 原语今天只落了 3 个执行器

实测注册点（`grep UE_DEFINE_EFFECT_STEP_EXECUTOR`）：

| 系统 | 注册数 | 明细 |
|---|---|---|
| **效果链**（`FTcsEffectChain`，技能/Buff 用） | **3** | `FTcsStepDamage`、`FTcsStepWaitDelay`、`FTcsStepSelectTargets` |
| 伤害流程（`FTcsDamageFlowContext`，独立系统） | 12 | 标准十阶段 + `FlowModify` / `FlowDelegate` |

对照设计（`04-module-effects.md` §2.1，D4-3 终版 **15 原语**）：控制流 6（WaitDelay/**WaitEvent**/Branch/Parallel/Repeat/RunSubChain）+ ModifyAttribute + SetVar + OnError + 领域 6（Damage/Heal/ModifyFlow/SelectTargets/ApplyState/PlayCue）。

**→ 效果链侧尚有 12 个原语未落地执行器**，其中包括 Buff/羁绊逻辑最需要的 **WaitEvent（事件等待）**、**Branch（分支）**、**ApplyState（挂状态）**、**Repeat / Parallel / RunSubChain（编排）**、**ModifyAttribute（改属性）**。

**→ 这意味着：即便脚本通道今天就打通，用 C# 也写不出"羁绊技能效果"和"Buff 效果"的完整逻辑——因为需要的原语步骤本身还没实现。** 这是比脚本选型更前置的缺口。

### 7.6 本节结论

对"用 C# 写技能/羁绊/Buff 逻辑"这个目标，**卡点全部在 TCS 侧，且与 USTRUCT 继承无关**：

| # | 缺口 | 性质 | 补法 |
|---|---|---|---|
| G-1 | 执行器签名 `TFunction` + 两个非反射 struct | 反射面 | 把上下文/运行态反射化（或提供反射镜像），执行器签名换成反射可见委托 |
| G-2 | `Register` 无 `UFUNCTION` 包装 | 反射面 | 补一个 `BlueprintCallable` 入口（D4-17 已承诺的"反射面"） |
| G-3 | 效果链 12 个原语执行器未实现 | **功能缺口** | 按 04 文档补步骤库（与脚本无关，C++ 侧工作） |
| G-4 | 参数源/选择器/过滤器走 C++ 虚分派 | 架构 | 见 §6——迁到注册表分派（可选，且仅影响"新参数源类型"，不影响技能逻辑） |

**而用户提出的"USTRUCT 继承"，对应的是 G-4 那一小部分（4 个策略基类），既不是技能逻辑的载体（G-3 才是），也不是脚本能跨过的层（§5）。**

> 顺带回答"两个 CS 仓库有没有一个能支持"：**就"写技能/Buff 逻辑"而言，两个库目前都不足以支撑**——不是库的缺陷，而是 TCS 的执行器通道尚不可反射、且原语步骤库未补齐。补完 G-1/G-2/G-3 后，**UnrealSharp 能支撑**（`FInstancedStruct` 开箱 + 可定义纯数据步骤 struct + 可实现 UInterface）；UnrealCSharp 仍因零 `FInstancedStruct` 支持而不行。

---

## 8. 选型建议

### 8.1 先裁决前置问题

用户已澄清（2026-09-23）：目标是**在 LAC 侧写游戏逻辑**，含技能/羁绊/Buff 效果逻辑。

据此，§7.6 给出修正后的判断：**卡点全部在 TCS 侧**。推荐的推进顺序是**先补 TCS 的执行器反射通道（G-1/G-2）与缺失原语（G-3），再谈脚本接入**——因为：

- 不补 G-1/G-2：脚本通道不可达，任何 C# 方案都写不了步骤逻辑。
- 不补 G-3：即便通道可达，也没有足够的原语去编排羁绊/Buff 效果。
- 这两项**与选哪个脚本语言无关**，是 TCS 自身的欠账。

### 8.2 若确需选型：推荐 UnrealSharp

理由（按权重）：

1. **`FInstancedStruct` 开箱** —— TCS 全系统用它承载载荷/步骤/条件，UnrealCSharp 零支持。这一条基本单独决定结论。
2. **社区规模与健康度** —— 1930 vs 794 stars，214 vs 120 forks；两者推送都活跃（同为 2026-09-22），但 UnrealSharp 的 issue 吞吐量与贡献者基数明显更大。
3. **无逐次访问堆分配** —— UnrealSharp 结构体是 C# 值类型；UnrealCSharp 生成 `class` + 句柄，其自身 issue #532 即报告 GC 分配开销。
4. **用户已有 fork** —— `LegendsTD/Plugins/UnrealSharp` 为 `Tirefly/UnrealSharp` fork（origin）+ 上游（upstream）双远程，已有工程经验沉淀。
5. **UE 5.8 明确支持**（README: 5.6–5.8）。

**风险与对策**：

| 风险 | 证据 | 对策 |
|---|---|---|
| Tirefly fork 落后上游 **135 个提交**（领先 9） | `git rev-list --count HEAD..upstream/main` = 135 | 接入前先评估 rebase/重克隆；记忆卡 `MEM-20260823-04` 指出"热重载报错的首要怀疑项 = 版本落后官方 main" |
| 热重载在 PIE/SIE 运行中禁用；bug 密集期在 2026-05 之后 | `MEM-20260823-04` | 升级到含修复的版本；`AutomaticHotReloading` 设为 OnEditorFocus |
| 每个非引擎 C++ 模块自动生成 `Script/<模块>.Glue/` 工程 | `MEM-20260823-04` | 用 `PublicDefinitions.Add("SkipGlueGeneration=1")` 逐模块关闭；保留主 Glue 的 ProjectReference 清单 |
| LAC 有 3 个 Tirefly 子模块（TCS/TIS/TBNS）+ TcsDev，Glue 工程会很多 | LAC `Plugins/Tirefly/` + `Source/TcsDev` | 同上，接入时统一规划 |
| 外部插件升级会覆盖 Build.cs 修改 | `MEM-20260823-04` | 记录清单，升级后重加 |

### 8.3 不推荐 UnrealCSharp 的核心理由

尽管它是**唯一在引擎层面实现了真 USTRUCT 继承**的库，但对 TCS 而言：

- 零 `FInstancedStruct` 支持 → TCS 的数据面完全用不了；
- 真继承也跨不过 C++ 虚分派（无 C++ 代码生成）→ 恰好是用户想要的那件事，它做不到；
- 结构体落地为 `class` + 句柄 → 与 UE `UScriptStruct` 的值语义不同构，逐次访问有分配开销。

**"它支持 USTRUCT 继承"这个表面优势，在 TCS 的真实需求面前不构成有效优势。**

---

## 9. Plan3 覆盖度核对（2026-09-23 用户质询后追加）

用户问："这些欠缺内容在 plan 3 落地后能否补全？"

**答案：不能。plan3（`2026-09-23-r4-plan3-trigger-row-and-modifier-channel.md`）对 G-1~G-4 四个缺口的覆盖是 0/4。**

### 9.1 逐条核对

| 缺口 | plan3 是否覆盖 | 证据 |
|---|---|---|
| **G-1** 执行器签名 `TFunction` + 非反射 struct | ❌ **零覆盖** | plan3 全文**零 `UFUNCTION` 新增**（`grep UFUNCTION` 空）；全文未讨论执行器签名的反射化；plan3 的 `FTcsStepModifyFlow` 是**又一个 C++ 执行器**，走的是同一套 `UE_DEFINE_EFFECT_STEP_EXECUTOR` 静态自注册宏 |
| **G-2** `Register` 无 `UFUNCTION` 包装 | ❌ **零覆盖** | plan3 新增的公共 API（`RegisterTriggerRow` / `UnregisterTriggerRow` / `UnregisterTriggerRowsBySource` / `SetTriggerGateTag` / `GetTriggerRowCount`）**全部无反射标记**——纯 C++ 面。plan3 的设计意图是"宿主 C++ 调用"，不是"脚本调用" |
| **G-3** 效果链 12 个原语执行器未实现 | ⚠️ **仅 1 个** | plan3 只补 **`ModifyFlow`**（且它属 TcsDamage 的**流程修改器通道**，不是效果链控制流原语）。剩余 11 个明确**不在本轮**——见 §9.2 |
| **G-4** 参数源/选择器/过滤器虚分派 | ❌ **零覆盖** | plan3 未触及 D3-7 v3 的策略载体 |

**结论：plan3 落地后，C# 依然写不了技能/羁绊/Buff 的步骤逻辑。** plan3 是"让事件能自己驱动链"（补上 TCS 的自动性），它的读者是**宿主 C++ 代码**，不是脚本层。

### 9.2 剩余 11 个原语的实际归属（据 plan3 轮次路线图）

plan3 的 §"轮次路线图（R4–R8）"明确了排期，与台账 R5-2 一致：

| 原语 | 归属轮次 | 对 Buff/羁绊的意义 |
|---|---|---|
| `ModifyFlow` | **R4（plan3，本轮）** | 伤害修改器提交侧 |
| `ApplyState` | **R5**（M3 状态层同批） | Buff 挂载/摘除的核心 |
| `WaitEvent` | **R6**（M5 技能层同批） | 事件等待（Buff 反应式触发） |
| `Branch` | **R6** | 条件分支 |
| `Parallel` | **R6** | 并行编排（多段效果） |
| `Repeat` | **R6** | 重复（周期性伤害/治疗） |
| `RunSubChain` | **R6** | 子链复用（羁绊组合效果） |
| `ModifyAttribute` | **R6** | 改属性（Buff 的核心效果） |
| `SetVar` | **R6** | 链内变量 |
| `OnError` | **R6** | 软失败接管 |
| `Heal` | **R5**（R5-3 余） | 治疗 |
| `PlayCue` | **R8**（TcsCue） | 表现 |

**即：Buff/羁绊逻辑最需要的原语（`ApplyState` / `ModifyAttribute` / `WaitEvent` / `Branch`）分别落在 R5 与 R6。** 按 plan3 的路线图，脚本层要能写出完整的 Buff/羁绊逻辑，**至少要等到 R6 之后**——而这还未计入 G-1/G-2 的反射化工作（当前不在任何轮次的交付清单里）。

### 9.3 一个值得注意的连带事实

plan3 的 R6 轮次描述里包含"**剩余 8 原语**"（对应台账 R5-2），但 R6 本身已被 plan3 标注为"**可能需拆两轮**"（§"说明"第 1 条：M5 与剩余原语都是重活）。这意味着 G-3 的实际完成时点可能比 R6 更晚。

**另**：plan3 的 Task 1 已经落地（触发行数据形状与条件求值器，规格库 23/23 全绿），Task 2~5 待执行。

### 9.4 对"脚本接入时机"的修正建议

综合 §7.6 与本节，建议的推进顺序应修正为：

1. **G-1/G-2（执行器通道反射化）**——**当前不在任何轮次**，需要**新立条目**（建议记入 `deferred-inputs-ledger.md`，与 R5-2 并列）。
2. **G-3 的 R5/R6 部分**——已在排期（R5 的 `ApplyState`、R6 的 8 原语），无需新立。
3. **脚本接入**——在 1、2 之后。若只为写关卡/流程/UI/存档，可立即并行推进（不依赖 TCS）。

> **建议**：把 G-1/G-2 作为独立条目补进 TCS 的 `deferred-inputs-ledger.md`（该台账的入册判据"决策已拍板、代码未落地、不在任何计划 Task 里"三条全中——D4-17 明文承诺了"反射面"，但既未落地也无轮次认领）。

---

## 10. 建议的下一步（按优先级）

**用户已澄清目标为"LAC 侧写游戏逻辑，含技能/羁绊/Buff 逻辑"（2026-09-23）。据此，§7.6 的四个缺口 G-1~G-4 就是下一步的实质内容（plan3 的覆盖度核对见 §9）。**

1. **补 G-1/G-2：TCS 执行器通道反射化**（这是脚本接入的**前置条件**，与选型无关）
   - G-2 最简：给 `Register` 加 `UFUNCTION(BlueprintCallable)` 包装（D4-17 已承诺的"反射面"）。
   - G-1 较重：`FTcsEffectContext` / `FTcsChainRun` / `FTcsDamageFlowContext` 反射化，或提供反射镜像 + 转换层；执行器签名换成反射可见的动态委托。
2. **补 G-3：效果链缺失的 12 个原语执行器**（按 `04-module-effects.md` §2.1 的 15 原语清单）
   - Buff/羁绊最急需：`WaitEvent`（事件等待）、`Branch`（分支）、`ApplyState`（挂状态）、`ModifyAttribute`（改属性）、`Repeat`/`Parallel`/`RunSubChain`（编排）。
   - 这是**纯 C++ 侧工作**，与脚本语言选择无关；不做则即便通道打通也写不出完整效果逻辑。
3. **可选：G-4 参数源/选择器/过滤器迁注册表分派**（见 §6）
   - 仅影响"新增参数源类型"这一小部分，**不影响技能/Buff 逻辑**。若用户接受"D3-7 v3 的 C++ only 代价"，可维持现状。
4. **脚本接入**（在 1~3 之后）
   - 选型：UnrealSharp（见 §8.2）。先做 fork 版本对齐（落后上游 135 提交）+ Glue 治理规划。
   - 最小验证走 §11 的 V-1/V-4，在 `TcsDev`（宿主开发代码独立编译单元）内做，验证完可整体摘除。
5. **关卡/流程/UI/存档等 LAC 游戏逻辑**（不依赖 TCS，可并行推进）
   - 这部分正常按 §8.2 接入 UnrealSharp 即可，无需等待 1~3。

---

## 11. 待验证项（本次调研未做运行时实证）

| # | 待验证 | 验证方式 | 优先级 |
|---|---|---|---|
| V-1 | C# 定义的结构体被 `TInstancedStruct<FTcsParamValueSource>` 虚调用时的实际行为（崩溃？静默 0？） | 最小装置：UnrealSharp 项目里定义一个 `[UStruct]` C# 结构体，`Make<T>()` 后塞进 TCS 的 `FTcsParamValue::Source`，在 `Evaluate` 调用点观察 | 高（决定 §5.3 的假设） |
| V-2 | UnrealCSharp 的 `UDynamicScriptStruct` 派生自带虚函数的原生 USTRUCT 时，`GetStructureSize()` 是否包含基类 vtable 位 | 同上思路，用 UnrealCSharp 定义 `[UStruct] class FMySource : FTcsParamValueSource`，观察内存与调用 | 中（若考虑 UnrealCSharp） |
| V-3 | ~~UnrealSharp 的 `FInstancedStruct.Make<T>()` 能否接受**引擎原生** USTRUCT（如 `FTcsStepDamage`）作为 T~~ **已由源码确认可行**：C++ 原生 USTRUCT 同样生成 `record struct`（`StructExporter.cs:89` 对全部 `UhtScriptStruct` 统一处理），满足 `where T : struct, MarshalledStruct<T>` 约束 | — | ✅ 已确认 |
| V-4 | LAC 现有 4 个 C++ 模块在 UnrealSharp 下的 Glue 工程规模与构建时长 | 在 `TcsDev` 或临时分支接入后实测 | 中 |
| V-5 | 用户 fork 与上游 135 提交差距中的热重载/结构体相关修复 | `git log HEAD..upstream/main --oneline` 过滤 struct/hot-reload 关键词 | 中 |

> V-1 / V-3 建议在 `TcsDev` 模块（宿主开发代码的独立编译单元，非 LAC 正式内容）内做，验证完可整体摘除。

---

## 12. 证据索引

### 引擎（UE 5.8，`E:/UnrealEngine/UE_5.8/Engine/Source`）

| 事实 | 位置 |
|---|---|
| 蓝图结构体不支持继承（注释原文） | `Editor/StructUtilsEditor/Private/SInstancedStructPicker.cpp:132` |
| 设了 `BaseStruct` 即排除蓝图结构体 | 同上 `:299` |
| `UUserDefinedStruct` 定义 | `Runtime/CoreUObject/Public/StructUtils/UserDefinedStruct.h` |
| `FInstancedStruct` 声明 + `BaseStruct` 元数据 | `Runtime/CoreUObject/Public/StructUtils/InstancedStruct.h:30` |
| `TInstancedStruct` 的 `IsChildOf` 校验 | 同上 `:368` |
| `PrepareCppStructOps` 只认延迟注册表 | `Runtime/CoreUObject/Private/UObject/Class.cpp:3120-3144` |
| 结构体尺寸来自 `CppStructOps->GetSize()` | 同上 `:944-949` |
| `TCppStructOps` / `DeferCppStructOps` | `Runtime/CoreUObject/Public/UObject/Class.h:2244-2270` |
| `InitializeStruct` 以 `Memzero` 起步 | `Runtime/CoreUObject/Private/UObject/Class.cpp:3775-3783` |
| `FInstancedStruct::InitializeAs` | `Runtime/CoreUObject/Private/StructUtils/InstancedStruct.cpp:66` |
| UHT：`Function` 表的父表是 `Struct` | `Programs/Shared/EpicGames.UHT/Utils/UhtTables.cs:394` |
| UHT：USTRUCT 只能有一个 USTRUCT 基 | `Programs/Shared/EpicGames.UHT/Types/UhtScriptStruct.cs:1100` |

### UnrealSharp（上游 main @ 46e9c2c）

| 事实 | 位置 |
|---|---|
| `[UStruct]` 仅限 C# `struct` | `Managed/UnrealSharp/UnrealSharp.Core/Attributes/UStructAttribute.cs:9` |
| Analyzer 硬校验 `TypeKind.Struct` | `Managed/UnrealSharp/UnrealSharp.Analyzers/UnrealTypeAnalyzer.cs:68` |
| 导出器展平继承链 | `Source/UnrealSharpManagedGlue/Exporters/StructExporter.cs:14-33` |
| 类型声明无基类（`record struct`） | 同上 `:89` |
| 对比：类声明有基类 | `Source/UnrealSharpManagedGlue/Exporters/ClassExporter.cs:55` |
| 结构体不导出函数（空 `UhtFunction` 表） | `StructExporter.cs:114/117/129/132` |
| 运行时类型 = 蓝图结构体 | `Source/UnrealSharpCore/Public/Types/CSScriptStruct.h` |
| 反射数据无 `SuperStruct` | `Source/UnrealSharpCore/Public/ReflectionData/CSStructReflectionData.h` |
| 结构体编译前 `SetSuperStruct(nullptr)` | `Source/UnrealSharpUtilities/Private/UnrealSharpUtils.cpp:74` |
| `FInstancedStruct` 原生绑定 | `Source/UnrealSharpCore/Private/Binds/Bind_FInstancedStruct.cpp` |
| `FInstancedStruct` C# 封装 | `Managed/UnrealSharp/UnrealSharp/Extensions/CoreUObject/InstancedStruct.cs` |
| `Make<T>` 约束 `where T : struct` | 同上 `:101` |
| `IsA<T>` 为精确相等 | 同上（`return nativeStruct == scriptStruct;`） |
| README 引擎版本 5.6–5.8 | `README.md:41` |

### UnrealCSharp（main @ 08c6a76）

| 事实 | 位置 |
|---|---|
| `[UStruct]` 仅限 C# `class` | `Script/UE/Dynamic/Struct/UStructAttribute.cs:7` |
| 生成 `partial class` 并保留基类 | `Source/ScriptCodeGenerator/Private/FStructGenerator.cpp:110`、`:315` |
| 从引擎读 `GetSuperStruct()` | 同上 `:100` |
| 运行时类型 = `UScriptStruct` 子类 | `Source/UnrealCSharpCore/Public/Dynamic/DynamicScriptStruct.h:17` |
| `BeginGenerator` 真调 `SetSuperStruct` | `Source/UnrealCSharpCore/Private/Dynamic/FDynamicStructGenerator.cpp` |
| 支持继承引擎原生 USTRUCT | 同上（`LoadObject<UScriptStruct>(nullptr, *ParentPathName)`） |
| 只生成 `.cs`，无 C++ 代码生成 | `Source/ScriptCodeGenerator/Private/FStructGenerator.cpp:350` |
| 无结构体方法生成 | 同上（对比 `FClassGenerator.cpp` 244 处） |
| `FInstancedStruct` 零引用 | 全仓 `grep -rin "instancedstruct"` 零命中 |
| README 引擎版本 5.0–5.8 | `README.md:24` |
| GC 分配开销（自身 issue） | issue #532 |

### TCS（本仓）

| 事实 | 位置 |
|---|---|
| `FTcsParamValueSource` 虚函数基类 | `Source/TcsCore/Public/Parameter/TcsParamValueSource.h` |
| `TInstancedStruct<FTcsParamValueSource>` 持有 | `Source/TcsCore/Public/Parameter/TcsParamValue.h:37` |
| 虚调用点 `Source.Get().Evaluate(Ctx)` | 同上 `:48` |
| `FTcsTargetSelectorStrategy::Resolve` | `Source/TcsTargeting/Public/Targeting/TcsTargetSelectorStrategy.h` |
| `FTcsTargetFilterStrategy::Pass` | `Source/TcsTargeting/Public/Targeting/TcsTargetFilterStrategy.h` |
| 步骤系统无基类、注册表分派 | `Source/TcsEffect/Public/Chain/TcsEffectStep.h` |
| 执行器注册表（`TFunction`，无 UFUNCTION） | `Source/TcsDamage/Public/Flow/TcsFlowStepExecutor.h` |
| `ITcsParamTableReader`（UINTERFACE） | `Source/TcsCore/Public/Parameter/TcsParamTableReader.h:38` |
| `ITcsDamageFlowDelegate`（UINTERFACE） | `Source/TcsDamage/Public/Flow/TcsDamageFlowDelegate.h` |
| C# 扩展地图（④新源类型 C++ only） | `Documents/combat-system-design/2026-09-10-param-value-source-decision-points.md:20` |
| 语言无关执行器 / 不内嵌脚本引擎 | `Documents/combat-system-design/04-module-effects.md:56`、`2026-09-02-r0-rebuild-position-paper.md:125` |

### 记忆卡

| 卡 | 内容 |
|---|---|
| `MEM-20260911-01` | C# 四问；宿主/脚本可见形状禁不可反射成员 |
| `MEM-20260823-04` | UnrealSharp 热重载触发条件与 `SkipGlueGeneration`；版本落后为首要怀疑项 |
| `MEM-20260910-04` | 策略载体 v3：USTRUCT 基类 + 虚函数分派 + `TInstancedStruct` 持有 |
