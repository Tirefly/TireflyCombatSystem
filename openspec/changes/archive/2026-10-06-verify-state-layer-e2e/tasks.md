## 1. 取证准备（无代码改动）

- [x] 1.1 冻结基线：记录 TCS / LAC 两仓的 `git rev-parse HEAD` 与 `git status --porcelain` 干净状态
- [x] 1.2 记录既有 16 个 `.uasset` 的大小/mtime 清单，作为"未误改既有资产"的对照
- [x] 1.3 记录改动前三命令读数基线：`Tcs.Test.Slice.Run` 即时 65/0、延时 72/0、`Tcs.Test.Slice.Reject` 11/0
- [x] 1.4 确认 `Content/TcsDev/E2E/` 不存在；建立 `Saved/TcsAcceptance/Task7/` 归档目录

## 2. 插件侧：修正器模板身份解析路径（`ATTR-1` 的触发条件）

> 本段是"验证"变成"验证 + 实现"的那一半。**必须排在内容资产之前**——真资产的 `TemplateTag` 要有一个
> 有解析方的根，而解析方就是本段落的代码。形状逐条照 `TcsDefinitionSubsystem_State.cpp` 先例。

- [x] 2.1 改 `Source/TcsIntegration/Public/TcsDefinitionSubsystem.h`：加 `ResolveAttrModDef(FGameplayTag TemplateTag) -> const UTcsAttrModDef*`
      （照 `ResolveStateDef` 的注释形状：裸指针 + 未命中返 nullptr + 不 ensure），加私有 `void DiscoverAttrModDefs();`，
      加两份成员——`TMap<FGameplayTag, TObjectPtr<UTcsAttrModDef>> AttrModDefs`（索引，**非** `UPROPERTY`）
      与 `UPROPERTY() TArray<TObjectPtr<UTcsAttrModDef>> AttrModDefAssets`（GC 锚定，照 `StateDefAssets` 形状）
- [x] 2.2 新建 `Source/TcsIntegration/Private/TcsDefinitionSubsystem_AttrModDef.cpp`（照 `_State.cpp` 84 行形状）：
      `IAssetRegistry::Get()` 不可得 → `FailureList` + Error；`IsLoadingAssets()` → `WaitForCompletion()`；
      `GetAssetsByClass(UTcsAttrModDef::StaticClass()->GetClassPathName(), …, /*bSearchSubClasses=*/false)` 返 false → Log 并返回（正常状态，全库当前 0 个）；
      **三校验**：加载失败/类型不符 → 进清单；`TemplateTag` 无效 → 进清单；`TemplateTag` 重复 → 进清单（不静默覆写）；
      内容级规则（`Def.Target` / 数值来源 / 约定列）**不在此重复**（归 `IsDataValid`）
- [x] 2.3 改 `Source/TcsIntegration/Private/TcsDefinitionSubsystem.cpp` 四处：
      ① `Initialize` 在 `DiscoverStateDefs()` 后加 `DiscoverAttrModDefs();`；
      ② 就绪日志加"修正器模板 %d 条"（`AttrModDefs.Num()`）；
      ③ `Deinitialize` 清两份新容器；
      ④ `#include "Attribute/TcsAttrModDef.h"`
      —— **`AddReferencedObjects` 不动**（新缓存的资产对象已由 `UPROPERTY` 数组锚定，理由写进该函数注释）
- [x] 2.4 TCS 双配置编译：`LegendAutoChessEditor Win64 Development` 与 `LegendAutoChess Win64 Shipping`
      → 均 `Result: Succeeded`，零 warning / 零 error
- [x] 2.5 `openspec validate verify-state-layer-e2e --strict --no-interactive` → `is valid`（三份 delta 全部解析）
- [x] 2.6 确认 `TcsIntegration.Build.cs` **无需改动**（该模块已依赖 `TcsAttribute`，`UTcsAttrModDef` 可达）

## 3. 标签与内容资产（编辑器 MCP）

