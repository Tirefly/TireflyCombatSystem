## 1. Implementation

### 策略契约（Public/Targeting/）

- [ ] 1.1 `Public/Targeting/TcsTargetSelectorStrategy.h`：`USTRUCT(meta = (Hidden)) FTcsTargetSelectorStrategy`——`virtual void Resolve(const FTcsEffectContext& Context, ITcsEntityQuery* EntityQuery, TArray<TWeakObjectPtr<AActor>>& OutTargets) const`；**中性默认实现**（不产出目标）+ 注释写明"禁纯虚（UHT TCppStructOps 需可默认构造；`PURE_VIRTUAL` 随 `CHECK_PUREVIRTUALS` 变语义——`CoreMiscDefines.h:100-102`）"与"执行器清空、策略只填充""产出弱引用"
- [ ] 1.2 ~~`Public/Targeting/TcsSelSelf.h`~~ **取消**（2026-09-20 用户拍板：R3 框架零默认选择器——与零默认 Filter 同条纪律）；选择器由装置提供（见 1.7）
- [ ] 1.3 `Public/Targeting/TcsTargetFilterStrategy.h`：`USTRUCT(meta = (Hidden)) FTcsTargetFilterStrategy`——`virtual bool Pass(const AActor* Candidate, const FTcsEffectContext& Context) const`；中性默认实现返回 `true`；注释写明"框架零默认 Filter（存活/敌对 = 宿主本体论）"与"AND + 短路由执行器保证"

### SelectTargets 步骤与执行器

- [ ] 1.4 `Public/Chain/TcsStepSelectTargets.h`：`USTRUCT() FTcsStepSelectTargets{ TInstancedStruct<FTcsTargetSelectorStrategy> Selector; TArray<TInstancedStruct<FTcsTargetFilterStrategy>> Filters; }`（`EditAnywhere`；`BaseStruct` 限定由 UHT 从模板实参自动写入）
- [ ] 1.5 `Private/Chain/TcsStepSelectTargets.cpp`：执行器 `ExecuteStepSelectTargets`——①清空 `Context.Targets`（保留 Selector 未配置分支）②取 `EntityQuery`（`Run.Owner` → 门面 `GetEntityQuery()`）③`Selector->Resolve(...)` 产出候选 ④逐候选过 `Filters`（AND + 短路，Pin 失败跳过）⑤通过者**保序**写回 `Context.Targets` ⑥恒返回 `TSR_Completed`；未配 Selector → 目标集原样 + Warning
- [ ] 1.6 同文件末尾：`UE_DEFINE_EFFECT_STEP_EXECUTOR(FTcsStepSelectTargets, ExecuteStepSelectTargets)`——**跨模块自注册**（TcsTargeting → TcsEffect 注册表，机制层零改动）

### 临时测试装置（`Private/Testing/`，**不入库**）

- [ ] 1.7 `Private/Testing/TcsTargetingTestRig.h/.cpp`：装置选择器 `FTcsTestSelFromQuery`（经注入的 `ITcsEntityQuery` 枚举——**Task 1 注入点的首个消费者**；`EntityQuery` 为空时产出空集 + Warning）+ 两个装置 Filter（`FTcsTestFilterRejectIndex` 按装置侧清单下标拒一个候选 / `FTcsTestFilterAlwaysPass`）+ 观察步 `FTcsTestStepObserveTargets`（读回 `Context.Targets` 数量与名字）+ `UCLASS() UTcsTestTargetingQuery : public UObject, public ITcsEntityQuery`（枚举装置登记的测试 Actor 列表）
- [ ] 1.8 `Tcs.Test.Targeting`（正路，**零故意 ensure/Error**）：PIE 世界临时 spawn 3 个测试 Actor 并登记进装置查询 → 注入查询 → 执行 `[SelectTargets(装置选择器 + 按下标拒第 2 个的滤波器) → 观察步]` 链 → 断言目标数 = 2 且**保序**（第 2 个缺席）；空 Filter 数组 → 3 个全过；未配 Selector → 目标集保持原样 + Warning；屏显分节排版；命令尾清理（销毁临时 Actor + 撤下注入）
- [ ] 1.9 `Tcs.Test.Targeting.Reject`（**opt-in**，只产 Warning、无 ensure/Error）：未注入查询时跑同一条链（装置选择器降级为空集 + Warning）；未配 Selector（目标集原样 + Warning）

## 2. Verification

- [ ] 2.1 UBT Development Editor 编译通过（零警告）
- [ ] 2.2 **跨模块自注册实证**：注册表可查到 `FTcsStepSelectTargets` 执行器；链执行分派到它
- [ ] 2.3 **机制层零改动自检**：`git diff --stat -- Source/TcsEffect` 为空（需要改机制层即为 Task 1 契约缺陷）
- [ ] 2.4 依赖面核对：`Source/TcsTargeting/` 不 include `TcsState/TcsSkill/TcsDamage/TcsIntegration`
- [ ] 2.5 用户 PIE 人工检查：装置命令输出（目标数/顺序 + 两条边界）
- [ ] 2.6 文档回写：plan2 Task 2 勾选 + 实施注记（含 EventTarget 顺延与 Filter 落点）；台账补 `IRelationResolver` 条目；README 检查点状态
