## MODIFIED Requirements

### Requirement: 最小编译集依赖方向

各模块 `Build.cs` 依赖 MUST 符合 `DEC-00` §9：`TcsCore` 仅依赖引擎基础（Core/CoreUObject/Engine/DeveloperSettings/GameplayTags，禁依赖任何兄弟/上层模块）；`TcsNotation` 仅依赖 Core/CoreUObject；**`TcsAssetButler` 仅依赖引擎基础（Core/CoreUObject/Engine/DeveloperSettings/GameplayTags/AssetRegistry）——MUST NOT 依赖任何 Tcs 业务模块**（它是各域**向下注册**的落点，方向朝下）；`TcsAttribute` 依赖引擎基础 + TcsCore + TcsNotation（D5-18 v2）；`TcsEffect` 依赖 Core/Attribute + `TcsAssetButler`（其两个 Def 资产类自注册）；禁止反向 include。

#### Scenario: 依赖按 R0 §9 落位

- **WHEN** 读取三模块 Build.cs 的 PublicDependencyModuleNames
- **THEN** TcsCore 不含任何 Tcs* 模块，TcsNotation 不含任何 Tcs* 模块，TcsAttribute 恰好含 TcsCore 与 TcsNotation

#### Scenario: 加载层零业务依赖且被各域向下依赖（2026-10-15 新增）

- **WHEN** 读取 `TcsAssetButler.Build.cs` 的 PublicDependencyModuleNames 与 `TcsEffect.Build.cs` 的同项
- **THEN** 前者**不含**任何 `Tcs*` 模块（只有引擎六项）；后者**含** `TcsAssetButler` —— 依赖方向严格朝下，不成环
