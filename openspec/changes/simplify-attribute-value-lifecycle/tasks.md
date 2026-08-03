## 1. Attribute 数值模型与创建 API
- [x] 1.1 从 `FTcsAttributeInstance` 删除 `InitialValue`、带数值的构造路径及其他创建时数值基线。
- [x] 1.2 移除 `AddAttribute` 与 `AddAttributeByTag` 的 InitValue 参数，并保持 DefinitionManager 解析、重复拒绝和范围依赖建立。
- [x] 1.3 确保新 Attribute 只以零值作为内部占位参与统一 Range Clamp，不将其保存为可恢复业务基线。
- [x] 1.4 更新所有 C++、Blueprint、UnrealSharp 反射声明和调用方，要求业务创建后通过 `SetAttributeBaseValue` 写入实际数值。

## 2. 删除模糊数值入口
- [x] 2.1 删除 `UTcsAttributeComponent::ResetAttribute` 的声明、实现、反射入口和相关 Modifier 删除副作用。
- [x] 2.2 删除 `UTcsAttributeComponent::SetAttributeCurrentValue` 的声明、实现和反射入口。
- [x] 2.3 将 `TestTcsDirectors` 的唯一 CurrentValue 直接写入迁移为明确的 BaseValue fixture 设置。
- [x] 2.4 搜索确认 TCS runtime、Blueprint 反射面、UnrealSharp Glue 源声明和用户脚本中不再存在 Reset 或直接 CurrentValue 写入入口。

## 3. Range 与传播语义
- [x] 3.1 验证 AddAttribute、SetAttributeBaseValue、旧 Modifier 重算与范围传播都对 BaseValue / CurrentValue 使用同一 AttributeRange 和 ClampStrategy。
- [x] 3.2 验证动态 Min / Max 只从同一 AttributeComponent 的依赖 Attribute CurrentValue 读取。
- [x] 3.3 确保动态容量降低时 BaseValue 与 CurrentValue 一并 Clamp，且容量恢复不返还此前超出上限的数值。
- [x] 3.4 保持命令型 API 在 Attribute runtime 未 ready 时拒绝且不产生部分副作用。

## 4. 回归测试
- [x] 4.1 增加 Attribute 创建测试：无 InitValue / InitialValue，业务 BaseValue 写入后才形成实际数值。
- [x] 4.2 增加共享 Clamp 测试：同一 Range / ClampStrategy 同时约束 BaseValue 与 CurrentValue。
- [x] 4.3 增加动态上限变化测试：容量降低截断两个数值层，容量恢复不返还溢出。
- [x] 4.4 增加 API 移除回归检查：不存在 ResetAttribute 或 SetAttributeCurrentValue 公开入口。

## 5. 验证
- [x] 5.1 执行 `openspec validate simplify-attribute-value-lifecycle --strict --no-interactive`。
- [x] 5.2 编译 `TireflyGameplayUtilsEditor Win64 Development`，确认 C++、UHT 与 UnrealSharp Glue 可生成。
- [x] 5.3 编译受影响的 `TireflyCombatSystem.Glue.csproj` 和 `ManagedTireflyGameplayUtils.csproj`。
- [x] 5.4 运行新增 Attribute 生命周期自动化测试。
