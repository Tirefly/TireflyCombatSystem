# Change: 落地内容资产竖切装置与框架内置 Self 选择器（targeting-strategy 补需求）

## Why
plan2 Task 6 动工前的规格先行提案。前五站交付的全是**机制与领域层**（链解释器 / 目标策略 / 流程机制 / 标准步骤库 / 集成层），装置一直是**临时件**（`Source/*/Private/Testing/`，不入库），且 **R3 至今没有一份内容资产**——于是 Task 5 的 `UTcsDefinitionSubsystem` 自动发现路径（`GetAssetsByClass` → 缓存 → 装配到世界）**从未被实证**，竖切剧本**检查点 6"零 C++ 加链"**也从未被验（Task 5 装置走的是手动 `RegisterChain`）。

本任务把竖切从"代码能跑"推到"**内容能配、策划能改**"：建真实内容资产（地图 / 2 单位 / 4 条属性 / 1 条链 / 1 个 delegate 实例）→ 让自动发现路径首次真实生效 → 验检查点 6 端到端；同时把**临时装置退役**（用户 2026-09-21 约束："plan2 Task 6 内容资产版装置就位后退役删除"）。

## What Changes

- **ADDED 能力规格 `targeting-strategy`**（1 条需求）：`FTcsSelSelf` 选择器进 R3（落 `Public/Targeting/TcsSelSelf.h`）。
  **动因 = 内容资产的类型稳定性**：内容资产里的 `TInstancedStruct` 存的是**类型身份**（包 import 表索引，非名字字符串，`InstancedStruct.cpp:199-234`）——链资产若引用切片侧类型，切片退役后该步骤会**静默变空**（降级路径 `InstancedStruct.cpp:237-248`，仅剩一条 `LogCore` Warning）。故"选自己"这一语义必须由**框架**提供。
  **与 Task 2 拍板的关系（显式交代）**：Task 2 拍过"R3 框架零默认选择器"，但该口径当时**被明确排除在规格之外**（用户 2026-09-20 要求"规格只装系统行为真相，'某一轮做到哪儿'归计划注记与台账"）——故本条是**纯新增**、无需撤销任何已归档规格文字；设计 `10 §2.2` 本就写"框架提供 Self / EventTarget"。`EventTarget` 仍留 R5（依赖"载荷 → 目标"通路，台账 R5-1）。
- **切片侧代码落宿主项目的 `TcsDev` 目录**（用户 2026-09-21 拍板："所有写入宿主项目 LAC 的代码，都统一放到 `TcsDev` 目录下，不作为 LAC 正式内容"）：公式 delegate、自研流程的 2 步、属性装配、屏显观测、`Tcs.Test.Slice.*` 命令族，全部住 `Source/TcsDev/`——**独立 UE 模块**，LAC 的正式模块 `LegendAutoChess` **零 TCS 依赖**（依赖边单向：`TcsDev → TCS 插件`，绝不反向）。
  **判据（"不作为正式内容"的物理形态）**：①模块名 `TcsDev` 而非 `LegendAutoChess` 的子目录——LAC 正式模块的 `Build.cs` 一行不改；②退役 = 从两个 Target 的 `ExtraModuleNames` 与 `.uproject` 的 `Modules` 摘掉两行 + 删 `Source/TcsDev/` + 删 `Content/TcsDev/`，**LAC 正式模块无残留**；③**LAC 正式模块 MUST NOT include `TcsDev` 的任何头**（装置契约是单向的：切片认识 LAC，LAC 不认识切片）。
  **收益**：用户追加的"宿主自研流程"验收（2–3 步、完全不用插件默认步骤）可用"**插件子模块 `git diff` 为空**"直接证明"零插件改动"。依据：设计写"插件零公式，公式 = **项目** delegate"、R3 剧本写"**测试项目** delegate"。
  **落点裁决（独立模块 vs LAC 模块内子目录）**：取**独立模块**——"不作为正式内容"的最强物理形态是"编译单元独立"（`TcsDev` 可整体不参与构建、可整体删除、可整体不随正式包发布）；若落 `Source/LegendAutoChess/TcsDev/` 子目录，则 LAC 正式模块的 `Build.cs` 必须加 TCS 依赖，且"退役"要改正式模块的构建文件（留下改动痕迹，与"不作为正式内容"的意图相悖）。
