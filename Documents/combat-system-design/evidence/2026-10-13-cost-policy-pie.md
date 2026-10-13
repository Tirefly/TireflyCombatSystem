# EVID-2026-10-13-cost-policy-pie — R6.5-CC 补齐：内容侧消费资产 + PIE 路线 + 三处夹具/产品缺陷

- **文档 ID**：`EVID-2026-10-13-cost-policy-pie`
- **类型**：EVID / 验收证据
- **权威范围**：`PLN-R6.5-CC` 的**取证补齐轮**——把 `EVID-2026-10-12-cost-policy` §7 边界 3（"未做 PIE 路线"）与"内容侧 Def 资产从未驱动过消耗策略"这两处缺口补上，并如实登记**本轮抓出的三处缺陷**（一处产品、两处夹具）。**不在本文**：消耗策略的**形状与设计裁定**（见 `EVID-2026-10-12-cost-policy`）、冷却侧既有读数（见 `EVID-2026-10-11-r65-cooldown-cost`）。
- **最后更新**：2026-10-13

---

## 1. 本轮补的是什么（三处缺口，逐条）

| # | 缺口 | 为什么它是缺口 | 本轮的补法 |
|---|---|---|---|
| **①** | **内容侧 `Def` 资产从未驱动过消耗策略** | R6.5-CC 的 `F`/`J`/`K` 三面用的全是**装置在运行期手搓**的 `FTcsSkillDefData` ⇒ "作者在编辑器里把 `Cost` 配上 → 存进 `.uasset` → 运行期由定义库装载 → 策略分派真扣"这条**内容侧链路一次也没跑过** | 新建 `DA_SkillDef_E2E_CostMana`（`Cost` = `ECT_OnCastStarted` + 内建 `ResourceAttr` 策略扣 `Attribute.Mana` **25**）+ 装置 **L 面**四条判据 |
| **②** | **未做 PIE 路线取证** | R6.5-CC 读数全来自无头；而**两条路线的前置状态不同**（PIE 有内容资产、无头为 0 条）⇒ 只在无头跑必然漏掉内容面 | 在编辑器里经 MCP 驱动 PIE 跑 `Run`/`Reject`/回归，并做**跨 PIE 重跑** |
| **③** | **`Mana`/`MaxMana` 无真资产** | 原 `Attribute.Check.R65Mana` 只是"检查用词"，不是项目真实资源词 ⇒ "Cost 直接消费属性"缺一个**内容侧真实落点** | 新建 `DA_Mana`（基值 100、上界**动态取 `MaxMana`**）+ `DA_MaxMana`（基值 100），并声明 `Attribute.Mana` / `Attribute.MaxMana` |

**判据（为什么三处 MUST 一起补）**：它们是**同一条链路的三段**——内容资产（①③）是"作者配得出"，PIE（②）是"那条配置真的被装载并生效"。只补任一段都会留下"另一段没验"的空档。

---

## 2. 读数

### 2.1 无头（`UnrealEditor-Cmd -game`）

| 命令 | 读数 | 日志 |
|---|---|---|
| `Tcs.Test.CooldownCost.Run` | **立即面 30/0 + 含跨帧尾段 33/0** | `Saved/TcsAcceptance/R65/FINAL2-Run.log` |
| `Tcs.Test.CooldownCost.Reject` | **7/0** | `FINAL2-Reject.log` |
| `Tcs.Test.SkillLayer.Run`（R6 回归） | **24/0**，`[复现性摘要]` 与冻存基线**逐字相同** | `FINAL2-Regression.log` |

**三份日志均 `FAIL=0` 且 `ensure=0`**（机械核对，见 §2.4）。

### 2.2 PIE（编辑器内，MCP `SlateInspectorToolset` 驱动）

| 命令 | 读数 | 备注 |
|---|---|---|
| `Tcs.Test.CooldownCost.Run` | **33/0 + 36/0** | **连跑三轮读数逐字相同** |
| `Tcs.Test.CooldownCost.Reject` | **7/0** | — |
| `Tcs.Test.SkillLayer.Run`（R6 回归，**同会话内**） | **24/0** | `[复现性摘要]` 与基线**逐字相同** |
| **跨 PIE 重跑**（`StopPIE` → `StartPIE` → 再跑 `Run`） | **33/0 + 36/0** | **跨 PIE 残留检查通过**；会话内 `ensure=0` |

