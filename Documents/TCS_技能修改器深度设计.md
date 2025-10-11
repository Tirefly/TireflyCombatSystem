TCS 技能修改器深度设计（StateTree 驱动｜组件持有运行态）

设计原则
- StateTree 优先：技能逻辑的执行主体是 StateInstance（其内部运行 StateTree）。SkillInstance 仅作为技能数据与参数计算上下文，不直接执行技能逻辑。
- 组件持有状态：一切“生效中的技能修改器”与其聚合缓存，归属到每个战斗单位的 UTcsSkillComponent，而非全局子系统。
- 单一聚合口：所有对技能参数的影响（含现有“参数修正器”能力）统一经由组件侧的聚合查询返回；SkillInstance 不再维护加乘数组。
- 数据驱动：修改器通过 DataTable + 执行器参数（InstancedStruct）配置；Execution/Merger/Filter/Condition 以类抽象实现扩展。
- 快照与实时：施放前对需要快照的参数一次性结算写入 StateInstance；非快照参数在 Tick 或事件驱动下增量更新。

一、原设计要点提取与对齐
- 技能实例（4.1）：作为运行时数据容器，包含等级、运行参数、冷却等。与本设计一致：保留基础值计算入口与同步接口。
- 技能修改器（4.2）：
  - 定义：ModifierName、Priority、SkillFilter、ActiveConditions、ModifierParameter、ExecutionType、MergeType。
  - 实例：ModifierDef、SkillModInstanceId、SkillInstance、ApplyTime、UpdateTime。
  - 算法：Execution::Execute、Merger::Merge、Filter::Filter。
- 技能组件（4.3）：应当持有“所有生效中的技能修改器”。本设计将运行态完全落在 UTcsSkillComponent。
- 技能管理器（4.4）：提供 Apply/Remove/Updated/Update 等接口，但作为门面（Facade），转发到目标 Actor 的 UTcsSkillComponent，不再持有 per-actor 状态。
- 备注：原文未显式“参数修改器”概念；当前实现中的“参数加乘数组/倍率”全部并入技能修改器体系，对外统一为数据驱动。

二、统一模型（面向 StateTree 的组件聚合）
- SkillInstance（数据容器）
  - 计算基础值（CalculateBaseNumericParameter）
  - 通过单一入口 CalculateNumericParameter 获取最终值（基础值 + 组件聚合效果）
  - 提供快照/实时同步到 StateInstance
- StateInstance（执行主体）
  - 读取 SkillInstance 同步的参数；在 StateTree 生命周期事件上（OnEnter/OnExit/StageChanged）通知组件进行条件重评估与打脏
- UTcsSkillComponent（每个战斗单位持有）
  - 存储：ActiveSkillModifiers、AggregatedCache、DirtySkills
  - 能力：Apply/Remove/Updated、Update(DeltaTime)、聚合查询 GetAggregatedParamEffect、接收 StateTree/标签/等级变更事件并打脏
- UTcsSkillManagerSubsystem（World 级门面）
  - 载入 DataTable（Definition 数据源/校验）
  - 对外便捷 API（批量 Apply/Remove/Update），内部查找目标 Actor 的 UTcsSkillComponent 并委派
  - 不存 per-actor 运行态，避免生命周期/泄漏复杂度

三、数据结构（与原文一致，补足实现细节）
- FTcsSkillModifierDefinition（DataTable 行）
  - ModifierName:FName；Priority:int32
  - SkillFilter:TSubclassOf<UTcsSkillFilter>
  - ActiveConditions:TArray<TSubclassOf<UTcsSkillModifierCondition>>
  - ModifierParameter:FTcsStateParameter（内部 InstancedStruct，表达 ParamName/Magnitude/Multiplier/Scalar 等）
  - ExecutionType:TSubclassOf<UTcsSkillModifierExecution>
  - MergeType:TSubclassOf<UTcsSkillModifierMerger>
