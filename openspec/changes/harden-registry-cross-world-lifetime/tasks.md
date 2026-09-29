# Tasks: harden-registry-cross-world-lifetime

> 执行纪律（本仓既有）：每步完成即**停点待检查**；**禁 TDD**（无失败测试先行）；验证 = UBT 编译通过 + 定向人工检查 + 用户 PIE 实测；**不含自动提交**（提交需用户明确授权）。

## 1. 值语义：两张步骤执行器注册表

- [ ] 1.1 `FTcsEffectStepExecutorRegistry`：条目结构加 **宿主对象弱引用**（`TWeakObjectPtr<UObject>`）+ **登记世界弱引用**（`TWeakObjectPtr<const UWorld>`）两个字段；`AddPending` 的静态自注册路径两个字段保持空（= 永不过期）
- [ ] 1.2 `FTcsFlowStepExecutorRegistry`：同款改造（与 Effect 侧结构对称，便于后续比对）
- [ ] 1.3 `Register` 记录来源：门面注册入口把 `Executor` 与 `GetWorld()` 一并传入（`Register` 签名加世界入参或加一个带世界的重载；**既有 `Register(StepStruct, Executor)` 形态 MUST 保持可编译**）
- [ ] 1.4 编译（Development）——此时**不改拒绝门**，行为与改前一致（保证这是可编译的中间态）

## 2. 值语义：两张触发注册表（本批只加寿命语义，MUST NOT 加脚本插槽）

- [ ] 2.1 `FTcsTriggerConditionRegistry`：同款字段与判据（内置条件全走静态自注册 ⇒ 不受寿命约束）
- [ ] 2.2 `FTcsTriggerPayloadReaderRegistry`：同款（注意其动态登记当前为 0，改动是纯前瞻性的，**回归面为零**）
- [ ] 2.3 **MUST NOT** 借本批新增条件求值器 / 载荷读取器的宿主脚本插槽（那属 `LEDGER-reflection` R-2 的独立提案；见 `proposal.md` 非目标）

## 3. 查询侧：失效判据与跨世界判定

- [ ] 3.1 `Find` 签名加**可选**世界入参：`Find(const UScriptStruct* StepStruct, const UWorld* World = nullptr)`（4 张注册表统一）
- [ ] 3.2 失效判据实现（三者任一即失效）：宿主弱引用为空 / 世界弱引用为空 / 条目世界 ≠ 传入世界。**传入世界为 nullptr 时 MUST 跳过世界校验**（只做弱引用校验），以保留纯 C++ / 测试装置场景可用
- [ ] 3.3 跨世界失效处置：视为未命中（返回 nullptr）+ **移除该条目** + 留含类型名的 **Warning** 日志——MUST NOT 静默按"未登记"处理
- [ ] 3.4 载荷读取器侧额外区分：跨世界失效（Warning）与"未注册载荷类型"（Verbose）MUST NOT 混为一谈

## 4. 拒绝门收窄为"同世界活对象重复"

- [ ] 4.1 `Register` 的重复判定改为先**求值既有条目是否失效**：失效 ⇒ **替换**（记 Log 说明替换原因，**MUST NOT ensure**）；有效且同世界（或静态自注册项）⇒ 沿用既有拒绝（ensure + 保留首个）
- [ ] 4.2 替换路径 MUST NOT 触发任何回收逻辑（失效条目已无活对象可回收）
- [ ] 4.3 定向自检：以"第一次登记 → 令弱引用失效 → 第二次登记"的顺序手走一遍，确认走到替换分支而非拒绝分支

## 5. 显式移除入口与门面收口

- [ ] 5.1 4 张注册表补**按键移除动态条目**的入口；该入口 MUST NOT 能移除静态自注册项（静态项是代码而非登记）
- [ ] 5.2 门面 `UTcsEffectSubsystem::Deinitialize`：撤销**本世界**登记的动态条目（`RegisteredStepExecutors` 的 `UPROPERTY` GC 可见持有 **MUST 保留**——两者互补，删任一半都留洞）
- [ ] 5.3 门面 `UTcsDamageSubsystem::Deinitialize`：同款
- [ ] 5.4 定向自检：门面 `Deinitialize` 后查同类型 ⇒ 未命中；而静态自注册类型仍可查到

## 6. `Find` 调用点显式传世界（5 处，全部 MUST 传）

- [ ] 6.1 `Source/TcsEffect/Private/TcsEffectSubsystem.cpp:414`（链步骤执行中）
- [ ] 6.2 `Source/TcsDamage/Private/TcsDamageSubsystem.cpp:212`（流程步骤执行中）
- [ ] 6.3 `Source/TcsDamage/Private/Flow/TcsFlowStepExecutor.cpp:57`（注册表内转发）
- [ ] 6.4 `Source/TcsEffect/Private/Trigger/TcsTriggerCondition.cpp:151`（条件求值，**热路径**）
- [ ] 6.5 `Source/TcsEffect/Private/Trigger/TcsTriggerEvaluator.cpp:112`（载荷读取，每事件一次）
- [ ] 6.6 定向人工检查：`grep 'Find(' Source/` 确认**无省参调用**（不得依赖默认参数绕过世界校验）
- [ ] 6.7 热路径实测判断：条件求值器侧若观测到开销影响，评估"本帧缓存解析结果"（属后续优化，**不在本批范围**——按实测数据决定，不预先优化）

## 7. 验证

- [ ] 7.1 编译 **Development Editor**：零警告零错误
- [ ] 7.2 编译 **Shipping**（本批含 `UCLASS`/`UPROPERTY` 面与弱引用字段，Shipping 必跑）
- [ ] 7.3 既有回归网原样通过：`Tcs.Test.Slice.Run` **10/10**、`Tcs.Test.Slice.Reject` **3/3**（双轨并存 ⇒ C++ 快路径行为 MUST 不变）；**常规命令 MUST 零红字**
- [ ] 7.4 **缺陷行为实证（本批的核心判据）**：验证"宿主脚本执行器"在登记它的世界结束后**再次登记同类型**能成功——第二轮 **MUST NOT** 出现"拒绝重复登记"ensure 红字。**实施时先检索宿主既有脚本装置可否复用**；不可复用则与用户确认是否值得为它专门搭一个最小装置（本项只记录，不改变设计——依 2026-09-27 用户裁定"先实测但只做记录"）
- [ ] 7.5 `git diff --check` 无空白错误；改动文件行尾 LF、无 BOM

## 8. 收束

- [ ] 8.1 三条 delta 归档：`openspec archive harden-registry-cross-world-lifetime --yes`，随后 `openspec validate --all --strict --no-interactive` 全绿
- [ ] 8.2 台账回写：`LEDGER-reflection` R-2 的**寿命缺陷部分**标为已修（R-2 的"脚本插槽"部分仍待独立提案）；`LEDGER-deferred` SCRIPT-8 的"仍未解决"项随之收窄（跨世界寿命缺陷已闭环）
- [ ] 8.3 设计文档回写：`SPEC-03-effects` 的执行器注册表段、`SPEC-08-damage` 的流程步骤注册表段、`SPEC-03-effects` 触发条件/载荷读取器段——补"寿命语义"现状（D4-14 / D4-17 的落点不变，只补值语义）
- [ ] 8.4 `DEC-04-callback-carriers` 状态从 `待落地` 更新为 `冻结`（实现已完成）或按实际进度收窄；`INDEX.md` §4.2 同步
- [ ] 8.5 证据固化：若 7.4 取得可复核输出，落 `evidence/` 并按 `EVID-*` 登记（含日志行号锚点）；否则只记"未取得行为证据"
