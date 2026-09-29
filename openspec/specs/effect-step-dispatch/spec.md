# effect-step-dispatch Specification

## Purpose
定义步骤执行器的签名与注册表、静态自注册宏、未知步骤类型的拒绝路径，以及以 UObject 基类替代 `TFunction` 的步骤执行器插槽。
## Requirements
### Requirement: 步骤执行器签名与注册表

`TcsEffect` MUST 以**执行器注册表**分派步骤执行（D4-14：机制层对步骤类型零硬编码 switch；领域步骤类型 + 执行器归领域模块）：

- 执行器签名（D4-17）：`FTcsStepExecute = TFunction<ETcsStepResult(const FInstancedStruct& StepData, FTcsEffectContext& Context, FTcsChainRun& Run)>`——步骤数据**只读**，上下文与运行态可写（挂起协议经返回值 + 运行态承载，见 `effect-interpreter`）；
- **注册键 = 步骤 struct 的反射类型**（`const UScriptStruct*`）：执行期唯一可得的类型身份即 `FInstancedStruct::GetScriptStruct()`，指针身份免去名字往返（不为注册另造 FName 键）；
- 注册入口 MUST 双份（D4-17 语言无关预留）：①C++ 静态自注册宏（见下一需求）；②动态注册 `Register(UScriptStruct*, FTcsStepExecute)`（反射层 / 脚本层 / 测试装置可达）；
- 同一类型重复登记 MUST 拒绝（ensure 提示 + 保留首个登记，不静默覆写）；
- 查询 `Find(UScriptStruct*)` 未命中 MUST 返回 nullptr 且不 ensure（"未知步骤类型"的处置归解释器）；
- **寿命语义（2026-09-29 新增，`DEC-04` 裁定 ⑤；修 `LEDGER-reflection` R-2 跨世界寿命缺陷）**：注册表是**进程级单例**且 MUST 保持如此——但**动态登记项 MUST 一并记录**（a）宿主对象的 `TWeakObjectPtr`、（b）**登记时所在世界**的 `TWeakObjectPtr<UWorld>`。**静态自注册的纯函数项 MUST NOT 受寿命约束**（无 UObject 寿命问题，永不过期；"静态初始化期零 UObject 触达"纪律不受本条影响）；
- **失效判据**：动态登记项在满足任一条时 MUST 视为**失效**——①宿主弱引用为空（对象已被 GC）；②世界弱引用为空；③条目世界 ≠ 查询方世界；
- **查询侧校验**：`Find` MUST 接受**可选**的世界校验入参（`const UWorld* World = nullptr`；既有签名 MUST 保持可编译）；**调用方持有世界时 MUST 显式传入**。判定为跨世界失效时 MUST **视为未命中**（返回 nullptr）并**移除该条目** + 留 **Warning** 日志（含步骤类型名）——MUST NOT 静默装作"未登记"（静默会把"世界已更换"表现成"配置写漏"，排障成本由此而来）；
- **拒绝门收窄为"同世界活对象重复"**：键已存在但既有条目**已按上述判据失效**时，新登记 MUST **替换**该条目而 MUST NOT 拒绝——这是"失效条目毒化后续所有 PIE"的根因所在；仅当既有条目**有效且属同一世界**（或为静态自注册项）时才 MUST 拒绝（ensure + 保留首个，沿用既有口径）；
- **显式移除入口**：注册表 MUST 提供按键移除**动态**条目的入口；该入口 MUST NOT 能移除静态自注册项（静态项是代码而非登记）。缺省不调用时，正确性 MUST NOT 受影响——失效判据须自足（对应用户 2026-09-27 裁定"TCS 侧兜底优先，注销作退路"）。

#### Scenario: 已注册类型可查到执行器

- **WHEN** 经动态入口为某步骤 struct 注册执行器后查询该类型
- **THEN** 返回该执行器（非空）

#### Scenario: 未注册类型查询为空

- **WHEN** 查询一个从未注册的步骤 struct 类型
- **THEN** 返回 nullptr（无 ensure——由解释器按"未知步骤类型"处置）

#### Scenario: 重复登记被拒

- **WHEN** 对同一类型在**同一世界**内、既有登记**仍有效**时再次注册执行器
- **THEN** 拒绝并保留首个登记，留 ensure 提示与日志

#### Scenario: 宿主对象被回收后该条登记自动失效

- **WHEN** 经动态入口登记的执行器对象已无其他强引用且已被 GC，随后查询该步骤类型
- **THEN** 返回 nullptr（该条登记判为失效并移除），MUST NOT 解引用已回收对象、MUST NOT 崩溃

#### Scenario: 跨世界登记被判定失效并替换

- **WHEN** 第一个世界登记了某类型执行器，该世界结束（对象与世界弱引用均失效）后第二个世界对同类型再次登记
- **THEN** 新登记**成功替换**旧条目，MUST NOT 触发"重复登记"拒绝、MUST NOT 产生 ensure 红字

#### Scenario: 跨世界查询返回未命中并留 Warning

- **WHEN** 条目仍属世界 A（其对象尚存活），调用方带世界 B 查询该类型
- **THEN** 返回 nullptr、移除该条目、留含类型名的 Warning 日志（MUST NOT 静默按"未登记"处理）

