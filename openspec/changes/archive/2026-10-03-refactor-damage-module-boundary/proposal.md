# Change: 伤害模块边界整肃（框架内定 → 契约默认）

## Why

框架本体边界审计（`Documents/combat-system-design/research/boundary-audit.md`）对 `Source/` 七模块做了全量边界复核，结论是**边界大体干净**——但 `TcsDamage` 的 `Execute` / `Completed` 两个默认模板步骤里有一批**框架替宿主下了结论**的硬编码。它们在审计的原始报告里被打包成一类（"S1"），实际混了**处置完全不同的几类东西**，照原始建议一并删会**把合法接缝当越界**。

本变更按审计定稿的**五分处置**逐类处理：

| 处置 | 含义 | 本提案的动作 |
|---|---|---|
| ① **硬编码常量字段** | 真死字段（取值恒定、零信息量） | 改为从契约键读回（**不删字段**） |
| ② **接缝但名字是宿主概念** | 合法的宿主注入点，只是名字带玩法色彩 | **只补注释 / 登记归属，零行为变更** |
| ③ **框架自下的模型结论** | 框架钉死了宿主无法替换的玩法模型 | 改为**契约默认**（新增委托方法，中性默认 = 现状） |
| ④ **契约默认** | 只引用框架自身契约 + 可被宿主覆盖 | **保留并保护**（删除禁令） |
| ⑤ **契约覆盖** | 框架抢先钉死宿主**已有的**配置点 | 删掉框架的钳制，交回宿主配置点 |

**主判据**（2026-09-30 用户拍板）：*凡是需要宿主词汇的内置策略，插件一律不提供。* 判据体系（四条 + 第三态双条件）已固化进 `unreal-development-workflow` 技能的「框架与宿主的边界」节。

## What Changes

- **⑤ 契约覆盖（本提案最强项，一行删除）**：`FTcsFlowExecute` 的扣血步骤自设零下限（`FMath::Max(0.0, Current - Executed)`），**抢在属性层已有的宿主配置点之前**下了结论——属性层的值域收口只在宿主配了边界时生效（`ETcsAttributeBoundMode::ABM_None` 是默认），即"过量伤害记负数"本是宿主可表达的语义，被这一步静默抹平。**删除该钳制**，值域交回属性层的宿主配置。
- **③ 框架内定 → 契约默认（2 处）**，手法 = **新增委托方法，中性默认实现 = 现状行为**（故默认零行为变更）：
  - **`ResolveExecutedDamage`**——"吸收 → 执行量"的**映射**。现状 `FMath::Max(0.0, Candidate - Absorbed)` 里的**下限钳**是合法的契约默认（收敛到"不介入"），但**减法模型本身不可替换**（宿主只能供吸收量，不能换成按比例分摊 / 带溢出上限等模型）⇒ `MUST NOT` 硬编码在步骤内，改为经委托（默认实现即当前减法 + 零下限）；
  - **`IsLethal`**——致死判定阈值。现状 `EvaluateCurrent(...) <= 0.0` 硬编码在 `Execute` 步内；框架自称"记账而非裁定"（且确实不杀实体），但 `<= 0.0` 这个阈值**就是**一次裁定 ⇒ 改为经委托（默认 `当前值 <= 0.0`）。
- **① 硬编码常量字段（2 处）**：
  - **`FTcsDamageRecord::bHit`** 现状被硬编码为 `true`（取值恒定、零信息量，且替宿主断言"本次一定命中"）⇒ 改为**从契约键 `DamageFlowKey.Hit` 读回**，与同函数 `bCrit` / `Absorbed` 的写法对齐（那两者本就是合法接缝）。**字段本身不删**——`Hit` 键与 `FTcsFlowHit` 步是合法接缝（见审计报告 §5 的 G5 / G13）；
  - **`FTcsDamageRecord::Element` 字段删除**（**BREAKING**：记录形状变更）——全库对它的操作只有一处且是**置空**、零消费者、与 `ClassificationTags` 语义重叠，且违反主判据（框架记录形状里烘进"元素"这个由宿主词表决定的玩法名词）。**本项由 `reroot-gameplay-tag-vocabulary` 移交而来**（见「跨提案依赖」）。删除后元素由 `DamageCategory` 词表 + `ClassificationTags` 承载。
- **② 接缝但名字是宿主概念（零行为变更，只动注释与登记）**：
  - `FTcsDamageRecord::bCrit` 补注释说明"写入者 `Crit` 步**不在默认模板**，故默认恒 false"；
  - `DamageFlowKey` 的 `Hit` / `Crit` / `Kill` 三键：登记"承载宿主判定语义"，**MUST NOT 删除**（删键即断掉步骤之间的接口）。