- [x] 3.1 `Config/DefaultGameplayTags.ini` 追加 7 行（LF、无 BOM）：`StateDef.Check.StateLayer`、
      `EffectChain.Check.ApplyState`、`EffectChain.Check.Behavior`、`TcsStateParam.Check.Override`、
      `TcsStateParam.Check.Level`、`TcsStateParam.Check.Modifier`、`AttrModDef.Check.StateLayer`；
      每行带 `DevComment` 说明"宿主声明·谁消费"。
      **词形纪律（本条是本轮第一处被门禁拦下的返工）**：检查词 MUST 取"功能根 + `Check` 段"
      （3 段）——其消费路径与正式内容词**完全相同**（链 id 仍须 `RegisterChain` 认、定义身份仍须定义库认），
      MUST NOT 另立根、MUST NOT 把生命周期词塞进路径中段。原稿的 `*.E2E.*` 形态命中后者，
      宿主契约 `host-gameplay-tag-registry` 把它**当反例点名**（原文举 `EffectChain.Probe.X`）；
      仓内正确先例 = `StateDef.Check.Burn`（Task 1）/ `EffectChain.Check.{SelfSub,Sort,…}`（R4）
- [x] 3.2 建 `Content/TcsDev/E2E/DA_ModDef_E2E`（`UTcsAttrModDef`）：`TemplateTag` =
      `AttrModDef.Check.StateLayer`（**新根 `AttrModDef`**，见 §2）；`Def.Target` = `Attribute.Armor`；`Def.Op` = `TAO_Add`；
      `Def.Operand.Kind` = `OPK_Literal`；`Def.Operand.Literal.Source` = `FTcsParamSource_ParamRef`
      （`Key` = `TcsStateParam.Check.Modifier`，`Fallback` = -999）；保存后 `IsDataValid == Valid`
- [x] 3.3 建 `Content/TcsDev/E2E/DA_State_E2E`（`UTcsBuffDefAsset`）：`DefTag` = `StateDef.Check.StateLayer`；
      `BuffDef.StatusTag` 同词；`LevelBase` = 1、`MaxLevel` = 0；`DurationPolicy` = `EDP_Finite`；
      `DurationTime` = 字面量 2.0；`Period` = 0.5；`PeriodRefresh` = `EPR_Keep`
- [x] 3.4 同资产补 `Params` **三行**（**2026-10-05 修正**：原稿写"两行"且键为 `.Override` + `.Level`，
      而 §3.2 的模板 `ParamRef.Key` 是 `.Modifier` ⇒ **该键不在 `Params` 行里** ⇒
      `FTcsParamSource_ParamRef::Evaluate` 恒 miss 落 `Fallback` -999 ⇒ §4.7 的 Y 断言必然不成立。
      核实机制：`BuildSnapshot` 只对 `Overrides.Find(Row.Key)` 命中，而模板在物化边界经快照读键
      （`TcsStateModifierMaterializer.cpp:35-42` 把实例快照装成 `Ctx.ParamTable`）⇒
      **模板引用键与 `Overrides` 键必须是同一个、且必须落在 `Params` 行上**。三行各自的消费者：
      ① `TcsStateParam.Check.Modifier`（`Base` = 字面量 10）——**模板 `ParamRef.Key` 指向它**（跨资产引用通道）；
      rig 的 `Overrides` 也用它 ⇒ 护甲变化 = Y（既不可能是定义值 10、也不可能是兜底 -999）；
      ② `TcsStateParam.Check.Override`（`Base` = 字面量 10）——**不进 `Overrides`** ⇒ **负对照**：
      未被列名覆盖就该保持定义值 10（若"覆盖"是整体替换而非按键命中，它会变成 Y ⇒ 判据能抓住）；
      ③ `TcsStateParam.Check.Level`（`Base` = `FTcsParamSource_StateLevelArray`，`Values` = [30, 60]、
      `Fallback` = -1）——**只读 `Context.EffectiveLevel`**，与 `LevelProvider` 无关（见 §4.8 修正）
- [x] 3.5 同资产补 `ModifierRows` 1 行 → `DA_ModDef_E2E`；补 `Triggers` 1 行：`EventTag` =
      `TcsEvent.State.Applied`、`EffectChainId` = `EffectChain.Check.Behavior`、`Priority` = 0
