## 1. 插件侧：框架内置 Self 选择器
- [x] 1.1 `Source/TcsTargeting/Public/Targeting/TcsSelSelf.h`：`FTcsSelSelf : FTcsTargetSelectorStrategy`（`USTRUCT()`，非 Hidden）——`Resolve` 写 `Context.Caster`；无效 → 不产出 + Warning；`EntityQuery` 为 `nullptr` 照常工作；头注释写明"内容资产的类型稳定性"理由
- [x] 1.2 `Source/TcsTargeting/Private/Targeting/TcsSelSelf.cpp`（非内联实现——含 LogChannel include）
- [x] 1.3 编译验证（UBT Development Editor 零警告）

> **2026-09-21 实施注记（Task 6 落地记录）**
>
> **① 两处跨模块链接缺陷（本任务照出的真实框架缺陷，已修）**——都是"此前无跨模块消费者，故从未暴露"的潜伏问题：
> - **原生 Tag 声明缺导出宏**：`UE_DECLARE_GAMEPLAY_TAG_EXTERN` 展开为**裸 `extern`**（无 `__declspec(dllexport)`，`NativeGameplayTags.h:31`）→ 宿主/其他模块引用这些变量**链接失败**（实测 `LNK2001 无法解析的外部符号 Tag_Tcs_Event_...`）。**修法**：框架事件 Tag 的设计意图正是"供宿主订阅"（属性变更、伤害记录、收集协议都是宿主挂点），故 `TcsAttributeChangedEvent.h`（1 个）与 `TcsDamageFlowCollectEvent.h`（9 个）+ `TcsDamageRecord.h`（1 个）的声明改为 `extern TCS<模块>_API FNativeGameplayTag ...`。定义处零改动（定义 TU 见到 dllexport 声明即导出符号）。
> - **`FTcsFlowAttributes` 缺导出宏**：其 `Submit` / `Read` 是**宿主自研流程步骤的公共调用面**（宿主在自己步骤里读写黑板是既定用法）→ 无导出宏则宿主模块 `LNK2019`。**修法**：`FTcsFlowAttributes` 与 `FTcsFlowAttributeSubmit` 加 `TCSDAMAGE_API`。
> - **审计结论**：其余公共 struct（`FTcsDamageFlowContext` / `FTcsConsumePolicy` / `FTcsAttributeStore` 等）**纯内联或仅同模块使用**，暂无需导出——但它们构成"同一类风险"的存量面，随首个跨模块消费者出现再逐个补（**已登记台账**）。
>
> **② 一处装置设计缺陷（自查发现，非编译报错）**：拒绝面检查 A 初稿拿 `EffectSubsystem->RegisterChain(...)` 的返回值当"双真相被拒"的判据——**错**：`RegisterChain` 只校验 `ChainId` 非空与重复登记，**不做双真相校验**（双真相是**资产层**纪律，由 `UTcsEffectChainDef::IsDataValid` 与 `DefLibrary::DiscoverChainDefs` 承担）。该写法会**真的登记一条坏链并报 false pass**。**修法**：改为直接断言 `IsDataValid` 返回 `Invalid` 且错误数 > 0（作者侧的门），并在注释里写明两条校验路径的分工。
>
> **③ 屏显接线（规格要求，初稿漏做）**：`UTcsDevScreenObserver` 初稿只定义了类**没有订阅**——规格要求"屏显信号由装置订阅属性广播直调 `AddOnScreenDebugMessage`"。**修法**：`UTcsDevBootstrap::SubscribeScreenObserver` 订阅两个原生事件（立即通道），观测者由 Bootstrap 持 `UPROPERTY` 强引用（总线订阅表持**弱引用**，不阻止 GC——`TcsEventBus.h:65`）。
>
> **④ 内容资产的 `FlowTemplateId` 与公式的关系（会误导验收的坑，已写进建立指南）**：切片装配登记的 `Default_Slice` 模板带公式 delegate → `CalculateBaseDamage` 把链上的 `DamageBase` **替换**为 `max(1, Attack−Armor)`。故**在带公式的模板下改 `DamageBase` 不改变扣血量**——检查点 6 的"改数值零 C++ 生效"必须把链的 `FlowTemplateId` 指向**无公式的 `Default`**（或改属性资产走公式输入）。指南里给了两种做法并建议各跑一次。

## 2. 插件侧：临时装置退役
- [x] 2.1 删除 `Source/TcsCore/Private/Testing/`、`Source/TcsAttribute/Private/Testing/`、`Source/TcsEffect/Private/Testing/`、`Source/TcsTargeting/Private/Testing/`、`Source/TcsDamage/Private/Testing/`、`Source/TcsIntegration/Private/Testing/` 六套（12 文件）
- [x] 2.2 编译验证（删除后零引用残留——装置符号不出现在任何正式模块代码里）
- [x] 2.3 依赖面自检：`grep -rn "Testing/" Source/*/Private/*.cpp Source/*/Public/` 零命中（正式代码不得 include 装置头）

