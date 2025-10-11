TCS 技能修改器：触发模型与事件对接（StateTree 优先｜组件聚合）

文档目的
- 明确“谁决定修改器生效”和“谁决定何时重算”的分工；
- 详细说明方案 A（组件订阅 State 事件）的落地方式；
- 对比方案 B（StateInstance 显式通知），给出优缺点；
- 梳理风险与缓解、落地前需明确的事项；
- 通过具体玩法示例说明事件触发→打脏→聚合→（快照/实时）同步的时序。

一、原则与分工
- 生效判定（做什么、作用到谁、算多少）：由数据决定（Definition/Execution/Filter/Condition + ModifierParameter/ParamEvaluator）；
- 触发时机（何时重评估）：由事件决定（修改器生命周期变更、技能集合/等级/标签变化、StateTree 生命周期变化、TryCast 快照时机等）。

二、完整触发模型（两路并行）
- 修改器侧触发
  - ModifierAdded：Apply → Filter 命中技能 → 逐个技能评估 Conditions → 写入 ActiveSkillModifiers → 标记受影响的 (Skill, Param) 为脏；
  - ModifierRemoved：从 ActiveSkillModifiers 移除 → 标记脏；
  - ModifierUpdated：
    - 结构变更（Filter/Condition/Execution/Priority）：重筛选+重排+重合并 → 标记脏；
    - 数值变更（ModifierParameter/ParamEvaluator 入参变化）：仅重算聚合值 → 标记脏。
- 上下文侧触发
  - 技能集合：Learn/Forget → 相关修改器重筛选；
  - 技能上下文：SkillLevelChanged → 依赖 Level 的条件/执行器重评估；
  - 实体上下文：OwnerTagsChanged → 依赖 Tag 的条件/执行器重评估；
  - StateTree 生命周期：OnStateInstanceAdded/Removed/OnStateStageChanged → 依赖 Stage 的条件重评估；
  - TryCastSkill：快照型参数在施放前一次性结算写入 StateInstance（非快照忽略）。

三、方案 A：组件订阅 State 事件（落地细化）
- 事件源（建议 UTcsStateComponent 暴露 BlueprintAssignable 多播）：
  - OnStateInstanceAdded(UTcsStateInstance* State)
  - OnStateInstanceRemoved(UTcsStateInstance* State)
  - OnStateStageChanged(UTcsStateInstance* State, ETcsStateStage OldStage, ETcsStateStage NewStage)
- 订阅与过滤
  - UTcsSkillComponent::BeginPlay 注册上述事件监听；
  - 在回调中先过滤：仅处理 ST_Skill（或 ByQuery 指定的 Slot/Tags 的技能）；
- 事件到打脏
  - Added/Removed：对所有挂在该 Actor 的修改器，重做 Filter/Condition；对命中的 (Skill,Param) 标记 Dirty；
  - StageChanged：仅重评估带 Stage 约束的条件；对其 (Skill,Param) 标记 Dirty；
- Tick 与时序
  - Tick 顺序：UpdateSkillCooldowns → SkillComponent.UpdateSkillModifiers(DeltaTime) → UpdateActiveSkillRealTimeParameters；
  - UpdateSkillModifiers：
    - 合并同帧多次事件（队列化、去重）；
    - 按 MergeType 合并、按 Priority 升序排序；
    - 仅对 Dirty 的 (Skill,Param) 重算 AggregatedCache；
    - 清空 Dirty，打印统计（命中率/重算量/耗时）。
- SkillInstance/StateInstance 协同
  - SkillInstance::CalculateBaseNumericParameter 计算基础值；
  - SkillInstance::CalculateNumericParameter 末端从组件查询 AggregatedCache 并叠加；
  - TryCastSkill 前拍快照（含修改器影响）写入 StateInstance；实时参数在下一帧由 UpdateActiveSkillRealTimeParameters 增量同步。

四、方案 B：StateInstance 显式通知（用于补充）
- 方式：在 UTcsStateInstance 生命周期、或 StateTree Task/Service 关键节点，直接调用 Owner 的 UTcsSkillComponent 事件入口（如 OnStateEnter/Exit/StageChanged）。
- 优点：粒度精准、可携带更多上下文、对特殊玩法灵活；
- 缺点：侵入性强，容易漏接，维护成本高；
- 建议：以方案 A 为通用基线；对个别技能/阶段需要更丰富上下文的场景再叠加少量显式通知，形成混合模式。

五、A vs B 对比（要点）
- A：集中、非侵入、对所有技能统一管理；事件量可能较大，需要过滤与队列化；
- B：精准、灵活、上下文丰富；侵入性强、易漏改；
- 混合：A 作为主干；B 用于极少数“关键节点需上下文”的特例，统一进入组件打脏管道。

六、风险与缓解
- 事件风暴/重复重评估：
  - 队列化事件，按 (Skill,Param) 去重；
  - 只重评估与事件相关的条件/修改器（可用 DependsOn 掩码标注：OwnerTags/Stage/SkillLevel/SkillSet 等）；
