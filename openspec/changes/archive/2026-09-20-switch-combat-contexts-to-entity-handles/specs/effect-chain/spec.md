## ADDED Requirements

### Requirement: 效果链上下文（黑板）

`TcsEffect` MUST 以运行态结构 `FTcsEffectContext` 承载链执行期间的数据流（04 §2.1 黑板即上下文）：

- **参与者一律为实体身份句柄**（`FTcsCombatEntityHandle`）——`Caster`（施法者）/ `Instigator`（发起者）为单句柄，`Targets` 为句柄数组；**MUST NOT 使用 `AActor*` / `TWeakObjectPtr<AActor>`**（D3-1 Actor 无关性；06 §33"Mass 适配核心零改动"的前提）；
- `EventPayload`（`FInstancedStruct`——触发事件载荷，无事件触发时为默认构造）；
- `Variables`（`TMap<FName, double>`——链内变量，SetVar/Branch 类步骤的载体）；
- **属性捕获（CapturedAttrs）与宿主能力引用不住这里**：前者归 TcsDamage 流程上下文（09 §2.1），后者经门面注入点取得（`GetEntityQuery` 等）——黑板只持本次执行的数据；
- 生命期：随链运行态（`FTcsChainRun`）自持，**MUST NOT 跨帧持有**（运行态释放即失效）；
- 结构 MUST NOT 为反射类型（纯运行态，非配置数据）。

#### Scenario: 参与者是句柄而非 Actor

- **WHEN** 检查 `FTcsEffectContext` 的参与者字段
- **THEN** 全部为 `FTcsCombatEntityHandle`（无任何 Actor 类型）——链与流程对"无 Actor 实体"（未来 Mass）同样可表达

#### Scenario: 目标存活由宿主语义判定

- **WHEN** 步骤需要确认某目标是否仍有效
- **THEN** 经注入接口 `IsAlive(句柄)` 询问宿主（框架不猜存活规则，也不持有 Actor 弱引用）
