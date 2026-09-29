# Tasks: harden-registry-cross-world-lifetime

> 执行纪律（本仓既有）：每步完成即**停点待检查**；**禁 TDD**（无失败测试先行）；验证 = UBT 编译通过 + 定向人工检查 + 用户 PIE 实测；**不含自动提交**（提交需用户明确授权）。

## 1. 值语义：两张步骤执行器注册表

- [x] 1.1 `FTcsEffectStepExecutorRegistry`：条目结构加 **宿主对象弱引用**（`TWeakObjectPtr<UObject>`）+ **登记世界弱引用**（`TWeakObjectPtr<const UWorld>`）两个字段；`AddPending` 的静态自注册路径两个字段保持空（= 永不过期）—— 落点：新增 `FTcsEffectStepExecutorLifetime` + 成员 `TMap<const UScriptStruct*, FTcsEffectStepExecutorLifetime> Lifetimes`（**只含动态项**，静态项不建条目）
- [x] 1.2 `FTcsFlowStepExecutorRegistry`：同款改造（与 Effect 侧结构对称）—— 落点：`FTcsFlowStepExecutorLifetime` + 同款 `Lifetimes`
- [x] 1.3 `Register` 记录来源：门面注册入口把 `Executor` 与 `GetWorld()` 一并传入 —— 落点：`Register(StepStruct, Executor, LifetimeObject = nullptr, LifetimeWorld = nullptr)`（**默认参数使既有两参形态保持可编译**）+ 门面两处调用点已改
- [x] 1.4 编译（Development）—— 中间态可编译（最终编译见 7.1）

## 2. 值语义：两张触发注册表（本批只加寿命语义，MUST NOT 加脚本插槽）

- [x] 2.1 `FTcsTriggerConditionRegistry`：同款字段与判据（内置条件全走静态自注册 ⇒ 不受寿命约束）—— 落点：`FTcsTriggerConditionLifetime` + `Lifetimes`
- [x] 2.2 `FTcsTriggerPayloadReaderRegistry`：同款（动态登记当前为 0，改动纯前瞻，**回归面为零**）—— 落点：`FTcsTriggerPayloadReaderLifetime` + `Lifetimes`
- [x] 2.3 **MUST NOT** 借本批新增条件求值器 / 载荷读取器的宿主脚本插槽 —— ✅ 遵守（本批只动寿命语义与移除入口，未新增任何 `UFUNCTION` / 未升格任何 `BlueprintType`）；另把两张注册表 JSDoc 里"与步骤执行器注册表同批解决"的**过期口径**更新为"与载荷读取器/条件求值器同批，属 R-2 独立提案"

## 3. 查询侧：失效判据与跨世界判定

- [x] 3.1 `Find` 签名加**可选**世界入参（4 张注册表统一）
- [x] 3.2 失效判据实现（三者任一即失效）—— 落点：`Find` 内联判定 + `DiscardIfStale` 共用同款三条件；`World == nullptr` 时跳过世界校验（保留纯 C++ / 测试装置可用性）
- [x] 3.3 跨世界失效处置：视为未命中 + **移除该条目** + **Warning** 日志（含类型名与两个世界名）
- [x] 3.4 载荷读取器侧区分 —— ✅ 跨世界走 `Warning`；"未注册载荷类型"仍走原 `Verbose` 路径（两条路径在代码注释中互相指名）

## 4. 拒绝门收窄为"同世界活对象重复"

- [x] 4.1 `Register` 先求值既有条目是否失效：失效 ⇒ **替换**（`Log` 说明替换原因，无 ensure）；有效且同世界（或静态自注册项）⇒ 沿用拒绝（ensure + 保留首个）
- [x] 4.2 替换路径无回收逻辑（失效条目已无活对象）
- [x] 4.3 定向自检 —— ⚠ **未执行构造性自检**：实际 PIE 实测中该分支**未被触发**（显式撤销先行清理），故"替换分支"仍属**未经行为验证**的路径。见 §9 的边界说明

## 5. 显式移除入口与门面收口

- [x] 5.1 4 张注册表补按键移除入口（`Unregister`）+ **键清单**（`GetDynamicKeys`）；`Unregister` 对静态自注册项返回 `false`（MUST NOT 可移除代码登记）
- [x] 5.2 `UTcsEffectSubsystem::Deinitialize` 撤销本世界动态条目；`UPROPERTY` 强持有**保留**（改为清空数组，未删字段）
- [x] 5.3 `UTcsDamageSubsystem::Deinitialize` 同款
- [x] 5.4 定向自检 —— ✅ **已由实测覆盖**：两次 PIE 各打印 `Deinitialize: 持有执行器 1 个，注册表动态条目 1 个`，说明条目确在注册表内且被本步清除（第二次 PIE 因此见到干净状态）

