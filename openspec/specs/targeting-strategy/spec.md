# targeting-strategy Specification

## Purpose
TBD - created by archiving change add-tcstargeting-strategies. Update Purpose after archive.
## Requirements
### Requirement: 选择器策略契约

`TcsTargeting` MUST 提供选择器策略基类 `FTcsTargetSelectorStrategy`（USTRUCT 反射基类，D3-7 v3）——解析目标集写入 `Context.Targets`：

- **抽象由约定达成，MUST NOT 使用纯虚（`= 0` / `PURE_VIRTUAL`）**：UHT 对每个 USTRUCT 生成 `TCppStructOps<T>`（需可默认构造），抽象类报 C2259；`PURE_VIRTUAL` 在 `CHECK_PUREVIRTUALS` 下展开为 `= 0`（`CoreMiscDefines.h:100-102`）。故基类 MUST 提供**中性默认实现** + `meta = (Hidden)`（编辑器 picker 不可选基类；先例 `FTcsParamValueSource`）；
- **填充纪律**：`Resolve` 只**填充** `OutTargets`（调用方先清空），MUST NOT 假设其为空；
- **注入可空**：`EntityQuery` MAY 为 `nullptr`——策略 MUST 容忍（降级 + Warning），MUST NOT 解引用空指针；
- **宿主扩展 = C++ 新 struct 子类**（零框架改动）：配置面经**裸 `FInstancedStruct`** 内嵌编辑（**2026-09-24 换型**，提案 `switch-strategy-carrier-to-plain-instanced-struct`），`BaseStruct` 限定由**手写 metadata** 提供（`meta = (BaseStruct = "/Script/TcsTargeting.TcsTargetSelectorStrategy")`）——**MUST NOT** 用 `TInstancedStruct<T>`（其字段在宿主脚本层导出为空壳，脚本层配不了，台账 S-6）；BP 策略扩展通道放弃（R0 §9）。
- **换型代价（明示接受）**：丢编译期类型限定 ⇒ 改由运行期校验兜底（`GetPtr<T>()` 的 `IsChildOf` 检查 + 调用点判空）；且 `BaseStruct` metadata **写错则 picker 静默不限**（`TryFindTypeSlow` 找不到返回 nullptr 不报错）——故该 metadata 的存在性与正确性 MUST 进 M8 校验矩阵（台账 R8-1 范围）。

#### Scenario: 基类不可被编辑器选中

- **WHEN** 在步骤的 `FInstancedStruct`（带 `BaseStruct` metadata）成员上打开类型 picker
- **THEN** 基类不在列表中（`Hidden` 元数据被过滤），只列 C++ 子类

#### Scenario: Resolve 只填充不清空

- **WHEN** 调用方预留了非空 `OutTargets` 后调用任一选择器的 `Resolve`
- **THEN** 策略只追加/改写自己的产出，清空责任在调用方

#### Scenario: 选择器字段对脚本层可读写

- **WHEN** 检查 `FTcsStepSelectTargets` 的宿主脚本绑定产物（UnrealSharp glue）
- **THEN** `Selector` / `Filters` 字段生成真实的读写代码（非空函数体）——脚本层可构造并配置选目标步骤

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

`TcsTargeting` MUST 提供链步骤 `FTcsStepSelectTargets`（`Public/Chain/TcsStepSelectTargets.h`）：字段 `FInstancedStruct Selector`（`meta = (BaseStruct = "/Script/TcsTargeting.TcsTargetSelectorStrategy")`）+ `TArray<FInstancedStruct> Filters`（`meta = (BaseStruct = "/Script/TcsTargeting.TcsTargetFilterStrategy")`）——**2026-09-24 换型**（原为 `TInstancedStruct<T>`，在脚本层导出为空壳导致脚本拼不出本步骤）。

- 执行器 MUST 经 `UE_DEFINE_EFFECT_STEP_EXECUTOR` **跨模块自注册**（TcsTargeting → TcsEffect 注册表，机制层零改动）；
- 执行序：**清空** `Context.Targets` → `Selector->Resolve(Context, EntityQuery, Candidates)`（`EntityQuery` 取自 `UTcsEffectSubsystem::GetEntityQuery()`，经运行态 `Run.Owner`）→ 逐候选过 `Filters`（AND + 短路）→ 通过者**保序**写回 `Context.Targets`；
- **MUST NOT 做存活过滤**：候选的有效性由宿主 Filter 表达（框架不认识"存活"）；选择器产出的失效句柄由 Filter 淘汰，无 Filter 时原样传递；
- **即时步骤**：恒返回 `TSR_Completed`；
- **未配置 Selector**：保持 `Context.Targets` 原样 + Warning（不静默清空、不 ensure）；
- **取用纪律（换型连带）**：`Selector` / `Filters` 元素的取用 MUST 走 `GetPtr<T>()` + **判空**（裸载体的编译期限定已移除，运行期 `IsChildOf` 校验失败时返回 nullptr）。