- [x] 3.6 `Fragments` **留空**（理由见 design.md D2：内容不得引用切片侧类型）
- [x] 3.7 建 `Content/TcsDev/E2E/DA_Chain_E2E_Apply`（`UTcsEffectChainDef`）：`ChainId` =
      `EffectChain.Check.ApplyState`；`Chain.ChainId` 同词；步骤 2 个——`FTcsStepSelectTargets`
      （`Selector` = `FTcsSelSelf`）+ `FTcsStepApplyState`（`Target` 留空、`DefTag` = `StateDef.Check.StateLayer`、
      **`Overrides` = 1 行 `TcsStateParam.Check.Modifier` → 25.0**）。
      **2026-10-05 补齐（原稿漏写，是规格缺口）**：`Overrides` 是 §4.7 那条读数（Y 值）的**唯一驱动来源**——
      装置侧只调 `ExecuteChain`，自己**不传**参数表也不传覆盖（`TcsStepApplyState.cpp:77-79` 里
      `ParamTable` 恒为空、`Overrides` 取步骤字段）⇒ 若不在这里写死，`BuildSnapshot` 的
      `Overrides.Find(Row.Key)` 必落空、模板拿 `Fallback` -999，§4.7/§4.8 的 Y 断言**建立不起来**。
      取值 25.0 的用意 = **与另两个候选值三者互异**（定义字面量 10、兜底 -999）⇒ 读数落在 25
      唯一地证明"跨资产引用通道经快照取到了覆盖值"，而不是"碰巧等于定义值"或"落兜底"
- [x] 3.8 建 `Content/TcsDev/E2E/DA_Chain_E2E_Behavior`（`UTcsEffectChainDef`）：`ChainId` =
      `EffectChain.Check.Behavior`；步骤 2 个——`FTcsStepSelectTargets`（`Selector` = `FTcsSelSelf`）
      + `FTcsStepModifyAttribute`（`Target` 留空、**`Attribute` = `Attribute.Attack`**、`Op` = `TAO_Add`、
      `Operand` 字面量 -5）。
      **2026-10-05 修正（落点由 `Armor` 改 `Attack`，用户裁定）**：原稿让行为链也改 `Armor`，与 §4.13
      的"属性复原（回到 X）"**算术上不可能同时成立**——根因不是级联摘除失效，而是两条条目的
      **来源锚点不同**：状态修正器挂 `Instance.CascadeAnchor`（物化器 `:108`），而链的 `ModifyAttribute`
      挂 `Context.RunSource`（`TcsStepModifyAttribute.cpp:80`，每次运行新发号）；到期只按前者摘
      （`TcsStateOps_Modifier.cpp:101`）⇒ 链那条**按设计常驻**（`FTcsStepModifyAttribute.h:26-27`
      "即时步骤……本原语表达的是链直接改一次账本"；既有检查 23c 注释同款表述）。
      **按 `CausedBy` 级联撤销已被裁定③明文禁止**（`decisions-log.md:249`；`effect-interpreter`
      规格有 `Scenario: 因果边不参与撤销`）——故不修机制，改落点：两个通道分属两个属性，
      "会回退的"与"永久留的"各自有干净读数。该特征登记为 `CHAIN-7`（见 §7.4）
- [x] 3.9 对 4 个新资产逐个跑 `IsDataValid`，确认 `Valid` 且零错误；记录每个资产的字节大小

## 4. 装置实现（LAC `Source/TcsDev/`）

- [x] 4.1 新建 `TcsDevSliceRig_Internal.h`，把 `GTcsDevRigScreenKey` / `GTcsDevRigScreenDuration` /
      `FTcsDevRigScreenSection` / `FindSliceRigWorld` / `TeardownSliceRigActors` / `SpawnSliceUnit` /
      `FindStateByDef` / `GTcsDevRigActors` / `GTcsDevRigSourceRegistry` 提升为共用
      （放 `namespace TcsDevSliceRigInternal`，不删任何注释）
- [x] 4.2 改 `TcsDevSliceRig.cpp` 消费内部头（同名 `using` 引入，函数体逐字不变）
- [x] 4.3 新建 `TcsDevSliceRig_State.h`：只声明 `void RunTcsDevStateRig(const TArray<FString>& Args, UWorld* World);`
      与 `void RunTcsDevStateRejectRig();`（照 `TcsDevSliceRig_Behavior.h` 先例）