- **内容资产落 `Content/TcsDev/`**（与代码同域）：地图 / 2 单位 / 4 条属性资产（`UTcsAttributeDef`）/ 1 条链资产（`UTcsEffectChainDef`）。**不建 delegate 资产实例**（见下方纪律 4）——delegate 由切片侧构造并 `SetDelegate` 到模板步骤，故**不需要任何插件侧契约类**。
- **临时装置退役**：`Source/*/Private/Testing/` **六套全删**（用户 2026-09-21 拍板）。**显式代价**（不藏）：其中 `TcsCoreTestRig` / `TcsAcceptanceRig`（TcsAttribute）属 plan1，覆盖检查点 2/3/4 与核心句柄/事件/时钟——**内容版装置替代不了**这些覆盖（它们测的是"批内 0 条广播""同属性两次变更只 1 条广播"这类机制内省），删除后 R3 内**无回归网**；其结论已随 plan1 归档，Task 7 不重验。
- **不做**（非目标）：不做触发行（M4a/R5）；不做 AttributeSet（R7-1）；不做流程模板资产化（T-8）；不做 `PrimaryAssetTypesToScan` 注册与加载层三策略（R7-3）；不做复制；不做 UI/图鉴页面；不做 Mass 适配；**插件侧除 `FTcsSelSelf` 外零新增类型、既有类型零改动**（本提案不新建任何"切片契约"类——理由见纪律 4）。

## 落点与口径

1. **四处拍板**（2026-09-21 前置问答与追加指令，用户）：
   - **切片落点 = 宿主项目**（否决"插件新模块 TcsSample"/"TcsIntegration/Sample/"/"独立样例插件"）；
   - **`FTcsSelSelf` 提前落 TcsTargeting**（否决"仍由切片侧提供"）；
   - **六套临时装置 Task 6 末全删**（否决"plan1 两套留到 Task 7"）；
   - **宿主侧代码统一住 `Source/TcsDev/`，不作为 LAC 正式内容**（追加指令）——独立模块，LAC 正式模块零 TCS 依赖、零 include、零构建文件改动。
2. **内容引用的类型必须长于内容**：链资产引用 `FTcsSelSelf`（插件类型）与插件自带步骤（`FTcsStepWaitDelay` / `FTcsStepSelectTargets` / `FTcsStepDamage`）——全部随插件长期存在，故 `Content/TcsDev/` 不因本任务产生"引用宿主类型"的失联风险。**该纪律对本任务成立、对切片退役后的正式内容不成立**——正式内容（将来的策划内容）必须只引用随插件长期存在的类型。
3. **属性装配路径（拍板）**：属性**定义**用 `UTcsAttributeDef` 资产（4 条，`[PrimaryAssetType, DefId]` 身份）；**施加**由切片侧代码做（`RegisterAttributeDef` + `AddAttribute`），**不建 AttributeSet 资产**（D2-15 属 R7-1，R3 竖切剧本原话"属性由测试装置直接添加"）。
   **待定项（实施期定，两候选皆合法）**：4 条属性资产是否也纳入 DefLibrary 发现 —— ①纳入（DefLibrary 泛化为多族发现，`integration-entity` 规格措辞需开增量提案）；②不纳入（切片侧经 `FAssetData::GetAsset()` 自行加载后调 `RegisterAttributeDef`，DefLibrary 保持"只管链资产"的既有规格）。**本提案不预设**，tasks.md 里作为决策点列出。
