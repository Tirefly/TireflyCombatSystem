# Tasks: 属性系统运行时完整性重构

本文档列出实施本提案所需的所有任务,按执行顺序排列。

## Phase 1: SourceHandle 索引重构

### Task 1.1: 修改数据结构
- [x] 在 `UTcsAttributeComponent` 中替换 `SourceHandleIdToModifierIndices`
  - 移除: `TMap<int32, TArray<int32>> SourceHandleIdToModifierIndices`
  - 添加: `TMap<int32, TArray<int32>> SourceHandleIdToModifierInstIds` (SourceId -> ModifierInstId 列表)
  - 添加: `TMap<int32, int32> ModifierInstIdToIndex` (ModifierInstId -> 当前数组下标)
- [x] 更新相关注释,说明新的缓存机制

**验证**: 编译通过 ✅

### Task 1.2: 更新插入逻辑
- [x] 修改 `UTcsAttributeManagerSubsystem::ApplyModifier` 中的插入逻辑
  - 在 `AttributeModifiers.Add(...)` 之后更新两个缓存
  - `ModifierInstIdToIndex[ModifierInstId] = NewIndex`
  - `SourceHandleIdToModifierInstIds[SourceId].AddUnique(ModifierInstId)`
- [x] 修改所有其他添加 Modifier 的路径(如果有)

**验证**:
- 添加 Modifier 后,两个缓存都正确更新 ✅
- 使用调试日志验证缓存内容 ✅

### Task 1.3: 更新删除逻辑
- [x] 修改 `UTcsAttributeManagerSubsystem::RemoveModifier` 使用 `RemoveAtSwap`
  - 使用 `ModifierInstIdToIndex` 定位元素下标
  - **在调用 RemoveAtSwap 之前,必须先拷贝要删除的 Modifier 数据(值拷贝),避免引用失效**
  - 从两个缓存中移除被删除的 id
  - 使用 `RemoveAtSwap(Index)` 删除
  - 只修正被 swap 过来的元素: `ModifierInstIdToIndex[SwappedId] = Index`
  - 使用拷贝的数据广播 `AttributeModifierRemoved` 事件
- [x] 更新所有其他删除 Modifier 的路径

**验证**:
- 删除 Modifier 后,缓存正确更新 ✅
- 被 swap 的元素索引正确 ✅
- 多次随机删除后,缓存仍然一致 ✅
- 广播事件时传递的是正确的被删除 Modifier 数据,而不是被 swap 过来的数据 ✅

### Task 1.4: 更新查询逻辑
- [x] 修改 `GetModifiersBySourceHandle` 实现
  - 从 `SourceHandleIdToModifierInstIds[SourceId]` 读取 id 列表
  - 对每个 id,通过 `ModifierInstIdToIndex` 解析数组下标
  - 若解析失败,从桶中剔除(自愈)
- [x] 修改 `RemoveModifiersBySourceHandle` 实现
  - 先拷贝桶内 id 列表
  - 再按 id 删除

**验证**:
- 查询返回正确的 Modifier 列表 ✅
- 陈旧 id 被自动剔除 ✅
- 移除操作不会遗漏任何 Modifier ✅

### Task 1.5: 添加索引重构测试
- [x] 添加单元测试: 大量插入后查询正确
- [x] 添加单元测试: 随机删除后查询正确
- [x] 添加单元测试: 陈旧 id 自愈
- [x] 添加单元测试: RemoveAtSwap 不影响正确性

**验证**: 所有测试通过 ✅

---

## Phase 2: 严格输入校验

### Task 2.1: 添加 CreateAttributeModifier 校验
- [x] 在 `UTcsAttributeManagerSubsystem::CreateAttributeModifier` 开头添加校验
  - `SourceName == NAME_None`: 失败 + error log
  - `!IsValid(Instigator) || !IsValid(Target)`: 失败 + error log
  - `Instigator` 与 `Target` 必须实现 `ITcsEntityInterface`: 失败 + error log
- [x] 确保所有失败路径都返回 false

**验证**:
- 传入非法参数时,函数返回 false 并输出 error log ✅
- 传入合法参数时,函数正常工作 ✅

### Task 2.2: 添加 CreateAttributeModifierWithOperands 校验
- [x] 在 `UTcsAttributeManagerSubsystem::CreateAttributeModifierWithOperands` 开头添加相同校验
- [x] 确保所有失败路径都返回 false

**验证**: 同 Task 2.1 ✅

### Task 2.3: 添加校验测试
- [x] 添加单元测试: SourceName == NAME_None 被拒绝
- [x] 添加单元测试: 无效 Instigator 被拒绝
- [x] 添加单元测试: 无效 Target 被拒绝
- [x] 添加单元测试: 非战斗实体被拒绝

**验证**: 所有测试通过 ✅

---

## Phase 3: AddAttribute 防覆盖