- [x] 4.4 实现 `RunTcsDevStateRig` 骨架：5 路 fail-fast 前置门（世界、状态门面、效果门面、属性门面、时钟）
      + 屏显段基址 **0x7C5D3000**（与观测者 0x7C5D0000、Slice 装置 0x7C5D1000、GC 探针 0x7C5D2000 错开）
      + 函数内 `FTcsDevRunCounters` 型计数器（照 `TcsDevSliceRig.cpp:414` 先例，为延时尾段共享计数）
- [x] 4.5 实现造单位与属性账本：`SpawnSliceUnit` × 2（主单位 + 对照单位）、`ApplySliceAttributes`、
      实体查询注入（**必须在造单位之前**）
- [x] 4.6 实现竖切即时段：`ExecuteChainForCaster(EffectChain.Check.ApplyState, Unit)` → 取句柄 → 断言
      `EAR_Applied`、实例在册、`CascadeAnchor` 有效且与外层不同
- [x] 4.7 实现属性面读数：`EvaluateCurrent`/`PeekPending` 与 `CountLedgerModifierSlots`，
      记录"减甲 X → Y"两侧读数——**Y 由 `Overrides` 里 `TcsStateParam.Check.Modifier` 的冻结值驱动**
      （该键即模板 `ParamRef.Key`；`Overrides` 的键 MUST 是 `Params` 里某个 `Row.Key`，
      否则 `BuildSnapshot` 的 `Overrides.Find` 落空、模板取到 `Fallback` -999）；
      同时读快照里 `TcsStateParam.Check.Override` 应为 **10**——**负对照**：
      未被列名覆盖 ⇒ 保持定义值，证明覆盖是**按键命中**而非整体替换
- [x] 4.8 实现等级源读数（**2026-10-05 修正**：原稿写"以 `LevelBase` + `LevelProvider` 驱动"，
      把两个源的驱动来源串成了一句 —— `TcsStateParam.Check.Level` 那行用的是
      `FTcsParamSource_StateLevelArray`，它**只读 `Context.EffectiveLevel`**
      （`TcsParamSource_StateLevel.h:68-72`），而 `EffectiveLevel` 由 `MakeContext` 填成 `Def->LevelBase`
      （`TcsStateOps.cpp:118`）⇒ **`LevelProvider` 在等级数组源这条路上一次都不被调用**；
      全库 `LevelProvider` 的消费者只有 `FTcsParamSource_InstigatorLevel*`
      （`TcsParamSource_InstigatorLevel.h:76,155`））：
      以 `Def.LevelBase` 驱动等级数组源，断言快照取到 `Values[LevelBase - 1]` = **30**；
      **`LevelProvider` 本轮未被消费**（其消费者是 `InstigatorLevel*` 两型，既有检查 20b 已验数组型，
      覆盖状态见 §7.2）
- [x] 4.9 实现真资产路径断言：断言 `DA_ModDef_E2E` 经 `LoadSynchronous()` 被消费——
      载荷点 = 账本里出现的 `Source`/`Target` 与被引用的模板一致（Task 4 边界①的闭合读数）
- [x] 4.10 实现新根解析读数（§2 的验收面，在装置里独立取证）：断言
      `UTcsDefinitionSubsystem::ResolveAttrModDef(AttrModDef.Check.StateLayer)` 返回的资产就是
      `DA_ModDef_E2E`（指针/身份一致），且定义库失败清单**不含**该资产；
      同时断言该资产**不产生**每世界登记（触发行计数与状态门面定义数不因它变化）
- [x] 4.11 实现事件面读数：`FTcsAttributeChangedEvent` 计数 +
      `UTcsDevScreenObserver` 六槽位（Applied/Refreshed/Removed/Expired/Periodic/StackChanged）计数快照
- [x] 4.12 实现延时尾段（`FTimerDelegate::CreateWeakLambda(World, …)`，绑 `UWorld` 不绑 Actor）：
      断言行为链已被内联触发行起过、**`Attribute.Attack` 确实被 `-5` 改过**
      （该条为链挂的常驻条目，`Source` = 该次运行的 `RunSource`）、周期回调计数 ≥ 1
      （**下界，不用等号**）。**2026-10-05 修正**：原稿断言 `Attribute.Armor` 被 -5 改过——
      见 §3.8 的理由，落点改 `Attack`（护甲留给状态修正器，保持 §4.13 的复原断言成立）
