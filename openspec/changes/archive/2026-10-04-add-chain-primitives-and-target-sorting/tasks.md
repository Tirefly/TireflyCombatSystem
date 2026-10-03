## 1. 提案与规格（本批 Step 1）

- [x] 1.1 `openspec validate add-chain-primitives-and-target-sorting --strict --no-interactive` 通过
- [x] 1.2 提案获用户评审批准（**未获批不得开工实施**）

## 2. 实施：TcsEffect 四个链原语

- [x] 2.1 `Public/Chain/TcsStepSetVar.h` + `Private/Chain/TcsStepSetVar.cpp`（`FTcsParamValue` 求值 + `EffectChainRunVar` 键空间 + 无效键跳过 + Warning + `UE_DEFINE_EFFECT_STEP_EXECUTOR`）
- [x] 2.2 `Public/Chain/TcsStepRunSubChain.h` + `Private/Chain/TcsStepRunSubChain.cpp`（**起链前取齐数据、起链后不碰 `Context`/`Run`**；`bWait=true` 且子链挂起 → `TSR_Running`；`ChainId` 未登记/为空 → 按完成 + 日志）
- [x] 2.3 `Public/Chain/TcsStepBranch.h` + `Private/Chain/TcsStepBranch.cpp`（条件走条件注册表；**链侧上下文只有 `Caster` 有来源**；`ThenChainId`/`ElseChainId`；`bWait` 复用 2.2 的唤醒机制——**MUST NOT 另写一套**）
- [x] 2.4 `Public/Chain/TcsStepWaitEvent.h` + `Private/Chain/TcsStepWaitEvent.cpp`（首入订阅 + 可选入到期堆；命中写 `Context.EventPayload`；超时**不**伪造成载荷；两路互相取消；无效 tag → 立即完成 + Warning；无总线 → 降级完成 + Error）
- [x] 2.5 `TcsChainRun.h`：运行态新增挂起锚（在等哪个 tag）与"等待方"字段；`ReleaseRun` 解锚清单同步补齐；注释块补"嵌套起链共用预算"与"深度上限"两条纪律
- [x] 2.6 `TcsEffectSubsystem.h` / `.cpp`：等待表（`Tag → 订阅句柄` 计数配对 + `Tag → 等待者集合`）+ `UPROPERTY` 锚定的共享事件 Handler（**弱引用不 root 会静默失效**）+ 按 tag 取快照 → 逐个代际校验 → 写载荷 → `ResumeRun`
- [x] 2.7 `TcsEffectSubsystem.cpp`：`MaxStepsPerFrame` 改为**最外层进入共享预算**（嵌套起链计入父链）+ **固定常量嵌套深度上限**（熔断断链 + Error 含深度值）
- [x] 2.8 `ReleaseRun` 通知"等待本链结束"的等待方（**异常结束路径同样通知**——正常走完 / 熔断 / 定义不可解析 / 未知步骤类型四条全过）
- [x] 2.9 子系统实现文件若超 300 行，按 `TcsEffectSubsystem_<Feature>.cpp` 先例拆分（`TcsEffectSubsystem_Trigger.cpp` 是既有样本）
- [x] 2.10 `TcsEffectContext.h`：`Variables` 的"R3 无写入方，留位"注记改为"首个写入方 = `SetVar`"

## 3. 实施：TcsTargeting 排序相位（P-A）

