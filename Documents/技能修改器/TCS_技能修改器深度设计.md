TCS 技能修改器深度设计（StateTree 驱动｜组件持有运行态）

一、设计目标与原则
- StateTree 优先：技能逻辑执行主体为 StateInstance（运行 StateTree），SkillInstance 仅做数据与参数计算上下文。
- 组件持有运行态：所有“生效中的技能修改器”和聚合缓存归属 UTcsSkillComponent（每 Actor），而非全局子系统。
- 单一聚合口：SkillInstance 计算基础值后，仅通过组件的聚合查询叠加修改器影响；SkillInstance 不再维护加乘数组。
- 数据驱动：Definition/Instance + Execution/Merger/Filter/Condition，参数载荷用 InstancedStruct（FTcsStateParameter.ParamValueContainer）。
- 快照与实时：TryCast 前结算快照；非快照参数按事件驱动 + Tick 增量更新。

二、架构总览（角色职责）
- UTcsSkillInstance（数据容器）
  - CalculateBaseNumericParameter：基于 ParamEvaluator + 临时 StateInstance 求基础值。
  - CalculateNumericParameter：末端从组件查询聚合结果并叠加（替换旧 ApplySkillModifiers）。
  - 提供 TakeParameterSnapshot / SyncParametersToStateInstance / SyncRealtimeParametersToStateInstance。
- UTcsStateInstance（执行主体）
  - 读取 SkillInstance 同步的参数；其生命周期（Enter/Exit/StageChanged）作为重要“事实变化”来源。
- UTcsSkillComponent（每 Actor 持有）
  - 运行态：ActiveSkillModifiers、AggregatedCache、Dirty 集合。
  - 能力：Apply/Remove/Updated、UpdateSkillModifiers(Delta)、GetAggregatedParamEffect。
  - 接收 StateTree/标签/等级变化等事件，做条件重评估与打脏（Dirty）。
- UTcsSkillManagerSubsystem（World 级门面）
  - DataTable 数据源/校验；对外包装 Apply/Remove/Update 并委派到目标 Actor 的技能组件；不持 per-actor 状态。

三、数据模型（与原始设计对齐）
- FTcsSkillModifierDefinition（DataTable 行）
  - 字段：ModifierName（FName）、Priority（int32）、SkillFilter、ActiveConditions、ModifierParameter（FTcsStateParameter）、ExecutionType、MergeType。
- FTcsSkillModifierInstance（运行时）
  - 字段：ModifierDef、SkillModInstanceId、SkillInstance（可空）、ApplyTime、UpdateTime。
  - 可扩展（v2）：Instigator、ExpireTime、StackCount 等。
- 执行器参数载荷（入 ModifierParameter.ParamValueContainer）
  - FTcsModParam_Additive{ ParamName:FName, Magnitude:float }
  - FTcsModParam_Multiplicative{ ParamName:FName, Multiplier:float }
  - FTcsModParam_Scalar{ Value:float }（全局倍率：Cooldown/Cost 等）
  - 允许扩展型结构：Curve/Threshold/Conditional 等，由 Execution 声明支持集合。

四、组件侧聚合（UTcsSkillComponent）
- 成员
  - ActiveSkillModifiers: TArray<FTcsSkillModifierInstance>
  - NameAggCache: TMap<UTcsSkillInstance*, TMap<FName, FAggregatedParamEffect>>
  - TagAggCache:  TMap<UTcsSkillInstance*, TMap<FGameplayTag, FAggregatedParamEffect>>
  - DirtyNames:   TMap<UTcsSkillInstance*, TSet<FName>>
  - DirtyTags:    TMap<UTcsSkillInstance*, TSet<FGameplayTag>>
  - FAggregatedParamEffect: { AddSum=0, MulProd=1, bHasOverride=false, OverrideValue=0, CooldownMul=1, CostMul=1 }
- 对外接口（最终以头文件实现）
  - bool ApplySkillModifiers(const TArray<FTcsSkillModifierDefinition>& Defs, TArray<int32>& OutInstanceIds)
  - void RemoveSkillModifierById(int32 InstanceId)
  - void UpdateSkillModifiers(float DeltaTime)
  - bool GetAggregatedParamEffect(const UTcsSkillInstance* Skill, FName Param, FAggregatedParamEffect& Out) const
  - bool GetAggregatedParamEffectByTag(const UTcsSkillInstance* Skill, FGameplayTag ParamTag, FAggregatedParamEffect& Out) const
  - 事件入口：OnStateInstanceAdded/Removed、OnStateStageChanged、OnOwnerTagsChanged、OnSkillLevelChanged（均用于打脏）
