# Tasks: add-skill-param-chain

> 对应计划 `PLN-R6` Task 4（`Documents/combat-system-design/plans/plan-r6-skill-layer.md`，Step 1~8）。
> **★ 本稿是重写版（2026-10-09 第二版）**：第一版按"定义侧 = 账本侧单一形状"写，
> 已按用户四条裁决（双形状 / `FTcsNumericParamModInstance` / Apply 落条目 / 加 `OverridePriority`）重写。
> **★ 2026-10-09 实施期落用户五条裁决**（见 §12）：A′ 快照消费面、`Level` 键声明（含声明模块订正）、
> 选择器自有枚举、**选择器改名 + 删 `ById` + 立 `ByCategoryTags` 空壳**；另记一处我自己的装置缺陷与两处历史潜伏撞名。
>
> **勾选口径**：`[x]` = 已落地**并有本轮实测取证**；`[ ]` = 尚未取得证据。8.x 依赖**无头 PIE 取证**
> （`UnrealEditor-Cmd -game`，配方见 `EVID-2026-10-08-skill-cast-runtime` §7）——未实测前 MUST 保持未勾选。

## 1. 双形状修正器（计划 Step 1；**本版核心修正**）

- [x] 1.1 **定义侧** `Source/TcsSkill/Public/Skill/TcsNumericParamModifier.h`——`USTRUCT(BlueprintType) FTcsNumericParamModifier{ParamKey, Op, Operand: FTcsParamValue, SortKey, OverridePriority, CompeteGroup, ValueConvention}`。**★ 经 2026-10-09 用户质疑删去 `Source`**（§12.7：它是假字段——非反射类型不能作 `UPROPERTY` ⇒ 进不了编辑器也进不了资产；且定义侧物化路径根本不读它，M2 的定义侧行同样没有它）
- [x] 1.2 **账本侧**同文件——**纯 C++ struct `FTcsNumericParamModInstance`**（**无 `USTRUCT`**）：`{ParamKey, Op, ResolvedValue: double, Source, SortKey, OverridePriority, CompeteGroup}`。**MUST NOT 含任何 `FInstancedStruct` / `TScriptInterface` / 对象引用成员**
- [x] 1.3 **双形状判据写进注释**（防后人"顺手合并"）：账本条目住 `TArray<FTcsLearnedSkillEntry>`（非 `UPROPERTY` 容器、元素非反射 struct）⇒ GC 不可见 ⇒ 账本侧若持 `FTcsParamValue`（可持 `TScriptInterface`）则那些对象被**静默回收**（源变空引用、不崩溃）。先例 = M2 `FTcsAttrModOperand`（`TcsAttrModInstance.h:156`，D2-13 双形状裁定）
- [x] 1.4 **`SortKey` 与 `OverridePriority` 并存**，注释写明各归其位：**`OverridePriority` = 仅 `TAO_Override` 带读**（折叠器读它）；**`SortKey` = 同带内审计位**（折叠器**不读**，带序唯一真相在 `Op`）。**MUST NOT 令 `OverridePriority = SortKey`**
- [x] 1.5 **零新枚举自检（作用域已收窄）**：`Op` 复用 `ETcsAttributeOp`、`Operand` 复用 `FTcsParamValue`、`ValueConvention` 复用 `ETcsValueConventionFlag`——**修正器族**零 `UENUM`（反查：`TcsNumericParamModifier.h` 内 `UENUM` 计数 = 0）。**该口径不覆盖选择器**（见 4.1 与 §12.3）
- [x] 1.6 **形状对照自检**：与 `FTcsAttrModInstance` 逐字段对照，能**直接构造 `FTcsAttributeBandEntry`** 而无需改名/合并字段（`TcsParamChain_Evaluate.cpp` 的摊平处即此判据的可执行形态）

## 2. 参数账本（计划 Step 3 的一半；**需求② Apply/Removal 的落点**）

