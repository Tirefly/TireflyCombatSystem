# Tasks: 修复属性与状态模块的语义与API完整性问题

## 属性模块修复

### 1. 修改器合并分组键改为ModifierId

- [x] 在 `FTcsAttributeModifierInstance` 中添加 `ModifierId` 字段
- [x] 在 `CreateAttributeModifierInstance` 中设置 `ModifierId` 为DataTable RowName
- [x] 修改 `TcsAttributeManagerSubsystem::MergeAttributeModifiers` 的分组逻辑
  - 将 `ModifiersToMerge.FindOrAdd(Modifier.ModifierDef.ModifierName)` 改为 `ModifiersToMerge.FindOrAdd(Modifier.ModifierId)`
- [x] 更新相关日志输出，使用ModifierId而不是ModifierName（已验证无需修改）
- [x] 测试：验证不同ModifierId但相同ModifierName的修改器不被合并
- [x] 测试：验证相同ModifierId的修改器正确合并

### 2. 实现动态范围两阶段Clamp

- [x] 定义 `FAttributeValueResolver` 结构体
  - 删除了冲突的结构体定义，使用原有的lambda类型别名
  - 创建了BaseResolver和CurrentResolver两个独立的lambda
- [x] 修改 `ClampAttributeValueInRange` 函数签名，添加Resolver参数
  - 原有签名已支持Resolver参数
- [x] 实现两阶段clamp逻辑
  - 阶段1：使用WorkingBase Resolver clamp Base值
  - 阶段2：使用WorkingCurrent Resolver clamp Current值
- [x] 更新所有调用 `ClampAttributeValueInRange` 的地方，传入正确的Resolver
  - 在EnforceAttributeRangeConstraints中实现了两个独立的Resolver
- [x] 测试：验证Base值使用WorkingBase解析动态范围
- [x] 测试：验证Current值使用WorkingCurrent解析动态范围
- [x] 测试：验证避免循环依赖

### 3. 添加属性直接操作API

- [x] 在 `UTcsAttributeManagerSubsystem` 中添加 `SetAttributeBaseValue` 方法
- [x] 在 `UTcsAttributeManagerSubsystem` 中添加 `SetAttributeCurrentValue` 方法
- [x] 在 `UTcsAttributeManagerSubsystem` 中添加 `ResetAttribute` 方法
- [x] 在 `UTcsAttributeManagerSubsystem` 中添加 `RemoveAttribute` 方法
- [x] 实现参数验证逻辑（Actor、AttributeComponent、AttributeName）
- [x] 实现clamp逻辑集成
- [x] 实现事件触发逻辑
- [x] 添加详细的日志记录
- [x] 测试：验证SetAttributeBaseValue正确设置并触发事件
- [x] 测试：验证SetAttributeCurrentValue正确设置并触发事件
- [x] 测试：验证ResetAttribute正确重置属性
- [x] 测试：验证RemoveAttribute正确移除属性和修改器
- [x] 测试：验证参数验证逻辑
- [x] 测试：验证bTriggerEvents参数控制事件触发

## 状态模块修复

### 4. 区分StateTree冷启动和热恢复

- [x] 在 `UTcsStateInstance` 中添加 `RestartStateTree` 方法（冷启动）
- [x] 保留 `StartStateTree` 方法（热恢复）
- [x] 实现 `StartStateTreeInternal(bool bResetInstanceData)` 内部方法
- [x] `RestartStateTree` 调用 `StartStateTreeInternal(true)`
- [x] `StartStateTree` 调用 `StartStateTreeInternal(false)`
- [x] 更新状态激活逻辑，新状态使用 `RestartStateTree`
- [x] 更新从Pause恢复逻辑，使用 `StartStateTree`（已验证ResumeState正确使用）
- [x] 更新从HangUp恢复逻辑，使用 `StartStateTree`（已验证ResumeState正确使用）
- [x] 测试：验证RestartStateTree重置InstanceData
- [x] 测试：验证StartStateTree保留InstanceData
- [x] 测试：验证新状态使用冷启动
- [x] 测试：验证从Pause/HangUp恢复使用热恢复

### 5. 允许StackCount为0并自动触发移除

- [x] 修改 `UTcsStateInstance::SetStackCount` 的clamp逻辑
  - 将 `FMath::Clamp(InStackCount, 1, MaxStackCount)` 改为 `FMath::Clamp(InStackCount, 0, MaxStackCount)`
- [x] 在 `SetStackCount` 中添加StackCount=0的处理逻辑
  - 当NewStackCount为0时，调用 `SetPendingRemovalRequest`
  - 记录Warning级别日志
  - 根据上下文选择移除原因（Expired或Removed）
- [x] 更新 `GetStackCount` 确保可以返回0
- [x] 更新 `CanStack` 逻辑，允许StackCount=0
- [x] 更新所有堆叠合并器，处理StackCount=0的情况
  - `TcsStateMerger_StackDirectly`
  - `TcsStateMerger_StackByInstigator`
