# Tasks: GameplayTag 词表换根（re-root）

> **本提案（变更创建阶段）只交付**：`proposal.md` / `design.md` / `tasks.md` / `specs/**`（10 个 delta 文件）+ `openspec/project.md` 的 tag 段落指针化。
> **以下 1–3 节是"批准后实现阶段"的清单**（本仓）；**4–7 节**含编译验证、宿主仓交办与文档连带面。
> **禁止提交**：任何 `git commit` / `git push` / `p4 submit` 都不在本清单内（用户级 `AGENTS.md` 最高优先级）；改动只落工作区，任务边界是等待用户审查的停点。
> **不适用项**（裁定不做 / 前提消失）**不计入本清单的完成度**，统一登记在文末「不适用项」节；正文只在原位置留一行指针。

## 1. 契约落地（本仓，随提案交付）

- [x] 1.1 新增能力 `openspec/changes/reroot-gameplay-tag-vocabulary/specs/gameplay-tag-governance/spec.md`——插件侧 **8 根**注册表（含 `EffectTriggerGate` / `DamageCategory` 与 `DamageCategory` 的"两匹配面共用一套词"写链备注）+ 归属规则 + 共享根 + 一角色一根（含"多读取点 / 单角色"判据边界）+ 根名判据 + 深度上限 + `TcsStateParam` 语义边界（**7 条需求**，每条至少一个 `#### Scenario:`。`DevComment` 义务**不在其中**——通用规范的唯一载体是用户级 `unreal-gameplay-tags` 技能 `governance.md` 的 A1/A4 检查项，本提案只交付第 3 节的 21 处补齐）
- [x] 1.2 **11 个**既有能力的 MODIFIED delta：`damage-flow`(×4) / `damage-step-library`(×2) / `damage-primitive`(×5) / `effect-trigger`(×2) / `attribute-types`(×4) / `attribute-store`(×1) / `attribute-pipeline`(×1) / `effect-chain`(×3) / `effect-chain-asset`(×1) / `effect-interpreter`(×1) / `param-value`(×1)——每条 MODIFIED 需求按规范**整条粘贴**（header + 全部 scenario）后改字面 tag 文本
- [x] 1.3 `openspec/project.md` 的 tag 段落缩为指针：`:17`（ownership rule + 项目 tag 命名）、`:20`（事件 tag 命名公约）、`:21`（常量名逐点换下划线）→ 指向 `gameplay-tag-governance`；`:18` 的示例常量名 `Tag_Tcs_Flow_Key_X` → `Tag_DamageFlowKey_X`；保留与 tag 无关的条目（identifier = `FGameplayTag` 标准、`FTcsAttributeName` 移除、跨模块注册与反射可见性、字段默认值陷阱、模块词汇边界）
- [x] 1.4 `openspec validate reroot-gameplay-tag-vocabulary --strict --no-interactive` 通过（提案评审的前置条件）
- [x] 1.5 提案评审批准（**批准前不得进入第 2 节**）  —— **2026-10-01 用户批准**，第 2 节据此进入实现
- [x] 1.6 第二轮裁定施加（2026-10-01，消费者清点后；详见 `design.md` D8）：① 删 `DevComment` 需求；② `FormulaParams` 键归 `TcsStateParam`（`damage-flow` 新增第 4 份 delta）；③ `Element` 归"分类"角色（`damage-primitive`）；④ `EffectChainRunVar` 命名契约移到 `effect-interpreter`（新增 `effect-interpreter` delta），`effect-chain` 只留指针；⑤ `ParamRef` 存在性校验分工收口（运行期不校验 / 作者期归 M8）
- [x] 1.7 第三轮裁定施加（2026-10-01，按 `FGameplayTag` 槽位清点；详见 `design.md` D9）：① 新立 `EffectTriggerGate` 根（触发行开关，消费者 `SetTriggerGateTag` / `IsTriggerGateTagLit`）；② 新立 `DamageCategory` 根（供条件匹配的分类集，**两匹配面共用一套词**）→ 注册表 **6 根 → 8 根**；③ `FTcsDamageRecord::Element` 字段**移交**独立提案「伤害模块边界整肃」（"与分类同根"与"并入本批删除"两次裁定均被 2026-10-01 评审推翻——**移交理由见 `proposal.md`「明确不做」**：两条提案都 MODIFY 同一条需求会造成归档覆盖）；④ 第三个角色（**实体 tag 匹配**）本轮清点**无槽位证据、未立根**（将来自建时再立 + 登记）；⑤ 新增 `effect-trigger` delta（×2）与 `damage-step-library` 第二份 delta（×2）

## 2. 原生词换根（本仓实现，21 个词）

