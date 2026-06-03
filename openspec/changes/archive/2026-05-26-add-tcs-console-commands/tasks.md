## 1. 设计与命名面
- [x] 1.1 固定 `TcsConsoleCommands` 的文件位置、职责边界和命名规则
- [x] 1.2 固定第一阶段最小命令集、参数格式、帮助文本策略和后续调试 UI 边界

## 2. 控制台命令共享头文件
- [x] 2.1 新增 `Source/TireflyCombatSystem/Public/TcsConsoleCommands.h`
- [x] 2.2 在共享头文件中定义 TCS 命令前缀、完整命令名、参数键和帮助文本

## 3. 第一阶段命令注册与调试路径收敛
- [x] 3.1 在 TCS 私有实现侧注册第一阶段最小 `State` 调试开关命令
- [x] 3.2 将 `GetSlotDebugSnapshot()` / `GetStateDebugSnapshot()` 的高成本调用点收敛到显式开关路径，并删除第一阶段 `dump / list` 查询命令
- [x] 3.3 首先处理 `TcsSTEvaluator_SlotDebug` 的常驻快照构造路径

## 4. 验证与文档
- [x] 4.1 重新编译 `TireflyGameplayUtilsEditor Win64 Development`
- [x] 4.2 回写基线文档 6.6 的当前状态与边界