4. **流程公式 delegate 的持有与 GC（两段实证，结论与初稿相反——留痕）**：
   - **① 内容资产里的 delegate 引用是 GC 安全的**（初稿误判为不安全，已核实推翻）：`FInstancedStruct` 自带 `WithAddStructReferencedObjects = true`（`InstancedStruct.h:275-283`），其 ARO 体递归调用 `Collector.AddPropertyReferencesWithStructARO(ScriptStruct, GetMutableMemory())`（`InstancedStruct.cpp:506-524`）；GC 侧 `FStructProperty::ContainsObjectReference` 对带该 flag 的结构体返回 true（`GarbageCollection.cpp:6733-6746`）→ `EmitReferenceInfo` 为元素发射 `EMemberType::MemberARO`（`:6941-6950`）→ `FArrayProperty::EmitReferenceInfo` 把内层 schema 逐元素展开（`:6869-6899`）→ `CollectStructReferences` 先调 ARO、再继续走内层 `RefLink`（`UObjectGlobals.cpp:5223-5245`）。**故"内容资产持 delegate 引用"这条路本身是通的**（前提：持有它的资产被 root——链资产由 DefLibrary 的 `ChainDefAssets` UPROPERTY 锚定，成立）。
   - **② 但代码组装的模板持不住对象引用**（**本任务发现的真实框架缺口**）：`UTcsDamageSubsystem::Templates` 是 `TMap<FName, TUniquePtr<FTcsFlowTemplate>>`（**非 UPROPERTY**）——GC 只走 `RefLink`，裸 `TUniquePtr` 不可见，故**模板步骤里的 `TScriptInterface` / `FInstancedStruct` 内对象引用不被子系统保活**。Task 4 装置已用 `TStrongObjectPtr` 绕过（`TcsDamagePrimitiveRig.cpp:253`），即该缺口的存在证据。
   - **本任务的做法**：R3 模板以代码组装（模板资产化属 **T-8**），切片侧 delegate 实例由切片侧持强引用（`TStrongObjectPtr` / UPROPERTY 容器）→ 装配期 `SetDelegate` 进模板步骤 → `RegisterTemplate`。**链资产不引用 delegate**（`FTcsStepDamage` 无 Delegate 字段——Task 4 已收窄），故内容侧无此风险。
   - **登记台账**：模板登记表应改为 GC 可见持有，或规格明文"模板内容 MUST NOT 持对象引用"——归 T-8 一并解决。
   - **副产物**：本任务**不新增任何插件侧"切片契约"类**——delegate 实现类住切片侧即可（它不出现在任何资产里，无需稳定类型锚）。
5. **装置纪律（本项目第三次重申）**：**任何"故意触发失败输出"的检查 MUST 独立成 opt-in 命令**；常规验收命令输出**零红字**（判据：任何以"某条 Error 是预期的"为注解的输出，都说明该检查放错了命令）。命名约定 `Tcs.Test.<域>`（零红字）/ `Tcs.Test.<域>.Reject`（自带"红字为预期"屏显声明）。
   **本任务的特殊约束**：DefLibrary 的发现是**全库扫描**——一个故意配错的资产会污染**所有**命令的失败清单，故**双真相资产 MUST NOT 常驻 `Content/TcsDev/`**：验收方式 = 装置内**运行期造瞬态 `UTcsEffectChainDef`**（`NewObject` + 双真相字段）走校验路径断言被拒。
6. **异步扫描门的首次真实生效**：`IsLoadingAssets()` / `WaitForCompletion()` 此前从未真正拦住过任何东西（无内容资产，扫描瞬间返回空）。本任务建资产后首次真实生效——装置 MUST NOT 把"空结果"当作"无链资产"（规格既有要求）。
7. **屏显与日志纪律**：插件模块零屏显调用（D0-6 v2）——屏显一律由切片/装置侧直调 `GEngine->AddOnScreenDebugMessage`；日志走原生分类（`LogTcsIntegration` / `LogTcsDamage` / `LogTcsEffect`）。
8. **验收命令落点**：内容版命令族住 **`TcsDev` 模块**（`Source/TcsDev/`）——它是宿主项目的竖切样例装置，与"切片落宿主、不作为正式内容"一致；插件侧不因本任务新增装置代码（既有 `Source/*/Private/Testing/` 六套删除）。
9. **`TcsDev` 模块的构建注册（LAC 侧唯一改动）**：`Source/TcsDev/TcsDev.Build.cs`（依赖 TCS 六模块 + `GameplayTags` 等）+ 两个 Target 的 `ExtraModuleNames.Add("TcsDev")` + `.uproject` 的 `Modules` 追加一项。**LAC 正式模块（`Source/LegendAutoChess/`）的 `Build.cs` 与源码一行不改**——这是"不作为正式内容"的可核对判据。