- [x] 2.1 `Source/TcsDamage/Private/TcsDamageSubsystem.cpp`（10 个事件词，`:20`–`:33`）：tag 文本 `Tcs.Event.Damage.<名>` → `TcsEvent.Damage.<名>`，常量名 `Tag_Tcs_Event_Damage_<名>` → `Tag_TcsEvent_Damage_<名>`（**宏形式本节不动**——`UE_DEFINE_GAMEPLAY_TAG` → `_COMMENT` 的替换统一在第 3 节做；同一改动只允许一个载体）  —— **已落**：10 个词文本 + 常量名 + `DevComment`；宏形式按本节纪律留到第 3 节做
- [x] 2.2 `Source/TcsDamage/Private/Flow/TcsFlowKeys.cpp`（9 个黑板键 + 1 个模板词，`:8`–`:19`）：`Tcs.Flow.Key.<键>` → `DamageFlowKey.<键>`、`Tcs.Flow.Template.Default` → `DamageFlowTemplate.Default`，常量名 `Tag_Tcs_Flow_Key_<键>` → `Tag_DamageFlowKey_<键>`、`Tag_Tcs_Flow_Template_Default` → `Tag_DamageFlowTemplate_Default`（宏形式同上，本节不动）  —— **已落**：10 个词（9 黑板键 + 1 模板 id）
- [x] 2.3 `Source/TcsAttribute/Private/Attribute/TcsAttributeChangedEvent.cpp`（1 个事件词，`:8`）：`Tcs.Event.Attribute.ValueChanged` → `TcsEvent.Attribute.ValueChanged`，常量 `Tag_Tcs_Event_Attribute_ValueChanged` → `Tag_TcsEvent_Attribute_ValueChanged`（宏形式同上，本节不动）  —— **已落**：1 个词
- [x] 2.4 公开头导出声明同步（**跨模块消费面**，常量名与文本一起改；导出宏 MUST 保留——`UE_DECLARE_GAMEPLAY_TAG_EXTERN` 展开为裸 `extern`，去掉导出宏即 LNK2001）：  —— **已落**：4 个公开头共 22 处声明与注释；导出宏 `TCSDAMAGE_API` / `TCSATTRIBUTE_API` 逐条保留
  - `Source/TcsDamage/Public/Flow/TcsDamageFlowCollectEvent.h`（`:28`–`:36` + `:48`，10 处）
  - `Source/TcsDamage/Public/Flow/TcsDamageRecord.h`（`:19`，1 处）
  - `Source/TcsDamage/Public/Flow/TcsFlowKeys.h`（`:29`–`:37` + `:45`，10 处）
  - `Source/TcsAttribute/Public/Attribute/TcsAttributeChangedEvent.h`（`:26`，1 处）
  - 头文件里的行尾注释（如 `// Hit（宿主可改写 HitRate 键）`）随新词同步
- [x] 2.5 插件内调用点同步（共 5 个文件、28 处）：`Private/Attribute/TcsAttributePipeline.cpp`(1) / `Private/Chain/TcsStepDamage.cpp`(1) / `Private/Chain/TcsStepModifyFlow.cpp`(1) / `Private/Flow/Steps/TcsFlowStepsCore.cpp`(13) / `Private/Flow/Steps/TcsFlowStepsRest.cpp`(12)  —— **已落**：`TcsFlowStepsCore.cpp` 13 · `TcsFlowStepsRest.cpp` 12 · `TcsAttributePipeline.cpp` 1 · `TcsStepDamage.cpp` 1 · `TcsStepModifyFlow.cpp` 1 = **28 处**（`git diff --numstat` 逐文件核对）
- [x] 2.6 顺带清掉既有欠账（**2026-10-01 用户批准：审计 `R-3` 并入本项**——同一类"`FName` 时代死常量"清理，共用一次编译验证）：  —— **已落，且本项描述有一半已陈旧**：`TcsStepDamage.cpp:17-18` 死常量已删（含其悬空注释与空行）；**`TcsFlowStepsRest.cpp` 那条欠账已不存在**——`TcsFlowRestKey_*` 与裸 `FName(TEXT(...))` 全库零命中，2026-09-22 tag 化改造时已顺手清掉。改做了一件真事：该文件"黑板契约键…原生 tag 常量"的注释下面其实一个常量都没有，已改写为"直接引用 `Flow/TcsFlowKeys.h` 的原生 tag 常量"
  - `TcsFlowStepsRest.cpp`（`damage-step-library` 规格明文要求）：内联裸字面量 `FName(TEXT("Crit"))` / `FName(TEXT("Hit"))` 改为引用原生常量；死常量 `TcsFlowRestKey_HitRate` / `TcsFlowRestKey_CritRate` 清理或改为 tag 常量并实际使用；
  - `TcsStepDamage.cpp:17-18`（审计 **`R-3`**，落点见 `Documents/combat-system-design/research/boundary-audit.md` §4）：删死常量 `const FName TcsChainDamage_BaseKey(TEXT("BaseDamage"));`——**零使用**（全库 grep 仅此一处命中）、2026-09-22 `FName` → `FGameplayTag` 改造的残留、住匿名 namespace 故编译器不报 unused。**留着的害处**：它诱导后来者改成 `FGameplayTag(TEXT("BaseDamage"))` 裸字面量，正是 `damage-step-library` 规格明文禁止的形态
