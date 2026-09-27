## 1. 建立基线与依赖门

- [x] 1.1 记录 `.uplugin`、`Source/` 模块目录、`openspec/project.md`、当前生效规格和 `add-host-scripting-slots` 的基线快照。
- [x] 1.2 确认 `add-host-scripting-slots` 已完成 6.1–6.4 与 7.1–7.5；前置提案已于 2026-09-27 归档为 `2026-09-27-add-host-scripting-slots`。

## 2. 收束插件元数据与项目上下文

- [x] 2.1 更新 `openspec/specs/plugin-descriptor/spec.md`，将当前 Runtime 模块规范为 `TcsCore`、`TcsNotation`、`TcsAttribute`、`TcsEffect`、`TcsTargeting`、`TcsDamage`、`TcsIntegration`，并说明十一模块是目标架构而非当前描述。
- [x] 2.2 更新 `openspec/project.md` 的 current modules、plugin dependencies 和 Def/identifier 段落，消除旧 `FName` 与当前 `FGameplayTag` 口径并存。
- [x] 2.3 检查设计文档中仍把七模块写成三模块或把当前缺失模块写成已实现的段落，逐处标注历史计划或当前状态。

## 3. 回写 S-8 生效规格

- [x] 3.1 应用 `effect-chain` delta：`FTcsChainRunHandle` 使用展平字段并为 `USTRUCT(BlueprintType)`；删除与代码冲突的 `MUST NOT 加 BlueprintType`。
- [x] 3.2 应用 `effect-interpreter` delta：公开门面、六个按句柄访问器和悬空句柄语义与代码一致；明确 `SetEntityQuery/GetEntityQuery` 仍为 C++ 专用，不列为脚本已达能力。
- [x] 3.3 应用 `damage-primitive` delta：五个委托方法使用 `FTcsDamageFlowContextView`、`BlueprintNativeEvent` 和 `Execute_*` 调用路径。
- [x] 3.4 应用 `targeting-strategy` delta：补充宿主选择器/过滤器插槽、空 Host 降级和内置 C++ 快路径双轨语义。
- [x] 3.5 与 `add-host-scripting-slots` 的 delta 逐项比对，消除重复、冲突或遗漏；重叠 requirement 已统一为同一完整正文与现行标题，`damage-flow` / `effect-step-dispatch` 的独有 delta 保留在前置提案，不复制其未完成的行为声明。

## 4. 同步设计文档与遗留台账

- [x] 4.1 将 `04-module-effects.md`、`09-module-damage.md`、`10-module-targeting.md` 和 C# 调研中的 S-8 状态改为证据分级表述。
- [x] 4.2 更新 `deferred-inputs-ledger.md`：S-8、S-2、S-4、S-5 分别记录已实现、已替代、仍未验证和仍为 C++ 专用的边界。
- [x] 4.3 为未完成 PIE/GC/跨语言闭环保留明确的“待验证”记录，不使用“已落地”覆盖未完成证据。

## 5. 建立追踪与验证门槛

- [x] 5.1 建立追踪矩阵：每个 affected requirement 映射到代码路径、设计章节、提案任务和验证证据。
- [x] 5.2 运行 `openspec validate reconcile-tcs-contract-drift --strict --no-interactive`。
- [x] 5.3 在所有规格和 delta 完成后运行 `openspec validate --all --strict --no-interactive`。
- [x] 5.4 代码、设计、规格、台账和验证矩阵已对齐；`add-host-scripting-slots` 已于 2026-09-27 归档，用户随后完成 L_UnrealSharpDev PIE 复核；归档前 delta dry-run 与全量严格验证通过。