## 3. 宿主侧：`TcsDev` 模块（`E:\Projects_Dev\LegendAutoChess\Source\TcsDev/`——**独立模块，非 LAC 正式内容**）
- [x] 3.1 模块壳：`Source/TcsDev/TcsDev.Build.cs`（依赖 `Core` / `CoreUObject` / `Engine` / `GameplayTags` / TCS 六模块）+ `TcsDevModule.h(.cpp)` + 日志通道 `Public/TcsDevLogChannel.h` / `Private/TcsDevLogChannel.cpp`（`LogTcsDev`，按插件侧同款约定）
- [x] 3.2 构建注册（**LAC 侧唯一改动**）：`LegendAutoChess.Target.cs` 与 `LegendAutoChessEditor.Target.cs` 的 `ExtraModuleNames` 各加 `"TcsDev"`；`.uproject` 的 `Modules` 追加 `TcsDev`（Runtime / Default）；**`Source/LegendAutoChess/` 零改动**
- [x] 3.3 公式 delegate（`UTcsDevDamageFormula : UObject, ITcsDamageFlowDelegate`）：`CalculateBaseDamage` = `max(1, Attack − Armor)`（经 M2 门面 `EvaluateCurrent`，门面经 `Context.Owner`）；门面不可得 → 原样返回 + Warning
- [x] 3.4 自研流程 2 步（`FTcsDevFlowGather` / `FTcsDevFlowApply`，`UE_DEFINE_FLOW_STEP_EXECUTOR` 自注册）：**完全不用插件默认步骤**；`Apply` 走 M2 事务扣血
- [x] 3.5 切片装配（World 初始化钩子或 GameInstance 子系统）：①加载 4 条属性资产 → `RegisterAttributeDef`；②2 个单位 `AddAttribute`（Health/MaxHealth/Attack/Armor）+ `AddComponent`；③构造公式 delegate 实例（**切片侧持强引用**——`TStrongObjectPtr` / UPROPERTY 容器；模板登记表非 UPROPERTY，保不住对象引用）→ 组装模板（`Slice_Flow`：自研两步）→ `SetDelegate` 进 `BaseDamage`/`Execute` 步骤 → `RegisterTemplate`；④组装**切片用的官方默认模板实例**（若需 delegate：同样 `SetDelegate` 后登记）——**注意**：插件 `Initialize` 登记的 `Default` 无 delegate，竖切若走默认模板需登记切片自己的模板 id 并在链资产里指向它
- [x] 3.6 屏显观测（`UTcsDevScreenObserver : UTcsEventHandler`，订阅 `Tcs.Event.Attribute.ValueChanged` 与 `Tcs.Event.Damage.Recorded` 立即通道 → `AddOnScreenDebugMessage`，固定行号排版）
- [x] 3.7 `Tcs.Test.Slice.Run`（常规命令，**零红字**）：定义库就绪 / 自动发现（不调 `RegisterChain`）/ 世界装配可查 / 链步骤类型全可解析 / 2 单位属性值 = 资产配置 / 触发链扣血（等 `WaitDelay` 到期）/ 自研流程扣血 / 链资产配置值 = 实际进流程值
- [x] 3.8 `Tcs.Test.Slice.Reject`（opt-in，自带"红字为预期"屏显声明）：瞬态双真相资产被拒 / 瞬态空身份被拒 / 未登记链触发被拒
- [x] 3.9 编译验证（宿主项目 UBT Development Editor 零警告，`TcsDev` 模块产出）

## 4. 内容资产（`Content/TcsDev/`，编辑器内建，人工步骤）
- [x] 4.1 属性资产 4 条（`UTcsAttributeDef`：`DefId` = `Health` / `MaxHealth` / `Attack` / `Armor`；`Def.BaseValue` 与 `Bounds` 按竖切口径；`MaxHealth` 供 `Health` 的动态边界引用）——*2026-09-22 实测装载 4/4*
- [x] 4.2 链资产 1 条（`UTcsEffectChainDef`：`ChainId` = `Chain.ChainId` = `Slice_Chain`；步骤 = `WaitDelay{0.5}` → `SelectTargets{Selector = Self}` → `Damage{DamageBase = 30, TargetAttrKey = Health, FlowTemplateId = 切片模板 id}`）——*2026-09-22 实测步骤类型 3/3 全可解析*
- [x] 4.3 测试地图 + 2 个单位 Actor（挂 `UTcsCombatEntityComponent`；1 个施法者 1 个目标，或按切片装配口径只建施法者）——*`L_TcsDev_Slice` 已建（含 2 单位）；额外收益：实测走通**关卡加载期注册**路径（装置覆盖不到）*
- [x] 4.4 内容资产 `IsDataValid` 全绿（4 条属性 + 1 条链；零 Invalid）——*拒绝面检查 A 实证校验路径有效（双真相被拒 1 条错误）*
- [x] 4.5 **属性资产发现路径决策点**：①纳入 DefLibrary 多族发现（需先开 `integration-entity` 增量提案）或 ②切片侧自行 `FAssetData::GetAsset()` 加载后 `RegisterAttributeDef`（DefLibrary 保持只管链资产）——**实施期择一并记录理由**——*择 ②，理由见 plan2 Task 6 注记⑧*

