## MODIFIED Requirements

### Requirement: 效果链上下文（黑板）

`TcsEffect` MUST 以运行态结构 `FTcsEffectContext` 承载链执行期间的数据流（04 §2.1 黑板即上下文）：

- **参与者一律为实体身份句柄**（`FTcsCombatEntityHandle`）——`Caster`（施法者）/ `Instigator`（发起者）为单句柄，`Targets` 为句柄数组；**MUST NOT 使用 `AActor*` / `TWeakObjectPtr<AActor>`**（D3-1 Actor 无关性；06 §33"Mass 适配核心零改动"的前提）；
- `EventPayload`（`FInstancedStruct`——触发事件载荷，无事件触发时为默认构造）；
- `Variables`（`TMap<FGameplayTag, double>`——链内变量，SetVar/Branch 类步骤的载体；**2026-09-22 改造：键类型 `FName` → `FGameplayTag`**）——**键的根 = `EffectChainRunVar`**；该根的定义（消费角色 / 声明方 / 形态）与命名契约住 `effect-interpreter` 的门面 API 需求（消费者 `SetRunVariable` / `TryGetRunVariable` 的定义处）与 `gameplay-tag-governance` 的根段注册表，本需求不重复规则；
- `RunSource`（`FTcsSourceHandle`——**链运行态来源锚点**，2026-10-05 新增）：本次运行的"施加方身份"载体，供**由本次运行产生的东西**（`ApplyState` / `ModifyAttribute` 步骤）作来源句柄——发放与继承规则住 `effect-interpreter` 的「链运行态来源锚点与因果边」需求，本需求只声明它是黑板字段；它 MUST NOT 被解读为"谁触发的"（那是下一条字段的事）；
- `CausedBy`（`FTcsSourceHandle`——**链运行态的因果边**，2026-10-05 新增）：**启动本次运行的那个来源句柄**（触发行起链 = 该行的 `Source`；子链起链 = 父链的 `RunSource`；宿主直调可无），写入规则同住 `effect-interpreter` 的「链运行态来源锚点与因果边」需求。**它只是溯源读数**：MUST NOT 参与撤销、MUST NOT 参与共存（续杯 / 叠层）判定——"每条对象自带指向成因的边、查询从已知起点沿对象逐级走"是 Remake 对因果链的既定立场（`SPEC-01` §2），本字段是该立场在链运行态上的第一段落点；
- **属性捕获（CapturedAttrs）与宿主能力引用不住这里**：前者归 TcsDamage 流程上下文（09 §2.1），后者经门面注入点取得（`GetEntityQuery` 等）——黑板只持本次执行的数据；
- 生命期：随链运行态（`FTcsChainRun`）自持，**MUST NOT 跨帧持有**（运行态释放即失效）；
- **MUST NOT 作配置数据载体**（纯运行态）：MUST NOT 加 `BlueprintType`、MUST NOT 出现在任何 Def 资产的可编辑字段里、MUST NOT 加 `EditAnywhere` 类 specifier（措辞口径见 `Documents/combat-system-design/ledger/reflection-terminology.md`）；`RunSource` 与 `CausedBy` 同受本条约束；
- **本约束不涉及类型反射可见性**（`USTRUCT()` 宏的有无）：类型是否反射可见与"能否作配置数据"是两件独立的事——前者由本需求不约束，若未来需让宿主脚本**直接持有/构造**本 struct（而非经门面按句柄访问器读写），须另行评估（台账 SCRIPT-3）。

#### Scenario: 参与者是句柄而非 Actor

- **WHEN** 检查 `FTcsEffectContext` 的参与者字段类型
- **THEN** 全部为 `FTcsCombatEntityHandle`，无任何 Actor 指针形态