- [x] 2.1 `FTcsSkillRegistry` 的条目增第七条 `TArray<FTcsNumericParamModInstance> NumericParamModInstances`（照 `FTcsAttributeInstance::AttrModInstances` 的"每条目一套槽位"）
- [x] 2.2 `ApplyParamModifiers(Unit, FTcsSkillEntrySelector, TArrayView<FTcsNumericParamModifier>, FTcsSourceHandle Source)`：先按选择器定位条目 → 逐条**物化**（见 §5）→ 追加进该条目 `NumericParamModInstances`。**单位未注册 / 无匹配条目 ⇒ 忽略 + 日志 + 返回 false，MUST NOT ensure**（同 M2 `ApplyModifier` 的 D2-14 口径）。**★ `Source` 是形参**（§12.7 订正：来源属"这一次施加"，不属声明）；**来源无效 ⇒ 拒绝且零写入**（实证 `.Reject` 的 `R6a`/`R6b`：返回 false、**槽位 0→0**）
- [x] 2.3 `RemoveParamModifiersBySource(Unit, Source)`：扫该单位**全部条目的 `NumericParamModInstances`**、摘 `Source` 匹配者、**返回摘除条数**；**0 条命中 = 正常路径，MUST NOT ensure**
- [x] 2.4 **成对性自检**：凡写入 `NumericParamModInstances` 的条目 MUST 带 `Source`，且该 `Source` MUST 能被 2.3 摘除（判据 = 不存在"摘不掉的条目"）——实证 `P6c`/`P6e`/`P10b`
- [x] 2.5 **门面转发**：`UTcsSkillSubsystem` 同名方法一行转发 `FTcsParamChainOps`（`TcsSkillSubsystem_ParamChain.cpp`，同 `FTcsSkillOps`/`FTcsCastOps`）
- [x] 2.6 **GC 纪律自证**：`NumericParamModInstances` 元素是纯 C++ 无对象引用 ⇒ `UTcsSkillSubsystem::AddReferencedObjects` **MUST NOT** 为此改动（反查：ARO 函数体内 `NumericParamModInstances` 计数 = 0）；`TcsSkillSubsystem.h:89` 那句"账本条目不持任何 `UObject` 引用"**保持成立**

## 3. 参数链求值（计划 Step 2；**`STAT-1` 收束 + 需求① 的落点**）

- [x] 3.1 `Source/TcsSkill/Public/Skill/TcsParamChain.h` + `Private/Skill/TcsParamChain_Evaluate.cpp`——求值流水：① 取该条目 `NumericParamModInstances` 中 `ParamKey` 匹配者 → ② `CompeteGroup` 分桶取**组内 `ResolvedValue` 最大者** → ③ 摊平 `FTcsAttributeBandEntry{Op, Value = ResolvedValue, OverridePriority = 该条 OverridePriority}` → ④ 调 `FoldTcsAttributeBands(初值, Entries)`
- [x] 3.2 **初值 = 该键参数行求值结果；无参数行则 0**（实证 `P8`：无参数行 + `Add+7` ⇒ 7）
- [x] 3.3 **`STAT-1` 收束自检**：`Source/TcsSkill/` 内 `FoldTcsAttributeBands` **仅此一处**调用；全仓**无**第二份五带折叠实现。**实测（2026-10-09 落地后复算）**：全仓**真实调用点 3 处** = M2 `TcsAttributePipeline.cpp:251` / TcsDamage `TcsFlowAttributes.cpp:45` / **M5 `TcsParamChain_Evaluate.cpp:98`**；符号总命中 **9 处**（定义 1 + **注释 5** + 调用 3）。**判据 MUST 数"真实调用点"，MUST NOT 数符号总命中**（**初稿我把注释数写错成 4/总数 8，复算订正为 5/9**——计数 MUST 复算、MUST NOT 凭记忆）
- [x] 3.4 **顺序无关**由折叠器纯函数性保证 —— **MUST NOT** 在调用点加"按带序排序"的补丁（实证 `P4`：Mul 先写 ⇒ 仍 330）
- [x] 3.5 **读路径 MUST 只读账本已解析值**，MUST NOT 在读路径重新求值参数源（`EvaluateParamKey` 只读 `ResolvedValue`；初值那一处是"参数行"而非"修正器源"，与物化边界同规则同转换）
- [x] 3.6 **竞争组无休眠池**：竞争在**求值时**按账本全量重算 ⇒ 最大者撤销后次大者**自动递补**（实证 `P6d`）
- [x] 3.7 **调用折叠器的第三个实参（tie-break）取默认值**，**MUST NOT 为 M5 另造 tie-break 字段**。自检：`Source/TcsSkill/` 内该调用**只传两个实参**（先例 = `TcsFlowAttributes.cpp:45`）

## 4. 条目选择器（计划 Step 3 的另一半）

