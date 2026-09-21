# Change: 落地标准步骤库、官方默认模板与 Damage 链原语（damage-step-library / damage-primitive）

## Why
plan2 Task 4 动工前的规格先行提案。Task 3 交付了流程机制层（模板登记 + 三层值空间 + 流程属性黑板 + 步骤注册表与自注册宏 + 同步单帧解释器 + 收集事件协议），但**表里还没有一颗步骤**、**没有官方默认模板**、**链上还没有 Damage 原语**——竖切剧本的 `WaitDelay → SelectTargets → Damage` 链条缺最后一环，且 `09 §2.2` 的标准阶段表与 `D7-2/PV-7`（流程零计算）尚未落成可执行规格。

本任务同时兑现两件设计承诺：①**D7-5 流程管线宿主化**——标准十阶段降级为"插件自带的标准步骤库 + 官方默认模板（可整表替换）"，流程阶段构成仍属项目知识；②**D7-6 伤害修改器唯一通道**——修改器 = 触发行 + 单步链，其响应经 `ModifyFlow` 提交进流程黑板（收集 ≠ 消费）。

## What Changes
- 新增能力规格 **`damage-step-library`**（4 条需求）：
  1. **标准步骤库（十步）**：`CollectStart`（发流程开始事件 + 重置收集）/ `PreHit` / `Hit` / `Crit` / `Element` / `BaseDamage`（**接收输入值写黑板**——流程零计算，PV-7）/ `AfterDamage` / `PreExecute`（收集免疫/减伤候选）/ `Execute`（裁决 → 宿主护盾 hook → **M2 事务扣血** → 成功才消费）/ `Completed`（发完成事件 + 记录）；全部经 `UE_DEFINE_FLOW_STEP_EXECUTOR` 自注册；
  2. **步骤 Conditions 挂点**：每步可带 `TArray<FInstancedStruct> Conditions`（复用 D4-5 条件最小集的**数据谓词**形状），R3 实现 `HasAllTags` / `Chance` 两个求值器（其余结构留位）；**由各执行器首行调用共享助手**（步骤无公共基类，D4-16）；
  3. **通用数据步骤**：`FTcsFlowModify{TargetKey, Op, Operand(FTcsParamValue), SortKey}`（数据化黑板写入——"破甲阶段" = 一个数据步骤）与 `FTcsFlowDelegate{TargetKey, Delegate}`（数据化委托调用）；
  4. **官方默认模板**：`CollectStart → BaseDamage → Execute → Completed` 一份，登记为 `Default`（**可整表替换**；Hit/Crit/Element/PreHit 的结构与执行器同批定义但 R3 模板不组装）。
- 新增能力规格 **`damage-primitive`**（3 条需求）：
  1. **`FTcsStepDamage` 链步骤与执行器**：`{FlowTemplateId（空=默认模板）, DamageBase(FTcsParamValue), FormulaParams(TMap<FName,FTcsParamValue>), Delegate(TScriptInterface<UTcsDamageFlowDelegate>), HealthAttrKey(FTcsAttributeName)}`——执行器构流程上下文 → `RunTemplate`；经 `UE_DEFINE_EFFECT_STEP_EXECUTOR` **跨模块自注册进 TcsEffect 注册表**（D4-14 的又一次实证）；
  2. **`UTcsDamageFlowDelegate`（宿主实现，降级逃生口）**：`GetBaseHitRate` / `GetBaseCritRate` / `ResolveElement` / `CalculateBaseDamage`（**PV-7 收窄后仅宿主特殊公式实现，普通项目零 delegate**）/ `ModifyShield`；全部带中性默认实现（宿主不实现也能跑）；
  3. **`FTcsDamageRecord` + 事件 + 环形缓冲**：扁平字段（FlowId / Source / Target / 元素 / Hit / Crit / Base / Final / Executed / Absorbed / Kill / 序号 / 时刻）、`Completed` 步骤填充、经**原生 Tag** `Tcs.Event.Damage.Recorded` 走总线立即通道、`UTcsDamageSubsystem` 持环形缓冲与读取口。
- **不做的**（非目标，保持 R3 边界）：不做任何公式（基础伤害值 = 链步骤配置的解算结果输入，PV-7/D7-2；命中/暴击/元素 = delegate 或宿主步骤）；不做元素克制表；不做护盾系统（只留 `ModifyShield` hook）；不做伤害数字 UI/统计报表；**不做 AttrCapture（属性捕获）**与 `ETcsAttrCaptureFrom`（见下）；不做 DoT；不做模板重定向栈（`FFlowRedirect`，随 M3 状态轮）；不做模板/链**资产类**（见下）；不实现复制（记录已是扁平字段，将来上 FastArray——见 `project.md` 过网结构纪律）。

## 顺延与落点裁决

