## MODIFIED Requirements

### Requirement: 战斗实体组件（三职责封顶）

`TcsIntegration` MUST 提供 `UTcsCombatEntityComponent`（`UActorComponent` 派生，适配器零逻辑——06 §5），**恰好三职责**：

1. **身份锚**：`BeginPlay` 把所属 Actor 注册为战斗实体（`UTcsAttributeSubsystem::RegisterUnit` 发放句柄）并记录自己的句柄；`EndPlay` 反向注销。挂组件 = 策划声明"本 Actor 是战斗单位"。**门禁**：注册前检查定义就绪（`UTcsDefinitionSubsystem::IsRuntimeReady`）——未就绪留 Warning 并跳过（时序是宿主责任，不 ensure 刷屏）；
2. **查询门面**：`GetCurrent(FGameplayTag) -> double`（转发属性门面求值；**2026-09-22 改造：参数从 `FTcsAttributeName` 改为 `FGameplayTag`**）+ 施加/撤销调试用修正器入口（供宿主与装置；R3 不做 AttributeSet）；
3. **手动触发 API**：`ExecuteChainById(FGameplayTag ChainId)`（**2026-09-22 改造：参数类型 `FName` → `FGameplayTag`**）——组 `FTcsEffectContext`（`Caster` = 自身句柄、`Targets` = 自身句柄、`Instigator` 留空）→ `UTcsEffectSubsystem::ExecuteChain`；返回运行态句柄（全即时链在返回前已走完，句柄活性由 `IsRunActive` 判定）。

组件 MUST NOT 越出三职责；MUST NOT 自 Tick；MUST NOT 认识具体链/步骤类型。

#### Scenario: 组件注册后可查询与触发

- **WHEN** 挂载组件的 Actor 进入 Play，随后调用 `GetCurrent` 与 `ExecuteChainById`
- **THEN** 前者转发属性门面返回当前值、后者起链并返回运行态句柄（链未走完时有效）

#### Scenario: 定义未就绪时跳过注册

- **WHEN** `BeginPlay` 时定义库未就绪
- **THEN** 留 Warning 并跳过注册（不 ensure 刷屏），组件句柄保持无效

### Requirement: 定义库（GameInstance 级最小版）

`TcsIntegration` MUST 提供 `UTcsDefinitionSubsystem`（GameInstance 级），职责：按类发现定义资产 → 校验 → 缓存 → 就绪标记 → 装配到每个世界。

- **发现**：`IAssetRegistry::GetAssetsByClass`（不依赖 `PrimaryAssetTypesToScan` 注册——属 M6 轮）；
- **校验**：空身份 / 双真相 / 重复登记 → 计入失败清单 + Error，不静默跳过。**2026-09-22 改造：空身份判定从 `ChainId.IsNone()` 改为 `!ChainId.IsValid()`**；
- **按 tag 解析**：`ResolveChain(FGameplayTag ChainId)`（**2026-09-22 改造：参数类型改 tag**）；
- **装配到世界**：`OnPostWorldInitialization` 时把缓存定义登记进该世界的消费方子系统（幂等）。

#### Scenario: 资产被自动发现并装配

- **WHEN** 内容目录存在一个合法链资产，GameInstance 初始化后检查
- **THEN** 该资产已进缓存且 `IsRuntimeReady` 为真；世界初始化后消费方可按 tag 查到该链

#### Scenario: 非法资产进失败清单

- **WHEN** 某链资产 `ChainId` 无效或其 `Chain.ChainId` 与自身不一致
- **THEN** 该资产被跳过、失败清单含其路径与原因、其余资产仍可用
