## 1. 实施

- [ ] 1.1 `Public/Trigger/TcsTriggerRow.h`——`FTcsTriggerRow`（D4-1 十字段 + `Source` 簿记）+ `ETcsExecutionGate`
- [ ] 1.2 `Public/Trigger/TcsTriggerConditions.h`——两个条件 struct + `FTcsTriggerContext` + `EvaluateTriggerConditions` 声明
- [ ] 1.3 `Private/Trigger/TcsTriggerConditions.cpp`——求值实现（短路 + 未知类型 Warning）

## 2. 验证（禁 TDD 纪律：编译 + 定向人工检查）

- [ ] 2.1 UBT Development Editor 编译零警告零错误
- [ ] 2.2 依赖面自检：`grep "^#include"` 零领域模块（TcsEffect 只 include 自身 + TcsCore/TcsAttribute + 引擎）
- [ ] 2.3 命名冲突自检：`FTcsTriggerCondition_*` 与 TcsDamage 的 `FTcsCondition*` 不重名、不互相 include
- [ ] 2.4 `openspec validate add-effect-trigger-row --strict` 通过

## 3. 归档

- [ ] 3.1 `openspec archive add-effect-trigger-row --yes`（新能力 `effect-trigger` 并入规格库）
- [ ] 3.2 规格库自检 `openspec validate --specs --strict` 全绿（预期 23 条）

## 4. 未覆盖（如实记）

- **无运行期验收**：本批只落数据形状与纯函数求值，**无调用方**（登记表/求值器属 Task 2）。故 2.x 全部为静态检查（编译 + grep + validate），**没有任何行为实证**——这是"数据形状先行"的固有代价，行为面在 Task 2/4 验。
- **留位字段**：`EventPayloadFilter` / `Cues` / `InterruptPriority` / `ExecutionGate` 非默认值四项**只存不裁**；D4-5 剩余四项条件（`AttributeCompare` / `VariableCompare` / `GateCheck` / Custom）未实现——均入台账（plan3 Task 5 统一登记）。