1. **AttrCapture 整体顺延（本次不落 `ETcsAttrCaptureFrom` 与捕获填充/读取）**：设计（09 §2.1）的 "本次攻击攻击力 +10%" 与"流程内读取一致性"在 R3 竖切**零消费者**（竖切只有一条 `DamageBase` 直配的字面量链，没有任何属性修正型修改器）——按"零消费者不预建"顺延，**记入台账**（触发条件 = M5 技能账本轮或第一个属性修正型修改器的真实场景）。`CapturedAttrs` 字段已就位，届时只补填充与读取语义。
2. **`FTcsDamageRecord.Kill` 的语义钉为"记账"而非"裁定"**：`bKill = 本次 Execute 事务提交后目标 Health ≤ 0`——**宿主仍可自行决定死不死**（09 §3：死亡判定归宿主；属性模块零词汇）。这样记录能反映"这一击是否把血打空"，而死亡规则不被框架窃取。
3. **官方默认模板以 C++ 组装登记（无资产类）**：计划写"默认模板注册（Subsystem 初始化）"——**模板/链资产类的落点问题与 Task 2/3 交接注记里那两条同源**（计划全篇未点名载体），统一归 **Task 5/6**（DefLibrary 资产发现那一轮）处理；R3 的官方默认模板是代码组装的标准件，宿主可整表替换（D7-5）。
4. **文件落点**：十步与两个数据步骤住 `Public/Flow/Steps/` + `Private/Flow/Steps/`（计划指定）；条件求值助手住 `Public/Flow/TcsFlowStepConditions.h`；链原语住 `Public/Chain/TcsStepDamage.h` + `Private/Chain/TcsStepDamage.cpp`（对齐 TcsTargeting 的 `Chain/` 落点）；delegate 与 record 住 `Public/Flow/`。
5. **承接句柄化**：`FTcsDamageRecord` 的 `Source` / `Target` **用实体句柄**（不是计划 sketch 里的 `AActor*`）——与 2026-09-20 的上下文句柄化同批口径。

## Impact
- Affected specs：`damage-step-library`（新建）、`damage-primitive`（新建）——**无 MODIFIED/REMOVED**（`damage-flow` 的既有 6 条需求均不变；AttrCapture 顺延故不动上下文需求）。
- Affected code（`Source/TcsDamage/`）：新增 `Public/Flow/Steps/`（十步 + `FTcsFlowModify` + `FTcsFlowDelegate` 各 .h）、`Private/Flow/Steps/`（执行器 .cpp）、`Public/Flow/TcsFlowStepConditions.h`、`Public/Flow/TcsDamageFlowDelegate.h`、`Public/Flow/TcsDamageRecord.h`、`Public/Chain/TcsStepDamage.h` + `Private/Chain/TcsStepDamage.cpp`、`Public/Flow/TcsDamageRecordedEvent.h`（原生 Tag）；`UTcsDamageSubsystem` 增默认模板注册（`Initialize`）+ 记录环形缓冲；装置扩检（正路：默认模板跑通 + 记录事件到达 + 环形缓冲可读；拒绝面：Conditions 不过则跳过）。
- 决策依据：`09-module-damage.md` v4（§2.2 标准步骤表 / §2.3 修改器唯一通道 / §2.4 接口与记录 / §3 收集≠消费）；D7-1~D7-7（尤其 **D7-5 流程管线宿主化**、**D7-6**、**D7-2/PV-7 流程零计算**）；D4-5（条件最小集）、D4-14（注册制分派）、D5-5 v3（黑板折叠共享件，Task 3 已落）；2026-09-18 Tag 公约；2026-09-20 句柄化与过网结构纪律。
- 验证：UBT 编译零警告 + 装置定向检查（默认模板四步走通、`BaseDamage` 读数 = 配置的 `DamageBase`、`Execute` 事务扣血生效、记录事件到达且字段正确、环形缓冲可读、Conditions 跳过语义）+ 跨模块注册自检（`FTcsStepDamage` 执行器在 Effect 注册表可查）+ 依赖面自检。

## 提案内钉名（计划/设计未钉或需收窄）

