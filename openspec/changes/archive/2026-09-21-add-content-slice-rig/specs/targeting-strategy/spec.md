## ADDED Requirements

### Requirement: 内置 Self 选择器

`TcsTargeting` MUST 提供**框架内置**的"选自己"选择器 `FTcsSelSelf`（`USTRUCT()`，住 `Public/Targeting/TcsSelSelf.h`）——**它是设计 10 §2.2 承诺的框架默认选择器之一**（Self / EventTarget；EventTarget 依赖"事件载荷 → 目标"通路，随触发行轮落地）：

- `Resolve` MUST 把 `Context.Caster` 写进 `OutTargets`（**只填充不清空**，与基类契约一致）；
- `Context.Caster` **无效时** MUST NOT 产出任何目标 + Warning 日志（不静默产空、不崩溃）——"没配施法者"与"选了空集"必须可区分；
- `EntityQuery` 为 `nullptr` 时 MUST 照常工作（本选择器**不需要**实体查询——它不解析位置/存活）；
- **MUST NOT 做存活过滤**（框架不认识"存活"，与 `SelectTargets` 执行器同款纪律）；
- **非 `Hidden`**：它是可选实现而非抽象基类，MUST 出现在编辑器类型 picker 中。

**存在理由（内容资产稳定性）**：`TInstancedStruct` 存的是**类型身份**（包 import 表索引，非名字字符串，`InstancedStruct.cpp:199-234`）——链资产若引用切片侧类型，切片退役后该步骤会**静默变空**（降级路径 `InstancedStruct.cpp:237-248`，仅剩一条 `LogCore` Warning）；内容资产引用**宿主**类型则会让插件 `Content/` 在别的项目失联。故"选自己"这一语义 MUST 由框架提供（**内容引用的每个类型都必须长于内容**）。

#### Scenario: 选自己产出施法者

- **WHEN** `Context.Caster` 为有效句柄时执行 `FTcsSelSelf::Resolve`
- **THEN** `OutTargets` 含该施法者句柄（原有内容不被清空——清空是调用方责任）

#### Scenario: 无施法者时不产出且留痕

- **WHEN** `Context.Caster` 为无效句柄
- **THEN** 不产出任何目标 + Warning 日志（不静默产空、不崩溃）

#### Scenario: 不需要实体查询

- **WHEN** `EntityQuery` 为 `nullptr`
- **THEN** `FTcsSelSelf` 照常产出施法者句柄（不降级、不报错——它不消费查询能力）

#### Scenario: 链上可直接配置

- **WHEN** 在链资产的 `FTcsStepSelectTargets::Selector` 上打开类型 picker
- **THEN** `Self`（`FTcsSelSelf`）在列表中可选，选中后无需任何 C++ 改动即可让链以施法者自身为目标
