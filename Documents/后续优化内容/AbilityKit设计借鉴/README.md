# AbilityKit 设计借鉴分析

本目录整理自对 GitHub 项目 [HOBOBO/AbilityKit](https://github.com/HOBOBO/AbilityKit) 的深入解读，目的是把其中可参考的优秀设计思想与架构实践提炼为对 TireflyCombatSystem（TCS）插件的借鉴建议。

## 性质声明

- 本目录下的文档是**设计参考与思想借鉴分析**，并非 OpenSpec 变更提案，也不直接对应任何代码改动。
- 文档中的 AbilityKit 证据均来自其公开源码与 `Docs/` 目录（克隆于 `C:\Users\TIREFL~1\AppData\Local\Temp\opencode\AbilityKit`，commit 为 master 分支最新提交）。
- 文档中的 TCS 现状均基于当前 `openspec/specs`、活动 `changes`、`Source` 代码与 `project.md` 交叉验证。
- 任何真正落地实现都需要走 TCS 的 OpenSpec 提案流程（在 `Plugins/TireflyCombatSystem/openspec/changes/` 下创建 proposal，经 `openspec validate --strict --no-interactive` 校验并评审后再实现）。
- 借鉴建议均标注了"是否兼容 TCS 现有定位"与"风险等级"，避免盲目照搬。

## 背景：AbilityKit 与 TCS 的哲学差异

| 维度 | AbilityKit | TCS |
|------|-----------|-----|
| 核心哲学 | 极致统一：一切皆 Effect，配置即类型 | 分层共享：State/Buff/Skill 继承共享基类但保留类型化字段 |
| 运行环境 | 纯 C#，逻辑可脱离引擎（服务器/测试） | 深度绑定 UE UObjects/Components/StateTree |
| 技能流程 | `pipeline` Phase 图（显式组合） | UE StateTree 状态机驱动 |
| 事件规则 | 独立 `triggering` 引擎（事件→条件→Action） | State 生命周期内的零散通知 |
| 状态管理 | `hfsm` 分层状态机（独立模块） | UE StateTree（引擎状态树）+ 八步移除时序 |
| 溯源 | `trace` 树状血缘 + explain | `FTcsSourceHandle` 线性 Id+CausalityChain |
| 网络 | 原生完整（FrameSync/StateSync/Prediction/Rollback） | 仅 `runtime-network-identity` 设计约束，无实现 |
| 模块边界 | 50+ UPM 包，极度细分 | 单插件两模块（Runtime/Editor） |

**结论**：两者哲学几乎相反、不可直接移植。AbilityKit 追求最大解耦与统一，TCS 追求深度 UE 生态集成与类型安全。借鉴应聚焦"思想与模型"，而非"代码与模块边界"。

## 借鉴模块清单

按优先级与风险排序：

| 文档 | 借鉴点 | 优先级 | 风险 |
|------|--------|--------|------|
| `01-四维设计语言WHO-WHEN-WHAT-HOW.md` | WHO/WHEN/WHAT/HOW 设计元语言 | P1 | 极低 |
| `02-持续效果统一思维.md` | "一切皆为持续效果"的统一抽象 | P1 | 低 |
| `03-Triggering事件规则引擎.md` | 事件→条件→Action 规则执行层 | P2 | 中高 |
| `04-多源数值引用NumericValueRef.md` | Const/Blackboard/PayloadField/Var/Expr | P2 | 中 |
| `05-溯源树Trace与explain.md` | 树状血缘 + 可解释输出 | P1 | 低 |
| `06-强类型ActionSchema与ActionCallPlan.md` | 配置化动作落到强类型运行时代码 | P2 | 中 |
| `07-Pipeline-Phase图概念.md` | Phase 显式组合（Sequence/Parallel/Conditional） | P3 | 高 |
| `08-原生网络分层蓝本.md` | FrameSync/StateSync/Snapshot/Prediction 分层 | P3 | 待时机 |

## 优先级定义

- **P1（立即借鉴）**：设计习惯或低侵入增量，不破坏 TCS 现有架构，可直接作为设计语言或文档补充。
- **P2（提案后借鉴）**：架构性增量，需要 OpenSpec 提案评审，但与 TCS 定位兼容。
- **P3（时机成熟后借鉴）**：与 TCS 现有引擎/边界重叠或时机未到，作为未来蓝本参考。

## 阅读建议

- 先读 `01-四维设计语言WHO-WHEN-WHAT-HOW.md` 建立设计语言共识。
- 再按优先级与 TCS 当前成熟度选择：Skill 系统正在推进时优先读 `03`/`04`/`06`；网络启动时优先读 `08`。
- 每篇文档均独立可读，包含"AbilityKit 设计思想"、"与 TCS 现状对比"、"借鉴建议"、"风险与前置条件"四节。

## 参考来源

- AbilityKit 仓库：https://github.com/HOBOBO/AbilityKit
- AbilityKit 关键文档：
  - `README.md`（总览）
  - `Docs/游戏技能系统本质抽象.md`（四维抽象）
  - `Docs/游戏效果系统统一抽象.md`（一切皆为效果）
  - `Docs/通用技能系统架构设计.md`（数据驱动设计）
  - `Docs/AbilityKit_vs_GAS_Comparison.md`（与 GAS 逐模块对比）
- TCS 规格基线：`Plugins/TireflyCombatSystem/openspec/project.md`、`openspec/specs/*`、活动 `openspec/changes/*`