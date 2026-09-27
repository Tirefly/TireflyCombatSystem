## ADDED Requirements

### Requirement: 步骤执行器插槽（`TFunction` → UObject 基类）

`TcsEffect` MUST 提供步骤执行器插槽——**让宿主用任意 UE 脚本语言定义步骤行为，零 C++ 改动**：

- 新增 `UTcsStepExecutor : UObject`（`UCLASS(Abstract, Blueprintable)`，住 `Public/Chain/TcsStepExecutor.h`），MUST 提供：
  - `UFUNCTION(BlueprintNativeEvent) ETcsStepResult Execute(FGameplayTag ChainId, FTcsChainRunHandle Run, const FInstancedStruct& StepData)`；
- **为什么需要它**：既有执行器签名 `FTcsStepExecute = TFunction<...>` 是**注册表的值**——键（`UScriptStruct*`）可反射而 `TFunction` 不可，故脚本层无法登记执行器（台账 S-2 的"差一层签名"）。**UObject 基类替代 `TFunction` 换签名**，是更省的路径（不动既有注册表结构、不动 C++ 快路径）；
- **反射注册入口**：`UTcsEffectSubsystem` MUST 提供 `UFUNCTION() bool RegisterStepExecutor(UScriptStruct* StepStruct, UTcsStepExecutor* Executor)`——内部包成 `TFunction` 转发进既有注册表（**键与查表逻辑零改动**）；拒绝面（ensure + false）：`StepStruct` 空、`Executor` 空、同类型重复登记；
- **`UScriptStruct*` 可作反射形参**（引擎先例：`UDataTableFunctionLibrary::FillDataTableFromCSVString` 的 `UScriptStruct* ImportRowStruct`）——脚本层据此传"步骤 struct 的反射类型"；
- **`ETcsStepResult` MUST 升格 `UENUM()`**：它作 `BlueprintNativeEvent` 的返回类型，非反射枚举拿不到 `IsParameterSupportedByBlueprint` cap（`UhtEnumProperty.cs:68-69`）⇒ UHT 拒绝。升格只加反射标记、不改枚举值（`TSR_Completed=0` / `TSR_Running=1` 不变）；
- **GC 可见持有（T-8 教训）**：门面 MUST 以 `UPROPERTY TArray<TObjectPtr<UTcsStepExecutor>>` 持有已登记执行器——**裸 C++ 注册表持不住对象引用**（不经 GC 的 `RefLink`），脚本执行器会被静默回收，表现为"步骤不生效"而非崩溃（先例：`UTcsDamageSubsystem::AddReferencedObjects` 的模板登记表修复）；
- **挂起语义不变**：脚本执行器返回 `TSR_Running` 时，解释器停驻原 PC 等唤醒——与 C++ 执行器同一协议（挂起锚住运行态，不住调用栈）；
- **双轨并存**：C++ 静态自注册宏路径（`UE_DEFINE_EFFECT_STEP_EXECUTOR`）MUST 保持原样；插槽只给宿主扩展，**MUST NOT** 把框架内置步骤改写成 UObject 执行器。

#### Scenario: 脚本层定义并登记步骤执行器

- **WHEN** 宿主脚本层定义一个 `UTcsStepExecutor` 子类实现 `Execute`，用自己定义的步骤 struct 类型经 `RegisterStepExecutor` 登记，然后把该步骤放进链
- **THEN** 解释器分派到该脚本执行器（键 = 步骤 struct 反射类型命中），链行为由脚本决定

#### Scenario: 脚本执行器不被 GC 回收

- **WHEN** 脚本执行器登记后，除门面外无其他强引用，随后触发 GC
- **THEN** GC 后该执行器仍存活（门面的 `UPROPERTY` 持有生效），步骤分派仍抵达它

#### Scenario: C++ 自注册路径不受影响

- **WHEN** 某模块用 `UE_DEFINE_EFFECT_STEP_EXECUTOR` 静态自注册一个步骤类型
- **THEN** 行为与插槽引入前一致（注册表结构与查表逻辑未改动）

#### Scenario: 脚本执行器可表达挂起

- **WHEN** 脚本执行器返回 `TSR_Running`
- **THEN** 解释器停驻原 PC（本步未完成），后续由唤醒源按运行态句柄重入——脚本执行器可据此自辨"首入 / 被唤醒"
