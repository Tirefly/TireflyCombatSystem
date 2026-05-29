# 变更：重组 TCS Runtime 模块文件布局

## 背景

当前 TCS 的 Attribute、State、Buff 三个运行时模块里，已经出现多份超过 500 行的 `.cpp` / `.h` 文件。问题本身不只是“文件太长”，而是这些超长文件混合了承担不同职责的实现与声明面，导致后续维护时很难快速定位边界，也使结构性重构越来越高风险。

这次变更的目标不是引入新玩法逻辑，也不是顺手重写运行时行为，而是在**不改变公开契约与行为语义**的前提下，把超长文件按稳定职责重新组织，并统一目标头文件的声明风格、region 结构与注释密度。

## 变更内容

- 仅处理 `Attribute`、`State`、`Buff` 三个模块中当前超长或边界失衡的目标文件。
- 对 `BuffComponent` 与 `StateInstance` 直接按职责拆分 `.cpp` 文件。
- 对 `AttributeComponent` 与 `StateComponent` 先重整头文件的 region 与声明布局，再根据新的职责边界决定 `.cpp` 切分方式。
- 对 `StateManagerSubsystem` 保持单 `.cpp` 文件实现，不做物理拆分；仅重整头文件的 region 与声明布局。
- 对纳入本次变更的目标头文件统一执行以下结构规则：
  - 全局声明、委托声明、前置声明三类块之间固定使用三行空白分隔
  - 类内成员按 `#pragma region` 归类，相邻 region 之间固定使用三行空白分隔
  - 所有成员函数声明与成员变量声明补齐注释
  - 对实现相对复杂的函数，在 `.cpp` 中补充说明执行流程的关键注释

## 非目标

- 不修改 Attribute / State / Buff 的运行时行为语义。
- 不在这次变更里推进 Skill 逻辑或 Skill 模块结构整理。
- 不为了拆文件而新增新的 `UObject` / `Subsystem` / `Component` 层级。
- 不把简单 getter / setter / 单行早返回 / 明显局部赋值强行补成高噪声注释。

## 范围与例外

- 当前首批目标文件包括：
  - `Source/TireflyCombatSystem/Private/State/TcsStateComponent.cpp`
  - `Source/TireflyCombatSystem/Private/Attribute/TcsAttributeComponent.cpp`
  - `Source/TireflyCombatSystem/Private/Buff/TcsBuffComponent.cpp`
  - `Source/TireflyCombatSystem/Private/State/TcsStateInstance.cpp`
  - `Source/TireflyCombatSystem/Public/State/TcsStateComponent.h`
  - `Source/TireflyCombatSystem/Private/State/TcsStateManagerSubsystem.cpp`
  - `Source/TireflyCombatSystem/Public/Buff/TcsBuffComponent.h`
- `Source/TireflyCombatSystem/Public/Attribute/TcsAttributeComponent.h` 虽然当前未超过 500 行，但由于现有 region 划分已经不适合作为 `.cpp` 拆分依据，因此纳入本次头文件结构重组范围。
- `Source/TireflyCombatSystem/Private/State/TcsStateManagerSubsystem.cpp` 虽然超过 500 行，但当前职责仍然收敛在定义加载 / 查询 / 跨 Actor apply 门面，不在本次变更中做 `.cpp` 物理拆分。

## 影响范围

- 受影响规范：
  - `runtime-module-file-layout`
- 受影响代码：
  - `Source/TireflyCombatSystem/Public/Attribute/TcsAttributeComponent.h`
  - `Source/TireflyCombatSystem/Private/Attribute/TcsAttributeComponent.cpp`
  - `Source/TireflyCombatSystem/Public/Buff/TcsBuffComponent.h`
  - `Source/TireflyCombatSystem/Private/Buff/TcsBuffComponent.cpp`
  - `Source/TireflyCombatSystem/Public/State/TcsStateComponent.h`
  - `Source/TireflyCombatSystem/Private/State/TcsStateComponent.cpp`
  - `Source/TireflyCombatSystem/Public/State/TcsStateManagerSubsystem.h`
  - `Source/TireflyCombatSystem/Private/State/TcsStateManagerSubsystem.cpp`
  - `Source/TireflyCombatSystem/Private/State/TcsStateInstance.cpp`