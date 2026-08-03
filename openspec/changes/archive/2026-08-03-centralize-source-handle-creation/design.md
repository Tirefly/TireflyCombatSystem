## 背景

SourceHandle 是 TCS 跨系统的因果追踪值对象。它当前被 State 创建，但会被 AttributeModifier、SkillModifier、Buff、装备、伤害等多个领域消费。把分配器放在 StateComponent 会让非 State 来源也依赖 State 模块，并让后续 AttributeModifier 重构无法稳定表达“所有有效请求必须携带有效 SourceHandle”。

这里的“无状态静态工厂”指不创建 UObject、Subsystem 或 owner-local factory 实例；进程级 ID 计数器允许封装在工厂内部，但调用方不能持有或复制分配状态。

## 目标

- 用 `FTcsSourceHandleFactory` 统一所有有效 SourceHandle 的创建。
- 让 Root / Child 创建语义成为唯一的因果链构造方式。
- 保证 `Id == 0` 是有效 SourceHandle，所有业务判断统一使用 `IsValid()`。
- 保持 SourceHandle 为值对象，不引入运行时对象注册表。
- 给 Blueprint 暴露最小转发入口，同时不绕过 authority 边界。

## 非目标

- 不实现 SourceHandle 网络复制策略变更。
- 不实现本地预测或 PredictionKey。
- 不为业务来源对象建立全局查询服务。
- 不在本 change 中改造 AttributeModifier 的 Application / Evaluator / Operator 模型。

## 决策

### `FTcsSourceHandleFactory` 是唯一分配器

新增 `FTcsSourceHandleFactory`，放在 SourceHandle 共享头/实现附近。它负责分配进程内唯一非负 ID，并构造有效 `FTcsSourceHandle`。

`UTcsStateComponent::CreateSourceHandle` 与 `NextSourceHandleId` 应删除或迁移到工厂内部，不保留兼容包装器。当前 TCS 仍处开发阶段，默认没有 Blueprint 资产引用该 API，因此不为猜测中的资产兼容保留旧入口。

### Root / Child API 固化因果链

Root API 创建没有父来源的 handle。Child API 必须接收父 `FTcsSourceHandle` 与直接父来源的 `FPrimaryAssetId`，并生成：

```text
Child.CausalityChain = Parent.CausalityChain + DirectParentSourceDefId
```

调用方不得手工拼接 `CausalityChain` 后再创建有效 SourceHandle。若父 handle 无效，或直接父来源 ID 无效，Child API 必须拒绝创建有效 handle，且不得消耗 ID。

### 有效性只看非负 ID

`FTcsSourceHandle::IsValid()` 必须等价于 `Id > -1`。`Id == 0` 是首个合法值，不得再使用 `SourceHandle.Id > 0` 判断有效性。

允许在已经通过 `IsValid()` 校验后读取 `SourceHandle.Id` 作为索引键，但业务有效性分支必须调用 `IsValid()`。

### Blueprint 只做转发

`UTcsGenericLibrary` 可以提供 Root / Child BlueprintCallable 转发函数。转发函数不得持有分配计数器、缓存或对象注册表，只能委托 `FTcsSourceHandleFactory`。

在 authority 敏感路径中，Blueprint 转发入口必须保留与 C++ 调用一致的 authority 限制。当前 change 不新增网络预测，但不能给未来预测客户端生成最终 authority SourceHandle 留下正向契约。

### 不建立对象反查注册表

SourceHandle 只保存 Id、Instigator、SourceTags 与 CausalityChain。工厂、GenericLibrary 或全局 subsystem 都不得保存 `SourceHandle.Id -> UObject` 映射。

需要 `SourceStateInstance`、`SourceSkillEntry`、BuffInstance、装备对象或伤害上下文的系统，必须从所属领域的运行时上下文或显式参数取得，不能通过 SourceHandle 隐式反查。

## 风险 / 取舍

- 旧调用点会因删除 `UTcsStateComponent::CreateSourceHandle` 而需要一次性迁移；这是有意的破坏性清理。
- `FTcsSourceHandle` 作为 `USTRUCT(BlueprintType)` 仍需要默认无效构造用于 UPROPERTY、序列化和容器初始化；实现时应限制或清除 TCS 内部直接构造非负 ID 的调用点。
- Blueprint 可创建 SourceHandle 会增加误用风险；通过 authority 限制、函数命名和测试保证它只是转发入口。

## 迁移计划

1. 新增 `FTcsSourceHandleFactory` Root / Child API 与内部 ID 分配。
2. 将现有 `UTcsStateComponent::CreateSourceHandle` 调用点迁移到工厂。
3. 删除 StateComponent 上的 SourceHandle 分配状态和旧创建入口。
4. 新增 `UTcsGenericLibrary` Blueprint 转发函数。
5. 搜索并清理 `SourceHandle.Id > 0` 等错误有效性判断。
6. 增加 SourceHandle 工厂与 State removal 清理回归验证。

## 开放问题

- 无。
