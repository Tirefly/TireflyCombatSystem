# Tasks: add-tcs-skill-module

> 对应计划 `PLN-R6` Task 1（`Documents/combat-system-design/plans/plan-r6-skill-layer.md`）。
> **本清单是 Task 1 的验收面**：每一项都 MUST 在实施后回读本文件逐条勾选（`MEM-20260916-01` 第 3 条的教训：**"计划写了"不等于"实施时照做"**）。
>
> **勾选状态口径（2026-10-06）**：`[x]` = 代码/文档已落地**并有本轮实测取证**；`[ ]` = 尚未取得证据。
> 7.5 / 7.6 依赖**人工在 PIE 内执行宿主装置**——用户已于 2026-10-07 执行完毕（`Prepare` → 重启 PIE → `Run` → `Reject`，合计 28 PASS / 0 FAIL），故两项**已勾选**，读数见各行与 `EVID-2026-10-07-skill-def-asset`。装置交付说明见 §8。

## 1. 模块入场

- [x] 1.1 新增 `Source/TcsSkill/TcsSkill.Build.cs`——照 `TcsState.Build.cs` 体例（`PCHUsage = PCHUsageMode.UseExplicitOrSharedPCHs` + `PublicDependencyModuleNames`），声明 `Core` / `CoreUObject` / `Engine` / `GameplayTags` / `TcsCore` / `TcsNotation` / `TcsAttribute` / `TcsEffect` / `TcsState`。**MUST NOT** 含 `TcsDamage` / `TcsTargeting`
- [x] 1.2 新增模块壳 `TcsSkillModule.h` / `.cpp`（`IMPLEMENT_MODULE`，**零注册**——不注册任何执行器 / 条件 / 步骤）
- [x] 1.3 新增 `TcsSkillLogChannel.h` / `.cpp`（`DECLARE_LOG_CATEGORY_EXTERN(LogTcsSkill, Log, All)` 成对）
- [x] 1.4 `TireflyCombatSystem.uplugin` 的 `Modules` **追加于数组末尾**（`TcsIntegration` 之后），LoadingPhase = `Default`；**既有 8 条顺序一字不动**（用 diff 核）

## 2. 数据形状与枚举

- [x] 2.1 新增 `Public/Def/TcsSkillEnums.h`——`ECastInstancing{InstancePerExecution, InstancePerEntity}` / `ECastQueryMode{DefSwitches, PhaseTable, Custom}` / `EMainChainStart{OnCastStarted, OnPhaseEnter, OnCastCompleted, Custom}`。**保持裸枚举名**（实测枚举两族并存，MUST NOT 顺手归一）
- [x] 2.2 新增 `Public/Def/TcsPhaseSpan.h`——`USTRUCT(BlueprintType) FTcsPhaseSpan{FTcsParamValue Duration, bool bInterruptible, bool bCanMove, FGameplayTag Tag}`
- [x] 2.3 新增 `Public/Def/TcsBoolSwitchRow.h`——`USTRUCT(BlueprintType) FTcsBoolSwitchRow{FGameplayTag Key, bool Base, ETcsParamMode Mode = EPM_Snapshot}`。**住 TcsSkill**（Q-8 改判）；`ETcsParamMode` 从 `TcsState` 复用，**不另立同义枚举**
- [x] 2.4 新增 `Public/Def/TcsCastAttrCapture.h`——`FTcsCastAttrCapture{属性键, 取值方(Instigator/Target)}`
- [x] 2.5 新增 `Public/Def/TcsSkillDefData.h`——`FTcsSkillDefData : FTcsStateDefBase`，**白拿**六字段且**不重复声明**；补施法语义字段（`BoolSwitches` / `Phases` / 查询契约三档 / `AttrCaptureList` / 主链 / `Instancing` / 关系字段 / `Triggers`）
- [x] 2.6 `FTcsSkillDefData` **MUST NOT** 声明冷却轨道 / 冷却时机 / Cost 策略 / Cost 时机 / `FTcsEntrySelector` 引用位（归 R6.5 与 Task 4）
- [x] 2.7 **`ParamChainRows` 本 Task MUST NOT 声明**——元素类型 `FTcsNumericParamModifier` 是 Task 4 交付物、此刻不存在，而 `UPROPERTY TArray<T>` 要求元素类型完整（不能前向声明占位）⇒ 延后到 Task 4 以 MODIFIED 增补本需求。**验收判据**：本 Task 的编译 MUST NOT 依赖该类型；Task 4 落地时 MUST 回补本字段并同步该需求

