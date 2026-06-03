# 变更：新增 TCS 最小控制台命令表面与常驻调试开关

> 归档说明：本目录是历史快照，不是当前事实的最高优先级来源。
> 若本文与 `openspec/specs/` 或活动 `openspec/changes/` 冲突，以当前 spec / 活动 change 为准。

## Why

当前 TCS 的部分调试字符串构造仍然容易留在常驻运行时路径里，尤其是 `GetSlotDebugSnapshot()`、`GetStateDebugSnapshot()` 这类体积较大的调试快照字符串。

基线文档 6.6 的目标已经很明确：这类高成本调试字符串应尽量限制在显式调试路径，而不是默认常驻执行。

同时，当前 TCS 模块内还没有统一的控制台命令命名表面。若直接在各个 `.cpp` 中散落原始命令字符串，后续命令名、参数格式、帮助文本与扩展方式都会继续漂移。

## What Changes

- 新增模块级共享头文件 `TcsConsoleCommands`，统一承载 TCS 控制台命令所需的命令字符串、参数约定和帮助文本。
- 为 TCS 的第一阶段控制台命令建立统一命名规则，但当前只保留“显式开关”这一类最小命令，不引入泛化的“命令管理器”对象。
- 把 `State` 调试快照构造从默认常驻路径中收回，避免 `StateTree` 调试求值器等常驻路径默认构造大字符串。
- 保持现有 `GetSlotDebugSnapshot()` / `GetStateDebugSnapshot()` 作为按需调试 API 存在，但不再为它们提供第一阶段控制台 `dump / list` 查询入口；后续若需要面向目标的查询，应交给调试 UI 面板承担。

## Impact

- 受影响规范：`debug-console-surface`
- 受影响代码：
  - `Source/TireflyCombatSystem/Public/TcsConsoleCommands.h`
  - `Source/TireflyCombatSystem/Private/TcsConsoleCommands.cpp`
  - `Source/TireflyCombatSystem/TireflyCombatSystemModule.cpp`
  - `Source/TireflyCombatSystem/Public/State/TcsStateComponent.h`
  - `Source/TireflyCombatSystem/Private/State/TcsStateComponent.cpp`
  - `Source/TireflyCombatSystem/Public/StateTree/Evaluator/TcsSTEvaluator_SlotDebug.h`
  - `Source/TireflyCombatSystem/Private/StateTree/Evaluator/TcsSTEvaluator_SlotDebug.cpp`
  - `Documents/后续优化内容/网络同步讨论/TCS网络同步审查（讨论基线-20260521）.md`
