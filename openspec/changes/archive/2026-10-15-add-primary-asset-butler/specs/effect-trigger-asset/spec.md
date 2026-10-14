## MODIFIED Requirements

### Requirement: 触发定义资产

`TcsEffect` MUST 提供触发定义资产类 `UTcsEffectTriggerDefAsset : UPrimaryDataAsset`（住 `Public/Trigger/TcsEffectTriggerDefAsset.h`，导出宏 `TCSEFFECT_API`）——供策划在编辑器里**零 C++** 创作"系统级规则"（全局常驻、与任何 Def 无关的触发规则，如"任何实体死亡时触发某链"）：

> **★ 2026-10-15 迁移（`PLN-REFACTOR-1` Task 8 · 裁定⑤）**：本类**自 `TcsIntegration` 搬入 `TcsEffect`**——判据同链资产（`TcsEffect` 已有 `Trigger/` 目录与触发机制）。**类名与字段集逐字不变**。**已落资产 MUST 配重定向**：实测 2 个已落资产引用 `/Script/TcsIntegration.TcsEffectTriggerDefAsset`，宿主 `[CoreRedirects]` 需配 `+ClassRedirects=(OldName="/Script/TcsIntegration.TcsEffectTriggerDefAsset",NewName="/Script/TcsEffect.TcsEffectTriggerDefAsset")`。**★ 一处取代旧文**：原文写本类"MUST 住 `TcsIntegration`（资产载体需要同时看到定义类型 `TcsEffect` 与发现机制 `UTcsDefinitionSubsystem`）"——**该判据已不成立**：搬到 `TcsEffect` 后它正好与定义类型同住，**不需要**看到发现机制（发现归定义库，定义库反过来引用它，方向天然朝下）。**本类同时是主资产注册项**（`UE_DEFINE_PRIMARY_ASSET_TYPE`）。

- **类名的 `Asset` 后缀是 UHT 硬约束下的明示例外**（2026-10-04 实测）：Def 资产命名标准是 `<Family>Def`，但本族的 `<Family>Def` 已被数据 struct `FTcsEffectTriggerDef` 占用，而 UHT 按"**去前缀后的引擎名**"判重——`UTcsEffectTriggerDef` 会被 UHT 直接拒绝（`Class '…' shares engine name '…' with struct '…'`，UHT 阶段即失败）。故资产类 MUST 取 `Asset` 后缀消歧，`openspec/project.md` 的标准条文已补该限定语；**MUST NOT** 因此改名数据 struct（它是已交付的公开面）；

- 字段：`FGameplayTag TriggerTag`（**内容身份**——不是资产名）+ `FTcsEffectTriggerDef Def`（触发定义数据：`EventTag` / `Conditions` / `EffectChainId` / `Priority` / `ExecutionGate` / `InterruptPriority` / `GateTags` / `bConditionMissIsSilent` / `EventPayloadFilter`）；
- **主资产身份**（Def 资产族统一约定 2026-09-17）：`static const FPrimaryAssetType PrimaryAssetType` **显式声明**（值取类名 `"TcsEffectTriggerDefAsset"`——族内一致）+ 覆写 `GetPrimaryAssetId()` 使**名取 `TriggerTag.GetTagName()`**（`FPrimaryAssetId` 的 name 位是 `FName`，tag 须经 `GetTagName()` 转换，且该名含根段）⇒ 资产文件可改名 / 挪目录而不破坏 `[PrimaryAssetType, TriggerTag]` 解析；
- **`TriggerTag` MUST 住 `EffectTriggerDef` 根**（`EffectTriggerDef.<名>`，2 段；宿主 ini 声明）——该根由本类与定义库的 `ResolveTriggerDef` 消费（根段注册表见 `gameplay-tag-governance`）；插件 MUST NOT 声明任何具体规则词；
- **MUST NOT 携带运行期字段**：`FTcsSourceHandle Source` / `FTcsEffectTriggerHandle Self` 等运行期发号句柄只住 `FTcsEffectTriggerInstance`（跨会话/跨机不同，MUST NOT 进内容资产）——故本资产可安全入库、可被复制；
- **编辑器校验（`WITH_EDITOR` 的 `IsDataValid`）**：`TriggerTag` 无效 → `Invalid`；`Def.EventTag` 无效 → `Invalid`；`Def.EffectChainId` 无效 → `Invalid`（这三项都会让 `RegisterTriggerRow` 走 ensure 拒绝面或让规则永不触发，MUST 在编辑器提前报出）；**无错时 MUST 把 `Super` 可能返回的 `NotValidated` 提升为 `Valid`**（`UObject::IsDataValid` 基类默认返回 `NotValidated`，不提升会让"已校验通过"显示为"未验证"，装置断言 `== Valid` 必然失败）；
- **依赖方向**：本类 MUST 住 `TcsEffect`（与它承载的定义类型 `FTcsEffectTriggerDef` 同模块）；发现机制 `UTcsDefinitionSubsystem` 住 `TcsIntegration` 并**反向引用本类**——依赖方向仍是 `TcsIntegration → TcsEffect`，MUST NOT 让 `TcsEffect` 依赖 `TcsIntegration`（依赖铁律）；本类另依赖 `TcsAssetButler`（主资产注册，方向朝下）；
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

#### Scenario: 搬迁后已落资产仍可打开（2026-10-15 新增）

- **WHEN** 搬迁后打开任一原引用 `/Script/TcsIntegration.TcsEffectTriggerDefAsset` 的已落规则资产
- **THEN** 该资产成功加载且其类为 `TcsEffectTriggerDefAsset`（重定向生效）——实测 2 个规则资产逐个验证通过