## 3. 资产类与表行

- [x] 3.1 新增 `Public/Def/TcsSkillDef.h`——`UCLASS(BlueprintType) UTcsSkillDef : UTcsStateDef`，持 `FTcsSkillDefData SkillDef`（**无 `BlueprintReadOnly`**，同族取舍）
- [x] 3.2 `UTcsSkillDef` 持 `static const FPrimaryAssetType PrimaryAssetType`（= `"TcsSkillDef"`）+ **覆写 `GetPrimaryAssetId()`**（`[PrimaryAssetType, DefTag.GetTagName()]`）
- [x] 3.3 `UTcsSkillDef` 的 `#if WITH_EDITOR` `IsDataValid` 派生段——六类内容级规则（键重复 / `ModifierRows` 空引用 / `Phases` 内 `Duration` 来源为空 / `Triggers` 内无效字段 / `CastChainId` 无效 / 描述配置项缺项）；**末尾 MUST 把 `NotValidated` 提升为 `Valid`**
- [x] 3.4 新增 `Public/Def/TcsSkillDefTableRow.h`——`FTableRowBase` + `{FGameplayTag DefTag, FTcsSkillDefData SkillDef}`；`DefTag` = 内容身份、`RowName` = 编辑期定位，**MUST NOT 要求同名**
- [x] 3.5 `UTcsStateDef` **MUST NOT** 被改动（技能专属字段不得上提到族基类）

## 4. 定义库第五条发现路径

- [x] 4.1 `UTcsDefinitionSubsystem` 增 `DiscoverSkillDefs()` / `ResolveSkillDef(FGameplayTag) -> const FTcsSkillDefData*`，照 `DiscoverStateDefs` / `ResolveStateDef` 体例
- [x] 4.2 资产缓存 `UPROPERTY TArray<TObjectPtr<UTcsSkillDef>> SkillDefAssets`（GC 锚定）+ 非 `UPROPERTY` 内容缓存；**`AddReferencedObjects` 逐条走四份缓存**（`ChainDefs` / `TriggerDefs` / `StateDefs` / 技能定义内容缓存）
- [x] 4.3 就绪日志增**第五计数**（技能定义条数）；失败清单沿用
- [x] 4.4 技能定义的发现期校验**三项身份级**（加载失败或类型不符 / `DefTag` 无效 / `DefTag` 重复）；内容级规则 **MUST NOT** 在此重复
- [x] 4.5 技能定义**不参与世界装配**（`OnPostWorldInitialization` 里 MUST NOT 为它建每世界结构）
- [x] 4.6 `TcsIntegration.Build.cs` 加 `TcsSkill` 依赖（直接使用其类型 ⇒ MUST 声明，否则 `LNK2019` + `LNK1120`）

## 5. 宿主 tag 词表

- [x] 5.1 宿主 `Config/DefaultGameplayTags.ini`（**LAC 仓**）新增 `SkillDef` 根 + 首个检查词（`SkillDef.Check.*` 形态，带 `DevComment`）
- [x] 5.2 该 `.ini` **MUST 保持 LF**（该文件曾被整文件翻成 CRLF ⇒ 改动面被放大；见 `MEM-20260929-02`）——改完 MUST 复核 CRLF 计数 = 0

## 6. 规格与文档同步

- [x] 6.1 `openspec/project.md` 模块计数在模块落地后 8 → 9；能力计数在本提案归档、`skill-def-asset` 进入主规格库后才 34 → 35。**归档前仍记录 34 个主规格能力**，MUST NOT 把活动提案中的新增能力提前计入；归档后以 `openspec/specs/` 的实际能力目录核对并更新
- [x] 6.2 `openspec/project.md:19` 命名限定语改判为**二选一判据**（资产名已有文档在位 ⇒ 数据加 `Data`；数据名已被交付类型占用 ⇒ 资产加 `Asset`），**MUST NOT 把任一支写成唯一正解**；家族表补 `FTcsSkillDefData`
- [x] 6.3 `gameplay-tag-governance` 的 `## Purpose` 根数 **11 → 12 手工同步**（归档器只替换需求、不碰该段）+ `:7` 根数沿革补一节
- [x] 6.4 `openspec/specs/state-def-asset/spec.md` "技能面板归 R6" 改判为 **R8**；`Source/TcsState/Public/Def/TcsDescriptionEntry.h` 同名过期注释同步改判（**本轮唯一一处 TcsState 改动，零行为**）
- [x] 6.5 `SPEC-04-skill` §2 的 `BoolSwitches` 落点父注改判为 **TcsSkill**（与 `DEC-02-fold-display` 同步）