**★ PIE 比无头多出的分母（4 条）正是本轮的 ①**：`L1`/`L2`/`L3`（内容侧资产）+ 1 条（`L2` 的读数在两条路线下**不同源**，见下）。**这是"两条路线读数不同"的正当理由**，不是回归——无头下 `L1~L3` **按设计跳过**并留 `[提示]` 说明（MUST NOT 读成通过）。

### 2.3 ★ 本档最值钱的一条：内容侧资产的消耗被**真扣**

```
[PASS] L1 内容侧资产 `Cost` 形状正确
       （档=1 期望 1=OnCastStarted；策略=内建 ResourceAttr；资源=Attribute.Mana 期望 Mana）
[PASS] L2 **内容侧资产的 Cost 真扣了资源**
       （授予=是；激活=6 期望 6；Mana 100.000→75.000 期望 −25.000
        ——读到 −30 即装置配置、−7 即宿主策略，均属串味）
[PASS] L3 同一形状摘掉策略 ⇒ fail-closed
       （可付=false 期望 false——证明 L2 的扣减由策略产生，不是旁路）
[PASS] L4 合成孪生同形状 ⇒ 真扣 25.000
       （授予=是；Mana 75.000→50.000；本条与 L2 形状同构、只在"谁登记了定义"上不同
        ⇒ 保证该能力跨路线可判）
```

**为什么取 `25` 这个数**：装置自持面写 `30`、宿主策略写 `7`、内容资产写 `25` —— **三处两两不同** ⇒ 任一面读到别人的数都能立刻判出串味。`L2` 读到 `−25` **只可能**来自内容资产自己的配置。

**`L4` 存在的理由（MUST 读，防后人删它）**：`L1~L3` 在无头下必被跳过 ⇒ 若只有它们，"内容资产驱动消耗"这件事就**没有跨路线可判的判据**。`L4` 用**同形状的装置合成定义**承担这条，与 `L2` 的差别**只在"谁登记了定义"**（内容资产 vs 装置）⇒ 于是"内容面没跑到"不再等于"这条能力没验"。

### 2.4 机械核对（不靠人眼）

```powershell
# 三份无头日志：FAIL 与 ensure 计数
foreach ($f in (Get-ChildItem Saved\TcsAcceptance\R65\FINAL2-*.log)) {
  "FAIL=" + (Select-String $f -Pattern '\[FAIL\]' | Measure-Object).Count +
  "  ensure=" + (Select-String $f -Pattern 'Handled ensure' | Measure-Object).Count
}
# 实测：全部 FAIL=0 / ensure=0

# 回归摘要逐字比对（基线 = 冻存的 K-Tcs-Test-SkillLayer-Run.log）
$base = (Select-String K-Tcs-Test-SkillLayer-Run.log -Pattern '复现性摘要' | Select-Object -Last 1).Line
$new  = (Select-String FINAL2-Regression.log        -Pattern '复现性摘要' | Select-Object -Last 1).Line
# 实测：$base -eq $new ⇒ True
```

**编译**：`LegendAutoChessEditor` 与 `LegendAutoChess`（Game）双配置均 **0 error / 0 warning**。

**内容资产装载的预期兑现**（PIE 下）：
```
定义库就绪——链定义 12 条，触发定义 2 条，状态定义 2 条，修正器模板 1 条，技能定义 5 条，失败 0 条
属性定义装载 6/6 条
```
- **技能定义 4 → 5 条**（新增 `DA_SkillDef_E2E_CostMana`）；
- **属性定义 4 → 6 条**（新增 `DA_Mana` / `DA_MaxMana`）；
- **失败 0 条**不变；链/触发/状态/修正器四条计数不变。
⇒ **分母移动是预先声明的**（新建资产前即写下本条），不是事后解释。

---

## 3. ★ 本轮抓出的三处缺陷（一处产品 + 两处夹具，逐条如实）

### 3.1 **产品缺陷：内建消耗策略脚本层不可达**（`BlueprintType` 缺失）

**症状**：Python 侧 `hasattr(unreal, "TcsSkillCostPolicy_ResourceAttr")` 为 **False**，而**姊妹类** `FTcsSkillCostPolicy_HostDelegate` **在**。

**根因（引擎源码）**：`IsScriptExposedStruct`（`PythonScriptPlugin/…/PyGenUtil.cpp:1560-1581`）**沿父链**逐个查 `BlueprintType` 元数据，**任一环有才导出**。本类是裸 `USTRUCT()`，与基类 `USTRUCT(meta=(Hidden))` **两者都没有** ⇒ 不导出。
**↳ 基类为何反而在脚本层**：它是**作为"已导出子类的父类"被顺带发出的**（`HostDelegate` 带 `BlueprintType`），**不是它自己够格**。

