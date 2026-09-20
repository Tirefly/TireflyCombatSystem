## ADDED Requirements

### Requirement: 步骤执行器签名与注册表

`TcsEffect` MUST 以**执行器注册表**分派步骤执行（D4-14：机制层对步骤类型零硬编码 switch；领域步骤类型 + 执行器归领域模块）：

- 执行器签名（D4-17）：`FTcsStepExecute = TFunction<ETcsStepResult(const FInstancedStruct& StepData, FTcsEffectContext& Context, FTcsChainRun& Run)>`——步骤数据**只读**，上下文与运行态可写（挂起协议经返回值 + 运行态承载，见 `effect-interpreter`）；
- **注册键 = 步骤 struct 的反射类型**（`const UScriptStruct*`）：执行期唯一可得的类型身份即 `FInstancedStruct::GetScriptStruct()`，指针身份免去名字往返（不为注册另造 FName 键）；
- 注册入口 MUST 双份（D4-17 语言无关预留）：①C++ 静态自注册宏（见下一需求）；②动态注册 `Register(UScriptStruct*, FTcsStepExecute)`（反射层 / 脚本层 / 测试装置可达）；
- 同一类型重复登记 MUST 拒绝（ensure 提示 + 保留首个登记，不静默覆写）；
- 查询 `Find(UScriptStruct*)` 未命中 MUST 返回 nullptr 且不 ensure（"未知步骤类型"的处置归解释器）。

#### Scenario: 已注册类型可查到执行器

- **WHEN** 经动态入口为某步骤 struct 注册执行器后查询该类型
- **THEN** 返回该执行器（非空）

#### Scenario: 未注册类型查询为空

- **WHEN** 查询一个从未注册的步骤 struct 类型
- **THEN** 返回 nullptr（无 ensure——由解释器按"未知步骤类型"处置）

#### Scenario: 重复登记被拒

- **WHEN** 对同一类型再次注册执行器
- **THEN** 拒绝并保留首个登记，留 ensure 提示与日志

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