## 7. 验收取证

- [x] 7.1 `LegendAutoChessEditor Win64 Development` 编译 **0 error / 0 warning**
- [x] 7.2 `LegendAutoChess Win64 Shipping` 编译 **0 error / 0 warning**（双配置）
- [x] 7.3 `openspec validate --all --strict --no-interactive` 全绿（**归档前取证：35 passed / 0 failed；主规格能力数 34 + 活动变更 1**）；**归档动作已获用户批准并执行（2026-10-08）**——`add-tcs-skill-module` 移入 `archive/2026-10-08-add-tcs-skill-module/`，`skill-def-asset` 进入主规格库，主规格能力数核实 **34 → 35**，`project.md` 能力计数回写随之完成（见 6.1）。**归档后复验**：`openspec validate --all --strict` = **35 passed / 0 failed**（= 35 能力 + 0 活动变更）
- [x] 7.4 `.uplugin` diff 复核：既有 8 条**一字未动**，`TcsSkill` 在末尾
- [x] 7.5 PIE 内配一条真技能资产（`DA_SkillDef_E2E`，`DefTag = SkillDef.Check.*`），`IsDataValid` **正反两路**可复现（正例 `Valid`；逐类缺陷各报一次错）——**2026-10-07 实测**：正例 `D16` PASS（`Valid` 且 0 error、非 `NotValidated`）；拒绝面 `R0` 基线 PASS + `R1a/R1b/R2/R3/R4a/R4b/R5/R6a/R6b` 六类 11 例逐类被拦下 + `R7` 空时段表反向对照 PASS。证据 = `EVID-2026-10-07-skill-def-asset` §1.2 / §1.3
- [x] 7.6 就绪日志含**第五计数**且与内容目录实际条数一致、失败 **0**——**2026-10-07 实测**：第一次 PIE 会话 `技能定义 0 条`（资产尚未创建）⇒ `Prepare` 落盘 ⇒ 重启 PIE 后 `技能定义 1 条`，两次均 `失败 0 条`，与 `发现技能定义资产 0/1 个` 一致。**`0 → 1` 的唯一变量是磁盘上多了一个 `.uasset`** ⇒ 同时证明发现路径真在读内容目录。证据 = 同上 §2
- [x] 7.7 全库反查：`Source/TcsState/**` 除 `TcsDescriptionEntry.h` 的一行注释外**零改动**（证明技能字段没上提）

## 8. 人工验证点（人工在编辑器内执行；装置已随本轮交付）

宿主装置 `TcsDevSkillDefProbe`（`Source/TcsDev/`，**开发用、非 LAC 正式内容**）提供三条命令：

1. **非 PIE** 执行 `Tcs.Test.SkillDef.Prepare` —— 创建并保存真资产 `/Game/TcsDev/E2E/DA_SkillDef_E2E`（`DefTag = SkillDef.Check.SkillLayer`；已存在则只核对、不覆写）；
2. **停止并重新开始 PIE**（定义库在 GameInstance 初始化时发现资产，新资产须下一次 PIE 才被扫到），执行 `Tcs.Test.SkillDef.Run` —— 核对发现计数、按 `DefTag` 解析、`PrimaryAssetId`、继承字段与施法语义保真、合法资产 `IsDataValid == Valid`、表行往返保真；
3. **非 PIE** 执行 `Tcs.Test.SkillDef.Reject` —— 六个配置错误类别各注入一份瞬态副本，逐类核对被 `IsDataValid` 拦下（含"空时段表不构成错误"的反向对照）。

**读数以 UTF-8 日志为准**（控制台回显为 ANSI，中文会显示为 `?`）：`Saved/Logs/LegendAutoChess.log` 内按 `TcsDevSkillDefProbe` 过滤，判据 = `SUMMARY Run: Failed=0` 与 `SUMMARY Reject: Failed=0`。

> **7.5 / 7.6 的取证经过（如实登记）**：本轮交付的是**装置与代码**，两项验收要求的是**人工在编辑器内执行后的读数**。交付当晚用户尚未执行该序列，故当时**如实保持未勾选**（`MEM-20260916-01` 的同一纪律）；2026-10-07 用户执行完毕后**按实测读数补勾**（三条命令各执行一次，`Prepare` 2/0 → `Run` 17/0 → `Reject` 11/0，合计 **28 PASS / 0 FAIL**）。装置本身已随编辑器构建通过并被 `-WarningsAsErrors` 覆盖。
