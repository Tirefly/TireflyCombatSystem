# `ModifyFlow` 提交侧验收：单次 PIE 行为证据

- **文档 ID**：`EVID-2026-09-30-modifyflow-acceptance`
- **类型**：EVID / 证据
- **状态**：FROZEN
- **权威范围**：提案 `add-damage-modifyflow-primitive`（已归档 `2026-09-30-add-damage-modifyflow-primitive`）的人工验收面——链步骤 `FTcsStepModifyFlow` 的**跨模块注册可达**与**降级路径**
- **最后更新**：2026-09-30

- **被测对象**：`Source/TcsDamage` 的 `FTcsStepModifyFlow`（链原语 + 执行器 + `UE_DEFINE_EFFECT_STEP_EXECUTOR` 自注册）与 `FTcsDamageModifierConsumePolicy`（纯数据可反射化改造）
- **验证范围**：TCS 插件 + LAC 宿主内容资产。地图 `L_TcsDev_Slice`，夹具 = 内容链资产 `Content/TcsDev/DA_SliceChain`（`ChainId = Tcs.Chain.Slice_Chain`，原 3 步：`WaitDelay → SelectTargets(Self) → Damage`），**临时追加第 4 步 `TcsStepModifyFlow`** 后跑 `Tcs.Test.Slice.Run`
- **完整来源**：`E:/Projects_Dev/LegendAutoChess/Saved/Logs/LegendAutoChess.log`
  - 取证快照：**364698 字节 / 2743 行**；行号锚点均以该快照为准
  - **SHA-256 `36873ac4f34c649ebce56bc32f6b49e2fc83c76220b7086f5f05c53cc753efad`**
  - **哈希口径（与先例不同，本条更干净）**：取证时**编辑器进程已退出**（`Get-Process UnrealEditor*` 计数 = 0），日志不再增长 ⇒ 该哈希与上表逐行锚点**严格属于同一快照**，无先例那份"哈希与行号非同时刻"的口径问题。编辑器重启后同名日志会被覆盖。

## 判据与结果（行号 = 上述快照）

| 序号 | 验证点 | 完整日志行 | 判定 |
|---:|---|---:|---|
| 1 | 链资产带第 4 步 `ModifyFlow` 被保存（探测准备） | 2324 | — |
| 2 | 装置命令执行 | 2556 `Cmd: Tcs.Test.Slice.Run` | — |
| 3 | 检查 0：定义库就绪且失败清单为空 | 2560 | PASS |
| 4 | 检查 1a / 1b：链资产被 AssetRegistry 扫到并**自动**缓存 2/2 | 2561 / 2562 | PASS |
| 5 | 检查 2：链装配进本世界 2/2 | 2563 | PASS |
| 6 | **检查 3：链步骤类型全可解析（11/11 步；不可解析 0）** | 2564 | **PASS——步数由 10（3+7）→ 11 = 新增的 `ModifyFlow` 步；即新步骤类型存档后仍可解析（4.1 持久化的运行期实证）** |
| 7 | 检查 4 / 5 / 6：属性装配 4/4；`Attack=30.0` `Armor=5.0` `Health=100.0`；宿主自研流程 100.0 → 75.0 | 2565 / 2578 / 2585 | PASS |
| 8 | 检查 7a：内容链起链成功 | 2591 | PASS |
| 9 | 即时判定汇总 | 2592 | **通过 9 / 失败 0** |
| 10 | **执行器降级 Warning（核心判据）** | 2601 `LogTcsDamage: Warning: ModifyFlow[Tcs.Chain.Slice_Chain]: 链上下文无流程收集载荷（EventPayload 为空或类型不符）——本步按完成处理` | **★ 通过——该行由本模块执行器打出 ⇒ 执行器被分派到（4.2 注册可达的直接证据）；且恰一条** |
| 11 | `检查 7b`（+2.0s 战斗时间后延迟判定）：施法者 `Health 100.0 → 55.0`、实际扣 `45.0` = 链资产配置的 `DamageBase` | 2605（数值）/ 2606（判定） | PASS |
| 12 | 夹具复原：`ModifyFlow` 步被删除并保存 | 2625 | — |
| 13 | 全日志 Error / ensure | — | **0 命中**（`LogTcsDamage: Error` / `LogTcsEffect: Error` / `Ensure condition failed` / `Fatal error`） |

## 边界（MUST NOT 外推）

1. **只覆盖降级路径**：链上下文的 `EventPayload` 为空（`TcsDevSliceRig.cpp:479-483` 构造 `FTcsEffectContext` 时只填 `Caster`/`Instigator`/`Targets`），执行器在 `Submit` **之前**即返回 ⇒ **成功提交路径（向黑板落一笔修正、后续 `Damage` 读到修正值）从未被执行**。
2. **"扣血仍 45" 不构成"降级不干扰流程值"的证据**：该 `ModifyFlow` 步**追加在链尾**（序列为 2596 起流程 → 2599 伤害记录 → 2600 流程完成 → 2601 Warning），故扣血值与它无因果关系。该结论只由**代码路径**支撑（无载荷 ⇒ 提前返回）。
3. **载荷读取器行为未验证**：`Caster` 是否真解析出本次流程的 `Attacker`（任务 `4.4`）需触发行，本次未涉及。
4. **可配/可存由人工确认，无自动化证据**：链资产 picker 中可见 `TcsStepModifyFlow` 与四字段可配可存，由用户目视确认；本条只有 UHT 产物层面的旁证（字段旗标字与既有可持久化属性逐位相同、两配置产物 SHA256 一致，见 `implementation-log` 2026-09-30 条）。
5. 与第 1/3 条同前置的 `4.3`（端到端"破甲"）连同 `4.4` 与 `4.5` 的命令化，已按用户 2026-09-30 裁定**移入 R4 Task 4**（共同前置 = 触发行夹具，全仓使用点为零、Task 2.5 资产载体未实现 ⇒ 触发行只能 C++ 注册）。