- [x] 4.13 实现到期段：等 `DurationTime` 自然到期 → 断言实例被回收、`Expired` 计 1、
      `UnregisterTriggerRowsBySource` 生效（效果门面触发行计数回到施加前）、
      **`Attribute.Armor` 复原（回到 X）**——此断言成立的**唯一**理由是两条条目分属两个属性
      （§3.8）；同时读 `Attribute.Attack` 确认它**停在施加前读数减 5**（链挂条目常驻的第二读数，
      与"护甲回到 X"互为对照，共同证明常驻语义而非笼统的"没摘干净"）
- [x] 4.14 在到期段末尾调用 `TeardownSliceRigActors()`（本命令自持完整生命周期，不依赖 Slice 装置）
- [x] 4.15 实现复现性读数：记录本轮全部读数为一行摘要，供第二次 PIE 逐字比对
- [x] 4.16 实现 `RunTcsDevStateRejectRig`（同步、无延时尾段、纯 `int32 Passed/Failed`，照
      `RunSliceRejectRig` 先例）：检查 a–g 覆盖——① 未登记 `DefTag` ② 无效 `Target` 句柄
      ③ 悬空（已回收）句柄 ④ 库未就绪 ⑤ `EDP_Infinite` 上调 `ExtendDuration`
      ⑥ `SetRemaining` ⑦ 未登记的 `TemplateTag` 调 `ResolveAttrModDef`（返 nullptr、不 ensure；
      **不是红字**——正常查询路径）；每条断言"被拒 + 留痕级别正确（Warning/Log/无）"
- [x] 4.17 在 `RegisterCommands()` 注册两条命令：`Tcs.Test.State.Run`
      （`FAutoConsoleCommandWithWorldAndArgs`，**Args 在前 World 在后**）与 `Tcs.Test.State.Reject`
      （`FAutoConsoleCommand`）；**不改 `TcsDevModule.cpp` / `TcsDev.Build.cs`**

## 5. 编译与回归

- [x] 5.1 Editor 编译：`LegendAutoChessEditor Win64 Development` → `Result: Succeeded`，零 warning/error
- [x] 5.2 Shipping 编译：`LegendAutoChess Win64 Shipping` → `Result: Succeeded`，零 warning/error
- [x] 5.3 回归三命令：`Tcs.Test.Slice.Run` 仍即时 65/0、延时 72/0；`Tcs.Test.Slice.Reject` 仍 11/0
- [x] 5.4 回归既有状态面检查：19/20/21/22/23 各段的**通过/失败结论**与改动前一致
      （尤其 20l 周期计数窗口未被污染）。
      **2026-10-05 修正**：原稿写"各段读数与改动前一致"，但新增内容资产必然改动**两处分母**，
      这是必然而非意外，故按结论回归、按增量核对分母：
      ① 检查 18：`IsDataValid == Valid` 分母 **1/1 → 2/2**（新 buff 资产也过作者侧门——这是收益不是回归）；
      ② 检查 19a：门面内定义数 **1 → 2**（新 buff 资产逐世界登记）；
      ③ 检查 20a/20b 等之间的探针定义**不受影响**（它们走运行期手搓定义，不读内容资产）
- [x] 5.5 回归定义库就绪行：**2026-10-05 修正**：原稿写"三计数与改动前**同值**"，
      但新增内容资产必然让其中两个涨——须按**增量核对表**逐项核，而不是要求同值：

      | 计数 | 改动前 | 期望 | 增量来源 |
      |---|---|---|---|
      | 链定义 | 9 | **11** | +2 = `DA_Chain_E2E_Apply` / `DA_Chain_E2E_Behavior` |
      | 触发定义 | 2 | **2** | +0（本轮**不新建**触发定义资产；内联触发行住 buff 的 `Triggers`，不进此计数） |
      | 状态定义 | 1 | **2** | +1 = `DA_State_E2E` |
      | 修正器模板 | （原无此计数） | **1** | +1 = `DA_ModDef_E2E`（本变更 §2 新增的第四计数） |
      | 失败 | 0 | **0** | **仍必须为空**——非空即判失败 |

      （改动前基线出自 `Saved/TcsAcceptance/Task6b/LegendAutoChess-6b-frozen.log:2341`
      的 `链定义 9 条，触发定义 2 条，状态定义 1 条，失败 0 条`）