- [x] 3.1 `Public/Targeting/TcsTargetScorerStrategy.h`（虚分派基类 + 中性默认实现 0.0 + `meta=(Hidden)`；`Score(Candidate, Context, EntityQuery)`；`EntityQuery` 可空且降级值 MUST 为 NaN）
- [x] 3.2 `Public/Targeting/TcsTargetSortItem.h`（`FInstancedStruct Scorer` + `ETcsTargetSortDirection`，值 0 = 升序）
- [x] 3.3 `Public/Targeting/TcsScorerDistance.h` + `.cpp`（唯一内置；只依赖 `ITcsEntityQuery::GetLocation`；分数 = **真实欧氏距离**；任一端无定位 → **NaN**，MUST NOT 用 `-Inf`/0）
- [x] 3.4 `Public/Host/TcsTargetScorerHost.h`（`UINTERFACE(MinimalAPI, Blueprintable)` + `UFUNCTION(BlueprintNativeEvent) double ScoreTarget(Candidate, Caster, Instigator)`）
- [x] 3.5 `Public/Targeting/TcsScorerHostDelegate.h` + `.cpp`（转发器；**空 Host = 0.0 + Warning**；脚本返回 NaN ⇒ 候选被排除；类型注释写明性能边界 SHOULD"评分优先 C++、插槽用于低频"）
- [x] 3.6 `Public/Chain/TcsStepSelectTargets.h`：新增 `SortItems`（`meta=(BaseStruct="/Script/TcsTargeting.TcsTargetScorerStrategy")`）与 `MaxCount = 0`
- [x] 3.7 `Private/Chain/TcsStepSelectTargets.cpp`：执行序改为 Resolve（**写入局部数组，末尾整体写回**——不是"先清空 `Context.Targets`"，措辞差异见台账 `CHAIN-4`）→ 过滤（AND+短路）→ **去重（键 = 句柄身份，无条件相位）** → **排序（严格字典序 + 每项独立方向 + 稳定键 `Id` 升序决胜 + NaN 排除）** → **取前 K** → 写回；**排序项为空时保持既有保序行为**；`Resolve` 契约一字不改
- [x] 3.8 评分**预计算**（每个评分器对每个候选只求值一次——逐次比较现算会把调用放大到对数倍）；**复用缓冲的完整形态裁定收窄**：P-C 预览面尚不存在，"执行器/预览共用 scratch"没有共用对象 ⇒ 本轮不引入 scratch 所有权，只做预计算 + 不引入池化基础设施/租约语义（登记进台账，归属 P-C）

## 4. 文档与注释回写（**与实施同批，否则库内自相矛盾**）

- [x] 4.1 `SPEC-06-targeting` §5「非目标」：删去"**不做目标评分/权重排序（未见需求）**"
- [x] 4.2 `SPEC-06-targeting` §3「确定性纪律」：补严格字典序 + 稳定键决胜（`Id` 升序）+ NaN 排除，并把"客户端重算一致"**收窄为同进程口径**（附依据）
- [x] 4.3 `SPEC-06-targeting` §2.2：补"资格不得伪装成低分"的分工纪律
- [x] 4.4 `deferred-inputs-ledger.md` 登记五笔（**已落 `CHAIN-1`~`CHAIN-5`**）：①`Branch` 在链侧无 `EventTag`/`ClassificationTags` 来源；②评分器插槽性能边界仅为 SHOULD；③嵌套深度上限的**定值依据**（实测）；④**`SelectTargets` 不在 `Resolve` 前清空 `Context.Targets`**（与规格"清空 `Context.Targets`"的措辞不一致；今天零消费者读取它，裁定**不改行为**只登记）；⑤复用缓冲收窄（归属 P-C）
- [x] 4.5 `PLN-R4` 回写本批的实施注记（含 `design.md` 的偏离表 + 深度上限实测值 + 熔断自检实测值）——**注记已落**，**两个实测值已回填**（深度 17 / 累计步数 17 / 16 层父链回卷；证据 = `EVID-2026-10-04-chains-primitives`），`PLN-R4` 的 Step 1–4 全部勾上

## 5. 编译验证（Step 3）

- [x] 5.1 `LegendAutoChessEditor Win64 Development` 编译 `Succeeded`，**0 error / 0 warning**
- [x] 5.2 `LegendAutoChess Win64 Shipping` 编译 `Succeeded`，**0 error / 0 warning**（本批含 `TcsChainRun` 结构改动与新 `UPROPERTY` 面，Shipping 必跑；P-A 搭同一次顺风车）
- [x] 5.3 依赖面复核：`TcsEffect` 零领域模块 include；`TcsTargeting` 零新增模块依赖；`Build.cs` / `.uplugin` 零改动（`git diff --stat` 取证）

## 6. 定向人工检查（Step 4）

