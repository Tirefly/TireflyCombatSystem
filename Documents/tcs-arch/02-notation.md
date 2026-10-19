# ARCH-02-notation · TcsNotation（策划记法底座）

- **文档 ID**：`ARCH-02-notation`
- **类型**：模块档案（现状 + 债务 + 未实现）
- **状态**：生效中
- **权威范围**：`TcsNotation` **今天实际落地了什么**。本模块是**词汇层**，绝大部分设计（描述视图体系 19 KB）**未实现**——这里的判定标准不是"代码好不好"，而是"**规格 19 KB 里哪 92 行是真的**"。
- **最后更新**：2026-10-17（首次审核）

---

## §1 模块定位与边界

**一句话**：提供**策划语言值 ↔ 规范值**的转换词汇（值约定标志位 + 转换助手）。**零战斗语义、零运行时状态、header-only 转换。**

| 项 | 内容 |
|---|---|
| 代码规模 | **92 行**（3 `.h` + 2 `.cpp` + 1 `.Build.cs`）；**其中实质代码 ≈ 60 行** |
| 编译期依赖 | **无**（仅 `Core` + `CoreUObject`）——与 `TcsCore` **平级互不依赖** |
| 被谁依赖 | `TcsAttribute` / `TcsState` / `TcsBuff` / `TcsSkill`（**4 个**） |
| 对应可执行需求 | `openspec/specs/value-convention/`（**唯一**主能力） |
| 旧模块规格 | `combat-system-design/spec/11-module-notation.md`（**`SPEC-07-notation`，19 299 字节，标 `PENDING`，已冻结**） |

### 1.1 一句话说清"为什么这么薄"

| 事实 | 数值 |
|---|---|
| 旧规格体量 | 19 299 字节 / 约 200 行 |
| 实际代码 | 92 行（含注释与日志通道） |
| 规格里**已实现**的部分 | §2.1 的**一个枚举 + 一个函数** + §3 的写入点接线（接在**别的模块**里） |
| 规格里**未实现**的部分 | §2.2 整节（描述视图体系）、§4 四分之三、§9 大部分验收钩子 |

**⇒ 本模块是"规格最厚、代码最薄"的一个**，落差 **≈ 95%**。这不是缺陷——旧规格自己写明"功能面随内容模块轮（M3/M5/M8）落地"。真正要判的是：**这些未实现项今天还成立吗**（见 §6）。

### 1.2 一个必须先说清的边界（最容易误判）

**描述配置的两个载体（`FTcsDescriptionEntry` / `FTcsDescriptionViewSlot`）已实现，但住 `TcsState`，不在本模块。**

- 依据：`DEC-02-fold-display` 的 D5-17 v3 命名批表明确写「描述视图槽位 / 配置条目 → **TcsState（Def 形状所在）**；视图策略基类 / 内置视图 / 视图构建上下文 → **TcsNotation**」。
- 代码：`Source/TcsState/Public/Def/TcsDescriptionEntry.h`（`TCSSTATE_API`）。
- **这正是"词汇归 Notation、形状归 Def 所在模块"的分工**，不是错位。判定见 §5 D-1。

---

## §2 已落地

### 2.1 值约定标志位【实测】

```
Source/TcsNotation/Public/TcsValueConvention.h:12
UENUM(BlueprintType, meta = (Bitflags, UseEnumValuesAsMaskValuesInEditor = "true"))
enum class ETcsValueConventionFlag : uint8
```

| 标志 | 值 | 语义 | UMETA ToolTip 里的例子 |
|---|---|---|---|
| `VCF_None` | 0 | 恒等（默认） | — |
| `VCF_Percent` | `1 << 0` | 策划语言值 ÷ 100 | 写 85 → 规范 0.85 |
| `VCF_OneMinus` | `1 << 1` | 取 `1 − v` | 写 25「减少25%」→ 0.75 |
| `VCF_Negate` | `1 << 2` | 取负 | 写 1000「减少1000生命」→ −1000 |

**三个 `UMETA` 的 ToolTip 都带具体数值例子**——这是给策划看的，同一组例子在 `value-convention` 能力的需求正文里逐字重复（判据：编辑者不必读代码就能确定语义）。

### 2.2 转换助手【实测】

```
Source/TcsNotation/Public/TcsValueConvention.h:34
static FORCEINLINE double FTcsValueConvention::ConvertToCanonical(double RawValue, int32 ConventionFlags)
```

**固定组合顺序 = 语义**：`Percent（÷100）` → `OneMinus（1−v）` → `Negate（取负）`。

