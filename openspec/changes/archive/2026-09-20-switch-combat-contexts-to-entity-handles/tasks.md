## 1. Implementation

### 契约与上下文改型（句柄化）

- [ ] 1.1 `Source/TcsEffect/Public/Host/TcsEntityQuery.h`：遍历签名改 `TFunctionRef<void(FTcsCombatEntityHandle)>`；补 `GetLocation(FTcsCombatEntityHandle, FVector&) -> bool` 与 `IsAlive(FTcsCombatEntityHandle) -> bool`（纯虚 → 默认实现 + 注释写明"句柄→Actor 映射归宿主"）；文件顶部注释更新为"三支能力"
- [ ] 1.2 `Source/TcsEffect/Public/Chain/TcsEffectContext.h`：`Caster` / `Instigator` 改 `FTcsCombatEntityHandle`；`Targets` 改 `TArray<FTcsCombatEntityHandle>`；注释写明"目标存活经 `IsAlive` 询问宿主、框架不持 Actor 生命周期引用"
- [ ] 1.3 `Source/TcsDamage/Public/Flow/TcsDamageFlowContext.h`：同上三字段改句柄
- [ ] 1.4 `Source/TcsTargeting/Public/Targeting/TcsTargetSelectorStrategy.h`：`Resolve` 出参改 `TArray<FTcsCombatEntityHandle>&`
- [ ] 1.5 `Source/TcsTargeting/Public/Targeting/TcsTargetFilterStrategy.h`：`Pass` 入参改 `FTcsCombatEntityHandle`
- [ ] 1.6 `Source/TcsTargeting/Private/Chain/TcsStepSelectTargets.cpp`：执行器改为句柄流转；**移除"弱引用 Pin 失败即跳过"**（存活交给宿主 Filter；框架不做存活过滤）——写回保序不变

### 装置适配（宿主侧映射样板）

- [ ] 1.7 TcsEffect 装置：测试 Actor 经 `UTcsAttributeSubsystem::RegisterUnit` 取句柄并自建 `句柄↔Actor` 映射；`UTcsTestEntityQuery` 实现三支能力（遍历吐句柄 / GetLocation 取 Actor 坐标 / IsAlive 判存活）
- [ ] 1.8 TcsTargeting 装置：同上（装置选择器/过滤器改句柄签名；夹具登记三个测试实体）
- [ ] 1.9 TcsDamage 装置：同上（流程装置不涉目标遍历，只需上下文句柄字段同步）

### 文档与交接

- [ ] 1.10 plan2 Task 5 交接注记：`UTcsCombatEntityComponent` 注册实体时记录"我的句柄"；实体查询实现是**宿主侧唯一映射点**（句柄↔Actor）
- [ ] 1.11 台账/设计文档标注：`FTcsCombatEntityHandle` 是**进程内发号、非网络身份**（跨机身份属复制契约另案——与本次切换解耦）

## 2. Verification

- [ ] 2.1 UBT Development Editor 编译通过（零警告）
- [ ] 2.2 三套装置命令全绿：`Tcs.Test.Effect`（挂起/唤醒/熔断）、`Tcs.Test.Targeting`（选择/过滤/边界）、`Tcs.Test.Damage.Flow`（流程/黑板/收集）
- [ ] 2.3 规格库 `validate --strict` 全绿（4 个能力 delta 并入后）
- [ ] 2.4 迁移面核对：`git diff --stat` 覆盖清单与提案"Impact"节一致（无遗漏的 Actor 消费者）
- [ ] 2.5 Actor 残留自检：`grep -rn "AActor\*\|TWeakObjectPtr<AActor>" Source/TcsEffect/Public/Chain Source/TcsDamage/Public/Flow Source/TcsTargeting/Public` 只应命中 `TcsEntityQuery.h` 的实现说明或为零
