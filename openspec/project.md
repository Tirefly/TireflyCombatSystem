# 项目上下文

## 目的

TireflyCombatSystem (TCS) 是 `TireflyGameplayUtils` 仓库中的核心战斗系统插件，面向 Unreal Engine 5.6+ 的现代化游戏玩法框架开发。这个 `project.md` 描述的是 `Plugins/TireflyCombatSystem/openspec` 这套**插件内 OpenSpec 工作区**，核心目标是为 TCS 的功能演进、架构调整与规格沉淀提供统一约束。

项目当前也已随仓集成 `Plugins/UnrealSharp`，用于在同一 UE 工程内维护 UnrealSharp 驱动的 C# 脚本层，并把托管脚本、Glue 生成和原生玩法框架并行纳入日常开发流程。

除非特别说明，后文中的 `openspec/`、`Source/`、`Documents/`、`Resources/`、`CODE_REVIEW_CHECKLIST.md` 等路径都以 `Plugins/TireflyCombatSystem/` 为根目录；引用仓库根目录文件时会显式写出仓库相对路径。

项目整体定位：
- 为中大型 UE5 项目提供可直接插拔的游戏玩法基础设施
- 强调数据-行为分离、策略模式扩展、可视化编辑
- 支持 `C++ + Blueprint + C# (UnrealSharp)` 混合开发
- 插件之间弱耦合，可按需启用

当前开发阶段补充事实：
- TireflyCombatSystem 目前仍处于架构设计与开发实践阶段。
- 当前仓库中默认不存在任何蓝图资产对 TCS API、委托、DefinitionAsset、编辑器创建入口或运行时行为的既有引用约束。
- 因此在评估 TCS 重构、提案、API 清理与编辑器创作流程调整时，不应把“可能已有蓝图资产依赖”当成默认前提。
- 如果未来开始出现蓝图资产引用，这条事实以用户后续明确通知为准，再统一更新相关规范与假设。

OpenSpec 工作流程**仅作用于 TireflyCombatSystem (TCS) 插件**，其他插件目前处于稳定状态、不纳入 OpenSpec 变更管理。

## 技术栈

- **引擎**: Unreal Engine 5.6+（当前本机验证目录 `E:\UnrealEngine\UE_5.7`）
- **主语言**: C++17（遵循 UE5 C++ 规范）+ Blueprint
- **托管脚本语言**: C#（UnrealSharp，依赖 .NET 10 SDK / `net10.0`）
- **构建系统**: UnrealBuildTool (UBT)
- **托管构建系统**: UnrealSharpBuildTool + MSBuild / dotnet
- **IDE**: Rider for Unreal Engine / Visual Studio 2022
- **目标平台**: Win64（主要）、Mac / Linux（预留）
- **核心引擎模块依赖**:
  - `StateTreeModule` / `GameplayStateTreeModule` —— StateTree 双层架构
  - `GameplayTags` —— 标签系统
  - `GameplayMessageRuntime` —— 消息路由
  - `GameplayAbilities`（部分子模块使用）
- **项目内插件依赖关系**:
  - `TireflyCombatSystem` → `TireflyObjectPool`
  - `GameplayMessageRouter` / `TireflyActorPool` / `TireflyBlueprintGraphUtils` 相互独立
  - `UnrealSharp` 作为 vendored 脚本插件独立维护，为项目提供 C# 脚本与 Glue 生成能力
- **规范工具**: OpenSpec（仅 TCS 插件使用）

## 统一术语表

为避免 OpenSpec 文档在中英之间来回漂移，项目级文档默认采用以下术语；代码标识、类名、命令行参数和 OpenSpec 校验器要求的结构关键字除外。