- [x] 4.1 `Source/TcsSkill/Public/Skill/TcsSkillEntrySelector.h`——`USTRUCT(BlueprintType) FTcsSkillEntrySelector{Mode: ETcsSkillEntrySelectorMode, DefTagFilter, CategoryTags, CustomFragment}`，四档**各有字段载体**。**★ 命名与档位经 2026-10-09 用户裁定两处修正**（§12.6）：类型名加 `Skill` 段；`ByTag` → `ByDefTag`；**原 `ById` 档整体删除**（句柄是运行时产物、编辑器不可配置）、其位置改为 **`ByCategoryTags` 预留空壳**
- [x] 4.2 **两个未实现档（`ByCategoryTags` / `Custom`）MUST 具名报出"未实现"且零修正被施加**——**MUST NOT** 静默当 `All`、MUST NOT 静默无操作。实证 = `.Reject` 的 `R3b-1`/`R3b-2`（返回 false、**槽位 0→0**）与 `R3-1`/`R3-2`
- [x] 4.3 选择器**只服务外部施加**：定义侧 `ParamChainRows` MUST NOT 接受它（类型上即不可能——字段是 `TArray<FTcsNumericParamModifier>`）
- [x] 4.4 **`ByDefTag` 两种粒度可分辨**（防"层级匹配"被写成"精确匹配"或反之）：装置 `P6a`/`P9` 用**完整 DefTag** 精确选中（`P9` 读数 A 0→2 / B 0→0——A、B 两个 DefTag 是**兄弟**不是父子，故精确性可证伪）；`P10a`/`P10b` 用**父 tag** 层级命中（摘除 **6 条** > 1）

## 5. 物化边界与字段回补（计划 Step 4）

- [x] 5.1 **物化边界单点**（`TcsParamChain_ResolveModifierValue`）：求值 `Operand`（经 `FTcsParamEvaluateContext`）→ 按行 `ValueConvention` 经 `FTcsValueConvention::ConvertToCanonical` **转一次** → 写 `ResolvedValue`；账本**不二次转换**。**外部施加与定义侧物化共用本函数**（"与声明式共用同一物化器"落在边界函数上）
- [x] 5.2 **能力位为假的源不转**——判据由**源自身** `AllowsValueConvention()` 声明；**实证双向**：`P7`（Literal 允许 ⇒ 85→0.85）+ `P14`（`ParamRef` 禁止 ⇒ 配 `VCF_Percent` 仍 0.85，未误转为 0.0085）
- [x] 5.3 `FTcsSkillDefData` 增 `TArray<FTcsNumericParamModifier> ParamChainRows`（**内联形态**；**MUST NOT** 引用 `UTcsSkillModDef`——实测该类型全仓 `Source/` 零命中，归 `R6.5-f`）
- [x] 5.4 **激活期物化**：激活时把 `ParamChainRows` 物化进该条目 `NumericParamModInstances`，`Source` = **`FTcsCastRun.RunSource`**；**并 MUST 在施法终结时按该来源摘除**（`FTcsCastOps::TerminateRun` 内落实）。实证 `P13`：两轮激活峰值均 3、终结后 0（漏摘则第二轮翻倍）
- [x] 5.5 **同批 MODIFIED 三个能力**（delta 已含，`check_mc2.py` 复验 = 3 MODIFIED / 14 源场景 / 0 未声明丢失）：`skill-cast-runtime`（快照消费面 + `Mode` 分流）+ `skill-registry`（字段六项→七项）+ `skill-def-asset`（`ParamChainRows` 已声明）
- [x] 5.6 **设计文本同批划改**：`spec/05-module-skill.md:56` 的五处失步（`SortKey` 与 `OverridePriority` / "退化为带权" / "Override 取最大值" / `Level` 键声明方 / 选择器枚举与快照消费面）**保留原文 + 就地注明订正理由**

## 6. `Level` 修正键接线（计划 Step 5）

- [x] 6.1 `ParamKey` = 特殊键 `Level` ⇒ 改**生效等级**（`clamp(0, LevelBase + Σ)`），且经**同一条**五带折叠（初值 = `LevelBase`），**不走**普通参数折叠；该键由插件原生声明为 `TcsStateParam.Level`，**声明处住 `TcsState`**（`Public/Param/TcsStateParamKeys.h`，§12⑤ 裁决；宿主零配置即生效）
- [x] 6.2 `EffectiveLevel` 快照时点 = **激活时**（`FTcsCastOps::Activate` 物化后重算并写回 `Run->Level`）；实证 `P11b`（`LevelBase=1` + `Level` 键 `Add+2` ⇒ run 等级 3）
- [x] 6.3 `Level` 之外的键一律走普通参数链，**MUST NOT** 影响生效等级；实证 `P11a`（`Level` 键不进普通参数表：对无参数行的普通键读取落 miss）

