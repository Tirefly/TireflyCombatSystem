# targeting-strategy Specification

## Purpose
TBD - created by archiving change add-tcstargeting-strategies. Update Purpose after archive.
## Requirements
### Requirement: 选择器策略契约

`TcsTargeting` MUST 以**可配置的数据策略基类**承载目标选择（D4-4 v2 策略模式 / D3-7 v3 载体）：

- `FTcsTargetSelectorStrategy`（`USTRUCT(meta = (Hidden))`，住 `Public/Targeting/TcsTargetSelectorStrategy.h`）MUST 声明
  `virtual void Resolve(const FTcsEffectContext& Context, ITcsEntityQuery* EntityQuery, TArray<FTcsCombatEntityHandle>& OutTargets) const`——**产出为实体句柄**（非 `AActor*`/弱指针）；
- **抽象由约定达成，MUST NOT 使用纯虚（`= 0` / `PURE_VIRTUAL`）**：UHT 对每个 USTRUCT 生成 `TCppStructOps<T>`（需可默认构造），抽象类报 C2259；`PURE_VIRTUAL` 在 `CHECK_PUREVIRTUALS` 下展开为 `= 0`（`CoreMiscDefines.h:100-102`）。故基类 MUST 提供**中性默认实现** + `meta = (Hidden)`（编辑器 picker 不可选基类；先例 `FTcsParamValueSource`）；
- **填充纪律**：`Resolve` 只**填充** `OutTargets`（调用方先清空），MUST NOT 假设其为空；
- **注入可空**：`EntityQuery` MAY 为 `nullptr`——策略 MUST 容忍（降级 + Warning），MUST NOT 解引用空指针；
- **宿主扩展 = C++ 新 struct 子类**（零框架改动）：配置面经 `TInstancedStruct<FTcsTargetSelectorStrategy>` 内嵌编辑，`BaseStruct` 限定由 UHT 自动写入；BP 策略扩展通道放弃（R0 §9）。

#### Scenario: 基类不可被编辑器选中

- **WHEN** 在步骤的 `TInstancedStruct<FTcsTargetSelectorStrategy>` 成员上打开类型 picker
- **THEN** 基类不在列表中（`Hidden` 元数据被过滤），只列 C++ 子类

#### Scenario: Resolve 只填充不清空

- **WHEN** 调用方预留了非空 `OutTargets` 后调用任一选择器的 `Resolve`
- **THEN** 策略只追加/改写自己的产出，清空责任在调用方

#### Scenario: 无 Actor 实体同样可被选中

- **WHEN** 宿主经查询枚举出无 Actor 的实体（未来 Mass）并配置进步骤
- **THEN** `Resolve` 产出其句柄、后续步骤照常消费（选择层无 Actor 依赖）

### Requirement: 过滤器策略契约

`TcsTargeting` MUST 以同款策略基类承载候选过滤：

- `FTcsTargetFilterStrategy`（同款抽象约定）MUST 声明
  `virtual bool Pass(FTcsCombatEntityHandle Candidate, const FTcsEffectContext& Context) const`——**候选为实体句柄**（需要存活/位置语义时经注入接口询问宿主）；
- **框架 MUST NOT 提供任何默认 Filter**（存活/敌对/阵营是宿主本体论，10 §2.2）；R3 竖切由测试装置实现；
- **组合语义 = AND 全过 + 短路**。

#### Scenario: 空 Filter 数组不淘汰任何候选

- **WHEN** 步骤的 `Filters` 为空数组
- **THEN** 选择器产出的全部候选进入 `Context.Targets`（无隐式默认过滤）

#### Scenario: 任一 Filter 不过即淘汰

- **WHEN** 候选对两个 Filter 分别返回 true / false
- **THEN** 该候选被淘汰（AND），且短路不再调用后续 Filter

### Requirement: SelectTargets 步骤与执行器

`TcsTargeting` MUST 提供链步骤 `FTcsStepSelectTargets`（`Public/Chain/TcsStepSelectTargets.h`）：字段 `TInstancedStruct<FTcsTargetSelectorStrategy> Selector` + `TArray<TInstancedStruct<FTcsTargetFilterStrategy>> Filters`。

- 执行器 MUST 经 `UE_DEFINE_EFFECT_STEP_EXECUTOR` **跨模块自注册**（TcsTargeting → TcsEffect 注册表，机制层零改动）；
- 执行序：**清空** `Context.Targets` → `Selector->Resolve(Context, EntityQuery, Candidates)`（`EntityQuery` 取自 `UTcsEffectSubsystem::GetEntityQuery()`，经运行态 `Run.Owner`）→ 逐候选过 `Filters`（AND + 短路）→ 通过者**保序**写回 `Context.Targets`；
- **MUST NOT 做存活过滤**：候选的有效性由宿主 Filter 表达（框架不认识"存活"）；选择器产出的失效句柄由 Filter 淘汰，无 Filter 时原样传递；
- **即时步骤**：恒返回 `TSR_Completed`；
- **未配置 Selector**：保持 `Context.Targets` 原样 + Warning（不静默清空、不 ensure）；
- **确定性（D0-1）**：遍历与过滤保序，同输入同输出。

#### Scenario: 选择 → 过滤 → 写回目标集

- **WHEN** 执行一条配了选择器与过滤器的 `SelectTargets` 步骤
- **THEN** `Context.Targets` = 选择器产出中通过全部过滤器的句柄，且保持产出顺序

#### Scenario: 跨模块自注册可查

- **WHEN** TcsTargeting 模块加载后查询执行器注册表
- **THEN** `FTcsStepSelectTargets` 的执行器已登记，链含该步骤时可被分派

#### Scenario: 未配置选择器时目标集不变

- **WHEN** 步骤的 `Selector` 为空且 `Context.Targets` 已有内容
- **THEN** 执行后目标集保持原样 + Warning（清空与"选了空集"不混淆）

#### Scenario: 即时完成不挂起

- **WHEN** 执行该步骤
- **THEN** 返回 `TSR_Completed`，链在同一次进入执行中继续走下一步

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