**同族先例（逐个核对过）**：`FTcsParamValueSource` / `FTcsParamEnumerableSource` 两个基类都是 `USTRUCT(meta=(Hidden))`，而 `FTcsParamSource_Literal` / `_ParamRef` / `_HostDelegate` / `_AttributeScaled` / `_InstigatorLevelArray` 等**全部具体类都带 `BlueprintType`** ⇒ 全在脚本层。**本类是这一族里唯一的例外。**

**实质影响**：**内建消耗策略脚本侧配不了**（Python / C# 都取不到该类型）——与 R4 裁定"内建也走同一套策略方案"以及 R6.5-CC 的脚本面插槽目标**直接冲突**。

**为什么它潜伏至今（MUST 读）**：**三条路里只有脚本那条断**——
- **C++ 面**：不受影响（`GetPtr<T>()` 走类型系统，与 `BlueprintType` 无关）；
- **编辑器 picker**：`FInstancedStructFilter::IsStructAllowed` 只看 `Hidden` / `AllowedClasses` 等元数据、**不读 `BlueprintType`** ⇒ **策划在细节面板里一直选得到它**；
- **脚本面**：断。
⇒ 只有"用脚本配它"这一条路会暴露，而本轮之前**从没跑过脚本侧**。

**处置**：加 `BlueprintType`（`TcsCostTypes.h`），并在类注释里写明"**具体策略类 MUST 带 `BlueprintType`；基类 MUST 只带 `Hidden`**——前者决定"脚本能不能配"，后者决定"picker 里能不能选到基类"，两件事别互相顶替"。
**回归证据**：修复后 `hasattr` 为 **True**，`unreal.TcsSkillCostPolicy_ResourceAttr()` 可构造且字段可写（`Attribute.Mana` 往返成功）。

### 3.2 **夹具缺陷 A：无头下 `ApplySliceAttributes` 触发 `ensure` 红字**

**症状**：每轮无头 `Run`/`Reject` 各吐**一处 Error 级 `ensure` + 11 行 callstack**：
```
Ensure condition failed: DefData != nullptr  [TcsAttributeSubsystem.cpp:148]
UTcsAttributeSubsystem::AddAttribute: 属性定义未登记（属性 Attribute.Health——宿主须先 RegisterAttributeDef）
  → UTcsDevBootstrap::ApplySliceAttributes()  [TcsDevBootstrap.cpp:128]
  → RunR65Rig()  [TcsDevSliceRig_CooldownCost.cpp:507]
```

**根因**：本装置直接调 `Bootstrap->ApplySliceAttributes(...)`（要四条切片属性），而属性定义**只由内容资产装载**，**无头下 AssetRegistry 发现 0 条** ⇒ 四条一条没登记 ⇒ `AddAttribute` 走 `ensureMsgf` 分支。装置自己**只补登记了 `R65Mana` 一条**。

**为什么它是"取证卫生"缺陷而非"已知边界"**：本命令族自己的纪律写着**"常规命令零非预期红字"**，而 `ensure` 是 **Error 级** ⇒ **验收信号被夹具自己污染**（用户跑 `Run` 会看到红字 + callstack，无法用"跑完只看 PASS/FAIL 行"判读）。与 `MEM-20261004-27`（"夹具不许自造红字"）是同一条纪律的又一次命中。

**为什么此前未被发现**：该 `ensure` 只在**首次**跑到该站点时报（`bExecuted` 原子标志，见 `unreal-development-workflow` 的 ensure 机制事实）⇒ 编辑器里先跑过别的命令、或只 grep `[FAIL]` 行，都会漏掉它。

**处置**：补 `EnsureR65AttributeDefs`（**手法与 `TcsDevSliceRig_Skill.cpp` 的 `EnsureSkillLayerAttributeDefs` 同款**——同一根因 R6 Task 6 已为先例），**且 MUST NOT 改成不调 `ApplySliceAttributes`**（两条路线对"单位持有哪些属性"的面 MUST 一致，否则读数不同源）。