实现就是三次 `if (Flags & bit)` 顺序变换，**不因标志的书写顺序改变**。`VCF_None`（三标志全无）时原值直通。

**为什么必须是这个顺序【实测】**：`85` 配 `Percent|OneMinus|Negate` ⇒ `0.85 → 0.15 → −0.15`。若顺序改成 `OneMinus` 先，则 `85 → −84 → −0.84 → +0.84`——同一个配置得到**完全不同的值**。规格把它写成"顺序即语义"，`openspec` 的 Scenario 也逐字钉住了 `-0.15` 这个读数。

### 2.3 「约定可配白名单」的实际落地方式【实测·重要】

旧规格说白名单由 **M8 强制**（编辑器期）。**实际是用「源能力位」数据化实现的，比规格更好**：

| 层 | 代码锚点 | 机制 |
|---|---|---|
| 基类默认 | `Source/TcsCore/Public/Parameter/TcsParamValueSource.h:131` `AllowsValueConvention()` | **默认 `true`**（`Literal` / 表型源天然可配） |
| 禁配 ① | `Source/TcsCore/Public/Parameter/TcsParamSource_ParamRef.h:61` | 覆写 `false`——读到的**已是规范值**，再转 = 二次转换 |
| 禁配 ② | `Source/TcsAttribute/Public/Attribute/TcsParamSource_AttributeScaled.h:116` | 覆写 `false`——结果是**乘积**，约定作用面歧义 |
| 可配 ③ | `Source/TcsBuff/Public/Param/TcsParamSource_InstigatorLevel.h:95, :174` | 覆写 `true` |
| 可配 ④ | `Source/TcsBuff/Public/Param/TcsParamSource_StateLevel.h:82, :150` | 覆写 `true` |
| 转发 | `Source/TcsCore/Public/Parameter/TcsParamSourceHost.h:128`（`FTcsParamSource_HostDelegate`） | 空 Host 守卫返回 `true`（与基类默认一致），否则**转发给宿主对象的同名方法** |

**全仓 `AllowsValueConvention() const override` 的覆写者恰好 5 个文件 / 7 个方法点**，与 D5-18 v3 的白名单**逐项对应**。

**执行点两处**：
- **作者期**：`TcsAttribute/.../TcsAttrModDef.cpp:58, :66`、`TcsBuff/.../TcsBuffDefAsset.cpp:121, :124`——勾了约定但源能力位为假 ⇒ `IsDataValid` **报错**。
- **物化/快照期**：`TcsBuffModifierMaterializer.cpp:79` / `TcsBuffOps_Snapshot.cpp:72` / `TcsCastOps_Snapshot.cpp:64` / `TcsParamChain_Evaluate.cpp:121` / `TcsParamChain_Materialize.cpp:27`——**能力位为假的源不转**（`bAllowConvention` 门）。

> **判据价值**：白名单**没有中心名单、没有 `switch`、没有 M8 硬编码**——判据由源自身声明、消费方按虚分派问。这符合本仓"能力探测一律走虚分派，MUST NOT 建立『源类型 × 能力』中心名单"的既有纪律。

### 2.4 消费者分布【实测】

| 模块 | 引用处数 | 角色 |
|---|---|---|
| `TcsSkill` | **8** | 字段（`FTcsNumericParamModifier::ValueConvention`）+ 参数链两处 + 施法快照 |
| `TcsBuff` | **4** | 字段（`FTcsNumericParamRow::ValueConvention`）+ 物化 + 快照 + 作者期校验 |
| `TcsAttribute` | **4** | 字段（`UTcsAttrModDef::ValueConvention`）+ 作者期校验 |
| `TcsState` | **3** | 字段（`FTcsNumericParamRow`，与 Buff/Skill 共用同一个行类型） |
| **合计** | **19 处引用 / 4 个模块** | — |

### 2.5 基础设施

| 设施 | 代码锚点 |
|---|---|
| 日志通道 `LogTcsNotation` | `Public/TcsNotationLogChannel.h` + `Private/TcsNotationLogChannel.cpp` |
| 模块类 `FTcsNotationModule : IModuleInterface`（空） | `TcsNotationModule.h:11` |
| `Build.cs` 最小编译集 | 只 `Core` + `CoreUObject`，注释注明"与 TcsCore 平级互不依赖" |

---

## §3 对外契约

### 3.1 稳定契约