| 概念 | 统一写法 | 说明 |
|------|----------|------|
| authoring | 创作 | 指编辑器中的资产创建、配置、分类与维护流程 |
| runtime | 运行时 | 指游戏执行期或运行期行为，不翻成“runtime” |
| State Core | 状态核心 | 指 TCS 共享状态宿主与生命周期框架 |
| DefinitionAsset / Def | 定义资产 / 定义 | `Def` 可作为简称，但正文优先用中文解释 |
| definition registry / registry | 定义注册表 | 指权威 Def 快照与查询来源 |
| manager subsystem | 管理子系统 | 对应 `UTcs*ManagerSubsystem` 这类全局宿主 |
| merger / merge policy | 合并器 / 合并策略 | 用于描述 Buff 或状态冲突收敛逻辑 |
| learned skill | 已学会技能 | 专指 `UTcsSkillEntry` 记录的拥有态 |
| dirty reason / dependency flags | 脏原因 / 依赖标记 | 用于运行时失效与重新处理判断 |
| slot-local | 槽位内 | 表示某个 `FTcsStateSlot` 作用域内的数据 |
| hot-refresh | 热刷新 | 指编辑器内无需重启的刷新能力 |

补充约定：
- OpenSpec 校验器要求保留的结构关键字继续使用英文，例如 `## Purpose`、`## Requirements`、`### Requirement:`、`#### Scenario:`、`SHALL`、`MUST`。
- Unreal 类型名、命令名、模块名、菜单名与代码标识按原文保留，不为了中文化强行改写。

## 项目约定

### 代码风格

遵循 Unreal Engine C++ 官方规范，并在此基础上叠加以下**强制性**项目规约（详见 `CODE_REVIEW_CHECKLIST.md`）：

**文件头部**：
- 所有 `.h` / `.cpp` 以 `// Copyright Tirefly. All Rights Reserved.` 开头
- Copyright 后 1 空行 → `#pragma once`（头文件）→ 1 空行 → include 块 → 3 空行 → 正文

**代码组织**：
- **必须使用 `#pragma region / #pragma endregion`** 组织代码区域
- **严禁**使用 `// ========== Section ==========` 类型的注释分区
- `#pragma region` 后 1 空行；`#pragma endregion` 前 1 空行；相邻 region 之间 2 空行

**间距规则**：
- 枚举 / 结构体 / 类声明之间：3 空行
- 前置声明块与后续类型之间：3 空行
- 委托声明之间：1 空行；委托块与类声明之间：3 空行
- `.cpp` 中函数实现之间：0 空行（函数直接相邻）

**命名规范**：
- TCS 插件所有类型以 `Tcs` 为模块前缀
- `UTcs*` —— UObject 派生类；`FTcs*` —— 结构体 / 非 UObject；`ETcs*` —— 枚举；`ITcs*` —— 接口
- 导出宏：`TIREFLYCOMBATSYSTEM_API`（每个模块使用自身 `<MODULENAME>_API` 宏）
- bool 成员变量以 `b` 前缀（`bIsActive`）
- 文件名与主类型名一致：`TcsAttributeComponent.h` / `TcsAttributeComponent.cpp`

**注释要求**（必须添加）：
- 类 / 结构体 / 枚举声明处
- 所有成员变量与成员函数
- 枚举的每个具体值
- 参数数 ≥ 2 的函数需对每个参数加注释
- 有返回值的函数需说明返回值含义
- 委托声明

**注释要求**（不应添加）：
- 显而易见的单行逻辑
- 一次性的局部变量
- 纯机械的 getter/setter 内部

**字符集**：默认 ASCII；仅当文件本身已使用中文时才允许中文注释。

**UnrealSharp C# 约定**：
- 公开给 Unreal 反射系统的 C# 类型遵循 Unreal 命名前缀：`A` / `U` / `F` / `E` / `I`
- 使用 UnrealSharp Attribute 体系声明反射接口：`[UClass]`、`[UStruct]`、`[UEnum]`、`[UInterface]`、`[UProperty]`、`[UFunction]`
- 业务逻辑写在用户脚本工程里，不写进 `*.Glue` 或 `obj/UHT/**/*.generated.cs`

### UnrealSharp 工作流

