## 1. 规格 delta

- [x] 1.1 `integration-entity`：ADDED「实体组件『定义未就绪』不是永久失效」+ 5 Scenario
- [x] 1.2 `integration-entity`：ADDED「两条销毁路径必须共用同一份注销实现」+ 5 Scenario
- [x] 1.3 `openspec validate fix-entity-component-late-registration --strict` 通过

## 2. 代码：注册可重入化

- [ ] 2.1 抽出私有方法 `RegisterAsCombatEntity()`：句柄已有效即返回（幂等）；查属性门面 → `RegisterEntity` → 登记查询映射
- [ ] 2.2 `BeginPlay`：已就绪 ⇒ 直接调 2.1；未就绪 ⇒ 订阅 + **二次复查** + 就绪则立即注册并退订
- [ ] 2.3 回调体：**先退订**再补注册（单次信号）；退订走 `FDelegateHandle` 有效性判据

## 3. 代码：提炼销毁期共用函数（D-2 裁定）

- [ ] 3.1 抽出私有方法 `TeardownCombatEntity()`（三件事：退订 → 四段级联 → 句柄置回默认构造）
- [ ] 3.2 `TeardownCombatEntity()` **MUST 幂等**——`DestroyComponent` 会连续触发 `EndPlay` 与 `OnUnregister`（`ActorComponent.cpp:2163-2179`），第二次调用必须因句柄已无效而 no-op
- [ ] 3.3 `EndPlay` 改为调 `TeardownCombatEntity()`（删除原内联的 130 行级联；末尾那行 `EntityHandle = FTcsCombatEntityHandle();` 一并在函数内）
- [ ] 3.4 新增 `OnUnregister` 覆写：调 `TeardownCombatEntity()`
- [ ] 3.5 复核：两条路径的级联顺序为契约（查询映射 → Buff → Skill → 属性账本），提炼时 MUST 逐字保留原注释的四条理由

## 4. 规格回写（本体落盘）

- [ ] 4.1 `openspec/specs/integration-entity/spec.md` 本体应用两条 ADDED 需求
- [ ] 4.2 `openspec validate --all --strict` 全绿

## 5. 档案回写

- [ ] 5.1 `Documents/tcs-arch/11-integration.md`：U-1 标注裁定 = **丙**，附五态实测判据（`Loading` 无内容 / `Failed` 无消费者）
- [ ] 5.2 `Documents/tcs-arch/INDEX.md`：R-20 两处（§7 修订记录 + §8 修复表）；`ARCH-11` U-1 状态行改「已裁定 = 丙，已落地」

## 6. 验证

- [ ] 6.1 `LegendAutoChessEditor Win64 Development` 编译 0 error 0 warning
- [ ] 6.2 `LegendAutoChess Win64 Shipping` 编译 0 error 0 warning
- [ ] 6.3 **PIE**：正常时序（定义库先就绪）⇒ 组件注册成功，行为与变更前一致
- [ ] 6.4 **PIE**：构造「定义库未就绪」时序 ⇒ 组件先跳过、就绪回调后补注册、句柄有效
- [ ] 6.5 **PIE**：`OnUnregister` 后句柄复位，再挂回**发新句柄**（`Id` 与旧值不同）
- [ ] 6.6 **PIE**：`DestroyComponent` 触发两路径连续调用，级联只执行一次（不重复注销、不 ensure）
- [ ] 6.7 反查：`OnDefinitionsReady` 有至少 1 个订阅者（变更前为 0）

---

## 已裁的三条（2026-10-26）

| # | 疑点 | 裁定 |
|---|---|---|
| **D-1** | 退订一套还是两套 | **两者都要**，共用同一份 `FDelegateHandle` 判据 |
| **D-2** | 四段级联是否与 `EndPlay` 共用 | **MUST 共用**——提炼成函数，两条路径都调 |
| **D-3** | 是否会走「`OnUnregister` → 再注册」 | **会**（宿主可能用对象池）；**再注册 MUST 发新 `EntityHandle`**，MUST NOT 复用 |

---

## 🔴 未裁：D-3 派生的连带问题（实施前 MUST 定）

**旧的 `EntityHandle` 已流到这些地方**（实测 `grep` 取证）：

| 持有者 | 位置 |
|---|---|
| `FTcsStateInstance::Entity` | `TcsState/Public/State/TcsStateInstance.h:78` |
| `FTcsLearnedSkillEntry::Entity` | `TcsSkill/Public/Skill/TcsSkillRegistry.h:65` |
| `FTcsAttributeChangedEvent::Entity` | `TcsAttribute/Public/Attribute/TcsAttributeChangedEvent.h:51`（**事件载荷，可能跨帧**） |
| `FTcsParamValueSource::Subject` / `Instigator` | `TcsCore/Public/Parameter/TcsParamValueSource.h:62,66` |
| `FTcsEffectContext::Caster` / `Instigator` / `Targets` | `TcsEffect/Public/Chain/TcsEffectContext.h:32,35,67` |

**现有保证**：句柄**无代际段、永不复用**（`TcsCombatEntityHandle.h:15`）⇒ 注销后旧句柄**悬空表现为「查不到」**，不会误命中别人的槽。本提案已把这条写成 Scenario（「旧句柄悬空表现为查不到而非误命中」）。

**待裁**：是否要在组件里**记录旧句柄历史**（`TArray<FTcsCombatEntityHandle> RetiredHandles`）供排障？

- **我倾向不建**：零消费者不预建；「永不复用」已经提供了安全边界，诊断需求要等真出现对象池用例再加。
- 若你觉得对象池场景下「消失的句柄查不出是谁」会很难排障，我可以加。

---

## 6.4 的可行性存疑（MUST 先解）

**「定义库未就绪」这条时序在 PIE 下能否被构造出来？**

- `UTcsDefinitionSubsystem::Initialize` 在 GameInstance 初始化时跑，**早于**任何 Actor 的 `BeginPlay`；
- 而它的 6 条 `Discover*` 是**同步**的（`AssetData.GetAsset()`）⇒ `Initialize` 返回时已就绪。

⇒ **除非**：① 宿主主动在定义库之前生成 Actor（需装置配合）；② 批次 B（异步预载）落地后「未就绪」成为**正常中间态**。**② 才是这条路径的真实来源**。

**⇒ 若 6.4 无法在 PIE 下构造，MUST 如实登记「该路径未取 PIE」，MUST NOT 标记该任务完成。**
