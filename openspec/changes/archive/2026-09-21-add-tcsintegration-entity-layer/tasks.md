## 1. Implementation

### 链资产类（收口交接注记①）

- [x] 1.1 `Public/Chain/TcsEffectChainDef.h` + `Private/Chain/TcsEffectChainDef.cpp`：`UTcsEffectChainDef : UPrimaryDataAsset`——`static const FPrimaryAssetType PrimaryAssetType`（显式，值 = **类名** `TEXT("TcsEffectChainDef")`——族内一致，2026-09-21 用户收口）+ `GetPrimaryAssetId()` 覆写（名取 `ChainId`）+ `IsDataValid`（双真相校验：`Chain.ChainId != ChainId` → Invalid）
  - **实施偏差（2026-09-21 编译实证）**：`Chain` 字段去掉 `BlueprintReadOnly` 只留 `EditAnywhere`——`FTcsEffectChain` 是 `USTRUCT()`（非 BlueprintType），而 BlueprintType 类的 `BlueprintReadOnly` 要求类型可蓝图化（UHT 报 `Type 'FTcsEffectChain' is not supported by blueprint`）。细节面板可编辑性只看 `CPF_Edit`，策划侧零损失、TcsEffect 零改动。

### 实体查询实现（收口交接注记②③）

- [x] 1.2 `Public/Entity/TcsPieEntityQuery.h` + `Private/Entity/TcsPieEntityQuery.cpp`：`UTcsPieEntityQuery : UObject, ITcsEntityQuery`——三支能力 + **句柄↔Actor 映射**（`RegisterEntity/UnregisterEntity` 供组件调用；遍历稳定序：按注册序）
  - **实施偏差（2026-09-21 编译实证）**：文件名与类名**都必须**与契约区分——原计划的 `TcsEntityQuery.h` 与 TcsEffect 契约头撞名时 UHT 直接报 `Two headers with the same name is not allowed`（UHT 要求**全项目头文件名唯一**，不是"同名不同模块即可"；提案原判断有误）。故文件改名 `TcsPieEntityQuery.h`（类名 `UTcsPieEntityQuery` 同时避开契约的 U 类占用）。

### 战斗实体组件

- [x] 1.3 `Public/Entity/TcsCombatEntityComponent.h` + `Private/Entity/TcsCombatEntityComponent.cpp`：三职责——`BeginPlay` 注册（含 DefLibrary 就绪门禁）/ `EndPlay` 注销 / `GetCurrent` / 施加撤销调试入口 / `ExecuteChainById`；`GetEntityHandle()` 查询自己的句柄

### 定义库

- [x] 1.4 `Public/TcsDefinitionSubsystem.h` + `Private/TcsDefinitionSubsystem.cpp`：`UTcsDefinitionSubsystem : UGameInstanceSubsystem`——`OnDefinitionsReady` 单出口（幂等）+ `IsRuntimeReady` + `GetFailureList` + **链资产发现**（`IAssetRegistry::GetAssetsByClass` 按类扫描，**不依赖 `PrimaryAssetTypesToScan`**；扫描前确认注册表就绪——`IsLoadingAssets()` / `WaitForCompletion()`，编辑器初始扫描是异步的）+ **定义缓存 + 装配到世界**（订阅 `FWorldDelegates::OnPostWorldInitialization`；ready 时若世界已存在立即装配；装配幂等——同一世界不重复登记）；失败进清单
- [x] 1.4b `TcsIntegration.Build.cs` 加 `AssetRegistry` 模块依赖（扫描用；`Engine` 里的那条是过渡期临时依赖，见 `Engine.Build.cs:98` 注释）

### 装置与验证

- [x] 1.5 装置（`Private/Testing/`，**不入库**）：`Tcs.Test.Integration`——spawn 带组件的 Actor → 断言句柄有效 → 注入 `UTcsPieEntityQuery` → 遍历吐句柄（稳定序）→ 登记一条链 → `ExecuteChainById` 触发 → 断言目标 Health 下降 + 记录产出；另验 DefLibrary 门禁（未就绪时组件不注册）
- [x] 1.6 编译 + 依赖面自检（`Source/TcsIntegration/` 只依赖 Core/Attribute/Effect/Targeting/Damage + AssetRegistry + 引擎）

## 2. Verification

- [x] 2.1 UBT Development Editor 编译通过（零警告）
- [x] 2.2 三处交接注记确认收口（链资产类 = 资产轨 / 实现类名 `UTcsPieEntityQuery` / 映射唯一归宿主）
- [x] 2.3a **用户 PIE：主命令 `Tcs.Test.Integration`（已跑，7/0 全绿 + 零红字）**——检查 0 定义库就绪（失败清单 0 条）/ 1 查询注入 / 2 组件身份锚（3 个句柄有效）/ 3 查询门面（Health=100）/ 4 遍历吐句柄（3 个 + 两次顺序一致）/ 5 句柄解析（含未登记句柄返回 false）/ 6 触发链扣血（100→70，`Record` Base=Final=Executed=30）；注销路径实证（3 个单位注销 + 句柄失效 + 映射摘除）
  - **首轮 8/0 含一条红字（已修）**：检查 7（未登记链 id → Error 拒绝）原住主命令，导致常规验收每次都有 `Error: ... 未登记——拒绝起链` 刷屏。**用户两次指出后修正**：该检查拆入 `.Reject`（纪律依据：故意触发失败输出的检查 MUST 独立成 opt-in 命令——2026-09-18 为 ensure 立过同一原理，本次为 Error 日志复犯，已泛化入 `unreal-development-workflow` 技能）。主命令回归**零红字**，检查项 8 → 7。
- [x] 2.3b **用户 PIE：边界命令 `Tcs.Test.Integration.Reject`（已跑，3/0 全绿，红字为预期）**——检查 A：门禁前置（定义库就绪）+ 就绪时注册通过（正向对照）；检查 B：**故意**未登记链 id → 无效句柄 + Error 日志（本命令存在意义即此）。命令屏显自带"红字为预期"声明。
  - **注**：R3 定义库在 `Initialize` 即就绪（同步单出口），**没有自然的"未就绪"窗口**，故检查 A 做的是"就绪前置 + 注册通过"的正向对照，"未就绪 → Warning + 跳过"分支只能靠组件 `BeginPlay` 内的 `IsRuntimeReady` 判定覆盖（负向实证待 R7-3 引入异步加载、出现真实 Loading 窗口时补）。
  - **未覆盖（归 Task 6，实施注记已录）**：DefLibrary 的**自动发现路径**（`GetAssetsByClass` → 缓存 → 装配到世界）在 R3 无内容资产文件，**未被实证**——装置检查 6 走的是手动 `RegisterChain`。提案的两条 Scenario（「链资产自动登记」「新世界初始化后链仍可查」）随之待 Task 6 内容资产版装置覆盖。
- [x] 2.4 文档回写：plan2 Task 5 勾选 + 实施注记；**06 文档类名修正**（`UCombatDefLibrarySubsystem` → `UTcsDefinitionSubsystem`）；README 检查点状态；台账 R7-3 登记