- 仓库已提供本地 UnrealSharp skill：`.github/skills/unrealsharp-agent-skill/SKILL.md`
- 遇到 UnrealSharp、`Script/*.csproj`、`*.Glue`、`generated.cs`、`BuildEmitLoadOrder`、热重载、编辑器启动卡在 75% 或托管构建失败等问题时，先读该 skill，再按其路由读取对应 reference 文件
- 当前项目的用户脚本工程位于 `Script/ManagedTireflyGameplayUtils/ManagedTireflyGameplayUtils.csproj`，对应 Glue 工程位于 `Script/TireflyGameplayUtils.RuntimeGlue/TireflyGameplayUtils.RuntimeGlue.csproj`；仓库当前使用 `Script/`（单数）而不是 `Scripts/`
- 优先依赖 `Plugins/UnrealSharp` 下的本地源码与配置回答和实现，不优先使用外部通用记忆
- 新建 C# 项目或 C# 插件时，优先使用 UnrealSharp 提供的创建流程，不手工拼装 `.csproj`
- 生成物禁改：`Script/**/*.Glue`、`obj/UHT/**/*.generated.cs`、托管输出目录中的文件都视为构建产物
- 如果某个 API 在 C# 侧不可见，先检查 UE 反射可见性，再怀疑生成器或 Glue 流程

### 架构模式

**组件化 + 管理子系统双轨**（TCS 正在进行的迁移）：
- 每个战斗实体持有 `UTcsAttributeComponent` / `UTcsStateComponent` / `UTcsSkillComponent` 等 ActorComponent
- 原有 `UTcs*ManagerSubsystem`（GameInstanceSubsystem）逐步降级为全局查询 / 跨 Actor 协调入口
- 业务逻辑优先在 Component 内完成，Subsystem 不再承担单体数据存储职责

**数据-行为分离**：
- 数据：`UPrimaryDataAsset` 派生的 `UTcs*DefinitionAsset`
- 运行时实例：`FTcs*Instance` / `UTcs*Instance`
- 行为：CDO 策略类（执行算法、合并、过滤、条件）

**策略模式（基于 CDO）**：
- 属性执行：`UTcsAttributeModifierExecution`
- 属性合并：`UTcsAttributeModifierMerger`
- 属性 Clamp：`UTcsAttributeClampStrategy`
- 状态条件：`UTcsStateCondition`
- 状态合并：`UTcsStateMerger`
- 同优先级策略：`UTcsStateSamePriorityPolicy`
- 技能修正：当前仍处独立议题阶段，不应假定已存在完整稳定的 `UTcsSkillModifierExecution` 系列
- 扩展新算法 = 新建子类，无需改动引擎代码

**StateTree 双层架构**：
- 第 1 层（静态）：StateTree 管理状态槽位、转换规则，在编辑器可视化配置
- 第 2 层（动态）：每个 `UTcsStateInstance` 或其派生执行态运行独立 StateTree 执行具体逻辑
- 共享执行主链以 `UTcsStateDefinition` + `UTcsStateInstance` 抽象层为核心，Buff / Skill 当前分别通过 `UTcsBuffDefinition` / `UTcsBuffInstance` 与 `UTcsSkillDefinition` / `UTcsSkillInstance` 等派生类型挂接进去

**SourceHandle 因果追踪**：
- `FTcsSourceHandle`（Id + CausalityChain + Instigator + SourceTags）贯穿所有效果施加路径
- 因果链使用 `FPrimaryAssetId`，天然兼容所有 `UPrimaryDataAsset` 子类
- Modifier 生命周期跟随 State：`FinalizeStateRemoval` 第 3.5 步自动清理
- **跨 Actor 修改禁止直接 `ApplyModifierWithSourceHandle`**，必须改为在目标 Actor 上施加独立 State，由目标 State 的 SourceHandle 管理其 Modifier

**GameplayTag 约定**：
- `StateSlot.Action` / `StateSlot.Buff` / `StateSlot.Debuff` / `StateSlot.Mobility`
- `State.Type.Skill` / `State.Type.Buff` / `State.Type.Debuff`

### 测试策略

