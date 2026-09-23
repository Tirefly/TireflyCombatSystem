## 1. 实现

- [x] 1.1 `UTcsDamageSubsystem` 加静态 `AddReferencedObjects` 覆写（`TcsDamageSubsystem.h` 声明 + `.cpp` 实现，逐模板调 `AddPropertyReferencesWithStructARO`）
- [x] 1.2 `UTcsEffectSubsystem` 加同款覆写（`TcsEffectSubsystem.h` 声明 + `.cpp` 实现，逐链调同 API）
- [x] 1.3 两处头文件写明**为什么必须自己实现**（裸容器不经 RefLink / `TUniquePtr` 非 GC 可见持有 / 静默回收的失败形态）与手法依据（`UDataTable::AddReferencedObjects` 先例、`FInstancedStruct` 递归）

## 2. 验证（禁 TDD 纪律：编译 + 定向人工检查）

- [x] 2.1 UBT Development 编译零警告零错误
- [x] 2.2 UBT Shipping 编译通过（照 `WITH_EDITOR` 类缺陷的配置）
- [ ] 2.3 定向人工检查：GC 场景实证（PIE 内 `obj gc` 后步骤引用仍有效）—— **未执行**：R3 无"无其他强引用的 delegate"夹具，宿主当前用 `UPROPERTY` 强引用绕过（使其成为冗余保险）；本条的实证留待第一个不绕过的消费者出现时补，或随 T-8 资产化轮一并验（资产 root 后本问题自然消解）
- [x] 2.4 全库自检：确认其余非 UPROPERTY 容器不持对象引用（`FTcsAttributeStore` 纯数值 + 句柄，无需处理）

## 3. 规格与文档

- [x] 3.1 `openspec validate fix-registry-gc-visible-holding --strict`
- [x] 3.2 提案归档（delta 并入 `damage-flow` / `effect-chain`）
- [x] 3.3 台账 T-8 更新（GC 面已修；资产化面仍待触发）
- [x] 3.4 README 决策日志追加条目

## 4. 未覆盖（如实记）

- **2.3 的 GC 实证未跑**：理由见该条。**这不是"已验证"**——规格新增的 Scenario（「模板步骤里的对象引用对 GC 可见」/「链步骤里的对象引用对 GC 可见」）**尚无实证**，其成立依据是引擎源码机制（`AddPropertyReferencesWithStructARO` 的递归语义，已核）而非实测。台账 T-8 应保留"未实证"标注。