- **明确不删（删除禁令）**：审计报告 §5 的 **32 项"合规但极易被误判为违规"** 全部保留。本提案**不碰**其中的任何一项——尤以伤害委托的 5 个中性默认实现（必中 / 不暴击 / 无元素 / 原样返回 / 无护盾）、官方默认模板四步骨架、`FTcsSelSelf`、空 Filter = 通过、`GetLocation` 为重。
- **审计 `R-2` 并入本提案（2026-10-01 用户裁定）：规格与实现失步的更正**——原规格声称 `FTcsStepDamage` 执行器"把 `DamageBase` 与 `FormulaParams` 写入上下文（**黑板契约键** `DamageFlowKey.BaseDamage` / 公式参数表）"，而实现**从不写黑板**：它把输入写进**上下文请求字段**（`Context.BaseDamageInput` / `FormulaParams` / `TargetAttrKey`），黑板写入归流程内的 `FTcsFlowBaseDamage` 步。
  **处置 = 改规格，不改实现**——实现的理由有实测支撑：黑板会被 `CollectStart` 重置（收集语义），若改由本步骤从黑板读输入，"链侧预写 + 本步提交"会造成**重复计数（实测 10 + 20 = 30）**。**不改的代价**：规格是唯一真相源，此处描述的是已被证伪的实现 ⇒ 下一个照规格改代码的人会把实测修好的重复计数 bug **重新引入**。
  **并入理由**：该失步落在「Damage 链步骤与执行器」——与本案同属**伤害模块的规格一致性**，且该需求同样被换根提案修改（⇒ 同样必须排在换根之后），合并后只需记**一条**跨提案归档依赖。
- **明确不做（本提案边界）**：
  - **不删任何"零消费者"字段**——判据优先级是**取值归属 > 有无消费者**，"没人在读"不是删除理由；
  - **不含真缺陷 `R-1`**（`Cast<UTcsPieEntityQuery>` 硬耦合 ⇒ 宿主换实现后**静默失效**）——它需要**设计改动**（把"句柄↔Actor 映射"抽为中立容器），是**缺陷而非边界问题**，另开提案排期；
  - **`R-3`（`FName` 时代死常量）不在本提案**——**已并入** `reroot-gameplay-tag-vocabulary` 的 `tasks.md` 第 2.6 项（2026-10-01 用户裁定：与该项同属"`FName` 时代死常量"清理，共用一次编译验证）。
  - **不改 `Content/` 资产、不改任何 ini、不改宿主仓（LAC）与 TGFS 仓**；本提案只产出提案与规格 delta，实现归 `tasks.md`。

## 跨提案依赖（MUST 遵守）

`reroot-gameplay-tag-vocabulary`（词表换根，**已落盘待评审**）与本案**同时 MODIFY 同两条需求**：

| 需求 | 换根改什么 | 本案改什么 |
|---|---|---|
| `damage-primitive`「伤害记录与事件」 | tag 文本（`Tcs.Event.Damage.Recorded` → `TcsEvent.Damage.Recorded`） | `bHit` 取值来源 / `Element` 字段删除 / `bKill` 阈值可覆盖 |
| `damage-primitive`「Damage 链步骤与执行器」 | tag 文本（`Tcs.Flow.Key.*` → `DamageFlowKey.*`、`Tcs.Flow.Template.Default` → `DamageFlowTemplate.Default`） | **`R-2` 更正**：执行器写的是**上下文请求字段**、MUST NOT 写黑板（含重复计数实测理由） |
| `damage-step-library`「标准步骤库（十步）」 | 契约键 tag 文本与根 | `Execute` 步的值域与映射纪律 / `Completed` 步的 `bHit` 读法 |

**OpenSpec 归档按 header 整条替换** ⇒ 后归档者会用自己的粘贴全文覆盖前者，**静默回滚先归档那侧的改动**。

**故本案的 delta MUST 以"换根后的文本"为基准撰写**（已如此撰写：本案 delta 里的 tag 名一律用换根后的新名），并遵守两条硬约束：

1. **归档顺序：`reroot-gameplay-tag-vocabulary` MUST 先归档**，本案在其后；
2. 若换根提案在评审中被改动词表或根名，本案 delta 的对应文本 MUST 同步复核。

## Impact