## 7. ★ 闭合 Task 3 的真实缺口（本版新发现，MUST NOT 漏）

- [x] 7.1 **`Mode` 列分流**：`BuildSkillSnapshot` 按 `FTcsNumericParamRow::Mode` 分派——`EPM_Snapshot` 求值冻结 / `EPM_Live` **标记跳过**。自检：函数体里**有** `Mode` 判断；实证 `P12a`/`P12b`
- [x] 7.2 **快照消费面接线（★ 按 §12.1 裁决 A′ 订正，MUST 读）**：快照 MUST 作为**求值期的参数表**被绑定（`FTcsCastOps::Activate` 用 `FTcsStateSnapshotScope` 把 `Run.ParamSnapshot` 装进物化上下文 ⇒ `ParamRef` 从中取值），**不是**"公共读口的冻结来源"——`GetNumericParam` 走**实时**通道（参数行初值 + 账本槽位折叠），**MUST NOT** 改读快照（判据：快照住每次 run 而读口签名无 run 段；同一 Entry 可有两个 run 并存 ⇒ "读哪一次"无唯一答案）
- [x] 7.3 **缺口登记入规格**（已在 delta 完成，并同批订正初版那句不成立的"`GetNumericParam` 改走快照"）——防后人以为初版已实现
- [x] 7.4 **8.2 的判据依赖本项**：`P3b`/`P4`/`P7`/`P8`/`P11b`/`P13` 全部**先激活再读数**（这正是本装置第一版的错误：未激活就读定义侧链行 ⇒ 5 项假 FAIL——见 §12.4）

## 8. 文件与编译

- [x] 8.1 **分片（如实校准，MUST NOT 读成"全部 ≤300 行"）**：**插件侧（TCS 交付物）新增/改动文件全部 ≤300 行**——`TcsNumericParamModifier.h` 170 / `TcsEntrySelector.h` 69 / `TcsParamChain.h` 189 / `TcsParamChain.cpp` 149 / `TcsParamChain_Evaluate.cpp` 165 / `TcsParamChain_Materialize.cpp` 59 / `TcsSkillSubsystem_ParamChain.cpp` 17 / **`TcsStateParamKeys.h` 37 + `.cpp` 6**（等级键，见 §12⑤）（按 `<Name>_<Feature>.cpp` 分片：物化 / 求值 / 门面转发三分）。
  **宿主装置侧（LAC `TcsDev`，开发用、非正式内容）** `TcsDevSkillParamChainProbe.cpp` = **713 行，超 300**（如实登记）。**判据澄清**：① "≤300 行"不是本仓 `openspec/specs/` 里的硬规则（全仓反查无此条），既有插件文件已到 626 行、宿主装置文件已到 **3094** 行；② 本文件的 713 行**沿用装置侧既有体例**（`TcsDevSkillCastProbe.cpp` 560 / `TcsDevSliceRig_State.cpp` 683），且它是**宿主开发用装置、不进任何交付物**；③ 若后续要收，按命令面拆（`Run` / `Reject` 已在函数层分离，可直接切两个 `.cpp`）——本轮未做，**如实记为未收项**。
- [x] 8.2 unity 合并撞名防护：file-local 符号带文件前缀（本仓实证本轮又抓到两处宿主装置的历史潜伏撞名，已按同规则修复——见 §12.4）
- [x] 8.3 `LegendAutoChess Win64 Shipping` 编译 **0 error / 0 warning**（实测 `Result: Succeeded`）
- [x] 8.4 `LegendAutoChessEditor Win64 Development` 编译 **0 error / 0 warning**（实测 `Result: Succeeded`）
- [x] 8.5 `openspec validate add-skill-param-chain --strict --no-interactive` = **valid**（并 `--all --strict` = 38 passed / 0 failed）

## 9. 验收取证（无头 PIE；每项 MUST 带阴性对照或前提自证）