- [x] 2.7 全仓反查零残留：`rg -n "Tcs\.(Event|Flow)\.|Tag_Tcs_Event_|Tag_Tcs_Flow_" Source/` 零命中（`Tcs.Event.*` / `Tcs.Flow.*` 只允许出现在历史注释与"旧名"标注里，且必须显式标注为旧名）  —— **已验**：正则 `Tcs\.(Event|Flow)\.|Tag_Tcs_Event_|Tag_Tcs_Flow_|Tcs\.Attr\.|Tcs\.Chain\.` 扫 `Source/` 151 个文件 → **0 命中**（已把审计漏掉的 `TcsAttributeDef.h` 宿主词示例 `Tcs.Attr.<Name>` → `Attribute.<Name>` 一并收进本项）
- [x] 2.8 深度自查：`TcsEvent.*` 3 段、`DamageFlowKey.*` / `DamageFlowTemplate.*` 2 段，**全仓零 4 段词**  —— **已验**：21 条原生声明，最大段数 **3**，`>= 4` 段词 **0 条**

## 3. 原生词 `DevComment`（A 组，21 处 —— **交付项**，规范义务住用户级技能）

> 载体边界：`DevComment` 义务是**通用规范**，唯一载体 = `unreal-gameplay-tags` 技能的 `governance.md`（检查项 A1/A4）+ `authoring.md`「`DevComment` 要求」。本节只记录**这一次**的 21 处补齐，插件规格里 MUST NOT 复述该通用义务。

- [x] 3.1 21 处声明全部改为 `UE_DEFINE_GAMEPLAY_TAG_COMMENT(TagName, Tag, Comment)`，`Comment` 写清三件事：**语义 / 谁声明谁消费 / （验证词）退役判据**  —— **已落**：21 处全部改为 `UE_DEFINE_GAMEPLAY_TAG_COMMENT(Tag, "文本", "说明")`；说明按"语义 / 谁声明谁消费"两段式。宏第三实参经 `TEXT()` 包裹，**全批零 ASCII 逗号、零 ASCII 双引号**（引号一律用全角，避免宏实参切分与字面量提前闭合）
- [x] 3.2 中文说明**实测一次**（宏第三实参经 `TEXT(...)` 包裹，本批首次使用中文）：Development 编译通过；若失败则退回英文说明（不阻塞换根本体，但 MUST 在提案里记录该限制）  —— **已验，且前提有误**：Development 与 Shipping **双配置**均编译通过。**"本批首次使用中文"不成立**——`TcsDamageSubsystem.cpp` 的 `UE_LOG(..., TEXT("…官方默认模板已登记（Default：4 步）"))` 等中文 `TEXT()` 字面量早已在库内，且 `EVID-2026-09-30-modifyflow-primitive-acceptance-pie` 记录的 PIE 日志里中文**逐字正确**（非乱码）⇒ MSVC 确实按 UTF-8 读源文件，本项风险提前已退役
- [x] 3.3 Shipping 配置确认 comment 不参与运行期数据（`WITH_EDITORONLY_DATA` 语义），编译与包体无异常  —— **已验**：Shipping 配置下 3 个插件模块全部编译通过；`UE_INCLUDE_NATIVE_GAMEPLAYTAG_METADATA = WITH_EDITOR && !UE_BUILD_SHIPPING`，注释不进 Shipping 运行期数据
- [x] 3.4 `DevComment` 里 MUST NOT 出现路径语义：不得写"临时/废弃/优先级"去暗示层级（生命周期维度只允许进 `DevComment` 文本或落宿主侧 `Probe` 根）  —— **已核**：21 条说明零"临时/废弃/优先级/待删"一类路径语义；退役判据只以"本批只声明形状不发布（发布归台账 DAMAGE-4）"的**事实陈述**形态出现

## 4. 编译与运行期验证