| 契约 | 说明 |
|---|---|
| `ETcsValueConventionFlag` 是**唯一**的"值书写口径"词汇 | 全仓无第二套约定枚举 |
| `ConvertToCanonical` 的**固定顺序**是语义的一部分 | `Percent → OneMinus → Negate`，MUST NOT 因标志书写序改变 |
| 转换**只发生在写入点** | 运行侧账本/快照恒为规范值；机制层（`TcsEffect` / `TcsDamage` / `TcsTargeting`）**不加 Notation 边** |
| `AllowsValueConvention()` 是"约定可配性"的**唯一判据** | 消费方 MUST NOT 硬编码"哪些源类型可配" |

### 3.2 MUST NOT 依赖

- `ConvertToCanonical` **没有副作用、无状态**——MUST NOT 期望它做校验或报错。它无条件转换，**合法性由调用点先问能力位**。
- 本模块**不认识任何参数键**（键空间归各域参数表）——MUST NOT 在这里加键注册表。

---

## §4 竖切影响

### 4.1 写入点：规格列 5 处，实测落地 4 处（第 5 处落在宿主侧，正常）

| # | 写入点 | 规格说的宿主 | 实测落地 | 状态 |
|---|---|---|---|---|
| 1 | SkillDef 参数行 Base → Entry 账本注册 | TcsSkill | `TcsSkill/Private/Skill/TcsCastOps_Snapshot.cpp:64` | ✅ |
| 2 | 状态参数行 Base → ParamSnapshot 构建 | TcsState | `TcsBuff/Private/State/TcsBuffOps_Snapshot.cpp:72` | ✅（实现住 `TcsBuff`） |
| 3 | 修正器模板默认 Literal → 物化 | TcsState / TcsSkill | `TcsBuff/Private/State/TcsBuffModifierMaterializer.cpp:79` + `TcsSkill/.../TcsParamChain_Materialize.cpp:27` | ✅ |
| 4 | 宿主装备 / 命令式 `ApplyParamModifiers` | **宿主配置域** | 插件内**无代码**（正确——账本不做二次猜测） | ✅（按设计不入插件） |
| 5 | 参数链修正行 Operand | TcsSkill | `TcsSkill/Private/Skill/TcsParamChain_Evaluate.cpp:121` | ✅ |

### 4.2 三条跨模块影响细节

1. **`TcsBuff` 是规格未登记的消费者**【实测】。旧规格 §1 的消费者列表是「TcsSkill / TcsState / TcsAttribute / TcsEditor / 宿主 UI」——**没有 `TcsBuff`**，因为 `TcsBuff` 是 `PLN-REFACTOR-1` Task 4 才拆出的模块。⇒ 消费者从 3 个变 4 个，而且 `TcsBuff` 是**用得最多**的一个（4 个文件，含唯一的"模板物化转换"实例）。

2. **"约定不改变源求值"是硬边界**：能力位只决定**转不转**，不是"转多少"。`FTcsParamSource_Literal` 的注释明写"无约定列时即规范值（恒等）；配约定列时由内容模块在写入点转换"——**转的是内容模块，不是源**。若哪天有源自己转，就会出现双重转换。

3. **`TcsState` 与 `TcsBuff` 共用同一个参数行类型**（`Source/TcsState/Public/Def/TcsParamRow.h`），只带**一个** `ValueConvention` 字段 ⇒ 约定是**行级**口径，不是参数级。同一参数的"Base 值"与"修正器 Operand"各自可配约定。

### 4.3 与 `TcsCore` 的解耦机制（跨模块核心约束）

本模块**不依赖** `TcsCore`，而"值源策略"（`FTcsParamValueSource`）住 `TcsCore`。二者靠**能力位虚分派**解耦：

```
TcsNotation（词汇）        TcsCore（值源策略）
  ETcsValueConventionFlag    FTcsParamValueSource
  ConvertToCanonical   ◄──── AllowsValueConvention()  ← 源自己声明
```

⇒ `TcsNotation` **完全不认识** `FTcsParamValueSource`；内容模块（`TcsBuff` / `TcsSkill`）是唯一同时认识两者的一方，由它接线。这是"零反向依赖"的机制保证。

---

## §5 技术债

### D-1 `openspec` 的 `state-def-asset` 对载体归属的说法与代码矛盾【实测·**规格缺陷在最高权威层**】