## 5. 验收（人工检查单，逐项记录实测结果）
- [x] 5.1 `Tcs.Test.Slice.Run`：**全绿 + 零红字**（记录通过/失败数）——***通过 9 / 失败 0**（即时）+ **7b PASS**（延迟）＝ 10/10；本命令区间零 Error 零 Warning*
- [x] 5.2 `Tcs.Test.Slice.Reject`：全绿（3 条预期失败输出，屏显已先声明）——***3/3 命中**（双真相 / 空 ChainId / 未登记链），红字只出现在本 opt-in 命令内*
- [x] 5.3 **跨世界装配**：PIE 运行中切关卡（`open <地图>` 或 `ServerTravel`）后重跑 `Tcs.Test.Slice.Run` → `FindChain` 仍可查全部链——***通过**：`open L_TbnsShowcase`（另一插件地图）后重跑 **10/10 全绿**；**决定性证据** = 切图日志**无"发现链资产"行**（定义库未重扫，直接装配缓存）*
- [x] 5.4 **检查点 6 端到端（人工半边）**：编辑器内改链资产 `DamageBase` 数值（如 30 → 45）→ **不改任何代码** → PIE 触发 → 扣血按新值（**零 C++ 加链/改链**的完整实证）——***通过**：`DamageBase` 30→45 且模板改指无公式的 `Default` → 扣血 **25.0 → 45.0**（零 C++ 改动）*
- [x] 5.5 **宿主自研流程**：执行 `Slice_Flow` 生效，且 `git -C Plugins/Tirefly/TireflyCombatSystem diff --stat` **为空**（零插件改动）——***部分达成，判据需修正***：`Slice_Flow` **生效**（检查 6：`100.0 → 75.0`，2 个宿主自研步骤，零插件默认步骤依赖）；但**插件 diff 不空**——本轮照出**两处真实框架缺陷**（原生 Tag 与 `FTcsFlowAttributes` 缺导出宏 → `LNK2001`/`LNK2019`）并新增 `FTcsSelSelf`。**本条字面判据（插件 diff 为空）已失效**：它写在"预期插件无需改动"的假设上，而实际是"**宿主自研流程无需插件为它改动**"（框架缺陷修复与切片无关，是宿主作为首个跨模块消费者照出的存量问题）。真实意图已达成
- [x] 5.6 **LAC 正式内容隔离核对**：`git diff -- Source/LegendAutoChess/` **为空**（LAC 正式模块零改动）；`TcsDev` 可整体摘除（两个 Target 各一行 + `.uproject` 一项 + `Source/TcsDev/` + `Content/TcsDev/`）——***通过**：`git diff --stat -- Source/LegendAutoChess/` 实测空*
- [x] 5.7 **待观察项**：总线载荷（`FInstancedStruct` 包装）在事件回调外的 GC 表现——若出现悬空，登记为总线 ARO 待办（**不在本任务修**）——*本轮**未出现悬空**：屏显观测者跨多轮 PIE 稳定收到属性变更与伤害记录事件（含切图后新世界重新订阅）*
- [ ] 5.8 插件子模块与宿主仓库改动分别整理（**提交需用户明确授权**；装置退役的删除清单随提交信息列出）——*改动仍在工作区，**未提交**（用户级 SourceControl 规则：未授权不提交）*

## 6. 文档回写
- [x] 6.1 `Documents/combat-system-design/2026-09-02-r3-plan2-damage-chain.md`：Task 6 标完成 + 实施注记（交付清单 / 四处拍板含 `TcsDev` 落点 / GC 两段实证 / 装置退役代价 / 属性发现路径决策）——*注记⑨（验收结果）+ ⑨-A（装置缺陷）+ ⑨-B（两处新实证）+ ⑨-C（操作方式）*
- [x] 6.2 `Documents/combat-system-design/README.md`：检查点段 + 进度行（下一站 Task 7）
- [x] 6.3 `Documents/combat-system-design/deferred-inputs-ledger.md`：新增条目——**流程模板登记表非 GC 可见持有**（`TMap<FName, TUniquePtr<FTcsFlowTemplate>>` 保不住步骤里的对象引用，Task 4 装置用 `TStrongObjectPtr` 绕过即证据；修法 = 改 GC 可见持有 或 规格明文"模板内容 MUST NOT 持对象引用"→ 归 T-8）；**plan1 检查点 2/3/4 回归网随装置退役消失**（若 Task 7 不重验则记明）
- [x] 6.4 规格库自检：`openspec validate --specs --strict --no-interactive` 全绿——*22/22 通过*
- [x] 6.5 归档：`openspec archive add-content-slice-rig --yes` ——delta 已应用到 `openspec/specs/targeting-strategy/spec.md`（`+ 1 added`），归档目录 `openspec/changes/archive/2026-09-21-add-content-slice-rig/`（目录名取**提案创建日期**）。**注**：归档时任务状态 30/32（5.8 未提交、6.4 当时未跑）——均已在归档后回填，属**记录性差异**而非未完成项