> **实测总读数**：`Tcs.Test.SkillParamChain.Run` = **25 PASS / 0 FAIL**，零红字、零 ensure；两轮复现
> 结论行**逐字相同**（26 行 zero-diff）。回归：`SkillCast.Run` **26/0**（= Task 3 基线）、
> `SkillCast.Reject` **10/0**、`Slice.Run` 红字 0。

- [x] 9.1 **五带折叠一次到位**：`Add +10` / `PercentAdd +0.5` / `Mul 2`，初值 100 ⇒ **330** → `P3b` 读数 330.0000
- [x] 9.2 **顺序无关**：打乱书写顺序再求值 ⇒ 逐字相同 → `P4` 读数 330.0000（取两次求值结果，未取"排序被调用过"）
- [x] 9.3 **Override 三级比较**：`P5` 读数 5.0000——**含阴性对照**（值 999 优先 0 的陷阱条未胜，故排除"取数值最大者"）；`Add +1000` 一并被覆盖
- [x] 9.4 **竞争组取组内最大**：同组 `+10`/`+30` ⇒ `P6b` 读数 30.0000（**不是 40**）
- [x] 9.5 **落选者撤销后自动递补**：`P6d` 读数 10.0000（无任何"唤醒"调用）
- [x] 9.6 **无参数行初值为 0**：`P8` 读数 7.0000
- [x] 9.7 **值约定能力位（双向）**：`P7` 允许源 85 ⇒ 0.8500；`P14` **能力位为假的源原样进账本**（`ParamRef` + `VCF_Percent` ⇒ 仍 0.8500，误转会是 0.0085）
- [x] 9.8 **`Level` 键进等级**：`P11b` 生效等级 3；`P11a` 该键不进普通参数折叠
- [x] 9.9 **`Custom` 档具名报出**：`Tcs.Test.SkillParamChain.Reject` 的 `R3a`（入口返回 false）+ `R3b`（**条目槽位数逐字不变 0→0**——MUST NOT 只查"有 Warning"）。**该检查 MUST 独立成 `.Reject` 命令**：它必然留一条 Warning，混进常规命令会破坏"零红字"这条验收信号（`MEM-20260918-07`）
- [x] 9.10 **Apply 落目标条目**：`P9a` A 0→2、`P9b` B 0→0（未选中条目不变）
- [x] 9.11 **按 `Source` 级联摘除**：`P6c` 恰摘 1；`P6e` 重复摘 ⇒ 返回 0 且不 ensure
- [x] 9.12 **`ParamChainRows` 激活期物化 + 来源 = 施法运行句柄**：`P13` 峰值 3/3、终结后 0；`P11b` 由激活路径写回等级
- [x] 9.13 **★ 快照作求值参数表的实证（按 A′ 改写原判据）**：`P3b`/`P4`/`P7`/`P8`/`P13` 的链行均为**激活期物化**产物——其求值经 `Scope.GetTable()`（= 本次 run 快照）装填上下文；**原判据"激活后改来源仍读旧值"已按 A′ 作废**（真实缺口是"零消费者 + `Mode` 未接"，不是"读口未接快照"）
- [x] 9.14 **★ `EPM_Live` 分流**：`P12a` Snapshot 行在快照（11.0000）、`P12b` Live 行**不在**（缺失 `Mode` 分流时本项必失败）
- [x] 9.15 **零意外红字**：常规命令 `redlines=0`；`SkillCast.Reject` 的 3 条红字为**预期**（独立 opt-in 命令，自带屏显声明）
- [x] 9.16 **两轮可复现性**：run7 vs run8 结论行 **26 行 zero-diff**（按行比，未比字节/行数）
- [x] 9.17 **全量搜 `ensure` / `Fatal`**：0（未按模块前缀过滤——全日志搜 `Ensure condition failed|Fatal error`）
- [x] 9.18 `Source/TcsDamage/**` / `Source/TcsTargeting/**` / `Source/TcsEffect/**` 反查：本轮**不改**（`git status --porcelain` 三者各 0 项）

## 10. 宿主装置（LAC 仓 `Source/TcsDev/`，**开发用、非正式内容**）

- [x] 10.1 扩 `TcsDevSkillCastProbe*` 加参数链读数（**复用既有装置**，不新建第三个；主文件已 660+ 行 ⇒ 按 `TcsDevSkillParamChainProbe.cpp` 分片，命令挂同一命令族 `TcsDevSkillCastProbe::RegisterParamChainCommands`）
- [x] 10.2 `Config/DefaultGameplayTags.ini` 补本轮检查键（`SkillDef.Check.*` / `TcsStateParam.Probe.*` 形态，**MUST NOT** 另立根）——含 `SkillDef.Check.ParamChainNoConv`（P14 用，漏声明会落"DefTag 无效"）
- [x] 10.3 读数以 **UTF-8 日志文件**为准（控制台回显为 ANSI）

