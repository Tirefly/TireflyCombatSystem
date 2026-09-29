# Change: harden-registry-cross-world-lifetime

## Why

2026-09-27 的反射专项调研（`LEDGER-reflection` R-2）在 SCRIPT-8 已落地的代码里发现一处**跨世界寿命缺陷**，2026-09-29 由代码实测确认三条事实：

1. **4 张执行器/条件注册表是进程级单例，且没有任何撤销入口**——`FTcsEffectStepExecutorRegistry` 的公开面只有 `AddPending` / `Register` / `Find`（`Source/TcsEffect/Public/Chain/TcsEffectStepExecutor.h:58-92`），Damage 侧、以及两张触发注册表同款；
2. **动态登记的重复判定只看键**——同类型再次登记一律 `ensureMsgf(false, "拒绝重复登记（保留首个）")`（`TcsEffectStepExecutor.cpp:40-47`）；
3. **门面 `Deinitialize` 不撤销动态登记**——`UTcsEffectSubsystem::Deinitialize` 清了 `TriggerRegistry` / `RunPool` / `ChainDefs` / `EntityQuery`，**唯独没碰** `RegisteredStepExecutors`（`TcsEffectSubsystem.cpp:19-31`）。

于是这条链成立：PIE 结束 → 子系统销毁 → `UPROPERTY` 数组（唯一强引用）释放 → 执行器对象被 GC；**但进程级注册表里的 `TFunction` 仍捕获着它的裸指针**。第二次 PIE 宿主重新登记 → 命中"保留首个" → **ensure 红字 + 拿到 stale 指针** → 步骤执行 = 野调用。

这不是"少写了一个 `Unregister`"，而是**容器契约与载体寿命的冲突**：注册表是**进程级、跨世界存活**的（这个设计至今正确且必须保持——见 `proposal` 下方"为什么不动注册表本身"），而 SCRIPT-8 让它的值第一次变成了**世界级 GC 对象**。设计文档 `DEC-04-callback-carriers` §3 已就此事给出完整裁定（2026-09-29，用户拍板五项全接受），本提案是**裁定 ⑤ 的第一步：A 类"值语义"改造**。

**当前被流程纪律掩盖着**：证据文档与追踪矩阵都写着"下一轮运行前须重启 Editor 进程"（`evidence/2026-09-28-host-scripting-e2e-pie.md:58`、`tcs-contract-traceability.md:33`）——重启 Editor 顺手清掉了进程级注册表。一旦不重启连着跑 PIE，缺陷就从"潜藏"变成"野调用"。

## What Changes

- **注册表的动态登记项获得寿命语义**：动态登记项 MUST 一并记录 **UObject 弱引用**与 **来源世界（`UWorld`）弱引用**；对静态自注册的纯函数项无影响（无 UObject 寿命问题）。
- **拒绝门从"键重复"收窄为"同世界活对象重复"**：键已存在但既有条目**已失效**（UObject 回收，或属于另一个世界）时，MUST **替换**该条目而 MUST NOT 拒绝——消除"stale 条目毒化后续所有 PIE"这一根因。
- **查询侧按世界校验**：`Find` 增加**可选**的世界校验入参；既有条目所属世界与调用方世界不同时 MUST **视为未命中**并移除该条目（留 Warning 日志，MUST NOT 静默装作未登记——那会把"世界已换"表现成"配置写漏"）。
- **显式注销入口**：注册表 MUST 提供按键或按来源移除动态条目的入口（静态自注册项 MUST NOT 被移除）。
- **`Deinitialize` 收口**：门面 `Deinitialize` MUST 撤销**本世界**登记的动态条目（`UPROPERTY` GC 可见持有 MUST 保留——它是防静默回收的另一半，两者互补而非替代）。
- **静态自注册路径的"静态初始化期零 UObject 触达"纪律 MUST 保持**（`effect-step-dispatch` 既有需求，本提案 MUST NOT 削弱）。
- **覆盖范围 = 4 张注册表**：`FTcsEffectStepExecutorRegistry`、`FTcsFlowStepExecutorRegistry`、`FTcsTriggerConditionRegistry`、`FTcsTriggerPayloadReaderRegistry`。**后两张当前没有任何 UObject 动态登记**（载荷读取器登记数为 0），本提案只给它们**加寿命语义与撤销入口**，MUST NOT 借此新增脚本插槽——那属 R-2 的独立批次。

