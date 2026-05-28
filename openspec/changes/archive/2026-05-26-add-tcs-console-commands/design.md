## 背景

这次变更的直接目标不是做一个新的“调试管理器”，而是为 TCS 增加一个足够稳定、足够薄的控制台命令表面，让高成本调试字符串的构造回到显式触发路径。

当前 `State` 调试快照已经有现成的数据生成接口：
- `UTcsStateComponent::GetSlotDebugSnapshot()`
- `UTcsStateComponent::GetStateDebugSnapshot()`

问题不在于“没有调试能力”，而在于“缺少统一命令入口与开关约束”，因此某些大字符串构造仍可能出现在常驻路径里，例如 `FTcsStateSlotDebugEvaluator::Tick()`。

## 目标 / 非目标

### 目标

- 新增统一的 `TcsConsoleCommands` 共享头文件，集中定义 TCS 模块自己的命令字符串、参数约定和帮助文本。
- 建立一套明确的 TCS 控制台命令命名规则，避免后续命令字符串散落在各个实现文件中。
- 为 `State` 调试字符串的常驻生成路径提供第一阶段显式开关，并把高成本快照构造从默认常驻路径中收回。
- 保留现有调试快照 API，但把“何时调用它们”切换为显式调试决策，而不是默认常驻行为。

### 非目标

- 不引入 UObject / Subsystem / Manager 形式的“命令管理器”。
- 不在这次变更里覆盖 Attribute / Buff / Skill 的全部调试命令，只先处理 `State` 相关的 6.6 核心问题。
- 不在这次变更里提供 `State` 快照的 `dump / list` 控制台查询入口；面向目标的查询留给后续调试 UI 面板。
- 不把所有调试输出统一成新的 UI 或屏幕覆盖层；这次只处理常驻路径的开关收敛。

## 决策

### 决策 1：共享头文件命名与位置

- 文件名固定为 `TcsConsoleCommands.h`
- 放置位置固定为 `Source/TireflyCombatSystem/Public/`，与 `TcsLogChannels.h`、`TcsSourceHandle.h` 同级

原因：
- 这是模块级通用命名入口，应该和其他模块级公共头文件放在同一层级
- `TcsConsoleCommands` 比 `TcsGenericConsoleCommands` 边界更清晰，不鼓励把任意“通用常量”继续塞进来

### 决策 2：`TcsConsoleCommands.h` 的职责边界

该文件只负责：
- 命令前缀与完整命令名
- 参数键名与参数约定字符串
- 帮助文本常量

该文件不负责：
- 注册命令
- 保存运行时开关状态
- 执行命令逻辑
- 目标对象查询与业务分发

原因：
- 这样可以解决“字符串散落”和“参数格式漂移”问题，同时避免把这个头文件做成半个管理器

### 决策 3：命令注册位置

命令注册逻辑保留在 TCS 私有实现侧。

推荐落点：
- 专门的私有实现文件，例如 `Private/TcsConsoleCommands.cpp` 或 `Private/Debug/TcsDebugConsoleCommands.cpp`
- 由模块启动阶段统一注册第一阶段命令与开关

原因：
- 共享头文件负责静态命名面
- 私有实现文件负责和 UE Console 系统对接
- 调用点只消费共享命名，不直接散落原始命令字符串

### 决策 4：第一阶段命令范围

第一阶段只保留一个最小命令：

- `tcs.state.debug_evaluator.enable`
  - 作用：控制 `FTcsStateSlotDebugEvaluator` 这类常驻路径是否允许构造快照字符串

`GetSlotDebugSnapshot()` / `GetStateDebugSnapshot()` 仍然保留为按需调试 API，
但不再为它们提供第一阶段控制台 `dump / list` 查询入口；
如果后续需要面向目标的快照查询，应通过调试 UI 面板承载，而不是继续扩展日志型控制台查询命令。

第一阶段不做：
- 通用 `tcs.help` 命令树
- Attribute / Buff / Skill 的完整调试命令集
- 多级权限、持久化配置、远程调试同步

原因：
- 先把 6.6 直接关心的问题收掉，避免一次性扩成整个 TCS 调试框架

### 决策 5：参数约定

参数采用 `key=value` 形式，而不是裸位置参数。

第一阶段约定：
- `enable=0|1`（用于开关类命令）

原因：
- `key=value` 更适合后续平滑扩展，而不会因为参数顺序增加而破坏旧命令
- 对日志帮助文本也更直观

### 决策 6：快照构造收敛策略

- `GetSlotDebugSnapshot()` / `GetStateDebugSnapshot()` 保留为按需调试 API
- 常驻路径不得默认调用这两个接口构造大字符串
- `FTcsStateSlotDebugEvaluator` 必须先检查显式调试开关，只有开关开启时才允许构造快照
- 第一阶段不再提供 `dump / list` 控制台查询命令；面向目标的快照浏览留给后续调试 UI

原因：
- 这两个接口本身仍是有价值的调试能力，不需要删除
- 6.6 要解决的是“默认调用路径”问题，而不是把调试能力整体删除

## 风险 / 取舍

- 风险：第一阶段只处理 `State`，后续 `Attribute` / `Buff` / `Skill` 如果也要接入调试命令，还需要继续扩展同一命名面
  - 缓解：从一开始就固定统一的命令前缀、参数风格和帮助文本结构

- 风险：如果以后又临时回到 `dump / list` 式日志查询，很容易再次把控制台命令扩成调试 UI 的替代品
  - 缓解：在设计边界里明确，把面向目标的查询保留给后续调试 UI，而不是继续堆日志型控制台命令

- 风险：如果把所有调试开关都收成静态全局布尔值，后续可能难以表达更细粒度的目标范围
  - 缓解：第一阶段只做最小必要开关，不预先引入复杂 per-target 状态存储

## 迁移计划

1. 新增 `TcsConsoleCommands.h`，固定命名面和参数约定
2. 新增第一阶段命令注册实现
3. 把 `State` 快照调用点迁移为“显式开关控制”
4. 重新编译验证并回写基线文档 6.6