- [x] 6.1 **熔断自检（嵌套）**：构造 `RunSubChain` 自激链，实测①子链步数计入父链预算、②深度上限触发熔断，两个实测值写进计划注记与本文——**实测：17 层起链 → 深度 17 熔断 → 16 层父链回卷完成；累计步数 17（预算 64 未触发）**；两次实跑一致
- [x] 6.2 **订阅配对自检**：N 条链等同一 tag 时总线只有一条订阅；释放后订阅计数回落（无泄漏）——**实测：`订阅`×1 + `登记等待者` 1/2/3 + 唤醒时 `摘除` 2/1/0 + `退订（等待者归零）`×1**（用 `DA_Check_WaitEvent3` 一条命令造 3 个并存等待者）
- [x] 6.3 **解锚自检**：**可达范围须知（2026-10-03 复核）**——今天**没有公开的取消/释放运行态入口**，而 `ReleaseRun` 的五条调用路径都要求"运行态正在跑"（挂起的链不跑）⇒ 本项只能按**世界反初始化**验证：PIE 内起一条停在 `WaitEvent` 的链 → 结束 PIE ⇒ 日志出现 `链事件等待：退订（事件=… 等待者归零）`、无 ensure/Error；再开一次 PIE → 发同一事件 ⇒ 无任何回调（跨 PIE 零残留）。`ReleaseRun → DropEventWait` 与 `ParentRun` 通知两条分支的真实覆盖**待 M5 打断/取消轮**（台账 `CHAIN-6`）——**实测：停 PIE 区间零红字；新 PIE 重新登记 5 条链（确为新会话）+ 之后 0 行命中唤醒**（注：`Reset` 的全量退订是静默的，不打逐 tag 的 `退订` 行——原预期已按实测更正）
- [x] 6.4 **P-A 追加检查**：排序项在链资产上可配（**零 C++**）；并列时稳定键决胜可复现（**同一输入两次跑结果集逐位一致**）；NaN 评分的候选被**排除**而非排末尾；去重后同一句柄只出现一次——**实测（`DA_Check_SortMulti`）**：`候选 4 → 通过 4 → 去重 1 → 排序项 1 → 取前 2 → 目标集 1`，两次逐字一致；距离并列（两单位同在原点的夹具条件下）由 `Id` 升序决胜（伤害落在先 spawn 的施法者）；取前 2 而非 3 = 未注册句柄被 NaN 排除
- [x] 6.5 **既有回归**：`DA_SliceChain` / `DA_FormulaChain` 未被改动；不配排序项的既有链**顺序**不变（`SortItems` 为空 ⇒ 不重排、`MaxCount = 0` ⇒ 不截断）；**唯一例外 = 无条件去重**（既有选择器都产单目标，不受影响——见 `proposal.md` 的 BREAKING 段）——**实测**：`SelectTargets[EffectChain.SliceChain]: … 去重 0 → 排序项 0 → 取前 1 → 目标集 1`（两次独立 PIE 各一次）+ 两资产 `git status` 零改动
- [ ] 6.6 无时钟/无总线/未注入 `EntityQuery` 三条降级路径各留痕（Error 或 Warning，按规格口径），**不崩溃、不挂死**——**未跑（如实留白）**：三条都要刻意撤掉设施，而装置每次注入查询/总线、且固定在游戏世界 PIE 下 ⇒ **无时钟 / 无总线在不改代码的前提下无法撤掉**；"未注入查询"可安排但需给装置加一个跳过注入的开关。归属 = M5 打断/取消轮顺带（届时"取消中设施正在拆"是真实场景）；详见 `EVID-2026-10-04-chains-primitives` 末节

## 7. 归档

- [x] 7.1 `openspec archive add-chain-primitives-and-target-sorting --yes`——已归档为 `2026-10-04-add-chain-primitives-and-target-sorting`（`effect-interpreter` +3/~2、`targeting-strategy` +3/~1；`openspec list` 显示无活动变更）
- [x] 7.2 **归档后人工核对两张规格的 `## Purpose`**（归档器不会更新既有能力的 Purpose；本批给 `targeting-strategy` 加了排序相位、给 `effect-interpreter` 加了控制流原语，Purpose 需同步）——**两张都需同步、已改**：`effect-interpreter` 补"三类唤醒源 + 嵌套深度护栏 + 链原语（等待/事件等待/链内变量/子链与分支）"；`targeting-strategy` 改为"选择器/过滤器/**评分器**三组基类 + 执行器四相位（过滤→去重→排序→取前 K）+ 内置距离评分器 + 排序项契约 + 三类插槽"
- [x] 7.3 `openspec validate --all --strict --no-interactive` 全绿——**25 passed / 0 failed**（改 Purpose 后复跑仍全绿）