**非目标（MUST NOT 顺手做）**：
- MUST NOT 把静态自注册的内置执行器改成 UObject / CDO 载体（`DEC-04` 裁定 ① 的 D 类：静态初始化期不能触达 UObject 设施，且内置执行器是纯函数、UObject 化零收益）；
- MUST NOT 动 `ITcsEntityQuery` 的 `TFunctionRef`（`DEC-04` 裁定 ③ 已定案"不换"）；
- MUST NOT 动 `FTcsConsumePolicy::OnConsumed`（`DEC-04` 裁定 ④：与台账 `DAMAGE-4` 同批）；
- MUST NOT 新增条件求值器 / 载荷读取器的宿主脚本插槽（属 `LEDGER-reflection` R-2 的独立提案）。

## Impact

- **Affected specs**：`effect-step-dispatch`（注册表寿命语义）、`damage-flow`（流程步骤注册表同款）、`effect-trigger`（条件求值器 + 载荷读取器两张注册表的寿命语义）
- **Affected code**：
  - 注册表：`TcsEffect/{Public,Private}/Chain/TcsEffectStepExecutor.*`、`TcsDamage/{Public,Private}/Flow/TcsFlowStepExecutor.*`、`TcsEffect/{Public,Private}/Trigger/TcsTriggerCondition.*`、`TcsEffect/{Public,Private}/Trigger/TcsTriggerPayloadReader.*`
  - 门面：`TcsEffectSubsystem`（`RegisterStepExecutor` + `Deinitialize`）、`TcsDamageSubsystem`（同款）
  - `Find` 的 5 个调用点：`TcsEffectSubsystem.cpp:414`、`TcsDamageSubsystem.cpp:212`、`TcsFlowStepExecutor.cpp:57`、`TcsTriggerCondition.cpp:151`、`TcsTriggerEvaluator.cpp:112`
- **验收**：Development + Shipping 双配置编译；`Tcs.Test.Slice.Run` 10/10、`Tcs.Test.Slice.Reject` 3/3 原样通过（双轨并存 ⇒ C++ 快路径行为不变）；**新增行为判据 = 连续两次 PIE 不再出现"拒绝重复登记"红字**
- **台账**：消费 `LEDGER-reflection` R-2 的**寿命缺陷部分**（R-2 的"脚本插槽"部分仍待独立提案）；`LEDGER-deferred` SCRIPT-8 的"仍未解决"项随之收窄

## 为什么不动注册表本身（进程级单例必须保持）

注册表是**函数局部静态单例**，这不是随手选的，而是三条设计约束共同迫使的（`DEC-04` §"决策链"②）：

1. 自注册器在**模块静态初始化期**构造（宏展开的载体）——那时**没有任何世界**；
2. 既有需求明令"**静态初始化期 MUST NOT 触达 UObject 设施**"（依据引擎 `FNativeGameplayTag::GetIfAllocated()` 同款纪律）；
3. "步骤是代码而非世界状态"——一个步骤类型全局只需一个执行器。

因此本提案**只改"值"的寿命语义，不改"容器"的级别**。这也是 2026-09-27 R-2 评审时用户裁定的方向——"**TCS 侧兜底优先**，A1（补 `Unregister` + 门面注销）作退路"——本提案实现的正是兜底：**对象死了或世界换了，该条登记自然失效**，不依赖谁记得撤销。显式 `Unregister` 仍提供，但它是**可选的整理手段，不是正确性的前提**。
