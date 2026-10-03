# effect-trigger-asset Specification

## Purpose
定义触发行的独立资产载体：以**显式主资产类型**登记、以**内容身份标签**（而非资产文件名）解析的触发定义资产——供策划零 C++ 创作"系统级规则"（全局常驻、与任何 Def 无关的触发规则）。资产的**发现、校验、缓存与世界装配**（装配成该世界的触发行）归 `integration-entity` 的定义库；触发行本身的形状、登记表与求值归 `effect-trigger`。

## Requirements

### Requirement: 触发定义资产

`TcsIntegration` MUST 提供触发定义资产类 `UTcsEffectTriggerDefAsset : UPrimaryDataAsset`（住 `Public/Trigger/TcsEffectTriggerDefAsset.h`）——供策划在编辑器里**零 C++** 创作"系统级规则"（全局常驻、与任何 Def 无关的触发规则，如"任何单位死亡时触发某链"）：

- **类名的 `Asset` 后缀是 UHT 硬约束下的明示例外**（2026-10-04 实测）：Def 资产命名标准是 `<Family>Def`，但本族的 `<Family>Def` 已被数据 struct `FTcsEffectTriggerDef` 占用，而 UHT 按"**去前缀后的引擎名**"判重——`UTcsEffectTriggerDef` 会被 UHT 直接拒绝（`Class '…' shares engine name '…' with struct '…'`，UHT 阶段即失败）。故资产类 MUST 取 `Asset` 后缀消歧，`openspec/project.md` 的标准条文已补该限定语；**MUST NOT** 因此改名数据 struct（它是 Task 1 已交付的公开面）；

- 字段：`FGameplayTag TriggerTag`（**内容身份**——不是资产名）+ `FTcsEffectTriggerDef Def`（触发定义数据：`EventTag` / `Conditions` / `EffectChainId` / `Priority` / `ExecutionGate` / `InterruptPriority` / `GateTags` / `bConditionMissIsSilent` / `EventPayloadFilter`）；
- **主资产身份**（Def 资产族统一约定 2026-09-17）：`static const FPrimaryAssetType PrimaryAssetType` **显式声明**（值取类名 `"TcsEffectTriggerDefAsset"`——族内一致）+ 覆写 `GetPrimaryAssetId()` 使**名取 `TriggerTag.GetTagName()`**（`FPrimaryAssetId` 的 name 位是 `FName`，tag 须经 `GetTagName()` 转换，且该名含根段）⇒ 资产文件可改名 / 挪目录而不破坏 `[PrimaryAssetType, TriggerTag]` 解析；
- **`TriggerTag` MUST 住 `EffectTriggerDef` 根**（`EffectTriggerDef.<名>`，2 段；宿主 ini 声明）——该根由本类与定义库的 `ResolveTriggerDef` 消费（根段注册表见 `gameplay-tag-governance`）；插件 MUST NOT 声明任何具体规则词；
- **MUST NOT 携带运行期字段**：`FTcsSourceHandle Source` / `FTcsEffectTriggerHandle Self` 等运行期发号句柄只住 `FTcsEffectTriggerInstance`（跨会话/跨机不同，MUST NOT 进内容资产）——故本资产可安全入库、可被复制；
- **编辑器校验（`WITH_EDITOR` 的 `IsDataValid`）**：`TriggerTag` 无效 → `Invalid`；`Def.EventTag` 无效 → `Invalid`；`Def.EffectChainId` 无效 → `Invalid`（这三项都会让 `RegisterTriggerRow` 走 ensure 拒绝面或让规则永不触发，MUST 在编辑器提前报出）；**无错时 MUST 把 `Super` 可能返回的 `NotValidated` 提升为 `Valid`**（`UObject::IsDataValid` 基类默认返回 `NotValidated`，不提升会让"已校验通过"显示为"未验证"，装置断言 `== Valid` 必然失败）；
- **依赖方向**：本类 MUST 住 `TcsIntegration`（资产载体需要同时看到定义类型 `TcsEffect` 与发现机制 `UTcsDefinitionSubsystem`）；MUST NOT 反向促使 `TcsEffect` 依赖 `TcsIntegration`（依赖铁律 `Core←Attribute←Effect←{Damage,Targeting,State}←Skill`）；
- **内联位是后续形态（非本批）**：`SkillDef` / `BuffDef` 落地时把 `TArray<FTcsEffectTriggerDef>` 内联进各自定义（定义类型已是纯配置，可直接内联）——本批只做**独立资产**这一条载体。

#### Scenario: 资产身份取 TriggerTag

- **WHEN** 一个 `TriggerTag = EffectTriggerDef.OnAnyDeath` 的规则资产被重命名为任意文件名（或挪到别的目录）
- **THEN** `GetPrimaryAssetId()` 仍为 `[TcsEffectTriggerDefAsset, EffectTriggerDef.OnAnyDeath]`（解析不受文件名影响；名位是完整 tag 文本，含根段）

#### Scenario: 定义不完整在编辑器报 Invalid

- **WHEN** 资产的 `TriggerTag` / `Def.EventTag` / `Def.EffectChainId` 任一无效
- **THEN** 该资产的 `IsDataValid` 返回 `Invalid` 且 `FDataValidationContext` 含对应错误说明

#### Scenario: 校验通过返回 Valid 而非 NotValidated

- **WHEN** 一个各字段均有效的规则资产被校验
- **THEN** `IsDataValid` 返回 `Valid`（已把基类的 `NotValidated` 提升）——编辑器显示为"已校验通过"

#### Scenario: 资产侧不含运行期字段

- **WHEN** 检查 `UTcsEffectTriggerDefAsset` 的字段集
- **THEN** 不含 `FTcsSourceHandle` / `FTcsEffectTriggerHandle` 或其它运行期发号句柄（内容资产可安全入库）

#### Scenario: 零 C++ 配出系统级规则

- **WHEN** 策划新建一个规则资产（填 `TriggerTag` / `Def.EventTag` / `Def.EffectChainId`，不改任何 C++、不写任何装置代码）并运行世界
- **THEN** 该规则经定义库装配成触发行；对应事件发布时触发并执行指定链（内容侧全程零 C++）