#### Scenario: 显式移除动态条目

- **WHEN** 调用移除入口撤销某动态登记后查询该类型
- **THEN** 返回 nullptr；且该入口对静态自注册项无效（静态项仍可查到）
### Requirement: 静态自注册宏

`TcsEffect` MUST 提供自注册宏对：`UE_DECLARE_EFFECT_STEP_EXECUTOR(ExecutorFn)` / `UE_DEFINE_EFFECT_STEP_EXECUTOR(StepType, ExecutorFn)`——领域模块在自己的步骤实现文件里用一行宏完成登记，**消去 `StartupModule` 样板**（D4-14，仿原生 GameplayTag 模式）：

- 登记发生在**模块静态初始化期**；该时点 MUST NOT 触达 UObject 设施——注册器只把"步骤类型 getter + 执行器"挂入**待解析表**，注册表**首次查询时**才调用 getter 取 `UScriptStruct*` 并建立键（依据：引擎 `FNativeGameplayTag` 构造函数的同款纪律——`UGameplayTagsManager::GetIfAllocated()`，静态初始化期不假定 UObject 设施已就绪）；
- 机制层 MUST NOT 认识任何领域步骤类型（依赖方向：领域模块 → 机制层）；领域模块无须任何模块启动代码即可完成自登记。

#### Scenario: 领域模块自登记后即时可执行

- **WHEN** 某模块在其 `.cpp` 用宏登记一个步骤类型（该模块已加载）
- **THEN** 该类型的执行器在注册表可查到，链步骤经解释器分派到它，全程无须 `StartupModule` 代码

#### Scenario: 静态初始化期零 UObject 触达

- **WHEN** 模块静态初始化（DLL 加载）时宏展开的注册器被构造
- **THEN** 只写入待解析表（不调用 `StaticStruct()`、不做反射查询）——反射类型在首次 `Find` 时才解析

### Requirement: 未知步骤类型拒绝

解释器遇到**未注册的步骤类型**时 MUST 断链：留 Error 日志（含链 id、步骤序号与类型名）+ 释放该链运行态；MUST NOT 崩溃、MUST NOT 静默跳过（静默跳过会把"配置写错"表现成"效果没生效"，掩盖真实缺陷）。

#### Scenario: 未注册步骤导致断链

- **WHEN** 起一条含未注册执行器步骤的链
- **THEN** 该链执行中止（运行态释放、后续步不执行），Error 日志给出链 id、步骤序号与类型名；先前步骤的副作用不回滚（止于未来）

### Requirement: 步骤执行器插槽（`TFunction` → UObject 基类）

`TcsEffect` MUST 提供步骤执行器插槽——**让宿主用任意 UE 脚本语言定义步骤行为，零 C++ 改动**：

- 新增 `UTcsStepExecutor : UObject`（`UCLASS(Abstract, Blueprintable)`，住 `Public/Chain/TcsStepExecutor.h`），MUST 提供：
  - `UFUNCTION(BlueprintNativeEvent) ETcsStepResult Execute(FGameplayTag ChainId, FTcsChainRunHandle Run, const FInstancedStruct& StepData)`；
- **为什么需要它**：既有执行器签名 `FTcsStepExecute = TFunction<...>` 是**注册表的值**——键（`UScriptStruct*`）可反射而 `TFunction` 不可，故脚本层无法登记执行器（台账 SCRIPT-2 的"差一层签名"）。**UObject 基类替代 `TFunction` 换签名**，是更省的路径（不动既有注册表结构、不动 C++ 快路径）；
- **反射注册入口**：`UTcsEffectSubsystem` MUST 提供 `UFUNCTION() bool RegisterStepExecutor(UScriptStruct* StepStruct, UTcsStepExecutor* Executor)`——内部包成 `TFunction` 转发进既有注册表（**键与查表逻辑零改动**）；拒绝面（ensure + false）：`StepStruct` 空、`Executor` 空、同类型重复登记；
- **`UScriptStruct*` 可作反射形参**（引擎先例：`UDataTableFunctionLibrary::FillDataTableFromCSVString` 的 `UScriptStruct* ImportRowStruct`）——脚本层据此传"步骤 struct 的反射类型"；
- **`ETcsStepResult` MUST 升格 `UENUM()`**：它作 `BlueprintNativeEvent` 的返回类型，非反射枚举拿不到 `IsParameterSupportedByBlueprint` cap（`UhtEnumProperty.cs:68-69`）⇒ UHT 拒绝。升格只加反射标记、不改枚举值（`TSR_Completed=0` / `TSR_Running=1` 不变）；
- **GC 可见持有（WAIT-8 教训）**：门面 MUST 以 `UPROPERTY TArray<TObjectPtr<UTcsStepExecutor>>` 持有已登记执行器——**裸 C++ 注册表持不住对象引用**（不经 GC 的 `RefLink`），脚本执行器会被静默回收，表现为"步骤不生效"而非崩溃（先例：`UTcsDamageSubsystem::AddReferencedObjects` 的模板登记表修复）；
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
