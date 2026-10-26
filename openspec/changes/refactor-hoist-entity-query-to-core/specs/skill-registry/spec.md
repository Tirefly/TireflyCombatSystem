## MODIFIED Requirements

### Requirement: 实体可操作性门禁（技能侧消费面）

技能侧 MUST 以注入的实体查询契约判定"门禁第一道：实体可操作"，且**MUST 经共享注入面取查询，MUST NOT 自持槽位**：

- **技能门面 MUST NOT 提供 `SetEntityQuery` / `GetEntityQuery`**：实体查询的注入槽位全仓只有一处（`UTcsEffectSubsystem`，见 `entity-query-contract` 的「注入面唯一」需求）。技能侧 MUST 经 `UTcsEffectSubsystem::GetEntityQuery()` 取；
- **降级条件是「宿主整个未注入」而不是「技能侧未注入」**：共享槽位为空是配置状态不是错误（同 `ITcsParamTableReader` 可空的口径）——此时门禁 MUST 降级为"只判句柄有效"，**MUST NOT** ensure、MUST NOT 拦死。**该改判的理由**：门禁降级的语义是"宿主没有提供实体查询能力"，而不是"技能模块忘了接线"——后者的失败面（`ARCH-11` 台账 `WAIT-12`）恰恰是**静默错配**，把它当作"合法降级"会掩盖缺陷；
- 门禁判据 MUST 为 `ITcsEntityQuery::IsEntityReady(FTcsCombatEntityHandle)`，**MUST NOT** 用 `IsAlive` 代替——两者是正交轴（`IsAlive` 答"宿主认为它活着吗"、`IsEntityReady` 答"框架能不能在它身上操作"），`IsAlive` 的 PIE 实现是"映射里还有这个句柄"而非"活着"，拿它当门禁会**双向误判**（死亡触发的被动 / 复活技能被误杀；待销毁尸体被误放）；
- **MUST NOT 反查 `TcsIntegration`**（依赖方向单向——`TcsIntegration` 是终端模块，反查会成环）。经 `UTcsEffectSubsystem` 取查询**不构成新依赖边**：`TcsSkill` 已依赖 `TcsEffect`（起链在用）。

#### Scenario: 未注入时降级而非拦死

- **WHEN** 共享槽位（`UTcsEffectSubsystem`）里从未注入实体查询实现时走门禁第一道（实体句柄本身有效）
- **THEN** 门禁**通过**（降级为"只判句柄有效"），无 ensure、无红字——MUST NOT 因"宿主未提供实体查询能力"把一切施法拦死

#### Scenario: 注入后按宿主判定拒绝

- **WHEN** 共享槽位里已注入实体查询实现，且宿主对某句柄返回 `IsEntityReady == false`
- **THEN** 门禁第一道**拒绝**（返回具名原因），MUST NOT 继续走后续门禁；同一句柄若 `IsAlive == true` 也 MUST NOT 因此放行——两问彼此独立

#### Scenario: 技能侧不存在私有槽位

- **WHEN** 全仓检索 `ITcsEntityQuery` 的**槽位持有者**（`UPROPERTY` 成员 + 配套 `Set`/`Get`）
- **THEN** 只命中 `UTcsEffectSubsystem` 一处；`UTcsSkillSubsystem` 上 MUST NOT 存在该成员，其门禁读数 MUST 随 `UTcsEffectSubsystem` 的注入状态变化而变化（而不是随某个只对技能侧生效的注入变化）
