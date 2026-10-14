## MODIFIED Requirements

### Requirement: 效果链资产

`TcsEffect` MUST 提供链资产类 `UTcsEffectChainDef : UPrimaryDataAsset`（住 `Public/Chain/TcsEffectChainDef.h`，导出宏 `TCSEFFECT_API`）——供策划在编辑器里创作链（2026-09-21 用户拍板**资产轨**；否决 DataTable 行轨与双轨）：

> **★ 2026-10-15 迁移（`PLN-REFACTOR-1` Task 8 · 裁定⑤）**：本类**自 `TcsIntegration` 搬入 `TcsEffect`**——判据 = 它是**链机制的家**（`TcsEffect` 已有 `Chain/` 目录与链解释器），而 `TcsIntegration` 是接线与定义缓存层。**类名与字段集逐字不变**，变的是**宿主模块与头路径**。**已落资产 MUST 配重定向**：实测 12 个已落资产以 `/Script/TcsIntegration.TcsEffectChainDef` 记录**类引用**（按包路径与对象名两段分别扫描得出），宿主 `[CoreRedirects]` 需配 `+ClassRedirects=(OldName="/Script/TcsIntegration.TcsEffectChainDef",NewName="/Script/TcsEffect.TcsEffectChainDef")`。**★ 本类同时是主资产注册项**：它经 `UE_DEFINE_PRIMARY_ASSET_TYPE` 向 `TcsAssetButler` 注册（类型名由 `StaticClass()` 反射派生），故 `TcsEffect.Build.cs` 依赖含 `TcsAssetButler`。

- 字段：`FGameplayTag ChainId`（链身份；**2026-09-22 改造：`FName` → `FGameplayTag`**；**2026-10-01 换根：根 = `EffectChain`**，形态 `EffectChain.<Id>`）+ `FTcsEffectChain Chain`（链数据：`ChainId` / `Steps` / `MaxStepsPerFrame`）；
- **主资产身份**（2026-09-17 标准）：`static const FPrimaryAssetType PrimaryAssetType` **显式声明**（不从类名派生，**值取类名**——族内一致：`UTcsAttributeDef` → `"TcsAttributeDef"`、`UTcsAttrModDef` → `"TcsAttrModDef"`、本类 → `"TcsEffectChainDef"`；2026-09-21 用户收口）+ 覆写 `GetPrimaryAssetId()` 使**名取 `ChainId.GetTagName()`**（**2026-09-22 改造：`FPrimaryAssetId` 的 name 位是 `FName`，tag 须经 `GetTagName()` 转换**——这是引擎类型约束下的必要一步，用户已确认可接受；资产文件可改名/移动而不破坏 `[PrimaryAssetType, ChainId]` 解析）。**`GetTagName()` 返回完整 tag 文本**（含根段），故身份名里同时带根——换根会改变该名（属**重定向 + 重新解析**能覆盖的迁移面，见本变更的重定向登记义务）；
- **双真相禁令**：`Chain.ChainId` MUST 与 `ChainId` 一致——不一致时**拒绝登记**（Error 日志 + 失败清单），MUST NOT 静默取其一；
- **步骤数组无编辑器类型收窄**（`TArray<FInstancedStruct>`，D4-16 有意为之）：作者侧校验（步骤类型合法性、Conditions 形状等）归 M8 校验矩阵（台账 TOOLS-2）；运行期由执行器注册表拒绝（未注册类型 → 中止 + Error）。

#### Scenario: 资产身份取 ChainId

- **WHEN** 一个 `ChainId = EffectChain.Whirlwind` 的链资产被重命名为任意文件名
- **THEN** `GetPrimaryAssetId()` 仍为 `[PrimaryAssetType, EffectChain.Whirlwind]`（解析不受文件名影响；名位是完整 tag 文本，故含根段）

#### Scenario: 双真相被拒

- **WHEN** 资产的 `ChainId = EffectChain.A` 而 `Chain.ChainId = EffectChain.B`
- **THEN** 登记被拒 + Error 日志（不静默取其一）

#### Scenario: 链数据即登记内容

- **WHEN** 资产登记后按 `ChainId` 查询链定义
- **THEN** 取回的 `FTcsEffectChain` 与资产内 `Chain` 字段逐字段一致

#### Scenario: 搬迁后已落资产仍可打开（2026-10-15 新增）

- **WHEN** 搬迁后打开任一原引用 `/Script/TcsIntegration.TcsEffectChainDef` 的已落链资产
- **THEN** 该资产成功加载且其类为 `TcsEffectChainDef`（重定向生效）——实测 12 个链资产逐个验证通过
