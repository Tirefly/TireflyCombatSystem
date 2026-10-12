# Tasks：移除 `ECT_OnEvent` 消耗档

## 1. 插件侧代码

- [x] 1.1 `Def/TcsCostTypes.h`：删 `ECT_OnEvent` 枚举值（`ECT_Manual` 3 → 2）、删 `TriggerEventTag` 字段、
      就地留下裁定注记（判据 + 伴随面清单）
- [x] 1.2 删 `Skill/TcsCostEventRelay.h` + `Private/Skill/TcsCostEventRelay.cpp` 两个文件
- [x] 1.3 `TcsSkillSubsystem.h`：删 `#include`、`GetCostEventRelay()` 声明、`CostEventRelay` 成员
- [x] 1.4 `TcsSkillSubsystem.cpp`：删 `GetCostEventRelay()` 实现、`Deinitialize` 的 `ResetAll` 块
- [x] 1.5 `Skill/TcsCastOps.cpp`：删激活路径的 `ECT_OnEvent` 登记块（含 `bTagValid` 与回退提交）
- [x] 1.6 `Skill/TcsSkillOps.cpp`：删三处撤销路径（`Revoke` / `RevokeBySource` / `UnregisterUnit`）的中继退订
- [x] 1.7 `Skill/TcsCastOps.h` / `TcsCastOps.cpp`：改注释面的档位计数（四档 → 三档）

## 2. 宿主装置（`Source/TcsDev`）

- [x] 2.1 删 `H1` / `H2` 两个检查（原测"激活时不扣、事件到达才扣"），留注记说明归属已转宿主
      （该玩法由 `G2` 宿主手动 `PayCost` 覆盖）
- [x] 2.2 删 `GR65OnEventDef` / `GR65TriggerEvent` 常量与 `OnEventEntry`
- [x] 2.3 `MakeR65Def` 去掉 `TriggerTag` 形参
- [x] 2.4 `A1` 检查词从 4 项降为 3 项（`TcsEvent.Check.R65Trigger` 退役）
- [x] 2.5 命令描述文本与头注释去掉 `OnEvent 档按事件提交`

## 3. 配置

- [x] 3.1 `Config/DefaultGameplayTags.ini`：退役 `TcsEvent.Check.R65Trigger` 与
      `SkillDef.Check.R65OnEvent` 两条检查用 tag

## 4. 规格与文档

- [x] 4.1 提案 + delta（本 change）：`skill-cost` 三条需求（1 RENAMED + 2 MODIFIED）
- [x] 4.2 `SPEC-04-skill` §11 实施注记：**保留 `D5-10` 原文** + 追加本修订（就地注记口径）
- [x] 4.3 `INDEX` / `GLOSSARY` / `log-03-skill-m5` / `plan-r6.5` 的档位计数与描述同步
- [x] 4.4 台账：核查是否产生"指不出归属"的待办（预期 = 否，属 Task 5 范围内的收窄）

## 5. 验证

- [x] 5.1 `openspec validate remove-cost-onevent-timing --strict --no-interactive` 通过
- [x] 5.2 全库零残留：`ECT_OnEvent` / `TriggerEventTag` / `CostEventRelay` / `R65OnEvent` / `R65Trigger`
      在 `Source/` 与 `openspec/specs/` 里**零命中**（文档里作为"已删除"的注记除外）
- [x] 5.3 双配置全量编译 `Result: Succeeded` 且 **0 warning / 0 error**
- [x] 5.4 编辑器 PIE 重跑：`Tcs.Test.CooldownCost.Run` / `.Run` / `.Reject`
      ⇒ 期望 `19/0 + 22/0 + 7/0`（**分母因删掉 H1/H2 各减 2**）
- [x] 5.5 无头路线重跑并确认与 PIE **同版本夹具**（本次基线 DLL 已写回，两条路线不再分叉）
- [x] 5.6 R6 回归 `Tcs.Test.SkillLayer.Run` 24/0 且 `[复现性摘要]` 逐字不变
- [x] 5.7 `openspec validate --all --strict` 全绿（能力数不变 = 42，本 change 是 MODIFIED 不是新增能力）

## 6. 归档

- [x] 6.1 `openspec archive remove-cost-onevent-timing --yes`
- [x] 6.2 归档后复算：`validate --all --strict`、能力数、场景数**逐能力零丢失**