| 项 | 内容 |
|---|---|
| **规格声称** | `openspec/specs/state-def-asset/spec.md:71`「`TcsBuff` MUST 提供描述配置的两个纯数据载体（**住 `TcsBuff`**：…该句是**当时**的模块名，自 Task 4 起 Def 形状住 `TcsBuff`）」 |
| **实际** | 两个载体住 **`TcsState`**：`Source/TcsState/Public/Def/TcsDescriptionEntry.h`，导出宏 `TCSSTATE_API` |
| **判定谁对** | **代码对，规格错**。判定依据是**架构必然性**：`Descriptions` 字段挂在 `FTcsStateDefBase`（`Source/TcsState/Public/Def/TcsStateDefBase.h:177`，`TCSSTATE_API`），而 `FTcsSkillDefData : FTcsStateDefBase` 也要白拿描述。若载体真住 `TcsBuff`，则 `TcsSkill` 必须依赖 `TcsBuff` —— 但 `TcsSkill` 的 `Build.cs` **明确不依赖 `TcsBuff`**（只 Core / Notation / Attribute / Effect / State）。⇒ 规格的说法**不可实现** |
| **根因** | `PLN-REFACTOR-1` Task 4 拆分时，**Buff 专属字段**搬去 `TcsBuff`，但**共用 Def 基类**留在 `TcsState`。规格把"Bug 专属 Def"与"共用 Def 基类"混为一谈，于是把"Def 形状住 TcsBuff"错误外推到基类载体上 |
| **影响面** | 读者会以为该类型在 `TcsBuff` 里，去找会扑空；且会误判 `TcsSkill` 依赖 `TcsBuff` |
| **建议** | 该句应改为「住 `TcsState`（`FTcsStateDefBase` 所在模块）」。**`openspec` 是最高权威层，这条必须修**——否则契约层持续记录一条假事实 |

### D-2 旧规格的消费者列表缺 `TcsBuff`【实测】

| 项 | 内容 |
|---|---|
| 规格声称 | §1：「消费者：TcsSkill、TcsState、TcsAttribute；TcsEditor（占位符扫描，M8）；宿主 UI」 |
| 实际 | `TcsSkill` / `TcsBuff` / `TcsAttribute` / `TcsState` **4 个模块、19 处引用**；`TcsBuff` 占 4 处且含唯一的「模板物化转换」实例 |
| 根因 | 规格写于 `TcsBuff` 拆出之前（`PLN-REFACTOR-1` Task 4，2026-10-14） |
| 影响面 | 误判"改 `TcsNotation` 只需回归 3 个模块" |

### D-3 旧规格的 `PENDING` 状态**低估**了落地度（与 D-1 相反方向）【实测】

| 项 | 内容 |
|---|---|
| 规格声称 | 状态 `PENDING`（"内容已定稿，但代码/资产未实现"） |
| 实际 | **已实现的比"PENDING"暗示的多**：①值约定词汇 + 转换助手（§2.1 全部）；②白名单（规格说归 M8 强制，**实际已用能力位数据化落地**）；③描述载体两类型（规格 §2.2 声称）；④描述作者期校验（三条规则，`TcsBuffDefAsset.cpp:42-90` + `TcsSkillDef.cpp:126-140`） |
| 影响面 | `PENDING` 会让读者以为"这个模块什么都没做"，从而**重复实现**已有内容 |
| 正确定性 | 应为**部分落地**——按本仓状态词表，更准确的是「`ACTIVE`（值约定已落地）+ 描述视图体系 `PENDING`（归 R8）」。**一个模块两个状态**，这正是旧五态词表不够用的地方 |

---

## §6 未实现

> 全部属"设计已定、代码不存在"。**本模块的未实现项占旧规格 95%**，故逐条判"今天还成立吗"。

### U-1 描述视图策略族（`FTcsParamView` + 4 个内置视图）【实测：不存在】

| 项 | 内容 |
|---|---|
| 规格声称 | `FTcsParamView` 策略基类（`BuildText` / `IsCompatible`）+ `FTcsParamView_Value` / `_Series` / `_Range` / `_Attribute` |
| 实际 | `FTcsParamView` 在代码里**只出现在注释**（`TcsState/Public/Def/TcsDescriptionEntry.h:16, :53` 与 `TcsCore/Public/Parameter/TcsParamEnumerableSource.h:24`），零声明 |
| 前置依赖 | `FTcsParamView_Series` 需要 `FTcsParamEnumerableSource` 的 `Enumerate`（也**未建**，见 `ARCH-01-core` §6 U-4） |
| **设计是否还成立** | **成立**。而且当前代码为它**留了正确的口**：`FTcsDescriptionViewSlot::View` 是**普通 `FInstancedStruct`、不带 `meta=(BaseStruct=…)`**，注释明写"基类落地时回补 `meta`，类型限制届时由它收紧；基类不存在时 MUST NOT 用 Kind 枚举之类的替代物假装有类型约束" |
| **这条的判据价值** | 这是本仓**"零消费者不预建"的正面样本**：没有用"Kind 枚举 + 载荷"的旧形态过渡（那会留下待清理的临时形态），而是**空着并写明为什么空着** |