#### Scenario: 未配置选择器时目标集原样

- **WHEN** 步骤的 `Selector` 为空载体（未配置）
- **THEN** `Context.Targets` 保持原样，留 Warning（不静默清空）

#### Scenario: 过滤按 AND 且保序

- **WHEN** 配两个 Filter（都通过 / 一过一不过）与多个候选
- **THEN** 只有全通过的候选留下，且保持选择器产出的顺序

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

### Requirement: 宿主脚本选择器插槽

`TcsTargeting` MUST 提供宿主选择器插槽——**让宿主用任意 UE 脚本语言（C#/AS/Luau/TS/蓝图）实现目标选择，零 C++ 改动**：

- 新增 `ITcsTargetSelectorHost`（`UINTERFACE`，住 `Public/Host/TcsTargetSelectorHost.h`），MUST 提供：
  - `UFUNCTION(BlueprintNativeEvent) void ResolveTargets(FTcsCombatEntityHandle Caster, FTcsCombatEntityHandle Instigator, TArray<FTcsCombatEntityHandle>& OutTargets)`——**填充语义同 `FTcsTargetSelectorStrategy::Resolve`**（只填充不清空；调用方负责清空）；
- 新增转发策略 `FTcsSelHostDelegate : FTcsTargetSelectorStrategy`（USTRUCT，可被编辑器 picker 选中）：持 `UPROPERTY TScriptInterface<ITcsTargetSelectorHost> Host`，`Resolve` 覆写为**纯转发**（含空实现守卫：`Host` 未配时留 Warning 并产出空集，MUST NOT 崩溃）；
- **转发是必需的，不是选择**：`FTcsTargetSelectorStrategy` 走 **C++ 虚分派**，而脚本定义的 struct **没有 C++ 类型**（`CppStructOps == nullptr` ⇒ vtable 指针位为 0 ⇒ 调用即野函数指针）——**引擎层面无解**。故 MUST 经"USTRUCT 转发器 → UObject 反射接口"两层；
- 框架默认选择器（`FTcsSelSelf`）与 C++ 策略子类 MUST 保持原样（**双轨并存**：内置走虚分派快路径，插槽只给宿主扩展）；
- 接口 MUST 加 `Blueprintable`（脚本层要能实现它）；
- 形参 MUST 全反射（`BlueprintNativeEvent` 触发 UHT 蓝图参数校验，`UhtFunction.cs:859`/`:1043-1053`）——故传**句柄**而非上下文 struct。

#### Scenario: 脚本层实现选择器并生效

- **WHEN** 宿主脚本层实现 `ResolveTargets` 返回自定义目标集，配进 `FTcsStepSelectTargets.Selector`（经 `FTcsSelHostDelegate` 转发器），链跑到该步
- **THEN** `Context.Targets` 等于脚本实现产出的目标集

#### Scenario: 未配 Host 的转发器安全降级

- **WHEN** `FTcsSelHostDelegate.Host` 为空（未配脚本实现）而该转发器被配进链
- **THEN** 不崩溃、产出空集 + Warning 日志（不静默通过、不 ensure——配置残缺是内容问题）

#### Scenario: 内置策略不受影响

- **WHEN** 链使用 `FTcsSelSelf`（框架默认）
- **THEN** 行为与插槽引入前完全一致（虚分派路径未被改动）

### Requirement: 宿主脚本过滤器插槽

`TcsTargeting` MUST 以同款手法提供宿主过滤器插槽：

- 新增 `ITcsTargetFilterHost`（`UINTERFACE`，住 `Public/Host/TcsTargetFilterHost.h`）：
  - `UFUNCTION(BlueprintNativeEvent) bool PassTarget(FTcsCombatEntityHandle Candidate, FTcsCombatEntityHandle Caster, FTcsCombatEntityHandle Instigator)`——**纯判定**（MUST NOT 改写候选或上下文）；
- 新增转发策略 `FTcsFilterHostDelegate : FTcsTargetFilterStrategy`（持 `TScriptInterface<ITcsTargetFilterHost> Host`）；
- **空 Host 的语义 = 通过**（与基类中性默认实现一致——"框架零默认 Filter"指不提供有语义的默认实现，而非把未配置变成淘汰）；
- **组合语义不变**：AND 全过 + 短路（由 `SelectTargets` 执行器保证，本插槽不改变它）。

#### Scenario: 脚本层过滤器参与 AND 组合

- **WHEN** 步骤的 `Filters` 含一个脚本实现转发器与一个 C++ Filter
- **THEN** 候选须两者都通过才进入 `Context.Targets`；脚本实现返回 false 时短路（不再问后续 Filter）

#### Scenario: 空 Host 不淘汰候选

- **WHEN** `FTcsFilterHostDelegate.Host` 为空而该转发器在 `Filters` 数组里
- **THEN** 候选通过（与"无 Filter"同效）

