## MODIFIED Requirements

### Requirement: 效果链资产

`TcsIntegration` MUST 提供链资产类 `UTcsEffectChainDef : UPrimaryDataAsset`（住 `Public/Chain/TcsEffectChainDef.h`）——供策划在编辑器里创作链（2026-09-21 用户拍板**资产轨**；否决 DataTable 行轨与双轨）：

- 字段：`FGameplayTag ChainId`（链身份；**2026-09-22 改造：`FName` → `FGameplayTag`**）+ `FTcsEffectChain Chain`（链数据：`ChainId` / `Steps` / `MaxStepsPerFrame`）；
- **主资产身份**（2026-09-17 标准）：`static const FPrimaryAssetType PrimaryAssetType` **显式声明**（不从类名派生，**值取类名**——族内一致：`UTcsAttributeDef` → `"TcsAttributeDef"`、`UTcsAttrModDef` → `"TcsAttrModDef"`、本类 → `"TcsEffectChainDef"`；2026-09-21 用户收口）+ 覆写 `GetPrimaryAssetId()` 使**名取 `ChainId.GetTagName()`**（**2026-09-22 改造：`FPrimaryAssetId` 的 name 位是 `FName`，tag 须经 `GetTagName()` 转换**——这是引擎类型约束下的必要一步，用户已确认可接受；资产文件可改名/移动而不破坏 `[PrimaryAssetType, ChainId]` 解析）；
- **双真相禁令**：`Chain.ChainId` MUST 与 `ChainId` 一致——不一致时**拒绝登记**（Error 日志 + 失败清单），MUST NOT 静默取其一；
- **步骤数组无编辑器类型收窄**（`TArray<FInstancedStruct>`，D4-16 有意为之）：作者侧校验（步骤类型合法性、Conditions 形状等）归 M8 校验矩阵（台账 R8-2）；运行期由执行器注册表拒绝（未注册类型 → 中止 + Error）。

#### Scenario: 资产身份取 ChainId

- **WHEN** 一个 `ChainId = Tcs.Chain.Chain_Whirlwind` 的链资产被重命名为任意文件名
- **THEN** `GetPrimaryAssetId()` 仍为 `[PrimaryAssetType, Chain_Whirlwind]`（解析不受文件名影响）

#### Scenario: 双真相被拒

- **WHEN** 资产的 `ChainId = Tcs.Chain.A` 而 `Chain.ChainId = Tcs.Chain.B`
- **THEN** 登记被拒 + Error 日志（不静默取其一）

#### Scenario: 链数据即登记内容

- **WHEN** 资产登记后按 `ChainId` 查询链定义
- **THEN** 取回的 `FTcsEffectChain` 与资产内 `Chain` 字段逐字段一致