### U-2 视图数据面契约（`FTcsViewBuildContext` / `FTcsViewProbe`）【实测：不存在】

| 项 | 内容 |
|---|---|
| 规格声称 | `FTcsViewBuildContext`：瞬时、非反射、不序列化的查询面（`GetValue` / `TryGetSeries` / `EvaluateAtLevel` / `TryGetConvention`）；`FTcsViewProbe`：纯数据能力探针（`bEnumerable` 等） |
| 实际 | 零命中 |
| **设计是否还成立** | **成立**。关键约束仍在：两者**都不含任何 `TcsCore` 类型**（保证 Notation 零反向依赖）——这一点没变 |

### U-3 组装器 `BuildDescription`【实测：不存在】

| 项 | 内容 |
|---|---|
| 规格声称 | `BuildDescription(const FTcsDescriptionEntry&, const FTcsViewBuildContext&) -> FText`；按槽名逐个 `View.BuildText(Ctx)` 产出富文本片段、`FText::Format` 命名参数替换；**不查表、不认识源类型、不认识参数键、零语法解析** |
| 实际 | 零命中 |
| **设计是否还成立** | **成立**，且它的"零语法"前提已经落地——文案里只有槽名（`FName`），机器语义全在 `FTcsDescriptionEntry` 的结构化字段里 |

### U-4 `DecomposeFromCanonical`（UI 反变换）【实测：不存在】

| 项 | 内容 |
|---|---|
| 规格声称 | `DecomposeFromCanonical(Canonical, Flags)`——正向的**逆序**执行（`Negate → OneMinus → ×100`），供 UI 把 `0.75` 显示回 `"25"` + % 样式 |
| 实际 | 零命中 |
| **设计是否还成立** | **成立**。旧规格把"为什么必须逆序"写清了（正向是 `Percent→OneMinus→Negate`），**这是全模块第二重要的语义**，不能随手按正序写 |
| 检查点 | 落地时 MUST 逐项验往返：`Decompose(ConvertToCanonical(v, F), F) == v`（规格明写"与逻辑同源往返，杜绝第二份真相"） |

### U-5 占位符扫描接口（M8）【实测：不存在】

| 项 | 内容 |
|---|---|
| 规格声称 | 槽名交叉校验（文本 `{槽名}` ∈ Views；孤儿视图 = 警告），**零语法解析** |
| 实际 | 零命中（`TcsEditor` 模块本身也不存在——见 `ARCH-INDEX` §3.2） |
| **设计是否还成立** | **成立**，但它依赖一个**尚不存在的宿主**（编辑器模块）。⇒ 实际前置 = `SPEC-09-editor` 模块立项 |

### U-6 编辑器预警四层【实测：第 1、2 层部分落地，第 3、4 层未落】

| 层 | 规格 | 实际 |
|---|---|---|
| ① 预防（Views 类型 picker + 源能力徽章） | 编辑器 Details 定制 | ❌ 未做（无编辑器模块） |
| ② 拦截（`IsDataValid`：`ParamKey` ∈ 键集 / `IsCompatible` / SlotName 重复或为空） | 主防线 | **⚠️ 部分**：`DescriptionId` 空 / `TextKey` 空 / `SlotName` 空或重复 **四条已实现**（`TcsBuffDefAsset.cpp:42-90`）；`ParamKey` 与 `IsCompatible` 依赖视图族（U-1）⇒ 未做 |
| ③ 文案交叉扫描 | 缺 = 错误、孤儿 = 警告 | ❌ 未做（依赖 U-5） |
| ④ 运行期只降级不报错 | 不变式 | ❌ 无运行期代码 |

**规格自己把 ② 定为"主防线"，而它是四层里**唯一部分落地**的一层**——这个顺序是合理的（先有数据校验，再有编辑器体验）。

---

## §7 文档资产对照