- 使用 Unreal Automation Framework：`IMPLEMENT_SIMPLE_AUTOMATION_TEST` 或 Spec 测试
- TCS 测试代码位于 `Source/TireflyCombatSystemTests/`
- 其他模块测试放在各自 `Source/<Module>/Private/Tests` 下
- 测试命名格式：`F<Module>_<Feature>Spec`（例如 `FTireflyCombatSystem_BasicDamageSpec`）
- UnrealSharp 变更优先验证 `Script/ManagedTireflyGameplayUtils/ManagedTireflyGameplayUtils.csproj` 与 `Script/TireflyGameplayUtils.RuntimeGlue/TireflyGameplayUtils.RuntimeGlue.csproj`，再决定是否扩大到 Editor 启动或完整目标编译
- 全量运行命令：
  ```
  UnrealEditor-Cmd.exe <uproject> -unattended -NullRHI -nop4 -nosplash \
      -ExecCmds="Automation RunAll; Quit" -ReportExportPath="Saved\Automation"
  ```
- 提交前必须通过的验证：`TireflyGameplayUtilsEditor Win64 Development` 编译 + 相关自动化测试
- 对于涉及 StateTree 执行流、SourceHandle 生命周期、状态移除时序的变更，必须补充回归测试

### Git 工作流

- 分支策略：
  - `main` —— 稳定可编译
  - 功能分支：`feature/<scope>-<short-desc>`
  - 修复分支：`fix/<scope>-<short-desc>`
- 提交信息使用**中文短命令式**，并复用仓库历史 Tag 前缀：
  - `#add` —— 新增功能 / 文件
  - `#modify` —— 修改已有逻辑
  - `#fix` —— 缺陷修复
  - `#refactor` —— 重构（不改变外部行为）
  - `#docs` —— 仅文档
  - 示例：`#add TCS: initial State component skeleton`
- PR 必须包含：目的、影响范围（模块 / 插件）、测试说明；UI / 编辑器变更需附截图或 GIF
- **禁止**提交 `Binaries/` / `Intermediate/` / `Saved/` / `DerivedDataCache/`
- **禁止**使用 `--no-verify` 绕过钩子；**禁止**在公共分支上 `git push --force`

## 领域上下文

**核心领域**：ARPG / MMO 类战斗系统建模。

**关键概念**：
- **战斗实体 (Combat Entity)**：实现 `ITcsEntityInterface` 的 Actor，具备属性 / 状态 / 技能
- **属性 (Attribute)**：具备 BaseValue + Modifiers → CurrentValue 的数值管线，带 Clamp 策略
- **状态 (State)**：统一抽象，覆盖普通状态 / Skill / Buff 三种 `ETcsStateType`
  - 生命周期：`Inactive → Active → HangUp/Pause → Expired`
  - 移除流程六步：停止 StateTree → 标记 Expired → 从容器移除 → 清理 SourceHandle Modifier → 广播事件 → 清理槽位 → `MarkPendingGC`
- **状态槽 (State Slot)**：由 `UTcsStateSlotDefinition` 定义，StateTree 静态结构中的占位
- **技能 (Skill)**：作为 `ETcsStateType::Skill` 的特化状态；当前代码已拆分 `UTcsSkillEntry`（已学会技能拥有态）与 `UTcsSkillInstance : UTcsStateInstance`（单次技能激活执行态）
- **Buff**：作为 `ETcsStateType::Buff` 的特化状态
- **SourceHandle**：效果来源的唯一 ID + 因果链，贯穿 Modifier / State / 事件归因
- **CDO 策略**：通过 `GetDefaultObject()` 使用 `UClass*` 字段引用的策略对象，避免运行时分配

**完成度快照**（随项目推进更新）：
| 模块 | 完成度 |
|------|--------|
| 属性系统 | 95% |
| 状态系统 | 90% |
| 技能系统 | 35% |
| StateTree 集成 | 85% |
| SourceHandle 机制 | 100% |

## 重要约束

**技术约束**：
- 必须与 UE 5.7 兼容，禁止使用更低版本引擎私有 API
- 禁止修改引擎源码；所有扩展通过插件 / 策略类 / 子类完成
- UnrealSharp 托管开发遵循插件锁定的 .NET SDK；不要脱离 `Plugins/UnrealSharp/Managed/global.json` 自行漂移 SDK 版本
- 跨 Actor 效果施加必须经由 "施加独立 State" 路径，禁止直接对其他 Actor 调用 `ApplyModifierWithSourceHandle`
- StateTree 执行期间的副作用必须考虑可重入 Stop：UE 5.7 引擎本身通过 `RequestedStop` 延迟机制自保护；TCS 代码不应再引入并行 Flag 绕过该机制
- 性能敏感路径应考虑对象池（`TireflyObjectPool` / `TireflyActorPool`）集成
- 禁止手工修改 UnrealSharp 生成物，包括 `*.Glue` 工程和 `obj/UHT/**/*.generated.cs`
- 对于 C# 不可见 API，先验证反射暴露与声明合法性，不要直接把问题归因到生成器

