## 1. 运行时账本与索引结构
- [x] 1.1 新增 `FTcsSkillModifierRuntimeEntry`，承载 SkillModifier 的权威运行时记录，并包含 `RuntimeModifierId`
- [x] 1.2 新增 `FTcsSkillModifierRuntimeIndex`，统一封装四组索引的增删改查逻辑
- [x] 1.3 在 `UTcsSkillComponent` 中持有 SkillModifier 账本与索引结构，而不是把索引散落到组件方法里
- [x] 1.4 为组件补齐单调递增的 runtime id 生成逻辑

## 2. SkillComponent 统一入口面
- [x] 2.1 为 `UTcsSkillComponent` 新增 SkillModifier 的创建 / 应用 / 查询 / 按 SourceHandle 移除入口
- [x] 2.2 要求 C++ / Blueprint / StateTree 全部复用同一套组件核心逻辑，禁止手写 `SkillEntry` 参数容器
- [x] 2.3 让 `EntrySelector` 在应用阶段解析目标 `SkillEntry`，并统一处理空目标、无效 Def 与类型不匹配错误
- [x] 2.4 将单次 apply 调用做成接近事务语义的流程：任一目标写入失败时 rollback 本次调用已成功写入的 runtime entry

## 3. 直接写入 SkillEntry 参数实例链
- [x] 3.1 将 SkillModifier 的唯一生效容器收敛到 `UTcsSkillEntry` 的 typed `StateParamInstances`
- [x] 3.2 保持 `UTcsSkillInstance` 继续透传读取 `SkillEntry` 参数实例，不新增第二套目标作用域
- [x] 3.3 复用现有 `FStateParam*ModifierInstance` 作为底层可执行链，而不是再引入新的求值层
- [x] 3.4 为三类 typed modifier instance 增加 `RuntimeModifierId` 与按 runtime id 精确移除的 helper

## 4. 生命周期清理与互斥恢复
- [x] 4.1 来源结束时支持按 `SourceHandle` 批量移除 SkillModifier
- [x] 4.2 `ForgetSkill`、技能实例取消 / 结束等场景下清理对应账本记录与索引
- [x] 4.3 移除 Exclusive SkillModifier 后，重新激活同组中最高优先级的候选实例
- [x] 4.4 技能实例结束走 `SourceHandle` 清理包装器，不单独复制另一套移除算法

## 5. Snapshot 与共享可见性语义
- [x] 5.1 固化 `Snapshot` 只冻结 Evaluator 重求值，不冻结 Modifier 链
- [x] 5.2 接受来源存活期间写入 `SkillEntry` 后对所有读取者共享可见的行为，并把该语义写进规范与注释

## 6. 外部入口面与验证
- [x] 6.1 新增 SkillModifier 对应的 StateTree 任务 / Blueprint 入口 / 调试查询入口
- [ ] 6.2 增加运行时验证，覆盖 Apply、按来源移除、ForgetSkill 清理、Exclusive 恢复、Snapshot 后写链等关键场景
- [x] 6.3 执行 `openspec validate add-skill-modifier-runtime-management --strict --no-interactive`

## 7. Effective-value 统一消费契约
- [x] 7.1 固化 ParamInstance 语义：`GetBaseValue()` = base，`GetModifiedValue()` = effective；禁止新增第三套同义公开求值名
- [x] 7.2 将 `UTcsStateInstance::Get*ParamByTag` 默认改为返回 effective，并经 virtual `Get*ParamInstance` 定位真实宿主（Skill 场景落到 Entry）
- [x] 7.3 为 host 增加显式 base 读取 API（`Get*BaseParamByTag` 或同等命名），供调试 / 迁移 / 明确只要 base 的调用方使用
- [x] 7.4 Attribute OperandBinding 刷新改为写入 StateParam effective，不再 `GetBaseValue()` 拉 base
- [x] 7.5 修正 Skill CD 进度分母、参数条件等现有旁路读取，统一走 effective
- [x] 7.6 约定后续新增公开 API 规则：读 StateParam 解析值默认 effective；读 AttributeModifier 已解析 Operand 默认读 `Operands`（刷新后），禁止各自再写 base 旁路
- [x] 7.7 补运行时验证：Apply SkillModifier 后，`Get*ParamByTag`、OperandBinding、参数条件、CD ratio 均反映 effective
- [x] 7.8 将 Param 求值上下文在 State 创建 / SkillEntry 创建时绑定到实例；SkillEntry / SkillInstance 固定使用所属 SkillComponent Owner；`GetModifiedValue()` 改为无参，外部不再反推或传入 `SkillEntry` / `Instigator`
- [x] 7.9 将 `GetValue()` 重命名为 `GetBaseValue()`，并将 SkillModifier Evaluator 的 `Evaluate` 收窄为仅 ParamInstance 的内部求值链可调
- [x] 7.10 收紧 `ActivateSkill` 为仅接收 `SkillDefId`，并固定 Owner == Instigator，不再允许调用方提供不同实体