| 资产 | 位置 | 时效判定 |
|---|---|---|
| `SPEC-07-notation` | `combat-system-design/spec/11-module-notation.md` | ⚠️ **状态过粗**（D-3）、**消费者列表缺 TcsBuff**（D-2）；§2.2 视图体系**设计仍有效**但全是未来 |
| `value-convention` | `openspec/specs/value-convention/` | ✅ **完全准确**——只写已落地的两件事（标志位 + 转换助手），**准确反映了"实际只有这 92 行"** |
| `state-def-asset` | `openspec/specs/state-def-asset/` | ❌ **载体归属说法与代码矛盾**（D-1，见 §5） |
| `DEC-02-fold-display` | `combat-system-design/decisions/dec-02-fold-display.md` | ✅ **归属划分的权威**：其命名批表 §180-183 逐项指定了"哪个类型住哪个模块"，且**代码与它一致**。这是本模块最可信的资产 |
| `param-value` | `openspec/specs/param-value/` | ✅ 与本模块**无重叠**（值源策略住 Core，分工清楚） |

---

## §8 审核记录

**方法**：全量读代码（92 行可整读）→ 读旧规格全文（19 KB）→ 读 `openspec` 三个相关能力 → 对规格声称的每个类型做 `grep` 存在性判定 → 统计消费者 → 追 `DEC-02` 的归属裁定。

| 核对项 | 结果 | 置信度 |
|---|---|---|
| 代码实质内容 | **1 枚举（4 值）+ 1 静态函数** | 【实测】 |
| `DecomposeFromCanonical` | **不存在** | 【实测】 |
| `FTcsParamView` 及 4 内置视图 | **只存在于注释，零声明** | 【实测】 |
| `FTcsViewBuildContext` / `FTcsViewProbe` | **不存在** | 【实测】 |
| `BuildDescription` | **不存在** | 【实测】 |
| `ETcsValueConventionFlag` | **存在，Bitflags + 掩码编辑** | 【实测】 |
| `ConvertToCanonical` | **存在，FORCEINLINE，固定序** | 【实测】 |
| 白名单机制 | **存在，5 个覆写者，与 D5-18 v3 逐项对应** | 【实测】 |
| `FTcsDescriptionEntry` / `ViewSlot` | **存在，但住 `TcsState`** | 【实测】 |
| 描述作者期校验 | **存在 4 条规则**（Buff + Skill 各一套） | 【实测】 |
| 消费者 | **4 模块 / 19 处**（旧规格列 3 模块） | 【实测】 |
| `DEC-02` 归属表与代码一致性 | **一致** | 【实测】 |

**未覆盖项（如实声明）**：
- 未验证 `KeepTextAlive` 类的 `FText` 生命周期问题（视图族未实现，无代码可审）。
- 未审 `TcsBuffDefAsset.cpp` / `TcsSkillDef.cpp` 里描述校验的**全部规则**，只核了 `DescriptionId` / `TextKey` / `SlotName` 三类。
- 本模块**无任何 PIE 证据**（值约定是纯函数，行为面可由单元级断言覆盖，但本仓无此装置）。

---

## §9 结论

**`TcsNotation` 是"薄得合理"的模块，不是"没做完"的模块。**

- **已落地部分质量高**：唯一一个真正需要判断的设计（约定可配白名单）**用源能力位数据化实现**，比规格写的"M8 强制"更好——没有中心名单、没有 `switch`，符合全仓虚分派纪律。
- **未实现部分设计仍全部成立**：§6 六项里**没有一项需要推翻**。这与 `TcsCore` 相反（那里 U-1/U-3 我建议撤销）——本模块的留白都是"等真实消费者"，而且**留白手法正确**（`FInstancedStruct` 裸持 + 注释写明"基类落地时回补 `meta`"，没有用临时形态过渡）。
- **唯一的硬问题不在本模块，而在 `openspec`**：`state-def-asset` 声称载体住 `TcsBuff`，而代码住 `TcsState`，且按依赖图**前者不可实现**（`TcsSkill` 不依赖 `TcsBuff`）。这是**最高权威层的一条假事实**，建议尽快修。

**给下一个模块（`TcsAssetButler`）的三个关注点**：
1. 它 1 058 行、**无模块规格**（只有 `DEC-2026-10-13-asset-butler`）——首次成篇，边界要自己判。
2. 它**无编译期依赖**却被 `TcsEffect` / `TcsBuff` / `TcsIntegration` 依赖——判它是不是"纯工具层"。
3. `ARCH-01` §6 提到的资产查询口（`FTcsPrimaryAssetQuery::GetAssetsForDefClass`，修 `REFACTOR-1` Task 6 那个"类路径索引静默返 0"的回归）很可能住这里——核实。
