## MODIFIED Requirements

### Requirement: 实体查询实现（PIE）

`TcsIntegration` MUST 提供 `ITcsEntityQuery` 的实现 **`UTcsPieEntityQuery`**（**类名不得用 `UTcsEntityQuery`**——该 U 类名已被接口占用）：

- `EnumerateEntities(TFunctionRef<void(FTcsCombatEntityHandle)>)`：遍历世界中带 `UTcsCombatEntityComponent` 的 Actor 并吐**句柄**（**稳定序**——按组件注册序或 Actor 名排序，同输入同输出）；
- `GetLocation(句柄, FVector&)` / `IsAlive(句柄)`：经**本实现持有的"句柄 ↔ Actor"映射**解析（组件注册时登记、注销时移除）；
- **本实现是宿主侧唯一的"句柄 ↔ Actor"映射点**：机制层（TcsEffect/TcsTargeting/TcsDamage）MUST NOT 依赖该映射，内容资产 MUST NOT 存句柄（授权约束）。
- 注入方式：宿主在 `BeginPlay`/DefLibrary 就绪后调 `UTcsEffectSubsystem::SetEntityQuery`（R3 由测试/宿主接线）。**这是全仓唯一的注入点**——宿主 MUST NOT 再向任何其他门面注入实体查询（`UTcsSkillSubsystem` 自 2026-10-26 起已无该注入面）；"同一实现注入两处"不再是宿主义务，因为第二处已不存在。

#### Scenario: 遍历吐句柄且稳定序

- **WHEN** 世界中三个带组件的 Actor 调用 `EnumerateEntities`
- **THEN** 访问者收到三个**句柄**（非 Actor），两次遍历顺序一致

#### Scenario: 句柄解析为定位与存活

- **WHEN** 以合法句柄调 `GetLocation` / `IsAlive`
- **THEN** 分别返回该 Actor 的坐标与"组件仍注册"的存活判定；未注册/已注销句柄返回 false

#### Scenario: 宿主只需接线一处

- **WHEN** 宿主向 `UTcsEffectSubsystem` 注入一个查询实现，之后走技能门禁第一道
- **THEN** 技能门禁 MUST 读到**同一个**实现给出的 `IsEntityReady` 结果；MUST NOT 存在"注入了却没生效"的第二处待接面