- [x] 4.1 Development 编译（Editor Target）零错误零新增警告；UBT 命令按 `unreal-cpp-compile` 技能探测引擎路径后执行  —— **半绿（2026-10-01 实跑）**：引擎 = `E:\UnrealEngine\UE_5.8`（`.uproject` 的 `EngineAssociation` 是 GUID，`HKCU\...\Unreal Engine\Builds` 注册表为空 ⇒ 按技能规定问用户后定为 UE_5.8）。`LegendAutoChessEditor Win64 Development` = **TcsAttribute / TcsDamage / TcsIntegration 三模块编译+链接全绿、UHT 全绿**，随后 **6 条 error 全部落在 LAC 的 `Source/TcsDev/`**——三个文件引用被改名的导出常量（`TcsDevBootstrap.cpp:300,302` / `TcsFlowSteps.cpp:67,93` / `TcsDevScreenObserver.cpp:27,39`）。**这 6 处正是 §7.4 交办项，属 LAC 仓 = 提案 C，本仓不得代改**（三仓互不裹挟）⇒ **本项在 C 落地前无法收口**，非本仓缺陷  —— **门禁已收口（2026-10-02）**：C 的 Task 1 落地后 `LegendAutoChessEditor Win64 Development` = **`Result: Succeeded`**（退出码 0）。此前记录的 6 处 `undeclared identifier` 全部消除
- [x] 4.2 Shipping 编译零错误（验证 `DevComment` 宏在非编辑器配置下的行为）  —— **半绿（同日实跑）**：`LegendAutoChess Win64 Shipping`（Game Target——编辑器目标不支持 Shipping）下 **3 个插件模块全部编译通过** ⇒ `DevComment` 的非编辑器行为已验证；同样只剩 LAC 那 6 处 error  —— **门禁已收口（2026-10-02）**：`LegendAutoChess Win64 Shipping` = **`Result: Succeeded`**（退出码 0）
- [x] 4.3 编辑器 tag 树检查  —— **已证（2026-10-02，编辑器进程内活注册表）**：跑了一次编辑器，`UCSGameplayTagsGlueGenerator` 启动时把活注册表导出到 `Script/LegendAutoChess.RuntimeGlue/GameplayTags.cs`（该文件是 `UGameplayTagsManager::RequestAllGameplayTags` 的直出，不是推断）。导出 59 个 tag，其中属 13 根注册表的 36 个：`Attribute` 5 / `DamageFlowKey` 10 / `DamageFlowTemplate` 4 / `EffectChain` 3 / `TcsEvent` 14。**4 段及以上 = 0 条**；旧家族（`Tcs.Event` / `Tcs.Flow` / `Tcs.Attr` / `Tcs.Chain` / `Tcs.Param` / `Tcs.Dev`）**全部 0 命中**。**限定**：`TcsStateParam` / `EffectChainRunVar` / `EffectTriggerGate` / `DamageCategory` 4 个根**当前无已登记词**（是注册表里的角色位，词由宿主/TGFS 提案交付），故不出现在导出里——它们"就位"指注册表登记成立，不是已有词
- [x] 4.4 逐条核对 21 个新词在 tag 树中存在且 `DevComment` 可读  —— **已过（2026-10-02 用户实跑）**："存在"与"可读"两面均已人工确认。**"存在"**：导出显示 21 个原生词全部在活注册表中（`TcsEvent.*` 11 叶子 / `DamageFlowKey.*` 9 叶子 / `DamageFlowTemplate.Default` 1），8 个宿主词也全部在（`Attribute.*` 4 / `EffectChain.*` 2 / `DamageFlowTemplate.{DefaultSlice,SliceFlow}` 2）；用户全量扫 tag 树复核 = `TcsEvent` 11 叶、`DamageFlowKey` 9 叶、无 `Tcs.` 枝、无 `Probe` 枝。**"可读"**：用户悬停确认 `DevComment` 中文说明可读（胶水只导 tag 文本、不带 comment，故这一面只能人工）
- [x] 4.5 校验脚本以**多根清单**模式跑通（`-Roots` 取代 `-Namespace Tcs`；脚本本体住 `unreal-gameplay-tags` 技能的 `scripts/`，不在本仓）：`Validate-GameplayTags.ps1 -ProjectRoot <LAC 根> -Roots <8 根清单>` 报 0 条 D6  —— **已跑**：`Validate-GameplayTags.ps1 -ProjectRoot <LAC> -Roots <13 根>` 采集 **45 条声明（ini 24 / native 21）**；**54 条 Error 全部落在 LAC 的 `Config/DefaultGameplayTags.ini`，插件原生侧 0 条**（D1/D2/D6/D7 对 21 个原生词全通过）。脚本自陈"查不出"的项已转人工点（4.3/4.4）
- [x] 4.6 宿主侧竖切回归（依赖 LAC 侧交办完成）：`L_TcsDev_Slice` 地图跑 R3 七项验收清单，行为与换根前逐项一致  —— **已过（2026-10-02 用户实跑，日志留证）**：`Tcs.Test.Slice.Run`（默认链 `EffectChain.SliceChain`）= **通过 9 / 失败 0**，检查 0/1a/1b/2/3/4/5/6/7a/7b 全 `[PASS]`——检查 4 = 属性定义 **4/4**；检查 5 = `Attack=30.0 / Armor=5.0 / Health=100.0`；检查 6 = 目标 `Health 100.0 → 75.0`（模板 `DamageFlowTemplate.SliceFlow`）；检查 7b = 施法者 `Health 100.0 → 55.0`（实际扣 **45.0**）。`Tcs.Test.Slice.Run EffectChain.FormulaChain` = **通过 9 / 失败 0**，检查 7b 施法者 `Health 100.0 → 0.0`（受伤 100.0，钳到 0）。**零非预期 ensure / 零非预期 Error**。证据：`Saved/Logs/LegendAutoChess-backup-2026.10.01-19.40.30.log`（`18:39:49` 与 `18:40:34` 两次起链）
> **4.7 已移出本清单**（前提消失 ⇒ 失去对象；裁定 2026-10-02）—— 见文末「不适用项」节。