## 6. PIE 验收（两轮，同编辑器进程）

- [x] 6.1 第一轮：运行 `Tcs.Test.State.Run`，断言**零非预期红字**（预期警示白名单见 §7.1）
- [x] 6.2 第一轮：运行 `Tcs.Test.State.Reject`，断言全部拒绝按预期留痕、计数与断言一致
- [x] 6.3 第二轮：重跑 `Tcs.Test.State.Run`，断言**复现性摘要行逐字相同**、且各检查的
      通过/失败结论与失败条数相同。
      **2026-10-05 修正**：原稿写"全部读数逐字相同"，但装置里有两处**按构造就不可能逐字相同**的读数，
      必须先把它们排除，否则判据自相矛盾：
      ① `FTcsSourceHandle` 的 `Id` 是**进程内单调计数器**——每起一轮 PIE，新建句柄的 id 就往前跳
         （实测首轮 `Src=2 Anchor=3`、第三轮 `Src=25 Anchor=26`）。它是"分配顺序"的痕迹，不是被测行为；
         故检查 S5 行里的 id 允许逐轮变化，**不变量是"两 id 互异"这一条**（该判据在摘要里已改写为
         `Decoupled=1`，与轮次无关）。
      ② 周期回调累计次数是**帧驱动**读数（到期堆按帧泵点推进），同一窗口内可能是 1 也可能是 2；
         检查 S11 只断言 `≥1`（下界），摘要里相应地只留 `PerOK=1`（"下界成立"），不留原始次数。
      这两条修正都落在**装置侧**（摘要写法），不改被测框架的任何行为。
- [x] 6.4 跨 PIE 残留检查：停止 PIE 后确认无残留世界/实例；重进 PIE 后
      `GetTotalStateCount()` 与触发行计数归零（R4 的"世界反初始化全退订"）
- [x] 6.5 冻结日志快照到 `Saved/TcsAcceptance/Task7/LegendAutoChess-task7-frozen.log` 并算 SHA-256

## 7. 证据与文档

- [x] 7.1 写 `evidence/2026-10-05-state-layer-pie.md`（`EVID-2026-10-05-state-layer`）：
      §1 基线与冻结快照 SHA-256；§2 竖切步骤与设计意图；§3 属性面读数表；§4 事件面读数表；
      §5 关键日志行引用（限冻结快照行号）；§6 边界清单（**每条注明"实测"或"未覆盖"**）；
      §7 与既有证据的关系（不复述）；§8 新根与解析路径的取证（`ATTR-1` 闭合读数）
- [x] 7.2 边界清单 MUST 覆盖：单机单世界单 PIE 进程；等级源只验 `StateLevel*` 两型中的数组型
      （`Map` 型与 `InstigatorLevel*` 的覆盖状态如实标注；**`LevelProvider` 读口本轮未被消费**——
      消费它的是 `InstigatorLevel*` 两型，而本轮内容资产的等级行走 `StateLevelArray`，
      该源按设计只读 `Context.EffectiveLevel`，两者别混为一谈）；关系表字段零消费者（→ R5.5-e）；
      `Cues` / `EventPayloadFilter` / `InterruptPriority` 仍是占位；跨 PIE 残留检查结论；
      **链挂的账本条目按设计常驻**（实测项：由"护甲回到 X、攻击停在 X−5"两个读数共同证明；
      机制见 §3.8，延后项 `CHAIN-7`）；
      **内容资产 `Fragments` 留空的理由**（D2）；
      **`ResolveAttrModDef` 运行期调用者为零**（本变更交付身份解析与索引，不交付"按 tag 取模板供物化"）；
      **`AttrModDef` 根下的词尚只有 `Check` 形态一个**（正式内容词未出现，根的代表词覆盖极薄）；
      既有诚实边界继承不重述但显式引用