## 6. `Find` 调用点显式传世界（**实测 4 处**，全部 MUST 传）

> 口径修正：原写"5 处"，其中 `TcsFlowStepExecutor.cpp:57` 实为**上一批修的注释行**，非调用点。真实调用点为 **4 处**。

- [x] 6.1 `TcsEffectSubsystem.cpp`（链步骤执行中）→ `Find(StepStruct, GetWorld())`
- [x] 6.2 `TcsDamageSubsystem.cpp`（流程步骤执行中）→ `Find(StepStruct, GetWorld())`
- [x] 6.3 ~~`TcsFlowStepExecutor.cpp:57`~~（非调用点，见上方口径修正）
- [x] 6.4 `TcsTriggerCondition.cpp`（条件求值，**热路径**）→ `Find(ConditionType, World)`；世界由 `EvaluateTriggerConditions` 新增的可选入参透传，调用方 `PassesConditions` 传 `Facade->GetWorld()`
- [x] 6.5 `TcsTriggerEvaluator.cpp`（载荷读取，每事件一次）→ `Find(PayloadType, Owner.IsValid() ? Owner->GetWorld() : nullptr)`
- [x] 6.6 定向人工检查 —— ✅ 脚本核过：**无省参调用**
- [x] 6.7 热路径实测判断 —— ✅ 实测**未观测到开销问题**（`Slice.Run` 与探针全通过、无卡顿报告）；按"不预先优化"结案，本帧缓存**不做**

## 7. 验证

- [x] 7.1 编译 **Development Editor**：零警告零错误 —— ✅ `LegendAutoChessEditor Win64 Development` **Result: Succeeded**（17 action / 64.76 s），**0 error / 0 warning**（判据取自完整 UBT 日志；另做一次强制重编确认非"0 action 空跑"）
- [x] 7.2 编译 **Shipping** —— ⚠ **本条口径已修正**：`LegendAutoChessEditor` **不支持 Shipping**（编辑器目标无此配置，首跑 `Failed (OtherCompilationError)`，UBT 原文 "LegendAutoChessEditor does not support the Shipping configuration"）。Shipping 验证 MUST 用 **Game 目标** `LegendAutoChess`：✅ **Result: Succeeded**（46 action / 159.38 s），**0 error / 0 warning**
- [x] 7.3 既有回归网原样通过 —— ✅ 三次 PIE 全部通过：`Tcs.Test.Slice.Run` 逐次跑到链执行完成、`Tcs.Test.Slice.Reject` 每次 `通过 3 / 失败 0`（检查 A/B/C 全 PASS）。**C++ 快路径行为不变**（双轨并存的预期）
- [x] 7.4 **缺陷行为实证（本批的核心判据）** —— ✅ **通过**，装置复用宿主既有 `ATcsHostScriptingE2EProbe`（`Script/LegendAutoChessCS/TcsProbe/`，无需新建）。详见 §9 实测记录
- [x] 7.5 `git diff --check` 无空白错误；改动文件行尾 LF、无 BOM —— ✅ 13 文件 CRLF=0 / BOM=0，tab 缩进，`diff --check` 无输出

## 8. 收束

- [x] 8.1 三条 delta 归档 —— ✅ `openspec archive harden-registry-cross-world-lifetime --yes` 已执行 → 归档为 `2026-09-29-harden-registry-cross-world-lifetime`；**应用结果 `~ 4 modified`**（`damage-flow` ×1、`effect-step-dispatch` ×1、`effect-trigger` ×2）；归档后 `openspec validate --all --strict --no-interactive` = **24 passed / 0 failed，零 ERROR 零 WARNING**
- [x] 8.2 台账回写 —— ✅ `LEDGER-reflection` R-2 行改为"**部分闭环**"（寿命缺陷已修；脚本插槽仍待独立提案）+ 变更记录新增一条；`LEDGER-deferred` SCRIPT-8 的"仍未解决"项收窄（跨世界寿命已闭环；`ITcsEntityQuery` 转"已裁决"）+ 变更记录新增一条（**不新开条目，总数仍 37**）
- [x] 8.3 设计文档回写 —— ✅ `04-module-effects.md` §5b 新增《注册表的寿命语义》完整口径（含失效判据、拒绝门、撤销、`UPROPERTY` 互补性）；`09-module-damage.md` §2.2 只记落点并**显式指向 04 不重复定义**（避免"同一机制两处定义"）
- [x] 8.4 `DEC-04` 状态更新 —— ✅ 身份块改为"冻结（决策已拍板）；**实现状态 = 裁定 ⑤ 第一批已落地**"；`INDEX.md` §4.2 与 `GLOSSARY.md` §5.1 同步
- [x] 8.5 证据固化 —— ✅ 新建 `evidence/2026-09-29-registry-lifetime-pie.md`（`EVID-2026-09-29-registry-lifetime`：含字节数 + SHA-256 + 逐行锚点 + 生效机制诊断 + 五条边界声明）；登记进 `INDEX` §4.6、`GLOSSARY` §5.1、`docs-convention` 篇数表（9→10）、`SPEC-TRACE`（新增 2 行 + 清理"下一轮验证前须重启 Editor 进程"的过期限制）