## 11. 台账与收束

- [x] 11.1 `STAT-1` 标**已消费（全三处）**（M2 / TcsDamage / **M5 本变更**）——已在台账就地勾销并附可复算的计数判据
- [x] 11.2 边界登记：**技能账本今天无"冻结机制"** ⇒ 摘除只扫在册条目；若将来出现"技能条目冻结"，扫描面 MUST 同批补上（已写入 `TcsParamChain.h` 的 `RemoveParamModifiersBySource` 注释）
- [x] 11.3 `R6.5-e` 的形状预留写进注释与规格：布尔修正器槽位应是**并列第二个数组**（布尔与数值**不同代数**，混装会让折叠按元素类型分派）——已写入 `TcsNumericParamModifier.h` 文件末的「形状预留」块（只记约束、**MUST NOT** 现在就建空数组）

## 12. 实施期裁定与留痕（2026-10-09，MUST NOT 删）

> 起草阶段的三条用户裁决 + 一处我自己的装置缺陷。**全部已回写进规格/设计文本**，此处只做索引与留痕。

- [x] 12.1 **① 快照消费面 = A′（用户裁定）**：快照只做**求值期的参数表载体**（先例 = 状态侧 `FTcsStateModifierMaterializer` 的 `FTcsStateSnapshotScope`）；`GetNumericParam` 保持**实时**语义；**不新增 run 作用域读口**（今天无消费者）。原因：原提案那句"`GetNumericParam` MUST 改走快照"**本身不成立**——快照住每次 run，而读口签名无 run 段，且 `CI_InstancePerExecution` 下同一 Entry 可有两个 run 并存（Task 3 已实测"读数 = 2"）⇒ "读哪一次施法的冻结值"**无唯一答案**。已同批改 `skill-cast-runtime` delta。
- [x] 12.2 **② `Level` 键由插件原生声明（用户裁定；声明模块经 ⑤ 订正）**：`TcsStateParam` 根的一般键归宿主 ini，但 `Level` 是框架自己解释的词（`clamp(0, LevelBase + Σ)` 由等级域求值）⇒ 按 `gameplay-tag-governance`「框架契约词 MUST 由插件模块原生声明」落为**原生 tag** + 模块导出宏。**判据**：下放宿主 ⇒ 漏配即 `Level` 修正静默失效（数值照进账本、等级永不动、零报错）。
- [x] 12.5 **⑤ `Level` 键的声明模块 = `TcsState`（用户 2026-10-09 追问后订正）**：初版落在 `TcsSkill`（`Skill/TcsSkillParamKeys.h`），用户指出"StateDef 里就有 Level 参数"⇒ 复核后**判定用户正确并迁移**。**三条判据**：① **语义归属**——`LevelBase`/`MaxLevel` 声明在 `FTcsStateDefBase`（技能 Def 继承它），D3-11 的家是状态侧规格；② **依赖方向（决定性）**——`TcsSkill` 已依赖 `TcsState` ⇒ 声明在上游则两侧消费零成本；若留在 `TcsSkill`，状态侧将来消费（状态等级修正）就要**反向依赖 TcsSkill = 成环**；③ 同词多域消费是常规（M2/M5/状态共用五带即先例）。**迁移动作**：新建 `TcsState/Public/Param/TcsStateParamKeys.h` + `Private/Param/TcsStateParamKeys.cpp`，删 `TcsSkill` 侧两文件，`TcsParamChain_Evaluate.cpp` 改 include，并清掉两处**死 include**（`TcsParamChain_Materialize.cpp` / 宿主装置都未用该符号）。**验收**：双配置 0/0，`Run` **26/0**（`P11b` 仍读 3 ⇒ 跨模块解析成立）、`Reject` **5/0**。
- [x] 12.6 **⑥ 选择器：改名 + 删 `ById` + 立 `ByCategoryTags` 空壳（用户 2026-10-09 裁定）**：
  - **改名** `FTcsEntrySelector` → **`FTcsSkillEntrySelector`**、`ETcsEntrySelectorMode` → **`ETcsSkillEntrySelectorMode`**（前缀 `ESS_`）、文件 `TcsEntrySelector.h` → **`TcsSkillEntrySelector.h`**。判据 = 它**只服务技能账本条目**（状态侧没有"已学条目"这一层）；名字里带 `Skill` 免得将来第二个域出现时逼出改名。
  - **`ByTag` → `ByDefTag`**：原档名含混（"Tag"在 TCS 里指属性词/状态词/事件词/参数键等七八种东西），改名后与字段名 `DefTag` 对齐。字段 `TagFilter` → **`DefTagFilter`**。
  - **删除 `ById` 档**（**MUST 留痕：这不是"漏做"，是判据充分的删除**）：① **不可被作者配置**——`FTcsSkillEntryHandle`（`Index` + `Generation`）是**运行时产物**，编辑器里无法预先填出合法值 ⇒ 对"以配置为入口的外部施加"而言该档等于不存在；② **其"Id"是历史遗留**——Def 资产身份原为 `FName DefId`，2026-09-22 起**已全量迁移 `GameplayTag`（`DefTag`）** ⇒ "按 Id 筛"的正当形态就是按 tag 筛，已被 `ByDefTag` 覆盖；③ 留一个**带代际的句柄过滤位**会诱导后人做长期引用精确命中，而代际使跨帧引用自然失效 ⇒ 形状"看起来能用、实际易碎"。**我原实现选 `EntryHandle` 的理由（如实登记）**：为了让 `ById` 档"有个能留存的东西"——那是**为保住一档而选了一个不可配置的载体**，方向错了。
  - **新增 `ByCategoryTags` 空壳**（承接删除位，台账 **`STAT-10`**）：字段 `FGameplayTagQuery CategoryTags`（引擎既有可反射类型，落下零成本、且让"该档将来吃什么"在形状上可见）；行为 = **具名报出未实现 + 零修正被施加**（与 `Custom` 同款）。**MUST NOT 落机制**——载体（StateDef 侧类别标识容器）不存在、档名与规则未拍板、且可能另立 `TcsGameplayTag` 模块。
  - **验收**：双配置 0/0；`Run` **26/0**（`P6a`/`P9` 改用完整 `DefTag` 精确选中、`P10` 用父 tag 层级命中）；`Reject` **5/0 → 7/0**（新增 `R3b-1`/`R3b-2` 两检查：`ByCategoryTags` 返回 false、槽位 0→0）。