**★ 该修复本身有过一次未到位（MUST 留痕）**：初版只登记 **5** 条，首跑即 **6 条 FAIL**——
漏掉的两类**成因完全不同**，但**症状都像"策略没被调到"**：
| 漏项 | 症状 | 误读方向 |
|---|---|---|
| `Attribute.Check.R65Mana` | 该属性读数恒 `0.000` ⇒ `F1`/`F2`/`G1`/`G2`/`A1b` 五条红 | "策略没被调到" |
| `MaxMana` | `Mana` 被动态上界钳到 0 ⇒ `L4` 红 | "策略没生效" |
⇒ 定稿 **7 条**（四条切片 + `MaxMana` + `Mana` + `R65Mana`），
并把判据明确为"**装置用到的每一条属性 MUST 在册——含被别的属性动态引用的那些**"。

**回归证据**：修复后三份无头日志 **`ensure=0`**（**`1 → 0`**，见 §3.4 证据表）。

### 3.3 **夹具缺陷 B：动态边界引用方未挂实例 ⇒ `Mana` 被钳到 0**

**症状**（极具误导性，日志里两句**同时**出现）：
```
消耗已提交：单位=3 属性=Attribute.Mana 100.000 → 75.000（扣 25.000）   ← 策略**真的扣对了**
[属性变更] 单位 3 的 Attribute.Mana：100.000 → 0.000                   ← 随即被边界钳回 0
⇒ [FAIL] L4 …（授予=是；Mana 0.000→0.000）
```
判据读 0 而 FAIL，**看起来像"策略没生效"**，实际是**边界把结果钳掉了**。

**根因**：`Attribute.Mana` 的**定义**带 `ABM_Dynamic → MaxMana` 上界（**刻意**与内容资产 `DA_Mana` 同规则，保持两条路线同源）。而动态边界**按目标属性的当前值求值**：单位**不持有** `MaxMana` 实例 ⇒ 上界求值落 **0** ⇒ 扣到 75 的 Mana 被钳回 0。
**⇒ 只登记"定义"是不够的：动态引用的被引用方 MUST 也挂到同一个单位上。**

**处置**：`EnsureR65AttributeDefs` 增列 `MaxMana`，两面的单位装配各补挂该实例。

**★ 由此带出的第二处（PIE 才暴露，MUST 读）**：补了 `MaxMana` 之后，PIE 下**又**出现 `ensure`——`AddAttribute` 对**已持有**属性是 `ensureMsgf(!Contains)`（`TcsAttributeSubsystem.cpp:125`）。而"是否已持有"**随路线而变**（PIE 内容资产已装载 ⇒ 属性面更宽）⇒ **无头绿、PIE 红**（**"无头给假绿灯"的又一实例**）。
**处置**：抽出 `R65AddAttributeIfAbsent`（**未持有才加**，以 `TryGetBaseValue` 作持有探测口——它正是本仓为"未持有 vs 持有且为 0"可区分而设的），**本装置一切 `AddAttribute` 调用 MUST 走它**。

**回归证据**：修复后无头与 PIE **均 `ensure=0`**（PIE 连跑三轮 + 跨 PIE 重跑）。

### 3.4 ★ 三处缺陷的**证据冻存状态**（MUST 读：哪一半可复核、哪一半不可）

**判据**：一处缺陷的完整证据 = **"修前坏"与"修后好"两半**。只有一半时，**"它确实坏过"这部分不可独立复核**。
本轮如实登记如下（**含一处我自己的取证卫生失误**）：

| 缺陷 | **修前**（坏） | **修后**（好） | 可独立复核？ |
|---|---|---|---|
| **①** `BlueprintType` | ✅ **`Saved/McpProbe/D1-BEFORE-fix.log`**：`has_ResourceAttr = **False**`（对照项 `HostDelegate=True` / `basePolicy=True`） | ✅ `D1-AFTER-fix.log` + `D1-AFTER-fix-r2.log`：**三项皆 `True`** | **✅ 两半都在盘上**（2026-10-13 受控临时回归补取，见下） |
| **②** 无头 `ensure` 红字 | ✅ **`Saved/TcsAcceptance/R65/CC2-Run.log`**（**已提交**）：`ensure=1`，原文 `Ensure condition failed: DefData != nullptr [TcsAttributeSubsystem.cpp:148]` | ✅ `FINAL2-Run.log`：`ensure=0` | **✅ 两半都在盘上** |
| **③** 动态边界钳零 | ⚠️ **不在盘上** | ✅ `FINAL2-Run.log`：`L4 [PASS]`（`Mana 100.000→75.000`） | **❌ 只有"修后"一半**（见下） |

