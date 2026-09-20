# targeting-strategy Specification

## Purpose
TBD - created by archiving change add-tcstargeting-strategies. Update Purpose after archive.
## Requirements
### Requirement: 选择器策略契约

`TcsTargeting` MUST 以**可配置的数据策略基类**承载目标选择（D4-4 v2 策略模式 / D3-7 v3 载体）：

- `FTcsTargetSelectorStrategy`（`USTRUCT(meta = (Hidden))`，住 `Public/Targeting/TcsTargetSelectorStrategy.h`）MUST 声明
  `virtual void Resolve(const FTcsEffectContext& Context, ITcsEntityQuery* EntityQuery, TArray<TWeakObjectPtr<AActor>>& OutTargets) const`；
- **抽象由约定达成，MUST NOT 使用纯虚（`= 0` / `PURE_VIRTUAL`）**：UHT 对每个 USTRUCT 无条件生成 `TCppStructOps<T>`（构造路径需可默认构造），抽象类报 C2259；且 `PURE_VIRTUAL` 在 `CHECK_PUREVIRTUALS` 开启的配置下展开为 `= 0`（引擎实证：`CoreMiscDefines.h:100-102`）——同一份代码会在 Development 编不过、Shipping 能过。故基类 MUST 提供**中性默认实现**（不产出任何目标），并以 `meta = (Hidden)` 使其不可被编辑器类型 picker 选中（同款先例：本仓 `FTcsParamValueSource`）；
- **填充纪律**：`Resolve` 只负责**填充** `OutTargets`（调用方先清空），MUST NOT 假设其为空；产出类型 MUST 为弱引用（`TWeakObjectPtr`——目标 Actor 生命周期归宿主）；
- **注入可空**：`EntityQuery` MAY 为 `nullptr`（宿主尚未注入实体查询）——策略 MUST 容忍（降级为可产出的结果 + Warning 日志），MUST NOT 解引用空指针；
- **宿主扩展 = C++ 新 struct 子类**（零框架改动）：配置面经 `TInstancedStruct<FTcsTargetSelectorStrategy>` 内嵌编辑，`BaseStruct` 限定由 UHT 从模板实参自动写入（引擎实证：`UhtStructProperty.cs:634`；显式再写 `meta=(BaseStruct=…)` 会报错）；BP 策略扩展通道放弃（R0 §9"蓝图不承诺"承责）。

#### Scenario: 基类不可被编辑器选中

- **WHEN** 在步骤的 `TInstancedStruct<FTcsTargetSelectorStrategy>` 成员上打开类型 picker
- **THEN** 基类 `FTcsTargetSelectorStrategy` 不在列表中（`Hidden` 元数据被 picker 过滤，引擎实证 `SInstancedStructPicker.cpp:104`），只列 C++ 子类

#### Scenario: Resolve 只填充不清空

- **WHEN** 调用方预留了非空 `OutTargets` 后调用任一选择器的 `Resolve`
- **THEN** 策略只往里追加/改写自己的产出，清空责任在调用方（契约不依赖"传进来一定是空的"这一隐含前提）

#### Scenario: 宿主新策略零框架改动

- **WHEN** 宿主新增一个 `FTcsTargetSelectorStrategy` 的 C++ 子类并配置进步骤
- **THEN** 无需修改 TcsTargeting / TcsEffect 任何代码即可被解析与执行（数据化 + 虚分派）

### Requirement: 过滤器策略契约

`TcsTargeting` MUST 以同款策略基类承载候选过滤：

- `FTcsTargetFilterStrategy`（`USTRUCT(meta = (Hidden))`，同款抽象约定）MUST 声明
  `virtual bool Pass(const AActor* Candidate, const FTcsEffectContext& Context) const`，中性默认实现 = 通过；
- **框架 MUST NOT 提供任何默认 Filter**：怎么算存活、怎么算敌人、阵营语义是**宿主本体论**（10 §2.2——原枚举式 `TTF_Alive/TTF_Hostile` 正是"框架假装认识宿主语义"的反例）。R3 竖切由测试装置实现（装置不入库，内容资产版装置随 plan2 Task 6）；
- **组合语义 = AND 全过 + 短路**：任一 Filter 返回 false 即淘汰该候选（不继续问后面的 Filter）。

#### Scenario: 空 Filter 数组不淘汰任何候选

- **WHEN** 步骤的 `Filters` 为空数组
- **THEN** 选择器产出的全部候选进入 `Context.Targets`（无隐式默认过滤）

#### Scenario: 任一 Filter 不过即淘汰

- **WHEN** 候选对两个 Filter 分别返回 true / false
- **THEN** 该候选被淘汰（AND 语义），且短路不再调用后续 Filter

### Requirement: SelectTargets 步骤与执行器

`TcsTargeting` MUST 提供链步骤 `FTcsStepSelectTargets`（`Public/Chain/TcsStepSelectTargets.h`；载体内嵌编辑，D3-7 v3）：

- 字段：`TInstancedStruct<FTcsTargetSelectorStrategy> Selector` + `TArray<TInstancedStruct<FTcsTargetFilterStrategy>> Filters`；
- 执行器 MUST 经 `UE_DEFINE_EFFECT_STEP_EXECUTOR` **跨模块自注册**（TcsTargeting 的步骤喂进 TcsEffect 的注册表——本任务是注册制分派的首个跨模块实证，机制层零改动）；
- 执行序：**清空** `Context.Targets` → `Selector->Resolve(Context, EntityQuery, Candidates)`（`EntityQuery` 取自门面注入点 `UTcsEffectSubsystem::GetEntityQuery()`，经运行态 `Run.Owner` 取得）→ 逐候选过 `Filters`（AND + 短路）→ 通过者**保序**写回 `Context.Targets`；
- **即时步骤**：MUST 恒返回 `TSR_Completed`（不挂起——目标选择无异步语义）；
- **悬空纪律**：候选弱引用 Pin 失败即**跳过**（不 ensure——Actor 中途销毁是正常竞态）；
- **未配置 Selector**（`TInstancedStruct` 为空）时 MUST **保持 `Context.Targets` 原样**并留 Warning 日志（MUST NOT 静默清空、MUST NOT ensure）——"没配选择器"与"选择器选了空集"必须是可区分的两种状态；
- **确定性（D0-1）**：遍历与过滤保序，同输入同输出（客户端重算一致）。

#### Scenario: 选择 → 过滤 → 写回目标集

- **WHEN** 执行一条配了选择器与过滤器的 `SelectTargets` 步骤
- **THEN** `Context.Targets` = 选择器产出中通过全部过滤器的候选，且保持产出顺序

#### Scenario: 跨模块自注册可查

- **WHEN** TcsTargeting 模块加载后查询执行器注册表
- **THEN** `FTcsStepSelectTargets` 的执行器已登记（无须任何模块启动代码），链中含该步骤时可被解释器分派

#### Scenario: 未配置选择器时目标集不变

- **WHEN** 步骤的 `Selector` 为空、`Context.Targets` 已有内容
- **THEN** 执行后 `Context.Targets` 保持原样 + Warning 日志（清空与"选了空集"不混淆）

#### Scenario: 即时完成不挂起

- **WHEN** 执行该步骤
- **THEN** 返回 `TSR_Completed`，链在同一次进入执行中继续走下一步