> **归档命令的两条非阻塞告警（记录在案）**：① `Why section should not exceed 1000 characters` —— 提案 Why 偏长，是有意为之（它承载缺陷的三条代码级事实）；② `30/35 tasks` —— 执行归档时本节尚未勾选，现已补齐为 **35/35**。

## 9. 实测记录（2026-09-29，Task 7.4）

**装置**：宿主既有 `ATcsHostScriptingE2EProbe`（C#，`Script/LegendAutoChessCS/TcsProbe/TcsHostScriptingE2EProbe.cs`）——它在 `BeginPlay()` 里调 `RegisterStepExecutor` 登记 `FTcsProbeEffectStep` / `FTcsProbeFlowStep` 两个类型；**登记失败会提前 `return` 并打 `FAIL：… 登记失败`**，故"是否走到后续检查"即是登记成败的判据。

**步骤**：同一 Editor 进程内两次 PIE（不重启）——第一次 PIE 结束时世界销毁，随后 `Repeating last play command` 紧接起第二次 PIE。

**日志锚点**（`Saved/Logs/LegendAutoChess.log`）：

| 行号 | 内容 |
|---|---|
| L2414 / L2416 | 第 1 次 PIE：`步骤类型 TcsProbeEffectStep 已登记宿主执行器` / `流程步骤类型 TcsProbeFlowStep 已登记宿主执行器` |
| L2568 | 第 1 次 PIE 结束：`Deinitialize: 持有执行器 1 个，注册表动态条目 1 个` |
| L2648 / L2649 | **第 2 次 PIE：同两个类型再次登记成功** |
| L2793 | 第 2 次 PIE 结束：`Deinitialize: 持有执行器 1 个，注册表动态条目 1 个` |

**判据与结论**：

| 判据 | 结果 |
|---|---|
| 缺陷签名（`拒绝重复登记` / `已有执行器` / `保留首个` / `登记失败`） | **全日志零命中** |
| 第二次 PIE 是否走到 `RegisterStepExecutor` 之后 | ✅ 走到（L2647 实体准备完成 → L2726 第一批验证完成 → L2746 `GC_READY`） |
| **失效机制由哪一半生效** | **显式撤销（`Deinitialize` 按世界 `Unregister`）** —— 诊断期实测 `注册表动态条目 1 个`（非 0），说明条目确实驻留在进程级注册表里，被 `Deinitialize` 清除后第二次 PIE 见到的已是干净状态 |
| `DiscardIfStale`（替换失效条目） | **未被触发**（"替换为新登记"日志零命中）——符合其设计定位：**正确性不依赖它**，它是显式撤销失效时的保险。**仍未取得行为证据** |

> ⚠ **本批未覆盖的边界**：`Find` 侧的**跨世界/已回收判定**与 `DiscardIfStale` 的**替换路径**在此次实测中均未经过（因为显式撤销总是先行清干净）。若要单独验它们，需构造"条目未被撤销但对象已回收"的场景（例如宿主不经过子系统销毁就重登记）。**按 2026-09-27 用户裁定"先实测但只做记录"，此处只记录，不因结果改变设计。**

> **测试上下文说明（不影响判据）**：第 1 次 PIE 按装置提示执行了 `obj gc`；第 2 次因外设故障未执行。对本批判据**无影响**——判据只依赖"登记被接受/拒绝"与"`Deinitialize` 清理"，与 GC 无关；且第 1 次已执行 `obj gc`（更严的方向：把执行器逼到只剩弱引用），第二次仍登记成功。第二次 PIE 未跑 GC 意味着探针自身的 GC 保活检查未复验（那是 SCRIPT-8 的验收面，非本批）。