### Task 3.1: 修改 AddAttribute 语义
- [x] 修改 `UTcsAttributeManagerSubsystem::AddAttribute` 实现
  - 若属性已存在,输出 warning log 并 return
  - 不覆盖已存在的属性

**验证**:
- 重复添加同一属性时,输出 warning 且不覆盖
- 首次添加属性时,正常工作

### Task 3.2: 修改 AddAttributes 语义
- [x] 修改 `UTcsAttributeManagerSubsystem::AddAttributes` 实现
  - 对每个属性应用相同的防覆盖逻辑

**验证**: 同 Task 3.1 ✅

### Task 3.3: 修改 AddAttributeByTag 返回值语义
- [x] 修改 `UTcsAttributeManagerSubsystem::AddAttributeByTag` 实现
  - 返回值表达"是否真的添加成功"
  - Tag 解析失败: 返回 false
  - 属性已存在: 返回 false
  - 成功添加: 返回 true
- [x] 更新函数注释

**验证**:
- Tag 无效时返回 false ✅
- 属性已存在时返回 false ✅
- 成功添加时返回 true ✅

### Task 3.4: 添加防覆盖测试
- [x] 添加单元测试: AddAttribute 不覆盖已存在属性
- [x] 添加单元测试: AddAttributes 不覆盖已存在属性
- [x] 添加单元测试: AddAttributeByTag 返回值正确

**验证**: 所有测试通过 ✅

---

## Phase 4: 动态范围约束

### Task 4.1: 设计范围约束传播机制
- [x] 阅读 design.md 中的设计决策
- [x] 确定采用 Option A (in-flight resolver) 还是 Option B (commit-first)
- [x] 确定事件语义(是否在 enforcement 时触发事件)

**验证**: 设计文档完整,决策明确 ✅

### Task 4.2: 实现范围约束传播核心逻辑
- [x] 在 `UTcsAttributeManagerSubsystem` 中添加 `EnforceAttributeRangeConstraints` 函数
  - 遍历所有属性
  - 对每个属性执行 Clamp (BaseValue 和 CurrentValue)
  - 迭代直到稳定或达到最大迭代次数
- [x] 实现动态范围解析(根据选择的 Option)
  - Option A: 扩展 clamp 逻辑,支持"值解析器"回调
  - Option B: 先提交,再执行 enforcement

**验证**:
- 单个属性的范围约束正确 ✅
- 多跳依赖的范围约束收敛 ✅

### Task 4.3: 集成到重算流程
- [x] 在 `RecalculateAttributeBaseValues` 之后调用 `EnforceAttributeRangeConstraints`
- [x] 在 `RecalculateAttributeCurrentValues` 之后调用 `EnforceAttributeRangeConstraints`

**验证**:
- MaxHP Buff 移除后,HP 被正确 clamp ✅
- 多个属性相互依赖时,约束正确 ✅

### Task 4.4: 处理事件语义
- [x] 根据设计决策,决定是否在 enforcement 时触发事件
- [x] 如果触发事件,确保事件参数正确(OldValue, NewValue, SourceHandle 等)
- [x] 更新相关文档

**验证**:
- 事件在正确的时机触发 ✅
- 事件参数正确 ✅

### Task 4.5: 添加范围约束测试
- [x] 添加单元测试: MaxHP 下降时 HP 被 clamp
- [x] 添加单元测试: 多跳依赖收敛
- [x] 添加单元测试: 循环依赖检测(达到最大迭代次数)
- [x] 添加单元测试: 事件语义正确

**验证**: 所有测试通过 ✅

---

## 最终验证

### Task 5.1: 集成测试
- [x] 运行所有现有测试,确保没有回归
- [x] 运行新增的所有测试
- [x] 在编辑器中手动测试典型场景

**验证**: 所有测试通过,无回归 ✅

### Task 5.2: 性能测试
- [x] 测试索引重构对性能的影响
- [x] 测试范围约束传播对性能的影响
- [x] 确保性能在可接受范围内

**验证**: 性能无明显下降 ✅

### Task 5.3: 文档更新
- [x] 更新 CLAUDE.md 中的相关章节
- [x] 更新 SourceHandle 使用指南(如果需要)
- [x] 添加迁移指南(如果需要)

**验证**: 文档完整准确 ✅

### Task 5.4: OpenSpec 验证
- [x] 运行 `openspec validate refactor-attribute-runtime-integrity --strict --no-interactive`
- [x] 修复所有验证错误

**验证**: OpenSpec 验证通过 ✅

---

## 依赖关系

- Task 1.x 必须按顺序执行(索引重构是基础)
- Task 2.x 可以与 Task 1.x 并行
- Task 3.x 可以与 Task 1.x 和 Task 2.x 并行
- Task 4.x 依赖 Task 1.x 完成(需要稳定的索引)
- Task 5.x 依赖所有前置任务完成

## 预计工作量

- Phase 1: 2-3 天
- Phase 2: 1 天
- Phase 3: 1 天
- Phase 4: 3-4 天
- 最终验证: 1-2 天

**总计**: 8-11 天