- FTcsSkillModifierInstance（运行时）
  - ModifierDef；SkillModInstanceId；SkillInstance（可空，Filter 命中集合动态维护）；ApplyTime；UpdateTime
  - 可扩展（v2）：Instigator 弱指针（便于溯源/撤销）、ExpireTime/StackCount（持续时间/堆叠）
- 执行器参数结构（放入 ModifierParameter.ParamValueContainer）
  - FTcsModParam_Additive{ParamName, Magnitude}
  - FTcsModParam_Multiplicative{ParamName, Multiplier}
  - FTcsModParam_Scalar{Value}（全局倍率：Cooldown/Cost 等）
- 默认实现（最小可用集）
  - Execution：AdditiveParam、MultiplicativeParam、CooldownMultiplier、CostMultiplier
  - Merger：NoMerge、CombineByParam（同执行器+同 Param 聚合）
  - Filter：ByQuery（复用 FTcsSkillQuery）、ByDefIds（白名单）
  - Condition：AlwaysTrue、SkillHasTags、SkillLevelInRange、StateStageEquals

四、UTcsSkillComponent：运行态与聚合（核心）
- 状态成员
  - ActiveSkillModifiers: TArray<FTcsSkillModifierInstance>
  - AggregatedCache: TMap<UTcsSkillInstance*, TMap<FName, FAggregatedParamEffect>>
  - DirtySkills: TSet<UTcsSkillInstance*>
  - FAggregatedParamEffect：{AddSum=0, MulProd=1, bHasOverride=false, OverrideValue=0, CooldownMul=1, CostMul=1}
- 核心接口（对外）
  - bool ApplySkillModifiers(const TArray<FTcsSkillModifierDefinition>& Defs, TArray<int32>& OutInstanceIds)
  - void RemoveSkillModifierById(int32 InstanceId)
  - void UpdateSkillModifiers(float DeltaTime)
  - bool GetAggregatedParamEffect(const UTcsSkillInstance* Skill, FName Param, FAggregatedParamEffect& Out) const
  - 事件入口：OnStateStageChanged/OnStateEnter/OnStateExit/OnTagsChanged/OnSkillLevelChanged（均标记 Dirty）
- 内部流程
  1) Apply：对每条 Definition
     - 执行 Filter，命中一组 SkillInstance
     - 对每个命中技能评估 Conditions（标签/等级/Stage）
     - 通过 Execution 注册到 ActiveSkillModifiers（或立即作用于聚合缓存）
     - 对受影响 (Skill, Param) 标记 Dirty
  2) Remove：按 InstanceId 清除，打脏对应 (Skill, Param)
  3) Updated：重新评估条件/目标集合，更新 ActiveSkillModifiers，并打脏
  4) UpdateSkillModifiers：
     - 合并（按 Definition.MergeType）与优先级排序（Priority 升序）
     - 重算 Dirty 目标的聚合缓存；清空 Dirty
- 聚合公式
  - 若存在 Override：取最高优先级的覆盖值（或按最后一个）
  - 否则：Final = (Base + AddSum) * MulProd
  - Cooldown/Cost 等全局倍率在末端乘上（由 Execution 决定写入 CooldownMul/CostMul）

五、UTcsSkillManagerSubsystem：门面与数据源
- 职责
  - 载入/校验 SkillModifierDefTable，提供获取/搜索 Definition 的工具方法
  - 便捷 API：Apply/Remove/Update（查找目标 Actor 的 UTcsSkillComponent 并委派）
  - 批量操作：对一批 Actor（如 Aura）进行遍历与转发
- 不持有 ActiveSkillModifiers/AggregatedCache/Dirty 集合（与组件解耦）

六、与 SkillInstance/StateInstance 的契合
- SkillInstance
  - CalculateBaseNumericParameter：保持现状（ParamEvaluator + 临时 StateInstance）
  - CalculateNumericParameter：末端调用 Owner->FindComponentByClass<UTcsSkillComponent>()->GetAggregatedParamEffect
  - 快照：TryCast 前 TakeParameterSnapshot；随后 SyncParametersToStateInstance(true)
  - 实时：SyncRealtimeParametersToStateInstance 在 Tick 中被调用，读取组件聚合缓存并智能更新
