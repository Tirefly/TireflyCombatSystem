# 战斗系统重建设计工作区（历史前言 + 导航）

- **文档 ID**：`README`
- **类型**：GOV / 导航
- **状态**：ACTIVE
- **权威范围**：**只作导航与历史前言**——四种职能已于 2026-09-29 拆分完毕，本文不再承载任何"当前状态"信息
- **最后更新**：2026-09-29

> ⚠️ **本文件不再是工作区入口**（2026-09-29 起）。原 README 同时承担四种职能，已按文档规范拆分：
>
> | 你想要的 | 去哪 |
> |---|---|
> | 找文档、看当前进度、查"该读哪一篇" | [INDEX.md](INDEX.md) |
> | 查缩写/编号含义（`R5`/`D5-17`/`SCRIPT-8`…） | [GLOSSARY.md](GLOSSARY.md) |
> | 文档写作纪律（类型/状态词/引用格式） | [docs-convention.md](docs-convention.md) |
> | 逐条拍板流水（历史裁决 / 模块物化 / 拍板记录） | [LOG-DECISIONS](log/decisions-log.md) |
> | 实施与验收记录、检查点状态 | [LOG-IMPLEMENTATION](log/implementation-log.md) |
> | 哪些事还没做 | [LEDGER-deferred](ledger/deferred-inputs-ledger.md) |
>
> 下方前言是 **2026-09-02 的原始记录**，保留作历史对照，**不代表当前进度**（当前 = R4 轮进行中，见 [INDEX](INDEX.md) §1）。

---

- 阶段（**历史记录，2026-09-02**）：设计阶段收束（M0–M9 全拍板），进入 R3 实施规划——R3 构成 A' 六模块定稿（Core/Attribute/Effect/Targeting/Damage/Integration），writing-plans 产出实施计划
- 输入三调研：`tcs-legacy-research/`（TCS 代码级，原 tirefly-combat-system-research）、`ability-kit-research` 技能（AbilityKit）、本目录既有裁决文档
- 工作法：每模块一轮 = 决策点提案 → 用户逐项拍板 → `NN-module-<name>.md` 设计文档 → 检查点纪律
- 本文件是决策日志：拍板一项更新一项（状态 待拍板 → 已拍板：日期+决定+一句话依据）