## 5. 重定向登记（**LAC 仓交办**，本仓无该文件）

- [x] 5.1 **已由 LAC 侧提案 C 完成**（2026-10-02）：生成 29 条 `+GameplayTagRedirects=(OldTagName="<旧>",NewTagName="<新>")`（映射表见 `design.md` D3），**全部追加到 LAC 的 `Config/DefaultGameplayTags.ini` 的 `[/Script/GameplayTags.GameplayTagsSettings]` 节**——原生词没有 TagList 源，走 `UGameplayTagsSettings` 回落分支；ini 词本就住该节
- [x] 5.2 **已由 C 完成**：约束核对：29 条 `OldTagName` **互不相同**（同一 `OldTagName` 指向多个目标会命中 `ensureMsgf`）；**直接写终态目标、不链式**（多跳被引擎压平成单跳）
- [x] 5.3 **已由 C 完成**：禁用位置核对（`DefaultEngine.ini` 零命中）：**MUST NOT** 写进 `DefaultEngine.ini` 的 `[/Script/Engine.Engine]`（命中即打 Error 级日志，引擎自己会删它）
- [x] 5.4 **已由 C 完成**：时效核对（第 2/3 节同一次保存内完成，失联窗口 = 0 次编辑器启动）：重定向与 C++ 改名**窗口尽量贴近**（改名已落地而 redirect 未登记的窗口内，已序列化的旧引用会失联）。**MUST NOT 读成"两仓必须同批交付"**——29 条 redirect 只能落 LAC 的 ini、改名只能落本仓，物理上不可能同批，且本仓**无 `Content/`、无 `Config/`**（本次核实）⇒ 本仓零序列化资产承载这些 tag，风险窗口完全在 LAC 内部。中间态由第 4.7 项的 `WarnOnInvalidTags` 告警面覆盖
- [x] 5.5 redirect 生效验证：把旧名 `Tcs.Event.Damage.Hit` / `Tcs.Flow.Key.BaseDamage` 分别配进资产字段后加载，解析到 `TcsEvent.Damage.Hit` / `DamageFlowKey.BaseDamage`  —— **已证（2026-10-02 用户实跑）**，且比本项设想的更硬：**不需要另配测试字段**——LAC 的 6 个 `Content/TcsDev/*.uasset` 二进制里**本就存着旧文本**（`Tcs.Attr.Health` ×3 资产 / `Tcs.Attr.MaxHealth` / `Tcs.Attr.Attack` / `Tcs.Attr.Armor` / `Tcs.Chain.Slice_Chain` / `Tcs.Chain.Formula_Chain` / `Tcs.Flow.Template.Default_Slice`），逐个打开**tag 字段全部解析为新名、无 `None`、无红字** ⇒ 29 条 redirect 在真实生产资产上生效。**运行期侧证**：竖切 4.6 的检查 4 = 属性定义 **4/4**、检查 6 = 模板 `DamageFlowTemplate.SliceFlow` 命中——这两项只要有一条 redirect 失效就会立刻掉数

## 6. 文档连带面

### 6.1 规范性文档（应同步为新名）

