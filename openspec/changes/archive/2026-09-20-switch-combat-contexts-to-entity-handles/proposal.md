# Change: 战斗上下文参与者改用实体身份句柄（效果链 / 目标选择 / 伤害流程三处 + 实体查询补解析能力）

## Why
2026-09-20 用户审阅 Task 3 代码时指出：**流程/效果链上下文里的参与者（Attacker/Instigator/Targets）现在是 `AActor*`，与设计承诺冲突**——

- 06 §33 明文承诺："**注册表 Actor 无关性（D3-1）保留为未来前提**，届时新增 `TcsMass` 模块，**核心零改动**"；Mass 备忘同款。
- 10 §2.3 给 `ICombatEntityQuery` 的原文是"实体遍历（**稳定序全量句柄**）/ GetLocation / IsAlive"——**设计那边已经在说句柄**。
- 而 `AActor*` 是**计划二自己选的**（plan2 的 Interfaces 块写死；04 §2.1 / 09 §2.1 两份设计文档都**没写字段类型**）。
- 结论（用户拍板）：毒池/捕兽夹这类**场景参与者同样是战斗实体**（判据 = 参不参与战斗：造伤害/被当目标/持属性，而非"是不是人形单位"）；故参与者一律改 `FTcsCombatEntityHandle`。

**不处理的后果**：Mass 落地那天"核心零改动"兑现不了——链与流程表示不了无 Actor 的实体，宿主只能给 Mass 单位配代理 Actor（正是 Mass 要避免的），或放弃链/流程对 Mass 的复用。

## What Changes
- **`entity-query-contract`（MODIFIED + ADDED）**：`ITcsEntityQuery` 的遍历改为**句柄**（`EnumerateEntities(TFunctionRef<void(FTcsCombatEntityHandle)>)`），并补设计点名的两支解析能力：`GetLocation(Handle, FVector& OutLocation) -> bool`、`IsAlive(Handle) -> bool`（**句柄 → Actor 的映射归宿主**：谁注册实体谁掌握映射，机制层不需要它）。
- **`effect-chain`（ADDED）**：新增"效果链上下文（黑板）"需求，把 `FTcsEffectContext` 的字段与**参与者类型 = 实体句柄**钉进规格（此前该结构的字段形状不在任何规格里——本次一并补上，避免再靠计划 sketch）。
- **`targeting-strategy`（MODIFIED）**：`Resolve` / `Pass` 签名的目标类型改句柄（`TArray<FTcsCombatEntityHandle>& OutTargets`、`Pass(FTcsCombatEntityHandle Candidate, …)`）。
- **`damage-flow`（MODIFIED）**：`FTcsDamageFlowContext` 的 `Attacker` / `Instigator` / `Targets` 改句柄；`Targets` 不再是弱指针数组（句柄无生命周期语义——存活查询走注入接口 `IsAlive`）。
- **不做的**：不改 `FTcsCombatEntityHandle` 本身（仍是进程内发号，**不是网络身份**——跨机身份属复制契约的活，另案）；不加"句柄 + Actor 缓存"双字段（用户选择单一真相：句柄）；不动 TcsAttribute（M2 本来就全程句柄）；不实现复制。

## 顺延与落点裁决

1. **代码影响面（已提交的 Task 1/2 一并改）**：`FTcsEffectContext`（TcsEffect）、`ITcsEntityQuery`（TcsEffect）、`FTcsTargetSelectorStrategy` / `FTcsTargetFilterStrategy` / `FTcsStepSelectTargets.cpp`（TcsTargeting）、`FTcsDamageFlowContext`（TcsDamage）、三处装置（TcsEffect/TcsTargeting/TcsDamage 的 Testing）——**规格与代码同步改**（Task 1/2 的规格要发 MODIFIED delta，见上）。
2. **装置侧适配**：装置们 spawn 的临时 Actor 现在要**登记为实体**（经 `UTcsAttributeSubsystem::RegisterUnit` 拿句柄）并在装置内自建句柄↔Actor 映射（宿主侧职责的样板）。
3. **Task 5 交接注记**：`UTcsCombatEntityComponent` 注册实体时把句柄留作"我的身份"、并在实体查询实现里提供句柄→Actor 的映射（宿主侧唯一映射点）。
4. **网络身份另案**：句柄是进程内发号（原子递增、永不复用）——**不能跨机**。跨机身份（NetGUID / 宿主复制 Id）属复制契约（台账 T-3）的活，与本次切换解耦。

## Impact
- Affected specs：`entity-query-contract`（MODIFIED + ADDED）、`effect-chain`（ADDED）、`targeting-strategy`（MODIFIED）、`damage-flow`（MODIFIED）。
- Affected code：`Source/TcsEffect/`（`TcsEffectContext.h` / `Host/TcsEntityQuery.h` + 装置）、`Source/TcsTargeting/`（两份契约头 + `TcsStepSelectTargets.cpp` + 装置）、`Source/TcsDamage/`（`TcsDamageFlowContext.h` + 装置）。
- 验证：UBT 编译零警告 + 三套装置命令复跑（`Tcs.Test.Effect` / `Tcs.Test.Targeting` / `Tcs.Test.Damage.Flow`）+ 依赖面不变。

## 提案内钉名

| 项 | 钉法 | 依据 |
|---|---|---|
| 遍历签名 | `EnumerateEntities(TFunctionRef<void(FTcsCombatEntityHandle)> Visitor)` | 10 §2.3"稳定序全量句柄"；改后宿主实现直接吐句柄，不再需要为每次枚举构造 Actor |
| 解析能力 | `GetLocation(Handle, FVector& Out) -> bool`、`IsAlive(Handle) -> bool` | 10 §2.3 点名的两支；**句柄→Actor 归宿主**（注册方掌握映射），机制层不引入 Actor 依赖 |
| 目标集类型 | `TArray<FTcsCombatEntityHandle>`（非弱指针） | 句柄无生命周期语义；"目标还在不在"用 `IsAlive` 查询（宿主语义），框架不猜 |
| 上下文缓存 | **不加** Actor 缓存双字段 | 用户拍板单一真相：句柄；需要 Actor 的宿主自行映射 |
| 旧装置 Actor | 装置内自建 句柄↔Actor 映射（宿主样板） | 装置即"最小宿主"——顺带实证宿主侧映射的写法 |
| **编辑器授权约束** | `FTcsCombatEntityHandle` **MUST NOT 进内容资产**：它是**运行期发号**（发号器实例内原子递增、永不复用；R3 由 M2 门面按世界发号）——同一实体在不同 PIE 会话/不同机器上的号不同；详情面板里它只是 `int64 Id` 的数字框（**机械可配、语义不可授权**）。编辑器里要指定"哪个实体"的场合 MUST 用**稳定可授权的内容标识**（角色/槽位/出生点的 FName），运行期由宿主解析成句柄——与既有 Def 引用规约同款（`FName` Id + DefLibrary 解析） | 2026-09-20 用户提问"换句柄会不会影响编辑器配置"的答复；`FTcsCombatEntityHandle.h` 的发号器为进程内原子递增；06 §33 的"宿主映射"口径 |

## 检查点
落点验收 = 编译零警告 + 三套装置命令全绿（Effect 挂起/唤醒、Targeting 选择过滤、Damage 流程与收集）+ 规格库 `validate --strict` 全绿 + 迁移面核对（`git diff --stat` 覆盖清单与"Impact"节一致）。