## Impact
- Affected specs：`targeting-strategy`（ADDED × 1：内置 Self 选择器）。**无 MODIFIED / REMOVED**（`integration-entity` / `effect-chain-asset` / `damage-*` 既有需求均不变）。
- Affected code：
  - **插件子模块（`Plugins/Tirefly/TireflyCombatSystem/`）**：新增 `Source/TcsTargeting/Public/Targeting/TcsSelSelf.h`（+ `Private/Targeting/` 执行器若需）；**删除** `Source/*/Private/Testing/` 六套；`Documents/` 三处回写（plan2 Task 6 注记 / README / 台账）。
  - **宿主项目（`E:\Projects_Dev\LegendAutoChess/`，非本子模块、单独提交）**：新增 `Source/TcsDev/`（模块壳 `TcsDev.Build.cs` / `Module.h/.cpp` / 日志通道 + 公式 delegate / 自研流程 2 步 / 属性装配 / 屏显观测 / `Tcs.Test.Slice.*` 命令）+ `Content/TcsDev/`（地图 / 2 单位 / 4 属性资产 / 1 链资产）；两个 Target 的 `ExtraModuleNames` 与 `.uproject` 的 `Modules` 各加一项；**LAC 正式模块（`Source/LegendAutoChess/`）零改动**。
- 决策依据：`10-module-targeting.md §2.2`（框架提供 Self/EventTarget）；`09-module-damage.md §2.4` + D7-2/PV-7（插件零公式、公式归宿主）；`2026-09-02-r3-vertical-slice-script.md`（检查点 1/5/6/7 + 固定装置清单）；`06-module-integration.md §2.1`（DefLibrary 装配到世界）；Task 5 实施注记⑤（自动发现路径未被实证）；`MEM-20260921-01`（注解正当化反模式）、`MEM-20260918-07`（故意失败输出独立成 opt-in）。
- 验证：UBT 编译零警告 + 插件依赖面自检（零反向 include）+ **PIE 实测**（正路命令零红字 + `.Reject` opt-in）+ **检查点 6 端到端**（改资产数值 → 生效，零 C++）+ **宿主自研流程**（插件子模块 `git diff` 为空）+ 内容资产 `IsDataValid` 全绿。

## 提案内钉名（计划/设计未钉或需收窄）