- [x] 7.3 更新 `INDEX.md`：§1 状态、§4.5 台账计数、§4.6 证据行、`下一步`
- [x] 7.4 更新 `ledger/deferred-inputs-ledger.md`：**`ATTR-1` 标记闭合**（写清闭合面与残留边界 =
      运行期调用者为零 + 根下正式内容词为零）、Task 4 边界① 行标记闭合 + 引用本证据；
      **新增两条（2026-10-05 用户裁定甲案，append-only 追加；两条归属均 = 触发条件）**：
      ① **`CHAIN-7`**——**链挂的账本条目没有框架侧回收触发点**：条目挂 `Context.RunSource`
      （`TcsStepModifyAttribute.cpp:80`）、回收口公开且已有真实使用者（`TcsDamageSubsystem.cpp:226`），
      但**链原语路径零调用**；全即时链的运行态**当场释放**（`TcsEffectSubsystem_Run.cpp:104-105`）
      ⇒ 连"随运行结束回收"的时间窗口都没有；按因果边回收**已被裁定③明文禁止**。三条路全堵
      ⇒ 今天唯一合法用法 = 宿主自存 `RunSource` 再自调 `RemoveBySource`。**与 `CHAIN-6` 同族**
      （"机制在、可达触发点不在"）。触发条件 = 出现"链改的属性必须随某生命周期回退"的真实内容需求；
      最近候选轮 R6（技能是第一个给链运行带生命周期的调用方），**不预设轮次**
      ② **`CHAIN-8`**——**链原语缺"直写基础值"**（等价 GAS Instant `Execute` 语义）：
      `SetBaseValue` 只有流程步骤内联直调（`TcsFlowStepsCore.cpp:202`，属伤害专属）与宿主门面，
      **无链原语封装**；而 `FTcsStepModifyAttribute` 落**修正器层**——会被任一 `TAO_Override` 整体抹掉
      （`TcsAttributeBandFold.h:161-164`：`bHasOverride` 直接 return，全部 `Add` 被丢弃）、
      参与 `PercentAdd`/`Mul` 复利、且可按来源摘除 ⇒ **效力不等同**。**不并入 `DAMAGE-2`**
      （后者框的是 D4-16 那张 15 原语清单，本条是**清单外新增**，并入会在该行的账与清单之间制造不一致）。
      行内 MUST 带**三项待拍板**：①语义 = 设绝对值还是加增量（伤害步用"当前值 ± Δ"而非绝对值）
      ②是否需要"可回收"（与 `CHAIN-7` 交叉）③与 D7-6 的边界照 `TcsFlowStepsCore.cpp:181` 的既有分工写清
      （"伤害是生命被削减，不是可被来源级联撤销的修正器"）。触发条件 = 出现"某效果要永久改基础值、
      且不该被后续 Override 抹掉"的真实内容需求；候选轮 R6（技能 Cost/成长）/ R8（内容）
      —— 落纸后复核：**表行数 = 唯一 id 数**、总数 **59 → 61**、`CHAIN` 前缀 **6 → 8**
- [x] 7.5 更新 `log/implementation-log.md`：Task 7 条目（含 MCP 内容资产制作方法与其失败模式；
      **含"验证任务因真资产校验要求而倒逼出一条实现路径 + 新根"这条经过**）
- [x] 7.6 更新 `log/decisions-log.md`：记录 `AttrModDef` 根（10 → 11）的裁定与 `ATTR-1` 触发条件的成立
- [x] 7.7 校验：`openspec validate verify-state-layer-e2e --strict --no-interactive` 通过
- [x] 7.8 三仓行尾/编码复核：改动文件零 CRLF、零 BOM

## 8. 归档（Task 8 执行，本变更不自行归档）

- [ ] 8.1 提案经用户复核后归档为 `changes/archive/2026-10-05-verify-state-layer-e2e/`
- [ ] 8.2 新能力 `state-layer-e2e-validation` 落 `specs/`；手工补 `## Purpose`（归档器写 TBD）；
      三份既有能力 delta 由归档器并入
- [ ] 8.3 `openspec validate --strict --no-interactive` 全库通过
- [ ] 8.4 **手工修 `gameplay-tag-governance` 的 `## Purpose` 根数**（归档器只替换需求、不动 Purpose）：
      该文件 §Purpose 现写"钉死插件那 **10** 个根段" ⇒ 改为 **11**，并同步该句里的根名单（若有）。
      **这是归档步的已知缺口，不是可选润色**——Purpose 里的数字与需求内的根表不一致即为规格自相矛盾
