# Design: TCS 契约漂移收束

## Context

本变更处理的是契约治理问题，不是新的运行时能力。当前工作树已经包含宿主脚本插槽的静态实现，但 `add-host-scripting-slots` 仍有 6/7 阶段未完成；同时部分生效规格保留了实施前的形状，`openspec/project.md` 也混用了历史基线与当前事实。

## Goals / Non-Goals

- Goals:
  - 让 `Source/`、`.uplugin`、设计文档和 `openspec/specs/` 对当前事实使用同一套术语。
  - 保留“目标架构 / 当前实现 / 未验证能力”的边界。
  - 给 `add-host-scripting-slots` 提供可审查的完成与归档门槛。
- Non-Goals:
  - 不修改 C++、C#、资产或插件描述文件。
  - 不替代后续功能轮次的设计工作。

## Decisions

### D1：新建独立收束变更

采用 `reconcile-tcs-contract-drift`，不把元数据与证据收束塞回仍在实施的 `add-host-scripting-slots`。前者依赖后者完成，但两者的任务边界保持可审查。

### D2：规格以当前代码和已确认运行语义为事实来源

当前 `.uplugin` 的七个 Runtime 模块、代码中的 `BlueprintType` 句柄、`FTcsDamageFlowContextView`、六个运行态访问器以及 Targeting 宿主转发器作为实现事实。设计文档负责解释取舍，规格负责表达稳定行为，不把轮次计划混进规范要求。

### D3：脚本能力按边界写实

已落地的是 UObject/反射插槽、流程委托、步骤执行器和按句柄访问器。`ITcsEntityQuery` 仍含 `TFunctionRef`，因此“脚本遍历世界、取位置、判存活”仍是未完成能力，不能由 S-8 的完成推导出来。

### D4：证据分级

每个收束条目必须标注：

1. 静态实现证据：代码符号和路径；
2. 生成物证据：glue/UHT 产物（如适用）；
3. 行为证据：PIE、GC 或跨语言往返结果；
4. 未验证边界：明确写出剩余风险。

## Dependencies

- `add-host-scripting-slots` 必须先完成其 PIE 端到端任务和文档同步任务，再执行本变更的规格归档。
- 本变更不得修改 `add-host-scripting-slots` 的任务勾选状态，除非该提案已按自身审批流程完成。

## Risks / Trade-offs

- 生效规格从历史 R3 三模块基线改为当前七模块事实，可能影响依赖旧基线的后续文档；迁移时必须保留“历史基线”说明。
- S-8 静态代码已存在但行为证据不足，收束后仍可能暴露 glue 或 GC 边界问题；因此归档门槛不能只依赖 `openspec validate`。

## Migration Plan

1. 先完成并审阅 `add-host-scripting-slots` 的未完成任务。
2. 更新本变更列出的规格 delta、`openspec/project.md`、设计文档和台账。
3. 建立追踪矩阵并补齐实际验证证据。
4. 运行 `openspec validate --all --strict --no-interactive`。
5. 只有所有未验证项被明确处理后，才归档本变更。

## Open Questions

- `plugin-descriptor` 的“当前七模块”与“未来十一模块目标”是否最终拆成两个独立 requirement，留待规格实施时确认；本提案暂用一个 requirement + 明确 current/target 语义处理。