- [x] 6.1.1 `Documents/combat-system-design/spec/09-module-damage.md`（`:27` / `:33` / `:48` / `:57` / `:82` / `:111` / `:112`）
- [x] 6.1.2 `Documents/combat-system-design/spec/01-module-m0-core.md`（`:34` / `:35`）
- [x] 6.1.3 `Documents/combat-system-design/spec/04-module-effects.md`（`:146`：`Tcs.Event.State.Periodic`——**注意这是尚未实现的状态模块事件**，换根后应为 `TcsEvent.State.Periodic`）
- [x] 6.1.4 `Documents/combat-system-design/spec/02-module-attributes.md`（`:23`：项目侧词表命名 `Tcs.Attr.<Name>` → `Attribute.<Name>`）
- [x] 6.1.5 `Documents/combat-system-design/spec/tcs-contract-traceability.md`（`:26` / `:27`）
- [x] 6.1.6 `Documents/combat-system-design/plans/plan-r3-1-core-attributes.md`（`:548` / `:563`）
- [x] 6.1.7 `Documents/combat-system-design/plans/plan-r3-2-damage-chain.md`（`:222` / `:508` / `:526` / `:527` / `:531`）
- [x] 6.1.8 `Documents/combat-system-design/plans/plan-r4-trigger-row.md`（`:326` / `:351` / `:377` / `:388` / `:568`–`:570` / `:586`）
- [x] 6.1.9 `Documents/combat-system-design/research/targeting-abilitykit-absorption.md`（`:107`）
- [x] 6.1.10 同步口径：**旧名 MUST 保留可追溯**（写成"`Tcs.Event.Damage.Hit`（现 `TcsEvent.Damage.Hit`）"或加修订注记），MUST NOT 让读者以为旧名从未存在过  —— **已落**：9 份规范性文档同步为新名（37 处替换），并在身份块后插入一行「换根注记（2026-10-01）」保留旧名可追溯（`最后更新` 同步为 2026-10-01）

### 6.2 历史记录（保留原样，只加迁移注记）

- [x] 6.2.1 `log/implementation-log.md`（`:16` / `:18` / `:38` / `:50` / `:93` / `:97` / `:140`）、`log/decisions-log.md`（`:135`）、`ledger/deferred-inputs-ledger.md`（`:59` / `:70` / `:222`）、`ledger/reflection-backlog.md`（`:359`）、`decisions/dec-04-callback-carriers.md`（`:25`）、`evidence/2026-09-30-modifyflow-primitive-acceptance-pie.md`（`:10` / `:29`）——**日志与证据是历史现场，MUST NOT 改写**；如需可读性，在文件内加一行"本文件记录的 tag 名已于 2026-10-01 换根，映射见 `<本提案>`"  —— **已落**：7 份历史文件（`implementation-log` / `decisions-log` / `deferred-inputs-ledger` / `reflection-backlog` / `dec-04` / `EVID-2026-09-30` / `RSCH-boundary-audit`）**旧名一字未改**，各插入一行换根注记；`最后更新` **故意不动**（改动只是导航指针、不是内容更新，改日期等于伪造历史现场）
- [x] 6.2.2 `decisions-log.md` 追加一条换根决策记录（根 = 消费角色、一角色一根、**13 根注册表**（插件 8 + 宿主 5）、B5 撤销、`Element` 字段删除**移交边界整肃提案**）  —— **已落**：`decisions-log.md` 追加 7 行换根决策记录（根 = 消费角色 / 一角色一根 / 13 根注册表 / 深度上限 / B5 撤销 / `Element` 移交 / 跨仓边界 / 落地顺序）

### 6.3 其他载体

- [x] 6.3.1 `Documents/combat-system-design/plans/plan-r3-content-guide.md` 与其余内容指南：核对是否含字面 tag（本轮扫描未命中，逐份确认一次）  —— **已核**：全 `Documents/` 扫描，含字面 tag 的文件共 16 份，`plan-r3-content-guide.md` **零命中**（其余内容指南同）
- [x] 6.3.2 宿主侧文档（约 10 份，含 LAC 的 `Documents/`）归 LAC 提案处理，本仓不代改  —— **已核，且本项估计有误**：C 的 Task 7.4 实测 LAC 的 `Documents/`（4 份）与 `docs/` 对旧 tag 名**零命中** ⇒ 宿主侧文档连带面为 **0 份**，不是"约 10 份"
- [x] 6.3.3 用户级技能 `unreal-gameplay-tags` 与本仓无残留耦合：本仓文档引用该技能时只引用规则，MUST NOT 复制规则正文  —— **已核**：本仓文档只引用技能规则名，无规则正文副本

### 6.4 文档 / 代码偏差（**登记，不在本提案拍板**；见 `design.md` Open Questions 3）