- 悬挂指针：
  - ActiveSkillModifiers/ActiveSkills 使用弱指针；
  - OnStateInstanceRemoved/OnEndPlay 做清理；
- 非确定性顺序：
  - 合并前按 Priority 升序；同 Priority 再按 ModifierName 稳定排序；
- 快照/实时错位：
  - 明确冷却与投射物类参数走快照；伤害/治疗系走实时；
- 配置错误（Override 冲突/结构不匹配）：
  - 加载 DataTable 做 Definition 预检查；运行期 DevCheck，默认取 Priority 最小者并告警。

七、落地前需明确的点
- 事件枚举与来源：统一采用 StateComponent 的 Added/Removed/StageChanged，还是混合少量 StateInstance 显式通知；
- StageChanged 的触发点：由 StateComponent 的活跃集/槽位变化检测触发，还是在 UTcsStateInstance::SetCurrentStage 内部广播；
- 聚合策略：Add 后再 Mul；Override 冲突视为错误配置（DevCheck）；
- ByQuery 支持维度：StateType/SlotTag/RequiredTags/BlockedTags（复用 FTcsSkillQuery 即可）；
- 参数名表达：FName（已定）；
- 扩展参数结构：允许 Execution 声明支持集合（Additive/Multiplicative/Scalar/Curve/Threshold/Conditional 等）。

八、实操示例（时序）
- A1 引导技能增伤（Channeling +30%）
  - Definition：Filter=ByDefIds[Beam]；Condition=StateStageEquals(Channeling)；Execution=MultiplicativeParam("Damage",1.3)；
  - 时序：TryCast 拍快照 → StateTree 进入 Channeling → StateComponent 发 StageChanged → SkillComponent 打脏 (Beam,"Damage") → UpdateSkillModifiers 聚合 → 实时读取生效；
- A2 火之光环（火系冷却 -20%）
  - Definition：Filter=ByQuery(RequiredTags 含 Skill.Category.Fire)；Execution=CooldownMultiplier(0.8)；
  - 时序：进入光环 Apply → 下次 TryCast 计算快照冷却用 0.8×；离开光环 Remove → 恢复 1.0×；
- A3 低血处决（HP<30% 额外 +50 伤害）
  - Definition：Filter=ByDefIds[Execute]；Condition=OwnerHasTags(HP.Low)；Execution=AdditiveParam("Damage",+50)；
  - 时序：OwnerTagsChanged → 打脏 (Execute,"Damage") → 下帧实时叠加 +50；
- A4 HoT 等级成长（每级 +5%）
  - Definition：Filter=ByDefIds[HoT]；Execution=MultiplicativeParam("HealPerTick", 1.0+0.05×Level)；
  - 时序：OnSkillLevelChanged → 打脏 (HoT,"HealPerTick") → 下帧治量提升。

九、文字版时序图（通用）
- 触发：事件（Added/Removed/Updated/StageChanged/TagsChanged/SkillLevelChanged/TryCast）
- → 组件：定位受影响修改器与技能，按 (Skill,Param) 打脏
- → Tick.UpdateSkillModifiers：合并、排序、仅重算 Dirty 的聚合缓存
- → SkillInstance：CalculateNumericParameter 末端从组件取聚合叠加
- → 同步：快照参数在 TryCast 前一次性写入 StateInstance；实时参数在 UpdateActiveSkillRealTimeParameters 批量推送

十、诊断与工具
- Definition 预检查：加载 DataTable 时校验 Filter/Condition/Execution/参数载荷匹配；
- 运行态可视化：显示当前 Actor 的 ActiveSkillModifiers、聚合链路、Dirty 列表、缓存命中率与重算耗时；
- 日志：关键事件（Added/Removed/StageChanged 等）、冲突（Override）、丢失引用等。

十一、性能要点
- 时间复杂度：事件驱动下常态为 O(Dirty×logN)；避免“修改器×技能×每帧”的全量扫描；
- 数据结构：按需从 TMap 过渡到紧凑数组/FlatHash；
- 门限与巡检：可选低频巡检兜底遗漏（如 0.25s），与事件驱动互补。

十二、Checklist（落地）
- 明确事件来源与路由（A/B/混合）；
- 补 StateComponent 事件（如缺失）；
- SkillComponent：实现事件队列与 Dirty 粒度；
- 聚合：实现 MergeType/Priority/缓存；
- SkillInstance：末端聚合查询；
- TryCast 流程：快照结算时机；
- 工具：预检查与可视化；
- 测试：筛选/条件/执行/合并/快照&实时/性能全覆盖。

附注（既有决策回顾）
- 参数名表达：FName；
- Override 冲突：设计上不出现；实现 DevCheck 并默认取 Priority 最小者；
- 旧接口：所有“技能参数修改器相关”旧接口直接删除；
- 参数载荷：允许扩展型结构。