**流程约束**：
- TCS 插件任何新增功能 / 破坏性变更 / 架构调整 / 重大性能或安全工作，必须走 OpenSpec 提案流程（`openspec/changes/`）
- OpenSpec **没有 `apply` 命令**，Stage 2 直接按 `tasks.md` 编码，完成后更新 `[x]`
- OpenSpec 归档使用 `openspec archive <change-id> --yes`
- 代码提交前必须通过 `CODE_REVIEW_CHECKLIST.md` 全部检查项
- 对外 API（`UFUNCTION` / `UPROPERTY` 公开项）变更需同步更新文档与测试
- 在用户没有明确说明“现已存在蓝图资产引用”之前，不要因为假设中的蓝图资产消费者而阻止 TCS 的架构性重构或 API 清理

**兼容性约束**：
- 插件之间保持弱耦合：除 TCS→ObjectPool 外，不得引入新的插件间强依赖
- 所有公开头文件不得使用绝对路径 include；依赖必须在 `.Build.cs` 显式声明
- 默认 ASCII；非 ASCII 字符仅在文件本身已使用时保留

## 外部依赖

**引擎内建模块**（通过 `.Build.cs` 引用）：
- `Core` / `CoreUObject` / `Engine`
- `StateTreeModule` / `GameplayStateTreeModule`
- `GameplayTags` / `GameplayTasks`
- `GameplayMessageRuntime`
- `DeveloperSettings`（开发者设置面板）
- `SlateCore` / `Slate` / `UMG`（编辑器 UI 与少量运行时 UI）

**第三方 / 外部服务**：无。项目不依赖任何外部网络服务、云 API 或私有 SDK。

**开发工具链**：
- Unreal Build Tool（内置于 `E:\UnrealEngine\UE_5.7\Engine\Binaries\DotNET\UnrealBuildTool\`）
- UnrealSharpBuildTool（位于 `Plugins/UnrealSharp/Managed/UnrealSharpPrograms/UnrealSharpBuildTool/`）
- Rider for Unreal Engine（主 IDE）
- OpenSpec CLI（管理 TCS 变更提案）

**重要路径**（Windows 本机）：
- 引擎根目录：`E:\UnrealEngine\UE_5.7`
- 项目根目录：`E:\Projects_Unreal\TireflyGameplayUtils`
- TCS 插件根目录：`E:\Projects_Unreal\TireflyGameplayUtils\Plugins\TireflyCombatSystem`
- OpenSpec 根目录：`E:\Projects_Unreal\TireflyGameplayUtils\Plugins\TireflyCombatSystem\openspec`
- UnrealSharp 插件根目录：`E:\Projects_Unreal\TireflyGameplayUtils\Plugins\UnrealSharp`
- UnrealSharp skill：`E:\Projects_Unreal\TireflyGameplayUtils\.github\skills\unrealsharp-agent-skill\SKILL.md`
- 当前用户 C# 工程：`E:\Projects_Unreal\TireflyGameplayUtils\Script\ManagedTireflyGameplayUtils\ManagedTireflyGameplayUtils.csproj`
- 当前 RuntimeGlue 工程：`E:\Projects_Unreal\TireflyGameplayUtils\Script\TireflyGameplayUtils.RuntimeGlue\TireflyGameplayUtils.RuntimeGlue.csproj`
- 刷新项目文件命令模板：
  ```
  E:\UnrealEngine\UE_5.7\Engine\Binaries\DotNET\UnrealBuildTool\UnrealBuildTool.exe \
      -projectfiles -project="<ProjectPath>.uproject" -game -rocket -progress
  ```