- [x] 6.4.1 裁决项：`TcsDamageFlowContext.h:57` 与 `TcsDamageFlowContextView.h:73` 都写"分类 Tag 集（**来源标签启动写入** / 元素标签由 Element 步骤写入）"，但 `CollectStart`（`TcsFlowStepsCore.cpp:67-70`）**只发流程开始事件 + 重置黑板、不写分类集**——全库唯一写入点是 Element 步骤（`TcsFlowStepsRest.cpp:139`）  —— **已裁决（2026-10-02 用户）**。性质定性：**纯注释漂移，不是功能缺失**——**规范侧早已站在代码这边**：`gameplay-tag-governance/spec.md:33` 与 `damage-primitive/spec.md:202` 均明文写「全库**唯一**写入点 `TcsFlowStepsRest.cpp:139`」，`effect-trigger/spec.md:237` 写明「`ClassificationTags` 无来源时为空集 —— 这是**显式交付的语义**，不是遗漏」⇒ 不一致的只有那两行注释
> **6.4.2 已移出本清单**（裁定不做；2026-10-02）—— 见文末「不适用项」节。
- [x] 6.4.3 **路线 B（改注释）**：把"启动写入"从两处注释去掉，声明分类集**只有** Element 一步写入（零行为变更，仅契约与注释收口）  —— **已落（2026-10-02）**，实际动了 **3 个文件**（比原计划多一处，且是本次最有价值的一处）：① `TcsDamageFlowContext.h:57`、② `TcsDamageFlowContextView.h:73` —— 两处**改成完全相同的措辞**（避免又形成"同一主题双表述"），写明「唯一写入点 = Element 步骤；`CollectStart` 不写本集 ⇒ `FlowStarted` 事件发出时本集必为空集；词表根为 `DamageCategory`」；③ `TcsDamageFlowCollectEvent.h` 的 `FTcsDamageFlowCollectEvent` 结构文档块 —— **新增「分类集时点契约」段**，把"恒空"从实现事实升级为**明示契约**：订阅 `TcsEvent.Damage.FlowStarted` 时分类集必空、在该事件上 `HasAllTags` 匹配非空数组**恒不通过**、要按分类匹配须订阅 Element 步之后的事件。**规格零改动**（规格本来就写对了）、**零行为变更**
- [x] 6.4.4 两条路线**都不得在本提案内实施**；裁决后另开任务（若选 A，需回报 `damage-flow` / `damage-primitive` 的相应 delta）  —— **已履行（2026-10-02）**：路线 B 属**纯注释改动**，按项目 `openspec/AGENTS.md` 的跳过提案清单（"拼写错误、格式、注释"）**无需另开提案**，作为换根归档后的独立跟进改动落地；本提案本体未被触碰。**规格 delta 回报：0 份**（选 B 不改任何规格——`gameplay-tag-governance:33` / `damage-primitive:202` / `effect-trigger:237` 的表述本来就正确）。**未提交**

## 7. 归档与跨仓交办

