## MODIFIED Requirements

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
