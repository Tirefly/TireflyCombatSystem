## 背景

运行时对象不共享单一“万能 InstanceId”。Definition 身份、长期拥有态身份、可并存运行时实例身份、事件因果链身份和未来预测期身份解决的是不同问题。把它们压成同一个字段会导致 authority 验证、预测 reconcile、存档和运行时账本相互污染。

当前 Definition 重构只需要保证 DefId 主路径和现有运行时实例可以工作。网络同步和本地预测必须作为后续独立实现议题，不应在没有明确网络需求时提前添加字段或 RPC。

## 身份边界

- `DefId` 是定义资产和长期条目身份：
	- `SkillEntry` 使用 `SkillDefId`。
	- 单 Owner 下的 `AttributeInstance` 使用 `Owner + AttributeDefId`。
- 可并存的运行时实例保留实例级 authority 身份：
	- `StateInstance`、`BuffInstance`、`SkillInstance` 使用 `StateInstId`。
	- `AttributeModifierInstance` 使用 `AttrModInstId`。
	- `SkillModifierInstance` 使用 `SkillModInstId`。
- `StateParamInstance` 从属于一个 `StateInstance`，通过 `StateInstance + StateParamName` 验证即可，不额外增加实例级 ID。

## SourceHandle

- `FTcsSourceHandle` 是 TCS 事件因果链的唯一 authority 存储结构。
- `FTcsSourceHandle::Id` 是 AuthorityOnly。
- `SourceHandle` 用于因果归因和 authority 有效性验证语境，不替代 `StateInstId`、`AttrModInstId` 或 `SkillModInstId`。
- 客户端预测阶段不得自行生成最终 authority `SourceHandle`。

## 未来预测与 Reconcile

- 只有未来明确进入本地预测主路径的对象才需要 `PredictionKey`；当前已知候选仅为 `SkillInstance`。
- 未来预测优先采用 GAS 风格：根请求携带 `PredictionKey`。
- 一个 `PredictionKey` 不会产生多个同类预测实例，因此不预先设计全局 `LocalInstanceId` 或 prediction-scope 局部序号。
- authority 在最终实例创建成功且其他有效性验证通过后，立即分配对应实例级 authority 身份。
- 客户端通过既有根请求确认/复制链路将 `PredictionKey` 关联到最终实例级 authority 身份；不得仅为实例 reconcile 新增专门 RPC。
- `AttributeModifierInstance` 和 `SkillModifierInstance` 当前不进入预测主路径；只有未来出现明确预测需求时才重新评估。

## 实现门槛

任何网络代码必须由单独获批的后续 change 实现，并至少明确：

1. 首个进入预测的主请求和对应对象范围。
2. authority allocator 的宿主、生命周期和服务器权限边界。
3. 预测确认、拒绝、回滚和断线恢复行为。
4. `SourceHandle` 与实例级 ID 的复制或事件载荷策略。
5. 现有 `Component static` ID 工厂的替换或保留迁移路径。