**★ ① 的"修前"半是事后补取的（受控临时回归，过程留痕 MUST 读）**：
初次实施时我只在**会话里**读了 `hasattr=False`，**没落盘**。为把这一半冻存，本轮做了一次**受控临时回归**：

| 步 | 动作 | 读数 / 结果 |
|---|---|---|
| ① | 先跑一次**当前（修后）**状态，存 `D1-AFTER-fix.log` | 三项皆 `True` |
| ② | 把该行临时改回裸 `USTRUCT()`，**并加显眼的 `⚠⚠⚠ 临时回归取证中` 标记**（防中断后缺陷静默留存） | — |
| ③ | **重编** | `Result: Succeeded`——**这正是本缺陷的险处：编译、C++ 面、picker 全都不报错** |
| ④ | 跑 `probe_d1.py`，存 `D1-BEFORE-fix.log` | **`has_ResourceAttr = False`**，而 `HostDelegate` / `basePolicy` **仍 `True`** |
| ⑤ | **改回 `BlueprintType` 并清除标记**；`git diff --stat` 复核 | `21 insertions(+), 1 deletion(-)`（与回归前一致 ⇒ 无残留） |
| ⑥ | **重编 + 重跑确认**（**不是"改回就算完"**），存 `D1-AFTER-fix-r2.log` | 三项皆 `True` |
| ⑦ | **双配置编译** | Editor 与 Game **均 `Result: Succeeded`** |

**④ 的读数为什么必须有对照项**：单看 `False` 分不清"**只有它没导出**"与"**整类都没导出**"——
两个对照项（`_HostDelegate` 带 `BlueprintType`、基类只带 `Hidden`）**实测仍为 `True`**
⇒ 证明**正是"缺 `BlueprintType`"这一条**导致它不可见，且顺带证实了
"**基类是被已导出子类顺带发出的**"这一判据读法（若它是自己够格，就与"只带 `Hidden` 不导出"矛盾）。

**取证脚本可独立复跑**：`Saved/McpProbe/probe_d1.py` + §7 第 5 步的配方。

**★ ③ 的"修前"半为何缺失（我的取证卫生失误，如实登记）**：
我把**三次失败的运行写进了同一个文件名** `PIE2-Run.log`——18:50 的"授予=否"、18:53 的"Mana 0.000→0.000"两次坏读数，
**被 18:57 的成功运行覆盖**了。
**⇒ 现状**：③ 的机理只有**本会话记录**可追溯，**盘上无独立可复核的"修前"证据**。
**其间接佐证（不构成独立证据）**：该轮会话里同时出现
`消耗已提交 …100.000 → 75.000（扣 25.000）` 与 `[属性变更] …100.000 → 0.000` 两句——
即"策略扣对了、随即被钳回"这一对现象。**未冻存。**
**纪律沉淀**：**失败现场 MUST 与成功读数分开命名**（如 `-RED-` / `-GREEN-` 前缀，或带轮次后缀），
**MUST NOT 复用同一日志名**——否则"修前坏"这一半会被后续成功运行静默覆盖，
而那正是全仓反复在拦的"事后无法复核"形态（同 `EVID-2026-10-11-r65-cooldown-cost` §1.1 的教训族）。
**注意 ③ 与 ① 的差别**：① 是"**当时没落盘**"（可事后受控补取，本轮已补），
③ 是"**落盘了但被自己覆盖**"——**后者更隐蔽**，因为文件看起来"有证据"。

---

## 4. ★ 一处**方法论**沉淀（本轮最有复用价值的一条）

**"三条路里只有一条断"的缺陷形态**（§3.1）：同一个类型同时被 **C++ 面 / 编辑器 picker / 脚本面** 三条路消费，而三者的元数据判据**各不相同**：
- C++ 面看类型系统；
- picker 看 `Hidden` / `AllowedClasses`（**不读 `BlueprintType`**）；
- 脚本面看 `BlueprintType`（沿父链）。

⇒ **"编辑器里能配"不能推出"脚本里也能配"**，反之亦然。**判据**：给框架加"宿主可配置"的类型时，MUST **逐条**核对它要出现的**每一条路**的判据，**不能只看在编辑器里点得出来**。

**同族的第二次命中**（§3.3）：**"动态引用的被引用方"是同一类盲区**——定义在册 ≠ 实例在册，而边界/引用是按**实例**求值的。**判据**：凡定义带**动态引用**者，装置 MUST 把被引用方的**实例**也备齐。

---

## 5. 一处 **API 事实变更**（旧脚本已失效，MUST 知）