- StateInstance（StateTree）
  - 在 OnEnter/OnExit/StageChanged 等事件中回调其 Owner 的 UTcsSkillComponent 打脏；必要时触发 UpdateSkillModifiers

七、生命周期与事件（事件驱动优先）
- 触发 Apply/Remove/Updated 的来源
  - Buff/装备/技能触发器/Aura 等系统
  - StateTree 生命周期事件（进入/退出/阶段变化）
  - 标签（GameplayTags）与等级变化
- 组件 Tick 次序（建议）
  - UpdateSkillCooldowns → SkillComponent.UpdateSkillModifiers(DeltaTime) → UpdateActiveSkillRealTimeParameters

八、性能与缓存策略
- 打脏粒度：(Skill, Param)；一次 Update 合并多次打脏
- 缓存命中：Aggregation 结果仅在打脏时重算，其他时间 O(1) 查询
- 结构优化（可选）：FlatHash/自定义池、紧凑数组；减少 TMap 分配与哈希
- 观测点：缓存命中率、每帧重算数、合并次数与耗时

九、与现有代码的改造点
- 移除 UTcsSkillInstance 的 Additive/Multiplicative 容器与 API；旧接口直接删除
- 替换 ApplySkillModifiers：改为调用组件聚合查询
- UTcsSkillComponent 扩展上述接口与状态；在 Tick 中调用 UpdateSkillModifiers
- UTcsSkillManagerSubsystem 作为门面转发与 Definition 数据源

十、可扩展 / 可改进 / 可优化 / 可移除
- 可扩展
  - 执行器族：OverrideParam、CurveParam、ThresholdTrigger、Randomized、ConditionalChain
  - 合并器族：按来源去重、按标签分组、按优先级聚类
  - 条件族：距离/扇形/目标属性/团队光环/战斗状态（战斗中/非战斗）
- 可改进
  - 与 StateTree 的信号对接：提供组件级 StageChanged/OnEnter/OnExit 注册点
  - 参数名 GameplayTag 化：减少硬编码字符串
  - DataTable 静态校验与编辑器可视化（当前 Actor 的生效修改器与聚合链路）
- 可优化
  - 缓存结构与批量重算策略；事件驱动优先于全量 Tick
- 可移除
  - SkillInstance 内参数加乘容器与直改倍率接口（迁移完成后）

十一、用例（设计层面）
- 全局冷却 -20%：Filter=ByQuery(StateType=Skill)，Execution=CooldownMultiplier(Value=0.8)，Priority=10
- 火系技能伤害 +10：Filter=ByQuery(RequiredTags 含 Skill.Category.Fire)，Execution=AdditiveParam(ParamName=Damage, Magnitude=10)，Priority=20
- Stage 导向增伤：Condition=StateStageEquals(Channeling)，Execution=MultiplicativeParam(ParamName=Damage, Multiplier=1.3)

十二、落地路线（与 StateTree 衔接）
- M1：落 Definition/Instance/Execution/Merger/Filter/Condition 抽象与默认实现
- M2：扩展 UTcsSkillComponent（运行态/聚合/事件入口/接口）
- M3：替换 SkillInstance 的 ApplySkillModifiers，打通快照/实时参数流
- M4：UTcsSkillManagerSubsystem 做门面与数据源，提供批量便捷 API
- M5：Editor Utility 与自动化测试（筛选/条件/执行/合并/快照&实时/性能）

附：名词对照
- 技能修改器：Definition/Instance/Execution/Merger/Filter/Condition 的组合，数据驱动
- 参数修正器：现存 SkillInstance 内的加法/乘法数组；本方案下迁移并移除
- StateInstance/StateTree：技能逻辑的执行主体；SkillInstance：数据与参数上下文；SkillComponent：运行态与聚合中心