- 聚合顺序与公式
  - 合并：按 MergeType 合并同类，按 Priority 升序排序（同 Priority 可按 ModifierName 稳定排序）。
  - 覆盖：若存在 Override，视为错误配置（DevCheck 报警），默认取 Priority 最小者。
  - 数值参数：Final = (Base + AddSum) * MulProd；Cooldown/Cost 等全局倍率末端乘上。

五、触发模型（摘要）
- 两路触发：
  - 修改器侧：Added/Removed → 重筛选与打脏；Updated → 结构变更重筛选/重排，数值变更仅重算。
  - 上下文侧：Learn/Forget、SkillLevelChanged、OwnerTagsChanged、StateTree Added/Removed/StageChanged、TryCast（快照在施放前结算）。
- 推荐：事件驱动为主（立即标脏）+ Tick 中批量重算 Dirty；可选低频巡检兜底。
- 详细设计与示例请见：Documents/技能修改器/TCS_技能修改器_触发模型与事件对接.md（已说明 Name/Tag 双空间各自独立打脏与聚合）。

六、默认实现（最小可用集）
- Execution：AdditiveParam、MultiplicativeParam、CooldownMultiplier、CostMultiplier。
- Merger：NoMerge、CombineByParam（同执行器+同 Param 聚合）。
- Filter：ByQuery（复用 FTcsSkillQuery）、ByDefIds。
- Condition：AlwaysTrue、SkillHasTags、SkillLevelInRange、StateStageEquals。

七、已确认的设计决策
- 参数名表达：FName。
- Override 冲突：设计上不出现；实现期 DevCheck 并默认取 Priority 最小者报警。
- 旧接口：所有“技能参数修改器相关旧接口”直接删除（无兼容开关）。
- 参数载荷：允许扩展型结构；Execution 声明其支持集合，不匹配时报错并忽略该条。
  - 参数键并立：支持 FName 与 GameplayTag 两套命名空间并立；不做映射与折叠；设计师可任选其一或两者并用。

八、迁移与清理
- 删除 SkillInstance 的 Add/Remove/Clear/GetTotalParameterModifier 及内部 Additive/Multiplicative 容器；
- SkillComponent 删除 ModifySkillParameter/SetSkillCooldownMultiplier/SetSkillCostMultiplier；
- 改为通过 Definition（Filter_ByDefIds/ByQuery + Execution）统一驱动；提供常用模板资产辅助迁移（如 Damage+X、CDR-%）。

九、风险与缓解（精简）
- 事件风暴：事件队列化、按 (Skill,Param) 去重，仅重评与事件相关条件；
- 悬挂指针：运行态用弱指针，OnStateRemoved/OnEndPlay 清理；
- 顺序不稳定：合并前排序（Priority→ModifierName）；
- 快照/实时错位：明确快照参数集合（冷却/投射物参数），其余实时同步；
- 配置错误：加载时做 Definition 预检查，运行期 DevCheck 报警。

十、落地路线（M1→M5）
- M1：落 Definition/Instance/Execution/Merger/Filter/Condition 抽象与默认实现。
- M2：扩展 UTcsSkillComponent（运行态/聚合/事件入口/对外接口）。
- M3：替换 SkillInstance 的 ApplySkillModifiers，为末端聚合查询；打通快照/实时流程。
- M4：UTcsSkillManagerSubsystem 精简为门面与数据源（委派到组件）。
- M5：Editor Utility（预检查/可视化）与自动化测试（筛选/条件/执行/合并/快照&实时/性能）。

十一、附：名词对照
- 技能修改器：Definition/Instance/Execution/Merger/Filter/Condition 的组合，数据驱动。
- 参数修正器：旧的 SkillInstance 内加法/乘法数组；本方案已移除并纳入“技能修改器”表达。
- StateInstance/StateTree：技能逻辑执行主体；SkillInstance：数据与参数上下文；SkillComponent：运行态与聚合中心。

附录：参数键并立设计
- 完整方案详见：Documents/技能修改器/TCS_参数键并立设计_FName与GameplayTag.md