**`unreal.GameplayTag.request_gameplay_tag` 在 UE 5.8 不存在**：引擎侧 `FGameplayTag::RequestGameplayTag` 是**普通 static 成员、没有 `UFUNCTION`**（`GameplayTagContainer.h:57`）⇒ 不生成 Python 绑定。
**⇒ 本仓既有脚本 `Saved/TcsAcceptance/Task7/author_assets.py` 用的正是它，今天已跑不动。**
**可用替代 = 空 tag 上 `import_text("A.B.C")`**（未注册时返回 `True` 但 `get_tag_name` 得 `None`、`is_gameplay_tag_valid` 为 `False` ⇒ **可判**）。

**同族的第二条：`FInstancedStruct` 的内层字段在 Python 侧写不进去**（`set_editor_property("x")` 报 `Failed to find property 'x' for attribute 'x' on 'InstancedStruct'`；`assign` / `reset` 后同样不行）。
**可用替代 = `import_text`**（`InstancedStruct` 继承 `StructBase` 的文本面）。**两类载体的文本形态不同——这是唯一的坑**：
- **具体结构体**（如策略本身）：`(Prop=Val,...)`，**不带**类型路径；
- **`FInstancedStruct`**：**必须带**类型路径前缀（`FInstancedStruct::ImportTextItem` **先把首个 token 读成 struct path**，见 `InstancedStruct.cpp`），如
  `'/Script/TcsSkill.TcsSkillCostPolicy_ResourceAttr(ResourceAttr=Attribute.Mana,Amount=(Source=/Script/TcsCore.TcsParamSource_Literal(Value=25.000000)))'`。
**★ 最危险处**：**缺类型路径时不报错、静默保留默认值 0** ⇒ 只看"没抛异常"会漏掉它。**本档的守卫** = 回读 `export_text()` 并断言**同时**含资源键与数量（见 §2.3 与资产脚本的 `[ASSET] cost_mode_has_*` 两行）。

**工具链事实（省后人的时间，四条错路已排除）**：

| 路线 | 结果 |
|---|---|
| 编辑器 GUI 的 `py` 控制台（Slate 合成键盘） | **不稳**：控制台**自动补全弹窗**会吞掉/改写逐字符输入，日志出现 `Failed to resolve the Value for Make Instanced Struct` 而**不是**我们送的命令 |
| `-run=pythonscript -script=<py>`（commandlet） | **脚本根本不执行**（commandlet 不跑游戏世界与延迟控制台命令） |
| `-game` + `-ExecCmds=py …` | **能跑游戏逻辑**，但**资产创建必崩**：`UEditorAssetLibrary::DuplicateAsset` 取 `GEditor->GetEditorSubsystem<>()`，而 `-game` 下 `GEditor` 为 **null** ⇒ `EXCEPTION_ACCESS_VIOLATION`（`SubsystemCollection.cpp:83`） |
| **`-ExecutePythonScript=<py>`** | **✅ 正解**：**完整编辑器模式**（`GEditor` 在）且**跑完自退**（`EditorPythonExecuter.cpp:125` 的 `QUIT_EDITOR`） |

**⇒ 判据**：**一切"资产创建/改名/保存"类 API 都 MUST 在编辑器模式下调**；`-game` 只适合"跑游戏逻辑的读数"。
**另注**：`DataAssetFactory` 造资产也走不通——`set_editor_property("data_asset_class", ...)` 报 `Failed to find property`（**stub 文本的 Editor Properties 列表 ≠ 运行期可写面**），换唯一可写的 `supported_class` ⇒ **同一处崩溃**。**资产创建请用 `EditorAssetLibrary.duplicate_asset(源, 目的)`**（**参数顺序是"源，目的"**，写反时把它当源的是尚不存在的目标）。

---

## 6. 边界（MUST NOT 外推）

