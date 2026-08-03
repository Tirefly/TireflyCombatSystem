## 1. SourceHandle 工厂
- [x] 1.1 新增 `FTcsSourceHandleFactory`，封装进程级 SourceHandle ID 分配与有效 handle 构造。
- [x] 1.2 实现 Root 创建 API，确保首个生成的 `Id == 0` 且 `IsValid()` 返回 true。
- [x] 1.3 实现 Child 创建 API，继承父 `CausalityChain` 并追加直接父来源 `FPrimaryAssetId`。
- [x] 1.4 对无效父 handle 或无效直接父来源 ID 的 Child 创建返回无效 handle，且不消耗 ID。

## 2. 旧入口迁移
- [x] 2.1 删除 `UTcsStateComponent::NextSourceHandleId` 与 `UTcsStateComponent::CreateSourceHandle`。
- [x] 2.2 将 Skill / State / Buff / AttributeModifier 相关 SourceHandle 创建调用点迁移到 `FTcsSourceHandleFactory`。
- [x] 2.3 搜索并清理 `SourceHandle.Id > 0` 等手写有效性判断，业务分支统一使用 `SourceHandle.IsValid()`。
- [x] 2.4 确认 `SourceHandle.Id` 仅在通过有效性检查后作为索引键或日志字段读取。

## 3. Blueprint 转发
- [x] 3.1 在 `UTcsGenericLibrary` 增加 Root SourceHandle 创建转发函数。
- [x] 3.2 在 `UTcsGenericLibrary` 增加 Child SourceHandle 创建转发函数。
- [x] 3.3 确认 Blueprint 转发不持有分配状态、对象缓存或 `HandleId -> UObject` 注册表。
- [x] 3.4 确认 Blueprint 转发不能成为预测客户端生成最终 authority SourceHandle 的正向契约。

## 4. 回归验证
- [x] 4.1 增加 SourceHandle 工厂测试：`Id == 0` 有效、连续分配唯一、默认 handle 无效。
- [x] 4.2 增加 Child 因果链测试：继承父链、追加直接父来源、不允许调用方手工拼接。
- [x] 4.3 增加无效 Child 输入测试：返回无效 handle 且不消耗 ID。
- [x] 4.4 增加 State removal 回归测试：`SourceHandle.Id == 0` 时仍调用 `RemoveModifiersBySourceHandle`。

## 5. 验证
- [x] 5.1 执行 `openspec validate centralize-source-handle-creation --strict --no-interactive`。
- [x] 5.2 编译 `TireflyGameplayUtilsEditor Win64 Development`，确认 SourceHandle API 迁移后 TCS 模块可编译。