| 项 | 钉法 | 依据 |
|---|---|---|
| 切片落点 | **宿主项目独立模块 `Source/TcsDev/`**（**非** `Source/LegendAutoChess/` 子目录）；内容落 `Content/TcsDev/` | 用户 2026-09-21 追加指令"所有写入宿主项目 LAC 的代码统一放 `TcsDev` 目录、不作为 LAC 正式内容"；独立模块 = 可整体不构建/可整体删除/LAC 正式模块零改动 |
| 切片模块名 | `TcsDev`（`Source/TcsDev/TcsDev.Build.cs`；模块类 `FTcsDevModule`；日志通道 `LogTcsDev`——若需要） | 与插件侧 `Tcs*` 命名族一致；"开发用、非正式内容"由模块名直接表达 |
| `FTcsSelSelf` | 落 `Source/TcsTargeting/Public/Targeting/TcsSelSelf.h`，`USTRUCT()`（**非 `Hidden`**——它是可选实现）；`Resolve` 写 `Context.Caster`，无效则不产出 + Warning；`EntityQuery` 为 `nullptr` 照常工作 | 设计 10 §2.2；内容资产的类型稳定性（切片类型会随退役失联） |
| 公式 delegate 类名/落点 | 住 **`TcsDev` 模块**（`Source/TcsDev/`），类名由切片自定（如 `UTcsDevDamageFormula`）；实例由切片侧持强引用 | **两段实证**：①内容资产里的 delegate 引用**是** GC 安全的（`FInstancedStruct` 带 `WithAddStructReferencedObjects`，ARO 递归到内层，`InstancedStruct.h:275-283` / `UObjectGlobals.cpp:5223-5245`）——初稿误判已推翻；②但**代码组装的模板登记表**（`TMap<FName, TUniquePtr<…>>`，非 UPROPERTY）保不住对象引用（Task 4 装置用 `TStrongObjectPtr` 绕过即证据）→ 故本任务 delegate 由切片侧持强引用 |
| 公式取属性路径 | 经 `UTcsAttributeSubsystem::EvaluateCurrent(句柄, 属性名)`（M2 门面），门面经 `Context.Owner` → `GetWorld()` 取用；属性名（`Attack` / `Armor`）由切片侧持有 | 02 文档属性门面；**插件机制层不认识任何属性名**（D7-2 零公式） |
| 自研流程步骤 | 2 步（读双方属性写黑板 → M2 事务扣血），**完全不用插件默认步骤** | 用户 2026-09-21 追加验收项；验"零插件改动 + 零默认步骤依赖" |
| 模板登记方式 | **代码组装 + `RegisterTemplate`**（插件默认模板与切片自研模板同款）；步骤里的对象引用由切片侧持强引用 | 流程模板资产化属 **T-8**；模板登记表的 GC 持有缺口同归 T-8 |
| 属性装配 | 定义用 `UTcsAttributeDef` 资产（4 条，住 `Content/TcsDev/`）；施加由切片侧 `RegisterAttributeDef` + `AddAttribute` | 剧本"属性由测试装置直接添加"；AttributeSet 属 R7-1 |
| 属性资产的发现路径 | **待定（实施期定）**：纳入 DefLibrary 多族发现（需开 `integration-entity` 增量提案）或切片侧自行加载 | DefLibrary 现行规格只写"链资产发现"——不擅自扩权，也不预设 |
| 双真相资产验收 | **不建常驻资产**；装置内运行期造瞬态 `UTcsEffectChainDef` 走校验路径 | **装置纪律**：DefLibrary 扫描是全库的，故意配错的常驻资产会污染所有命令的失败清单 |
| 临时装置退役 | **六套全删**（`Source/*/Private/Testing/`）；拒绝面覆盖由 `TcsDev` 命令继承 | 用户 2026-09-21 拍板；plan1 两套的检查点 2/3/4 覆盖缺口已在提案内声明 |
| 插件侧新增面 | **仅 `FTcsSelSelf`**（1–2 文件）；既有类型零改动、零新契约类 | 保持 R3 收窄；"零插件改动"判据对本任务的**机制面**成立 |
| LAC 正式模块的改动 | **零**（`Source/LegendAutoChess/` 的 `Build.cs` 与源码一行不改）；LAC 侧改动仅限：两个 Target 的 `ExtraModuleNames` + `.uproject` 的 `Modules` + 新增 `Source/TcsDev/` 与 `Content/TcsDev/` | "不作为 LAC 正式内容"的可核对判据；退役时摘两行 + 删两目录，正式模块无残留 |
| 内容资产 delegate（竖切不需要） | 本任务**不建** delegate 资产实例（`FTcsStepDamage` 无 Delegate 字段；模板走代码组装） | 无消费者；内容化 delegate 属 T-8 与模板资产化同批（届时 ARO 路径已实证可用） |

## 检查点
落点验收 = UBT 编译（Development Editor 零警告，含 `TcsDev` 模块）+ 插件依赖面自检 + PIE 实测（正路命令零红字 / 拒绝面 opt-in）+ 检查点 6 端到端（改资产数值零 C++ 生效）+ 宿主自研流程（插件 `git diff` 为空）+ 内容资产 `IsDataValid` 全绿 + **LAC 正式模块零改动核对**（`git diff -- Source/LegendAutoChess/` 为空）。**Task 7（检查点 1/5/7 端到端）不在本提案**。