1. **单机 / 单世界 / 单进程**；无头读数为 `UnrealEditor-Cmd -game`，PIE 读数为编辑器内 in-viewport PIE。
2. **脚本（C#）侧宿主策略仍未做往返实测**——本轮修好了**类型可达性**（`BlueprintType`，§3.1），并验到 Python 侧可构造、字段可写；但 **C# / UnrealSharp 侧的往返仍未验**。"能导出 ≠ 能往返"是本仓既有教训（台账 `SCRIPT-9` 同族）。
3. **`L1~L3` 只在 PIE 有意义**：无头下内容资产发现 0 条（`PrimaryAssetTypesToScan` 未配，归 R7 `INTEG-3`）⇒ 该三条**按设计跳过**并留 `[提示]`。**MUST NOT** 把"无头少了这三条"读成通过或读成回归。
4. **`MaxMana` 的动态边界只验到"Mana 被正确钳制"这一向**：本轮验的是"不持有 `MaxMana` ⇒ 钳到 0"（阴性，§3.3）与"持有 ⇒ 扣减生效"（阳性）。**MUST NOT** 外推为"上界语义全部验过"（超上限写入、`AVD_Wrap` 等未测）。
5. **`CostMode` 的 `BaseStruct` 收窄只保证 picker 过滤**：运行期取内层仍需 `GetPtr<T>()` 判空（`EVID-2026-10-12-cost-policy` §7 边界 4，未变）。
6. **跨单位成本仍未做**（`BeginBatch`/`Commit` 是逐单位的）；**"宿主策略在框架批内改账本"仍未做行为级覆盖**（装置策略刻意不改账本）。两条边界与 R6.5-CC 一致，未因本轮改变。
7. **三份新资产是 LAC 宿主侧内容**（`Content/TcsDev/`），**不属插件交付物**；插件侧本轮只有一处改动（`TcsCostTypes.h` 的 `BlueprintType`）。
8. **缺陷③ 的"修前"读数未冻存**（§3.4）：其机理只有本会话记录可追溯，盘上**无独立可复核的坏状态证据**。**MUST NOT** 把"③ 已修好"这一半读成"③ 的坏状态也已被独立复核"。**缺的那一半是可补的**（配方见 §7 第 5 步），但本轮**未补**。

---

## 7. 复算脚本（本机逐字可跑）

```powershell
# ── 0) 关掉编辑器与残留无头进程（否则链接 LNK1104 / DLL 被占）──
Get-Process UnrealEditor,UnrealEditor-Cmd -ErrorAction SilentlyContinue | Stop-Process -Force

# ── 1) 双配置编译 ──
$ubt='E:\UnrealEngine\UE_5.8\Engine\Binaries\DotNET\UnrealBuildTool\UnrealBuildTool.exe'
& $ubt LegendAutoChessEditor Win64 Development -Project='E:\Projects_Dev\LegendAutoChess\LegendAutoChess.uproject' -WaitMutex -NoHotReload
& $ubt LegendAutoChess         Win64 Development -Project='E:\Projects_Dev\LegendAutoChess\LegendAutoChess.uproject' -WaitMutex -NoHotReload

# ── 2) 内容资产（**编辑器模式**；`-ExecutePythonScript` 跑完自退）──
& 'E:\UnrealEngine\UE_5.8\Engine\Binaries\Win64\UnrealEditor-Cmd.exe' `
  'E:\Projects_Dev\LegendAutoChess\LegendAutoChess.uproject' `
  '-ExecutePythonScript=E:\Projects_Dev\LegendAutoChess\Saved\McpProbe\make_cost_mana_assets.py' `
  -unattended -nosound -nullrhi -noPause `
  '-ABSLOG=E:\Projects_Dev\LegendAutoChess\Saved\McpProbe\make_cost_mana_assets.log'
# 期望：cost_mode_text 含 ResourceAttr=(TagName="Attribute.Mana") 与 Value=25.000000

# ── 3) 无头三条命令（**`-game` 不自行退出 ⇒ Start-Process + Stop-Process**）──
$exe='E:\UnrealEngine\UE_5.8\Engine\Binaries\Win64\UnrealEditor-Cmd.exe'
$proj='E:\Projects_Dev\LegendAutoChess\LegendAutoChess.uproject'
$out='E:\Projects_Dev\LegendAutoChess\Saved\TcsAcceptance\R65'
foreach ($j in @(@('Tcs.Test.CooldownCost.Run','FINAL2-Run'),
                 @('Tcs.Test.CooldownCost.Reject','FINAL2-Reject'),
                 @('Tcs.Test.SkillLayer.Run','FINAL2-Regression'))) {
  $log = "$out\$($j[1]).log"
  Start-Process -FilePath $exe -ArgumentList @("`"$proj`"",'/Game/TcsDev/L_TcsDev_Slice','-game','-nullrhi',
    '-nosound','-noPause','-unattended',"-ABSLOG=`"$log`"","-ExecCmds=`"$($j[0])`"") `
    -WorkingDirectory 'E:\Projects_Dev\LegendAutoChess'
  Start-Sleep -Seconds 130
  Get-Process UnrealEditor-Cmd -ErrorAction SilentlyContinue | Stop-Process -Force
  # 判据：FAIL=0 且 ensure=0
}

