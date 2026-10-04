# 参数求值上下文补齐与来源发号器统一：零行为变更回归证据

- **文档 ID**：`EVID-2026-10-04-param-context-and-source-issuer`
- **类型**：EVID / 证据
- **状态**：FROZEN
- **权威范围**：R5 Task 0（`PLN-R5`）的验收面——三处前置契约落地后**既有行为不变**（`Tcs.Test.Slice.Run` 19/0 零红字 + 延迟判定 7b），以及台账 `WAIT-6` 修复的**行为级实证**（来源发号器进程唯一 ⇒ 级联摘除不再误摘他人来源）
- **最后更新**：2026-10-04
- **被测对象**：`Source/TcsCore` 的 `FTcsParamEvaluateContext`（新增 `Subject` / `EffectiveLevel`）、`FTcsParamEnumerableSource`（新增可选能力基类）、`FTcsSourceHandleRegistry`（发号器改出线 + 导出）；宿主 `Source/TcsDev/Private/Dev/TcsDevSliceRig.cpp`（撤"直写高位 Id"规避）
- **验证范围**：TCS 插件 + LAC 宿主装置。**本轮代码改动 4 文件**（插件 3：2 改 1 新；宿主 1；另有 TcsCore 新增 `.cpp` 1 个）
- **完整来源**：`E:/Projects_Dev/LegendAutoChess/Saved/Logs/LegendAutoChess.log`
  - 行号锚点以"命令所在行 = `2413`"为基准；本证据的区段 = **L2413–L2698（286 行）**
  - **区段 SHA-256 `ccf10b19f2f9dba64ed267ce60d995b7ff732b254124bf66382c6a5ea044108b`**
  - **哈希口径（与 R4 的 `EVID-2026-10-04-modifier-channel` 不同，如实标注）**：取证时编辑器**正在运行**且日志持续增长 ⇒ **不记整文件哈希**（不可复现）。上列哈希 = 对该**区段文本**（L2413–L2698 按行以 `\n` 连接、UTF-8 编码）计算，范围写明故可复算。**日志时间戳为 UTC，本地时间 = UTC + 8 小时**（本次命令 `07.14.38` = 本地 `15:14:38`）

## 0. 夹具与前置

**内容侧零新增**——沿用 R4 Task 4 的竖切夹具：链资产 **9 个**、触发定义 **2 条**、属性定义 **4 个**（`Health` / `MaxHealth` / `Attack` / `Armor`），装置自造两个单位（`Actor_0` / `Actor_1`）。

**编译前置（本证据的另一半）**：`-projectfiles` → `LegendAutoChessEditor Win64 Development` → `LegendAutoChess Win64 Shipping` 三命令退出码 **0 / 0 / 0**（`L` 无；日志 `Saved_UBT/r5-task0-build.log`），全日志 `warning|error` **零命中**。
**反射面生效证据（静态，非运行期）**：UHT 产物 `TcsParamValueSource.gen.cpp` 含 `NewProp_Subject`（`FStructPropertyParams`，struct getter = `Z_Construct_UScriptStruct_FTcsCombatEntityHandle`，偏移 `STRUCT_OFFSET(FTcsParamEvaluateContext, Subject)`）与 `NewProp_EffectiveLevel`（`FIntPropertyParams`）；`TcsParamEnumerableSource.generated.h` / `.gen.cpp` 均已产出。
**执行方式**：用户在编辑器内起 PIE 后敲命令——本命令只在 PIE/Game 世界成立 ⇒ **人工验证点**（装置不提供无人值守入口）。

## 1. 即时判定与汇总

| 行号 | 读数 | 判定 |
|---:|---|---|
| 2413 | `Cmd: Tcs.Test.Slice.Run` | 命令入口（受测链 = `EffectChain.SliceChain`，L2416） |
| 2417 | `[PASS] 检查 0：定义库就绪且失败清单为空（IsRuntimeReady=true；失败 0 条）` | 定义库就绪 |
| 2418 / 2419 | `[PASS] 检查 1a：扫到链资产 9 个` / `检查 1b：定义库自动缓存链 9/9` | 发现层未受影响 |
| 2420 / 2421 | `[PASS] 检查 2：链已装配进本世界 9/9` / `检查 3：链步骤类型全可解析（22/22 步；不可解析 0）` | 装配与步骤解析未受影响 |
| 2422 | `[PASS] 检查 4：属性定义已装载 4/4` | 属性面未受影响 |
| 2652 | `===== 即时判定汇总：通过 19 / 失败 0（检查 7b 见延迟判定）=====` | **★ 即时判定 19/0** |
| 2669 / 2671 | `—— 检查 7b（+2.0s 战斗时间后的延迟判定）——` / `[PASS] 检查 7b：WaitDelay 到期后续走，Damage 步扣血生效` | **★ 延迟判定通过**（挂起-恢复链路未受影响） |

