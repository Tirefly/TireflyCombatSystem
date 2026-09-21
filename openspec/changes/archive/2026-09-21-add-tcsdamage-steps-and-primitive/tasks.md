## 1. Implementation

### 标准步骤库（Public/Flow/Steps/ + Private/Flow/Steps/）

- [ ] 1.1 `Public/Flow/TcsFlowStepConditions.h`：`ShouldRunFlowStep(Conditions, Context) -> bool` 共享助手 + `FTcsConditionHasAllTags` / `FTcsConditionChance` 两个数据谓词 struct（含各自求值分派）
- [ ] 1.2 十步 struct（各 .h）：`FTcsFlowCollectStart` / `FTcsFlowPreHit` / `FTcsFlowHit` / `FTcsFlowCrit` / `FTcsFlowElement` / `FTcsFlowBaseDamage` / `FTcsFlowAfterDamage` / `FTcsFlowPreExecute` / `FTcsFlowExecute` / `FTcsFlowCompleted`——每个持同名字段 `TArray<FInstancedStruct> Conditions`
- [ ] 1.3 十步执行器（`Private/Flow/Steps/*.cpp`，每个文件末尾 `UE_DEFINE_FLOW_STEP_EXECUTOR`）——**每个执行器首行调 `ShouldRunFlowStep`**；Core 四步（CollectStart / BaseDamage / Execute / Completed）为完整实现，其余六步最小实现（发事件 / 写黑板 / 可留空实现）
- [ ] 1.4 `FTcsFlowExecute` 的裁决与扣血：免疫/减伤候选（`Blackboard.FindSubmits` 读回带消耗策略的提交，键 `PreExecute` 收集）按 `SortKey` 选一 → `ModifyShield` hook → `BeginBatch/SetBaseValue/Commit` 事务扣血 → 成功才调 `OnConsumed`
- [ ] 1.5 通用数据步骤：`Public/Flow/Steps/TcsFlowModify.h`（`FTcsFlowModify`）+ `TcsFlowDelegate.h`（`FTcsFlowDelegate`）与其执行器

### 默认模板与门面

- [ ] 1.6 `UTcsDamageSubsystem::Initialize` 组装官方默认模板（`CollectStart → BaseDamage → Execute → Completed`，`TemplateId = Default`）并登记
- [ ] 1.7 环形缓冲：容量常量（初值 128）+ `GetRecentRecords` 读取口

### 链原语与委托（与 Effect 注册表对接）

- [ ] 1.8 `Public/Flow/TcsDamageFlowDelegate.h`：`UTcsDamageFlowDelegate`（UINTERFACE）+ 五个带中性默认实现的函数
- [ ] 1.9 `Public/Flow/TcsDamageRecord.h` + `Public/Flow/TcsDamageRecordedEvent.h`：记录 struct（全扁平 + 句柄参与者）+ 原生 Tag `Tcs.Event.Damage.Recorded`
- [ ] 1.10 `Public/Chain/TcsStepDamage.h` + `Private/Chain/TcsStepDamage.cpp`：链步骤 + 执行器（构上下文 → 写 `BaseDamage`/公式参数 → `RunTemplate`）+ `UE_DEFINE_EFFECT_STEP_EXECUTOR`

### 装置（`Private/Testing/`，**不入库**）

- [ ] 1.11 装置扩检：①默认模板四步跑通（`BaseDamage` 读数 = 配置值）②`Execute` 事务扣血（目标 `Health` 下降，用装置夹具的实体 + 属性）③记录事件同步到达 + 环形缓冲可读 ④Conditions 跳过（`HasAllTags` 不过则某步不执行）⑤`Delegate` 为空也能跑
- [ ] 1.12 命令：`Tcs.Test.Damage.Primitive`（正路）/ `Tcs.Test.Damage.Primitive.Reject`（opt-in：未知条件类型 → Warning；未配 `HealthAttrKey` → 拒绝/降级日志）

## 2. Verification

- [ ] 2.1 UBT Development Editor 编译通过（零警告）
- [ ] 2.2 跨模块注册自检：`FTcsStepDamage` 的执行器在 **TcsEffect 注册表**可查（零启动代码）
- [ ] 2.3 依赖面自检：`Source/TcsDamage/` 不 include `TcsTargeting/TcsState/TcsSkill/TcsIntegration`
- [ ] 2.4 零公式自检：`Source/TcsDamage/` 不得出现伤害公式（grep 佐证：无 `Attack - Armor` 一类推导；基础值只从 `DamageBase` 来）
- [ ] 2.5 用户 PIE 人工检查：装置两条命令输出
- [ ] 2.6 文档回写：plan2 Task 4 勾选 + 实施注记（含 AttrCapture 顺延、命名收窄、`Kill` 记账语义、扣血走 `SetBaseValue` 的判断）；台账补 AttrCapture 条目；README 检查点状态