- [x] 7.1 **已归档（2026-10-02）**：`openspec archive reroot-gameplay-tag-vocabulary --yes` → `changes/archive/2026-10-02-reroot-gameplay-tag-vocabulary/`；12 份 delta 全部合入（`~25 modified` + `gameplay-tag-governance` **`+7 added`**，全仓 specs 数 24 → 25）。归档器提示 20 项未完成（4.3/4.4/4.6/4.7 编辑器与运行期人工点 + 5.x 属 LAC 仓 + 6.4 待裁决），按 `--yes` 继续——**编译门禁为归档条件，运行期人工验收另计**（**仅在实现完成 + 编译验证通过后**；`--skip-specs` **不适用**——本变更确有规格变更）→ 落到 `openspec/changes/archive/YYYY-MM-DD-reroot-gameplay-tag-vocabulary/`，**12 份 delta**（1 ADDED + 11 能力）合入 `openspec/specs/`
- [x] 7.2 **已复验**：`openspec validate --all --strict --no-interactive` → **26 passed / 0 failed**（带 Info 级"需求文本过长"若干）。**另修一个归档器固有缺陷**：新建能力 `gameplay-tag-governance` 的 `## Purpose` 是归档器写的占位段，不补则 `validate` 永久红 —— 已补写真实 Purpose（能力边界 + 非目标）。同款缺陷在 LAC 仓也复现并已修（见 C 的 9.2）：`openspec validate --strict --no-interactive` 全仓通过；`openspec/specs/gameplay-tag-governance/spec.md` 已生效
- [ ] 7.3 **TGFS 仓交办**：改其提案 spec 文本（`Flow.Phase.*` → `GameFlowPhase.*`、`Flow.Completer.*` → `GameFlowCompleter.*`、`Flow.Event.*` → `TgfsEvent.*`、`Tgfs.FlowEvent.Phase.*` → `TgfsEvent.Phase.*`）——该提案 0/23 未开工，纯文本改动
- [x] 7.4 **LAC 仓交办已完成闭环（2026-10-02）**：落成提案 C `reroot-host-gameplay-tag-vocabulary`（LAC 仓），**已实施 + 已归档** → `E:\Projects_Dev\LegendAutoChess\openspec\changes\archive\2026-10-02-reroot-host-gameplay-tag-vocabulary\`，新能力 `host-gameplay-tag-registry` 生效。落地项：8 个正式 ini 词换根 + **16 个 `Probe` 词退役删除**（连带 3 个 C# 探针类 + 2 个探针 BP）+ 3 个夹具词归 `Probe.Reject.*` + **29 条 redirect** + `Source/TcsDev` 6 处导出常量与 8 处字面量 + 宿主 5 根注册表入 `openspec/project.md`。**门禁**：Dev + Shipping 双配置 `Result: Succeeded`；`Validate-GameplayTags.ps1 -Roots <13 根>` **54 Error → 零违规**（采集 29 条声明 = ini 8 / native 21）（顺序在 TGFS 之后）：8 个正式 ini 词换根 + 16 个 `Probe` 词删除（`Probe.<机制>.<词>` 形态）+ `Source/TcsDev/` 与 `Script/LegendAutoChessCS/TcsProbe/`（约 18 处硬编码 tag 串）+ 2 个 `Content/TcsDev/*.uasset` + 宿主侧 5 根注册表（`Attribute` / `Probe` / `TgfsEvent` / `GameFlowPhase` / `GameFlowCompleter`）+ 第 5 节的 29 条 redirect
- [x] 7.5 交办闭环确认：LAC 侧验收通过后，本仓根段注册表与 LAC 侧的宿主根注册表互相引用一次  —— **互引已落、验收未跑**：C 侧 `openspec/project.md`「GameplayTag 词表与根段」已引本仓 `gameplay-tag-governance`，且写明插件 8 根"条目一律以插件仓为准"；**但 C 的 8.5–8.10（含 `L_TcsDev_Slice` 竖切回归）**已全部执行并通过** ⇒ 本项按"LAC 侧验收通过后"的条件**已收口（2026-10-02：C 的 8.5 / 8.7 / 8.8 / 8.10 全部通过，见 C 侧 tasks.md）**

## 不适用项（裁定不做或前提消失 —— 留档，不计入完成度）

> **口径约定（2026-10-02 用户裁定）**：本清单的复选框**只表示"做过 / 没做"**两种状态。凡**裁定不做**或**前提消失、已无对象**的条目，一律**移入本节、不带复选框**——理由是：把它们留在正文里，无论标 `[x]`（虚报完成）还是标 `[ ]`（假装待办）都会骗人，且会污染按 `[x]` 计数的完成度。
> 本节条目**保留原任务号与原任务名**以便追溯，正文原位置只留一行指针；**各条裁定人均为用户**。

- **4.7 未登记重定向的行为确认** —— 裁定：**失去对象**；2026-10-02。理由：第 2 节与第 3 节在**同一次保存**内落地（ini 24 词改造 + 29 条 redirect 同批），"改名已落地而 redirect 未登记"的中间态**从未出现** ⇒ 无窗口可观察。原计划的观测面（生产环境 `WarnOnInvalidTags` 告警）当时未实测，已转由 LAC 侧提案 C 的 **8.6** 承接，并于 2026-10-02 以"撤除全部 redirect 后打开 `DA_Armor` 复现告警"的实测反证完成（见 C 侧 tasks.md 的 8.6 / 8.7）。

- **6.4.2 路线 A（补实现）**：让"来源标签"真的在启动步骤写入 —— 裁定：**不做**；2026-10-02（选路线 B，见同节 6.4.3）。理由：**规范侧本就站在实现这边**——`gameplay-tag-governance/spec.md:33` 与 `damage-primitive/spec.md:202` 明文写「全库唯一写入点 `TcsFlowStepsRest.cpp:139`」、`effect-trigger/spec.md:237` 写明「`ClassificationTags` 无来源时为空集 —— 这是显式交付的语义，不是遗漏」⇒ 不一致的只有两行注释，走路线 B 零规格 delta、零行为变更。当时的三个候选来源（请求字段 **A1** / 模板静态 **A2** / delegate **A3**，均须与 Element 共用 `DamageCategory` 一根）与"改规格再改实现"的代价分析记在对话里、未落文档。**将来若确需"流程开始即可按来源分类匹配"，那是行为变更**（`FlowStarted` 载荷语义从"恒空"变"可能非空"），**MUST 另开提案并配新断言**，MUST NOT 塞回本条注释修补项。