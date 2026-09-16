<!-- OPENSPEC:START -->
# OpenSpec Instructions

These instructions are for AI assistants working in this project.

Always open `@/openspec/AGENTS.md` when the request:
- Mentions planning or proposals (words like proposal, spec, change, plan)
- Introduces new capabilities, breaking changes, architecture shifts, or big performance/security work
- Sounds ambiguous and you need the authoritative spec before coding

Use `@/openspec/AGENTS.md` to learn:
- How to create and apply change proposals
- Spec format and conventions
- Project structure and guidelines

Keep this managed block so 'openspec update' can refresh the instructions.

<!-- OPENSPEC:END -->

## TAH 引用说明（本仓库唯一）

- **TAH = TireflyAgentHarness**：Tirefly 的个人 Agent Harness 主仓库，`~/.agents`（用户级 AGENTS.md、skills、memory）与其保持同步。全称依据用户级 `~/.agents/AGENTS.md`「跨电脑部署约定」中的主仓库名。
- 本节是本仓库内对 TAH 的**唯一引用说明**：其他文档、spec、change 提案提及 TAH 时引用本节，不再重复解释。
- TAH 行为规则的唯一定义在用户级 `~/.agents/AGENTS.md`「个人 Harness 工作流」一节，本节只作引用不复制规则正文。要点：
  - **回答后 TAH 判定**：每次最终回答前按触发清单快速自检（`harness-retro` 触发条件 + 检查点纪律）；命中即必须以 `ask_user_question` 问答框主动提议 `harness-retro`，不得纯文本提议（MEM-20260902-03）；未命中静默跳过。
  - 记忆卡只能经 `harness-retro` 门控流程写入 `~/.agents/memory/`，绝不自动写卡、绝不删除，以 supersede 取代。
  - 实质性任务开始时先调用 `harness-router`。