# ── 4) PIE 路线（编辑器内；MCP 直连 streamable-http）──
# 详见 Saved/McpProbe/{call.ps1, drive.ps1, runheadless.ps1}
# 要点：StartPIE → 开 Output Log 抽屉（button "Output Log"）→ 快照取 Cmd 文本框 ref（tb3）
#       → FillForm 写命令 → PressKey Enter → GetLogEntries 取 SUMMARY
# **MUST NOT 用 Type 逐字符**（自动补全弹窗会吞字符）。

# ── 5) 缺陷① 的"修前/修后"两半取证（受控临时回归；配方可独立复跑）──
# 目的 = 冻存 `BlueprintType` 有无对**脚本层可见性**的影响（§3.4 那一行）。
# 探针 = Saved/McpProbe/probe_d1.py（三行：目标类 + 两个对照项）
& 'E:\Projects_Dev\LegendAutoChess\Saved\McpProbe\runpy.ps1' `
  -Script 'E:\Projects_Dev\LegendAutoChess\Saved\McpProbe\probe_d1.py' -Tag D1
Copy-Item 'Saved\McpProbe\probe_d1.log' 'Saved\McpProbe\D1-AFTER-fix.log' -Force
#   ↑ 期望：has_ResourceAttr = True（三项皆 True）

# 然后**手工**把 TcsCostTypes.h 的 `USTRUCT(BlueprintType)` 临时改回 `USTRUCT()`
#   ⚠ MUST 同时加显眼的"临时回归取证中"标记（防中断后缺陷静默留存）
#   ⚠ MUST NOT 只改不重编（元数据走 UHT，改头文件不重编则读数不变）
& $ubt LegendAutoChessEditor Win64 Development -Project='E:\Projects_Dev\LegendAutoChess\LegendAutoChess.uproject' -WaitMutex -NoHotReload
#   ↑ 期望 Result: Succeeded —— **这正是本缺陷的险处：编译不报错**
& 'E:\Projects_Dev\LegendAutoChess\Saved\McpProbe\runpy.ps1' `
  -Script 'E:\Projects_Dev\LegendAutoChess\Saved\McpProbe\probe_d1.py' -Tag D1
Copy-Item 'Saved\McpProbe\probe_d1.log' 'Saved\McpProbe\D1-BEFORE-fix.log' -Force
#   ↑ 期望：has_ResourceAttr = **False**，而两个对照项仍 **True**
#     （⇒ 证明"只有它没导出"，不是"整类没导出"）

# 改回 `USTRUCT(BlueprintType)`、清除标记，然后 **MUST 重编 + 重跑确认**（不是改回就算完）：
& $ubt LegendAutoChessEditor Win64 Development -Project='E:\Projects_Dev\LegendAutoChess\LegendAutoChess.uproject' -WaitMutex -NoHotReload
& $ubt LegendAutoChess         Win64 Development -Project='E:\Projects_Dev\LegendAutoChess\LegendAutoChess.uproject' -WaitMutex -NoHotReload
& 'E:\Projects_Dev\LegendAutoChess\Saved\McpProbe\runpy.ps1' `
  -Script 'E:\Projects_Dev\LegendAutoChess\Saved\McpProbe\probe_d1.py' -Tag D1
Copy-Item 'Saved\McpProbe\probe_d1.log' 'Saved\McpProbe\D1-AFTER-fix-r2.log' -Force
#   ↑ 期望：三项皆真；并复核 `git diff --stat -- Source/TcsSkill/Public/Def/TcsCostTypes.h`
#     应为 `21 insertions(+), 1 deletion(-)`（与回归前一致 ⇒ 无残留）

# ── 6) **失败现场 MUST 与成功读数分开命名**（§3.4 的纪律，MUST 遵守）──
# 反例（本轮已犯）：把三次失败运行都写进 `PIE2-Run.log` ⇒ 坏读数被成功运行静默覆盖。
# 正例：`-RED-` / `-GREEN-` 前缀，或带轮次后缀（如 `-r1` / `-r2`）。
```

**装置命令的既有纪律（未变）**：常规命令 `Tcs.Test.<域>` 应**零非预期红字**；拒绝面独立成 `.Reject`（自带屏显声明"红字为预期"）。本轮的 §3.2 正是这条纪律被夹具自己破坏的一次，已修。