**全区间 `[PASS]` 行 20 / `[FAIL]` 行 0**（19 项即时 + 7b 延迟），与 R4 收束时的计数一致。

## 2. 关键读点：零行为变更 + `WAIT-6` 的行为级实证

| 行号 | 读数 | 为什么是判据 |
|---:|---|---|
| 2591 | `[PASS] 检查 14：按来源级联摘除（摘掉 1 条；实际扣 15.0；黑板折叠值 15.0；期望 15.0）` | **★ 本批最重要的行为证据**：检查 14 走的是 `UnregisterTriggerRowsBySource(装置来源)`。**修复前**装置只能直写高位 Id 才能保证"恰好摘掉自己那 1 条"；**现在**装置走正常发号（`GTcsDevRigSourceRegistry.Allocate()`），与定义库来源同处一个 Id 空间，**返回值仍恰为 1** ⇒ 进程唯一发号在真实调用面上生效、无跨来源误摘 |
| 2498 | `[PASS] 检查 10：**资产路径**生效（点灯后减半）（实际扣 15.0 …）` | 资产路径未受影响（`GateTags` 点灯语义不变） |
| 2522 / 2544 / 2568 | 检查 11（挂 C++ 行 ⇒ 两笔 0.5 叠加，扣 7.5）/ 检查 12（摘行还原 ⇒ 回 15.0）/ 检查 13（重挂生效 ⇒ 7.5） | 触发行的三段生命周期未受影响（`Instance.Source` 换成正常发号句柄后语义不变） |
| 2627 | `[PASS] 检查 15：载荷读取器（AfterDamage 行 ⇒ 施法者被扣 7.0；期望 7.0 = 载荷解出的 Caster 即本次流程 Attacker）` | 载荷读取器未受影响（本批零改动该面，属回归） |
| 2650 | `[PASS] 检查 16：反向对照（摘掉载荷行 1 条 ⇒ 施法者扣 0.0；目标 15.0 = 资产行未受牵连）` | **另一条来源隔离证据**：摘装置载荷行不牵连资产行 |
| 2477 | `[PASS] 检查 9：对照组（资产行未点灯，门③不通过）（实际扣 30.0 …）` | 门③对照组未受影响 |
| 2651 | `[PASS] 检查 17：触发定义资产作者侧门（IsDataValid == Valid 2/2 个；累计错误 0 条）` | 作者侧校验面未受影响（资产仍 `Valid`，非 `NotValidated`） |

## 3. 红字扫描（零红字 = 常规验收命令的验收信号）

区间 **L2413–L2698** 内 `Error:` / `Warning:` / `Ensure condition` / `Fatal error` / `[FAIL]` **零命中**。命令自带声明（L2414-L2415）：本命令"零故意 ensure / 零 Error"，拒绝面检查在 `Tcs.Test.Slice.Reject`——本批未跑该命令（本批零拒绝面改动）。

## 4. 本证据证明什么 / 不证明什么（MUST NOT 外推）

- **证明**：① 三处前置契约落地后**既有行为不变**（19/0 + 7b，零红字，逐项检查均 PASS）；② `WAIT-6` 的修复在**真实调用面**成立——装置来源与定义库来源共存时级联摘除恰摘 1 条；③ 反射面两字段与可枚举基类确经 UHT 生成（静态证据，见 §0）。
- **不证明（五条边界）**：① **只跑单轮**——未做"两轮关键行逐字一致"（R4 Task 4 的口径更强，本批零行为变更故未重复）；② 未验 `FTcsParamEnumerableSource` 的**任何派生源**（基类当前零消费者，`GetIndexForLevel` 的中性默认未被真实源覆写后调用）；③ 未验 `Subject` / `EffectiveLevel` 被**任何真实源读取**（首个消费者是 Task 3 的等级源；本批只保证字段可反射、可读写）；④ `FTcsCombatEntityHandleRegistry`（`WAIT-6` 的同族第二实例）**未动、未验**——今天无活体碰撞；⑤ 区段哈希基于**活动日志**，复算时 MUST 按同一行区间取文本。