| 项 | 钉法 | 依据 |
|---|---|---|
| 十步命名 | `FTcsFlow<Name>`（`FTcsFlowCollectStart` / `FTcsFlowBaseDamage` / `FTcsFlowExecute` / `FTcsFlowCompleted` …） | 计划未钉；**`FTcsFlowStep*` 前缀已被 Task 3 的注册表族占用**（`FTcsFlowStepExecute` / …Registry），视觉撞车可读性差；`FTcsFlow<Name>` 与计划已定的两个数据步骤 `FTcsFlowModify` / `FTcsFlowDelegate` 同族 |
| Conditions 挂点 | 每个步骤 struct 持**同名字段** `TArray<FInstancedStruct> Conditions`；各执行器**首行**调用共享助手 `ShouldRunFlowStep(Conditions, Context)`；求得 false → 跳过该步（返回 true 继续流程） | 步骤**无公共基类**（D4-16）→ 没有基类虚函数可挂；用"同名字段 + 首行调用"的纪律替代（写进头注释与规格） |
| R3 条件求值器 | `FTcsConditionHasAllTags{ TArray<FGameplayTag> Tags }`（用 `ClassificationTags` 判定）+ `FTcsConditionChance{ double Probability }`（**确定性纪律例外**：概率型条件显式接受随机，其随机源经注入——R3 装置用固定种子/固定值） | 计划写"R3 只实现 HasAllTags/Chance 两个条件"；Chance 与 D0-1 确定性冲突 → 必须显式声明其随机源（若宿主不注入随机源则退化为固定值，可复现） |
| `Execute` 的 M2 扣血 | `BeginBatch(目标)` → 读当前 Health → 写新值（`SetBaseValue`？否——**扣血走"挂修正器"还是"改基值"？** R3 钉：**改基值**（`SetBaseValue`），因为伤害是"生命值被削减"而非"增益修正"；`Commit` 提交） | 02 §2.2a 的 `SetBaseValue` 是"等级成长等宿主升级事务的落点"——伤害扣血同属"外部对基础值的改写"；挂修正器会把伤害变成可被来源撤销的修正（不符合伤害语义：伤害不可回滚）。**该钉法是本提案的判断，可被否决** |
| 记录事件 Tag | `Tcs.Event.Damage.Recorded`（原生声明，`Public/Flow/TcsDamageRecordedEvent.h`） | 09 §2.4 写 `Damage.Record`（早于 2026-09-18 公约）→ 按公约落（`Tcs.Event.<域>.<事件名>`，PascalCase 动词短语） |
| 环形缓冲 | 住 `UTcsDamageSubsystem`（容量 = 常量 `Tcs.Damage.RecordBufferSize` 初值 128，CVar 可调）+ `GetRecentRecords(TArray<FTcsDamageRecord>& Out)` 读取口 | 计划写"环形缓冲（统计）"未钉宿主；记录是流程产物（流程门面持有最自然），且与模板登记表同居一处便于将来接 FastArray |
| delegate 默认实现 | `UTcsDamageFlowDelegate` 的五个函数**全部带中性默认实现**（命中/暴击 = 1.0 或 0.0、元素 = 空 Tag、`CalculateBaseDamage` = 返回输入、护盾 = 不减） | "普通项目零 delegate"（PV-7 D7-2 收窄）要求宿主不实现也能跑通默认模板；`CalculateBaseDamage` 作为**降级逃生口**，默认实现即"不改动输入" |
| **上下文补请求字段**（实施期发现，PIE 实测暴露） | `FTcsDamageFlowContext` 增 `double BaseDamageInput` 与 `FTcsAttributeName TargetAttrKey`（**调用方指定的请求参数，不随 `CollectStart` 的收集重置清除**）；`FTcsFlowBaseDamage` 读前者（不再从黑板读）、`FTcsFlowExecute` 的属性键解析 = 步骤级 `AttrKey` 优先、否则用后者 | **PIE 首轮 4 项 FAIL 暴露**：①链步骤预写黑板 → 被 `CollectStart.Reset()` 冲掉（输入归零）；②不写黑板而由 `BaseDamage` 步骤自提交 → 与链侧预写**重复计数**（实测 10+20=30）；③**插件组装的官方默认模板不可能知道项目词表**（`Health` 是项目侧的），`AttrKey` 留空导致默认模板永不扣血。请求字段把"输入与目标"从"收集产物"里分离出来——黑板 = 收集产物（会被重置），上下文请求字段 = 调用方输入（不随收集清除） |
| `FTcsStepDamage` 的目标 | **消费 `Context.Targets`**（不内嵌 selector——D4-4 v2） | 计划明确；与 Task 2 的目标选择步骤串联 |
| **上下文补门面弱引用**（实施期发现的必要项） | `FTcsDamageFlowContext` 增 `TWeakObjectPtr<UTcsDamageSubsystem> Owner`，由 `RunTemplate` 填充；步骤经 `Context.Owner->GetWorld()` 取 M2 属性门面 / 事件总线 / 时钟 | **句柄化后上下文里没有 Actor 可借道取世界**（`Context.Attacker` 已是句柄，旧写法 `Attacker->GetWorld()` 不再可用）；该缺口在 Task 3 的装置里已现苗头（装置只能靠 `GEngine` 世界查找兜）——补 `Owner` 是**与链运行态 `FTcsChainRun::Owner` 同款**的做法（链侧自 Task 1 起就持门面弱引用） |

## 检查点
落点验收 = UBT 编译（Development Editor，零警告）+ 装置定向检查（默认模板四步、`BaseDamage` 读数、`Execute` 事务扣血、记录事件与环形缓冲、Conditions 跳过）+ 跨模块注册自检（`FTcsStepDamage` 在 Effect 注册表）+ 依赖面自检（不 include TcsTargeting/TcsState/TcsSkill/TcsIntegration）。AttrCapture、模板/链资产类、复制不在本提案。
