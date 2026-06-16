## 1. 提案与边界

- [x] 1.1 固定本次 change 的目标文件清单、例外项与非目标，避免把 Skill 或行为变更混入同一 change。
- [x] 1.2 确认“500 行仅作为审查阈值，不作为机械切分规则”写入 proposal 与 design。
- [x] 1.3 固定 Buff 直接拆分、Attribute/State 先重排头文件、StateManagerSubsystem 只重整头文件的执行顺序。

## 2. 头文件结构先行整理

- [x] 2.1 重整 `Source/TireflyCombatSystem/Public/Attribute/TcsAttributeComponent.h` 的 region、空白分隔与成员注释，使其成为后续 `.cpp` 拆分依据。
- [x] 2.2 重整 `Source/TireflyCombatSystem/Public/State/TcsStateManagerSubsystem.h` 的 region、空白分隔与成员注释，不拆分对应 `.cpp`。
- [x] 2.3 重整 `Source/TireflyCombatSystem/Public/State/TcsStateComponent.h` 的 region、空白分隔与成员注释，使其成为后续 `.cpp` 拆分依据。
- [x] 2.4 对纳入本次范围的目标头文件统一执行“全局声明 / 委托声明 / 前置声明三段块之间三行空白、region 之间三行空白”的布局规则。

## 3. Buff 模块拆分

- [x] 3.1 重整 `Source/TireflyCombatSystem/Public/Buff/TcsBuffComponent.h` 的 region、空白分隔与成员注释。
- [x] 3.2 将 `Source/TireflyCombatSystem/Private/Buff/TcsBuffComponent.cpp` 按稳定职责拆分为多个实现文件。
- [x] 3.3 保持 Buff merge 主链在单一职责切片内，不做机械碎片化拆分。
- [x] 3.4 完成一次 editor-target 编译验证。

## 4. StateInstance 模块拆分

- [x] 4.1 将 `Source/TireflyCombatSystem/Private/State/TcsStateInstance.cpp` 按 Initialization / Parameters / StateTree 等稳定职责拆分为多个实现文件。
- [x] 4.2 为拆分后的实现文件补齐必要的流程说明注释。
- [x] 4.3 完成一次 editor-target 编译验证。

## 5. Attribute 模块拆分

- [x] 5.1 基于新的 `TcsAttributeComponent.h` region 结构，重新划定 `TcsAttributeComponent.cpp` 的职责边界。
- [x] 5.2 将 `Source/TireflyCombatSystem/Private/Attribute/TcsAttributeComponent.cpp` 按新的职责边界拆分为多个实现文件。
- [x] 5.3 保持 `RecalculateAttributeBaseValues` / `RecalculateAttributeCurrentValues` / `EnforceAttributeRangeConstraintsInternal` 所在的高耦合计算链路局部完整。
- [x] 5.4 完成一次 editor-target 编译验证。

## 6. State 模块拆分

- [x] 6.1 基于新的 `TcsStateComponent.h` region 结构，重新划定 `TcsStateComponent.cpp` 的职责边界。
- [x] 6.2 将 `Source/TireflyCombatSystem/Private/State/TcsStateComponent.cpp` 按 Apply / Removal / SlotActivation / QueryDebug / StateTreeIntegration 等稳定职责拆分为多个实现文件。
- [x] 6.3 保持槽位激活主链与移除主链的局部完整，不做跨职责的机械切割。
- [x] 6.4 完成一次 editor-target 编译验证。

## 7. 最终验证

- [x] 7.1 运行 `openspec validate refactor-runtime-module-file-layout --strict --no-interactive`。
- [x] 7.2 在全部文件重组完成后，再次执行 editor-target 编译验证。
- [x] 7.3 手动编辑器回归（已跳过，纯代码重组不改变行为）