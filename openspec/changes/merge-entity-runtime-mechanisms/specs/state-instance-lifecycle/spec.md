## MODIFIED Requirements

### Requirement: 状态实例句柄

TcsState MUST 提供 `FTcsStateHandle`（`USTRUCT(BlueprintType)`）：两字段 `int32 Index` 与 `int32 Generation`（**展平形态**，与 `FTcsChainRunHandle` / `FTcsEffectTriggerInstance` 同款——反射与宿主脚本往返需要可反射的标量字段，对象身份不外传）。

**★ 本类型 MUST 保持为独立类型**（MUST NOT 与 `FTcsCastRunHandle` / `FTcsSkillEntryHandle` 合并——合并会移除"跨桶误传编译期失败"这道现存护栏，判据见 `entity-runtime-mechanism` 能力的「统一运行态句柄判据与转换」需求）；但**判据 / 相等比较 / 哈希 / 池位模式转换的语义 MUST 收敛到一处公共件**，本类型委托它。

- `IsValid()`：`Index >= 0` 且 `Generation > 0`（0 恒为“从未分配”）；
- 相等比较按 `(Index, Generation)` 全等；MUST 提供 `GetTypeHash`（供 `TMap` / `TSet` 键控）；
- `Index` MUST 是实体桶内槽位下标；**分配代际 MUST 由 TcsState 模块出线的进程级发号器统一发放不复用的正奇数号**，世界拆解 MUST NOT 复位发号器；不同桶、被删除后重建的桶、不同世界即使使用同一下标，也 MUST NOT 发放相同实例身份；
- 释放 MUST 将该槽代际置为偶数；再次分配 MUST 取新的进程级奇数号，MUST NOT 仅将桶内旧代际 +1 当作分配身份；
- 句柄仍相对其世界门面有义，MUST NOT 将旧句柄跨世界重用；旧身份在新世界 MUST 无法命中。反射 `int32` 的正值代际空间耗尽时 MUST 明确失败，MUST NOT 回绕复用。

#### Scenario: 未分配句柄无效

- **WHEN** 取一个默认构造的 `FTcsStateHandle` 并调 `IsValid`
- **THEN** 返回 false（`Index` 为负或 `Generation` 为 0）

#### Scenario: 分配出的句柄可直接判有效

- **WHEN** 施加一个状态并取回其句柄（`Index` 可能为 0）
- **THEN** `IsValid` 返回 true（合法性判据是“`Generation` 非 0”，不是“下标非 0”——下标 0 是合法槽位）

#### Scenario: 用过的句柄在槽位回收后失配

- **WHEN** 移除一个状态使槽位回收，随后用旧句柄查询
- **THEN** 查询被拒绝（代际失配），且同一槽位新分配的实例不受影响

#### Scenario: 不同实体的相同槽位身份互异

- **WHEN** 两个实体各自分配桶内第 0 个状态实例
- **THEN** 两个句柄的 `Index` 可以相同，`Generation` MUST 互异；各自调用 `GetState(Handle)` 命中各自实体，按句柄移除其中一个 MUST NOT 影响另一个

#### Scenario: 新世界不重用旧状态身份

- **WHEN** 世界 A 建出状态实例后被拆解，世界 B 再分配同一下标
- **THEN** 新句柄的代际与 A 互异，A 的旧句柄在 B 查询返回 nullptr

### Requirement: per-entity 桶注册表与代际校验

TcsState MUST 提供 `FTcsEntityRuntimeRegistry<FTcsStateInstance>`：**per-entity 桶**——`TMap<FTcsCombatEntityHandle, TUniquePtr<Bucket>>`（`TUniquePtr` 间接层，`TMap` 扩容**不搬移桶地址**）。**该注册表是共享层的统一模板实例**（详见 `entity-runtime-mechanism` 能力的「统一 per-entity 桶与注册表」需求）——它**取代**了此前状态层专属的 `FTcsStateRegistry` / `FStateBucket`，且其桶**载体改为 `TTcsInstancePool`**（不再手写槽位 + 代际 + 空闲栈）。

桶 = 槽位数组 + 每槽代际数组 + 空闲槽栈（**由 `TTcsInstancePool` 提供**）。分配 MUST 优先复用空闲槽；每次分配的身份 MUST 遵守「状态实例句柄」需求的进程级唯一代际规则；释放将代际 +1 置为偶数，使旧句柄失效。MUST NOT 令各桶各自从代际 1 重新发放相同实例身份。

**脏句柄一律拒绝 + `Warning`**（不 `ensure`——这是**时序竞态**语义，与“配置错误”分开）：代际失配 / 下标越界 / 实体未注册。拒绝 MUST NOT 影响同桶其它实例。

`UnregisterEntity` 删桶时 MUST 回收该实体全部槽位（否则池占用统计残留）。

#### Scenario: 脏句柄被拒绝且不影响他人

- **WHEN** 对同一实体建两个实例后，用代际失配的旧句柄调移除
- **THEN** 拒绝该调用并留 Warning，两个实例仍在册（无实例被误移除），不出现 ensure

#### Scenario: 桶地址不随登记表增长失效

- **WHEN** 持续为不同实体建桶与实例（`TMap` 发生多次扩容）后，用早先实体的句柄查询
- **THEN** 查询照常命中（桶经 `TUniquePtr` 间接持有，登记表扩容不搬移桶地址）

#### Scenario: 实体销毁后桶被回收

- **WHEN** 对某实体建若干实例后调 `UnregisterEntity`
- **THEN** 该实体全部实例被移除、桶被删除，且用其旧句柄查询一律被拒绝

#### Scenario: 与技能侧共用同一桶实现

- **WHEN** 检查状态侧与技能侧的桶类型
- **THEN** 两者是**同一模板的不同实例化**（`TPayload` 分别为 `FTcsStateInstance` 与施法运行态），MUST NOT 存在第二份手写的槽位 / 代际 / 空闲栈逻辑