- [x] 12.3 **③ 选择器自有枚举（用户裁定）**：落 `ETcsEntrySelectorMode`，并把"零新枚举"口径**收窄**为"修正器的三个字段类型复用既有类型"（`Op`/`Operand`/`ValueConvention`）。已同批改 `skill-param-chain` delta 的需求正文 + 场景标题/判据。
- [x] 12.4 **④ 我自己的装置缺陷与两处历史潜伏撞名（留痕防复犯）**：
  - **(a) 装置第一版 5 项假 FAIL**：我直接 `GrantSkill` 后就读定义侧链行，而 `ParamChainRows` 的契约是"**激活期物化**"——未激活时 `NumericParamModInstances` 本来就是空的 ⇒ 读数退化成"只看到参数行初值"（100/100/100/0/miss）。**教训 = `MEM-20261008-02`「标称验 X 的检查 MUST 在自身判据里证明 X 的前提真的成立」**：检查名说"折叠生效"，而它读的时刻**折叠的输入还没被物化**。修法 = `ActivateReadThenTerminate`（先激活 → 读数 → 终结）。
  - **(b) 由此暴露出一条我漏实现的明文需求**：delta 写明物化行"随施法终结**级联摘除**"，而 `TerminateRun` 当时只摘 `RunHandles` ⇒ 槽位会逐次累积（"放两次技能、参数翻倍"，且**单次激活读数完全正常** ⇒ 只验一次的检查必漏）。已在 `TerminateRun` 落实（摘的账本是 M5，与 `CHAIN-7` 的 M2 属性账本回收互不代劳），并立 `P13` 专测。
  - **(c) `TcsDev` 模块两处历史 unity 合并撞名**（**非本轮引入**，因我的头文件改动迫使 UBT 把该模块全部 `.cpp` 重排进**同一个** unity TU 而暴露）：`GTcsDevSkillCastUnimplDefTag` 在两个 Cast 探针文件各定义一次（C2374/C2086）；两个内部件的 `ResolveSkillSubsystem` 同名 `using` 落进同一作用域（C2668 二义调用，**报错位置指向"自己没改的文件"**）。已按本仓既有规则修复：常量带 `Reject` 前缀、该函数一律**全限定调用**。