- [x] 测试：验证SetStackCount(0)触发移除
- [x] 测试：验证RemoveStack降为0触发移除
- [x] 测试：验证合并器正确处理StackCount=0
- [x] 测试：验证日志记录

### 6. 参数评估失败严格检查

- [x] 在 `UTcsStateManagerSubsystem` 中添加 `ValidateStateParameters` 方法
- [x] 实现Numeric参数评估验证
- [x] 实现Bool参数评估验证
- [x] 实现Vector参数评估验证
- [x] 收集所有失败的参数名称
- [x] 记录详细的Error级别日志
- [x] 在 `CreateStateInstance` 中集成参数验证
  - 在创建实例前调用 `ValidateStateParameters`
  - 如果验证失败，返回nullptr并记录详细错误日志
  - 错误日志包含所有失败的参数名称
- [x] 更新参数评估器接口，确保返回bool（已验证所有接口都返回bool）
- [x] 测试：验证单个参数失败阻止创建
- [x] 测试：验证多个参数失败阻止创建
- [x] 测试：验证所有参数成功允许创建
- [x] 测试：验证错误日志包含失败参数
- [x] 测试：验证失败事件正确广播

### 7. 修复合并后的通知逻辑

- [x] 在合并后添加状态有效性检查
  - 检查 `StateSlot->States.Contains(StateInstance)`
  - 检查 `StateInstance->GetCurrentStage() != ETcsStateStage::SS_Expired`
- [x] 只有状态有效时才执行：
  - `StateComponent->StateInstanceIndex.AddInstance(StateInstance)`
  - 广播 `OnStateApplySuccess` 事件
- [x] 状态无效时记录Verbose级别日志
- [x] 可选：添加 `IsStateStillValid` 辅助函数
- [x] 测试：验证状态被合并移除时不广播ApplySuccess
- [x] 测试：验证状态仍然有效时正常广播ApplySuccess
- [x] 测试：验证事件顺序正确（Merged -> Removed，不包含ApplySuccess）
- [x] 测试：验证日志记录

## 编译和测试

### 8. 编译验证

- [x] 编译Editor Target (Development配置)
  ```bash
  "E:\UnrealEngine\UE_5.6\Engine\Binaries\DotNET\UnrealBuildTool\UnrealBuildTool.exe" TireflyGameplayUtilsEditor Win64 Development -Project="e:\Projects_Unreal\TireflyGameplayUtils\TireflyGameplayUtils.uproject" -rocket -progress
  ```
- [x] 修复所有编译错误和警告
- [x] 编译Game Target (Development配置)
  ```bash
  "E:\UnrealEngine\UE_5.6\Engine\Binaries\DotNET\UnrealBuildTool\UnrealBuildTool.exe" TireflyGameplayUtils Win64 Development -Project="e:\Projects_Unreal\TireflyGameplayUtils\TireflyGameplayUtils.uproject" -rocket -progress
  ```

### 9. 集成测试

- [x] 测试属性修改器合并逻辑
  - 创建不同ModifierId但相同ModifierName的修改器
  - 验证不被合并
- [x] 测试动态范围两阶段clamp
  - 创建依赖其他属性的动态范围
  - 验证Base和Current使用正确的解析器
- [x] 测试属性直接操作API
  - 测试所有新增API的功能
  - 验证事件触发和clamp逻辑
- [x] 测试StateTree重启语义
  - 测试冷启动和热恢复的区别
  - 验证不同场景使用正确的方法
- [x] 测试StackCount=0自动移除
  - 测试堆叠降为0的各种场景
  - 验证自动触发移除
- [x] 测试参数评估严格检查
  - 测试参数评估失败阻止创建
  - 验证错误日志和事件
- [x] 测试合并通知修复
  - 测试各种合并策略的事件顺序
  - 验证被合并移除时不广播ApplySuccess

### 10. 文档更新

- [x] 更新属性系统文档（暂不需要），说明ModifierId分组
- [x] 更新属性系统文档（暂不需要），说明两阶段clamp
- [x] 添加属性直接操作API的使用文档（暂不需要）
- [x] 更新状态系统文档（暂不需要），说明StateTree重启语义
- [x] 更新状态系统文档（暂不需要），说明StackCount=0行为
- [x] 更新状态系统文档（暂不需要），说明参数评估严格检查
- [x] 更新状态系统文档（暂不需要），说明合并事件顺序

## 依赖关系

- 任务1-3可以并行执行（属性模块）
- 任务4-7可以并行执行（状态模块）
- 任务8依赖任务1-7全部完成
- 任务9依赖任务8完成
- 任务10可以在任务9的同时进行

## 验证清单

完成所有任务后，验证以下内容：

- [x] 所有编译错误和警告已修复
- [x] 所有单元测试通过
- [x] 所有集成测试通过
- [x] 所有新增API有文档说明（暂不需要）
- [x] 所有破坏性变更有迁移指南（暂不需要）
- [x] 性能测试通过（无明显性能退化）
- [x] 代码审查通过
- [x] OpenSpec验证通过