- **Affected specs**：
  - `damage-primitive`（MODIFIED × 3：「伤害记录与事件」`bHit` / `Element` / `bKill`；「流程委托契约（宿主实现）」5 方法 → **7 方法**；「Damage 链步骤与执行器」**`R-2` 更正**——输入通道是上下文请求字段、MUST NOT 写黑板）
  - `damage-step-library`（MODIFIED × 1：「标准步骤库（十步）」——`Execute` 步的值域与吸收映射纪律、`Completed` 步的 `bHit` 读法）
- **Affected code**（本提案**不改**，实现归 `tasks.md`；改动面收束在**单文件双函数**）：
  - `Source/TcsDamage/Private/Flow/Steps/TcsFlowStepsCore.cpp`——`ExecuteFlowExecute`（删值域钳制、映射改走委托、致死判定改走委托）· `ExecuteFlowCompleted`（`bHit` 改读契约键、删 `Element` 写入点）
  - `Source/TcsDamage/Public/Flow/TcsDamageRecord.h`——删 `FGameplayTag Element` 声明 + `bHit` / `bCrit` 注释
  - `Source/TcsDamage/Public/Flow/TcsDamageFlowDelegate.h`——新增 2 个 `BlueprintNativeEvent` + 中性默认实现
  - `Source/TcsDamage/Private/Flow/TcsFlowStepsRest.cpp`——`FTcsFlowHit` / `FTcsFlowCrit` 调用点与注释
- **兼容性**：
  - **新增委托方法不破坏既有宿主实现**（`BlueprintNativeEvent` + 接口内 `_Implementation` 默认体，UHT 不生成 stub）——宿主**无需适配**，但需**重编译**；
    > **实施期更正（2026-10-03，`tasks.md` 7.5）**：**"宿主零适配"对 UnrealSharp 宿主不完整**。接口声明在**插件模块** `TcsDamage`、实现类在**宿主模块** `TcsDev` 时，宿主那份 **glue 镜像**（`TcsDevDamageFormula.generated.cs`）**不会随接口刷新**——它按"声明该类型的模块"判脏、没有沿"实现了哪个接口"的反向依赖边 ⇒ 首次构建 `CS0535`。**宿主源码仍需零改动**，但 MUST 让**实现模块重新导出** glue（dirty 该模块 / 清理重建）。**纯 C++ 宿主与 fresh clone 不受影响**。修正后的完整表述 = **"宿主源码零适配，UnrealSharp 宿主需重导实现模块的 glue"**。
  - **`Element` 字段删除是 BREAKING**：记录是过网/载荷结构，已序列化的蓝图/资产/DataTable 若手工拼过该字段会在加载期失配。**宿主影响面已核 = 0**（LAC `Source/` + `Script/` 零引用）；
  - **⚠️ `bHit` 的默认行为会变**：默认模板**不含** `FTcsFlowHit` 步 ⇒ `Hit` 键无提交 ⇒ `bHit` 由恒 `true` 变为 `false`。这是"框架不再替宿主断言命中"的直接后果，**消费者若依赖恒 true 必须显式配 `Hit` 步**（详见 `design.md` D3 的备选与被否理由）；
  - **⚠️ 删除零下限后，宿主 MUST 给生命属性配 Min 边界**（否则过量伤害会把生命记为负数）——这是**迁移动作**，见 `tasks.md` 第 4 节。
- **Affected docs**：审计报告（本提案的判据来源）已在库内；实现落地后 MUST 在 `log/decisions-log.md` 追加一条边界整肃决策记录。

## 检查点（人工验证点，实现阶段执行）

1. **编译**：Development 与 Shipping 双配置零错误零新增警告（新增 2 个 `BlueprintNativeEvent` 的 UHT 生成）。
2. **默认零行为变更**：不配任何 delegate 时，一次默认模板伤害的 `Base` / `Final` / `Executed` / `Absorbed` / `bKill` / 记录事件与整肃前**逐字段一致**（唯一例外 = `bHit`，见上）。
3. **值域交回宿主**：给生命属性配 `Min = 0` 后，过量伤害钳到 0；**不配 Min 时值可以为负**（证明框架不再越权钳制）。
4. **两个新钩子可覆盖**：脚本层（C#）与 C++ 两种实现各验一次——`ResolveExecutedDamage` 返回自定义映射后执行量随之一致；`IsLethal` 改判后 `bKill` 随之变化。
5. **`bHit` 两态**：不配 `FTcsFlowHit` 步时 `bHit = false`；配上且命中时 `= true`。
6. **`Element` 零残留**：全仓反查只见步骤名 / 事件名 / 注释。
7. **提案校验**：`openspec validate refactor-damage-module-boundary --strict --no-interactive` 通过。