- [x] 12.7 **⑦ 删定义侧 `Source` + 账本侧改名（用户 2026-10-09 质疑后订正）**：
  - **删 `FTcsNumericParamModifier::Source`**。用户质疑原话："为什么 `FTcsNumericParamModifier` 也需要携带 `SourceHandle`，它不能作为纯 Def 类内容吗"。**复核后判定用户正确**，且它比"可以更纯"更严重——**它是假字段**，三条判据：
    ① **它不是作者可配、也不会被序列化**：`FTcsSourceHandle` 是**非反射纯 C++ struct**（`TcsSourceHandle.h:20` 裸 `struct`）⇒ **不能作 `UPROPERTY`**。**UHT 产物实测**：`FTcsNumericParamModifier` 的反射属性恰为 `ParamKey / Op / Operand / ValueConvention / SortKey / OverridePriority / CompeteGroup`，**`NewProp_Source` 命中数 = 0** ⇒ 它**既不进细节面板、也不进资产**，只活在内存里。
    ② **定义侧路径根本不读它**：`MaterializeParamChainRows` 盖的是 **`RunSource`**（`TcsParamChain_Materialize.cpp:65`），不是 `Row.Source`。**全库唯一读 `Row.Source` 的地方是外部施加路径**（`TcsParamChain.cpp:122`）。
    ③ **M2 的既有形状即此**：`FTcsAttrModDefTableRow`（定义侧行）**没有 `Source` 字段**，它是 `MakeFromDef(Row, Operand, InSource)` 的**形参**。我把形参提升成了字段。
    **语义根因**：**"源"从来不是"这一行"的属性**——同一个 `ParamChainRows[0]`，甲玩家靠装备给、乙玩家靠天赋给 ⇒ **来源因实例而异，属实例而非 Def**。
  - **`ApplyParamModifiers` 新增 `FTcsSourceHandle Source` 形参**（外部施加入口由调用方声明"这次是谁给的"）。
  - **连带新增一条守卫**：**`Source` 无效 ⇒ 拒绝施加且零写入**（无锚点的条目摘不掉 = 违反成对性判据；表现为"参数改不回去"，静默且远离根因）。实证 = `.Reject` 新增 `R6a`/`R6b`（返回 false、**槽位 0→0**；判据 MUST 是"槽位未变"，MUST NOT 只查"返回 false"——"先写后判"也能返回 false）。
  - **账本侧改名 `FTcsNumericParamInstance` → `FTcsNumericParamModInstance`**（用户指出"名字像参数实例、注释却写修正器实例"，**名实不符**）。**判据 = 与 M2 逐字同构**：M2 账本侧是 `FTcsAttr**Mod**Instance`，我的初版把 `Mod` 丢了。命名规则 = **`[表词] + Mod + 面后缀`**。
  - **同批订正我注释里自造的布尔侧名字**（用户追问"BoolSwitch 的相关变量命名都是什么样的"）：我曾在形状预留里写 `FTcsBoolSwitchParamInstance`——**错**。规则下布尔侧表词 = **`BoolSwitch`**（`FTcsBoolSwitchRow` / `FTcsBoolSwitchModifier` / **`FTcsBoolSwitchModInstance`**）；初版那个名字把布尔表词与数值表词 `Param` 混装，两头都不沾（`FTcsBoolSwitchRow` 本身也不带 `Param`——设计管它叫"布尔**开关**表"）。
  - **验收**：双配置 0/0；`Run` **26/0**（`P6a` 改为**两次独立施加**——`Source` 成了调用参数，"两个来源各挂一条"必须走两次调用，这与真实用法一致）；`Reject` **7/0 → 9/0**（+`R6a`/`R6b`）。
  - **可迁移判据（本轮第三次同族错误，MUST 记）**：给一个值选载体时先问"**它的生命周期属于谁**"——**因实例而异的值 MUST NOT 住 Def 类型**；而 **Def 类型上的字段 MUST 是"作者可配 + 会被序列化"的**，否则即假字段。（前两次同族：`ById` 档选了不可配置的 `EntryHandle`；`STAT-5` 同族。）
