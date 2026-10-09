// Copyright Tirefly. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "GameplayTagContainer.h"
#include "Parameter/TcsParamValue.h"
#include "Handle/TcsSourceHandle.h"
#include "TcsValueConvention.h"

// `ETcsAttributeOp`（封闭五带）住 M2——**M5 复用同一套带序**，MUST NOT 另立同义枚举（D5-5 v3）
#include "Attribute/TcsAttrModInstance.h"

#include "TcsNumericParamModifier.generated.h"



/**
 * 参数修正行 —— **一步运算**（"改哪个参数、怎么改、改成多少"）。
 *
 * **它是给谁用的**：技能资产上的「参数链行」表，一行填一步。多行拼起来就是一条算式。
 * 例：想做 **"攻击力 × 技能倍率"**，就写两行 —— 第 1 行「加 · 属性换算(攻击力)」、
 * 第 2 行「乘 · 参数引用(技能倍率)」。**行的先后顺序不影响结果**。
 *
 * **三列怎么填**：
 * - `ParamKey`（改哪个）：本技能参数表里的键名。填 `Level` 是特例 —— 表示"改这个技能的生效等级"。
 * - `Op`（怎么改）：`加` / `百分比加` / `乘` / `平加` / `覆盖`。合成顺序固定为
 *   `((初值 + Σ加) × (1 + Σ百分比加)) × Π乘 + Σ平加`；`覆盖` 会整个替换掉算式结果。
 * - `Operand`（值多少）：`字面量` 直接写数；`参数引用` 取本技能另一个参数键的值；
 *   `属性换算` 取单位某属性的当前值 × 系数；`等级表` 按等级取档。
 *
 * **与属性修正器的关系**：同**一套**五个运算带、同一套折叠公式（故直觉一致）。
 * 差别只在"落在哪"：属性修正器改单位的属性，本行改技能的参数。
 *
 * ---
 *
 * **以下为实现与约束（策划可略）**：
 *
 * 技能参数修正器（**定义侧**，D2-13 双形状的定义面 / D5-5 v3 五带）：
 * 一行"在某参数键上施加某带某值"的声明，`Operand` 是全插件统一数值载体 `FTcsParamValue`
 * （Literal / ParamRef / 等级表 / 属性换算等源）⇒ 它**可以**持有对象引用。
 *
 * **为什么与账本侧分成两个形状（判据，不是偏好）**：账本条目住
 * `TArray<FTcsLearnedSkillEntry>`——**非 `UPROPERTY` 容器、元素还是非反射 struct** ⇒ GC 看不见它。
 * 若账本侧持本形状（`Operand` 是 `FTcsParamValue`，可装持 `TScriptInterface` 的源），那些对象会被
 * **静默回收**（症状 = 参数链里的源变成空引用，**不崩溃**）。先例 = M2 的
 * `FTcsAttrModOperandDef`（定义侧，持 `FTcsParamValue`）/ `FTcsAttrModOperand`（账本侧，`double Literal`），
 * 其注释原文即「账本只为聚合热路径服务，不进反射面」「Literal 恒为已解析规范值（物化器单点转换保证）」。
 * ⇒ 两形状的分工是**值约定与求值的收口**：本形状说"值从哪来"，账本侧说"值是多少"。
 *
 * **★ 本形状 MUST NOT 携带 `Source`（2026-10-09 用户质疑后订正，MUST NOT 加回）**：
 * 初版曾在定义侧放一个 `FTcsSourceHandle Source`，那是**假字段**——三条判据：
 * ① **它不是作者可配置的、也不会被序列化**：`FTcsSourceHandle` 是**非反射纯 C++ struct**
 *    （`TcsCore/Handle/TcsSourceHandle.h:20` 是裸 `struct`，无 `USTRUCT`）⇒ **不能作 `UPROPERTY`**
 *    ⇒ 它**不进细节面板、不进资产**（UHT 产物实测：本类型的反射属性只有
 *    `ParamKey / Op / Operand / ValueConvention / SortKey / OverridePriority / CompeteGroup`，
 *    无 `Source`）。一个 Def 类型上"配不了也存不下"的字段只会误导后人。
 * ② **定义侧路径根本不读它**：`MaterializeParamChainRows` 盖的是 **`FTcsCastRun.RunSource`**
 *    （"这一次施法"的来源），不是行上的值。
 * ③ **"源"从来不是"这一行"的属性**：同一个 `ParamChainRows[0]`，甲玩家靠装备给、乙玩家靠天赋给
 *    ⇒ **来源因实例而异**，属**实例**而非 Def。M2 正是这么做的——`FTcsAttrModDefTableRow`
 *    （定义侧行）**没有 Source 字段**，Source 是 `MakeFromDef(Row, Operand, InSource)` 的**形参**。
 * ⇒ **Source 只住账本侧**（`FTcsNumericParamModInstance::Source`，级联摘除锚点），
 * 由**施加方**给出：定义侧物化传 `RunSource`，外部施加由 `ApplyParamModifiers` 的 `Source` 形参传入。
 */
USTRUCT(BlueprintType)
struct TCSSKILL_API FTcsNumericParamModifier
{
	GENERATED_BODY()

// 目标键
#pragma region Key

public:
	/**
	 * **改哪个参数**。
	 *
	 * 填**本技能参数表里已配过的一个键**（即本技能 `Params` 表里的 `Key`）。若这里填的键
	 * 在本技能参数表里不存在，本行**依然会被应用**（参数键也允许"只由本表创建"），
	 * 但更常见的做法是先参数表里配初值、再在这里改它。
	 *
	 * **特例：填 `Level`** ⇒ 这一行不是改参数，而是**改这个技能的生效等级**
	 * （"某天赋让技能等级 +2"这类需求）。该键由引擎内置，**策划无需在任何地方声明它**。
	 * 等级只影响"按等级取档"的数值来源，改它不会改参数表读数。
	 *
	 * ---
	 *
	 * **以下为实现（策划可略）**：被修正的参数键（`TcsStateParam` 根——"State" 取广义、
	 * 含技能激活运行态）。**为什么 `Level` 键的声明方不是本模块**：`LevelBase` / `MaxLevel`
	 * 声明在状态侧 `FTcsStateDefBase`（技能 Def 继承它），且 `TcsState` 是依赖上游 ⇒
	 * 声明在上游，两侧消费都零成本、不成环（`Param/TcsStateParamKeys.h` 的 `Tag_TcsStateParam_Level`）。
	 */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Numeric Param Modifier")
	FGameplayTag ParamKey;

#pragma endregion


// 运算
#pragma region Op

public:
	/**
	 * **怎么改**（运算带）。五个选项与属性的修正器**完全同一套**，故两边直觉一致：
	 *
	 * | 选项 | 含义 | 在算式里的位置 |
	 * |---|---|---|
	 * | `加` | 加法累加 | `初值 + Σ加` 的第一步 |
	 * | `百分比加` | 按整体缩放 | 再 `× (1 + Σ百分比加)` |
	 * | `乘` | 连乘 | 再 `× Π乘` |
	 * | `平加` | 最后平坦加 | 最后 `+ Σ平加`（**不受**百分比与乘法影响） |
	 * | `覆盖` | 整个算式被它替换 | 结果直接 = 它的值（其余带全部作废） |
	 *
	 * **书写顺序无关**：上表顺序是引擎固定的合成顺序，所以先写"乘"还是先写"加"结果都一样。
	 *
	 * **`覆盖` 配「覆盖优先级」**：同一个参数上有多条 `覆盖` 时，按「覆盖优先级」大者胜；
	 * 优先级打平才比数值大小。只想"谁最后配谁生效"就别用 `覆盖`，用 `加`/`乘` 即可。
	 */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Numeric Param Modifier")
	ETcsAttributeOp Op = ETcsAttributeOp::TAO_Add;

	/**
	 * **值是多少 / 值从哪来**。
	 *
	 * | 来源 | 含义 | 适合 |
	 * |---|---|---|
	 * | `字面量` | 直接写一个数 | 固定加成、固定倍率 |
	 * | `参数引用` | 取**本技能参数表**里另一个键的值 | "倍率"这种可被别处改动的中间值 |
	 * | `属性换算` | 取单位某属性当前值 × 系数 | "攻击力 × 1.2"里的攻击力 |
	 * | `等级表` | 按当前等级取档 | 每级递增的数值 |
	 *
	 * **注意（当前能力边界）**：`属性换算` 这一来源**今天全仓无实现者**，配了它只会落兜底值
	 * （不会报错）。本轮实测可用的是 `字面量` / `参数引用` / `等级表`。
	 *
	 * ---
	 *
	 * **以下为实现（策划可略）**：运算数（定义侧形状——物化时求值一次并转规范值，
	 * 之后进账本为已解析值）。
	 */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Numeric Param Modifier")
	FTcsParamValue Operand;

	/**
	 * **这行的数按什么约定书写**（本行**自己写的**数，不含引用来的）。
	 *
	 * 最常用的是 `百分比`：你想写"减少 25%"，就填 `25` + 勾 `百分比` ⇒ 引擎存成 `0.25`，
	 * 不必自己换算成小数。`一减` / `取负` 供"减少类"配置使用。默认 `无` = 写多少就是多少。
	 *
	 * **只在"本行自己写的数"上生效**：`参数引用` / `属性换算` 取来的值**不会**再被转换一次
	 * （防二次换算），这几个来源也不提供该列的可配性。
	 *
	 * ---
	 *
	 * **以下为实现（策划可略）**：D5-18 v3 —— 作用域 = 本行自己书写的数值，物化边界经
	 * `FTcsValueConvention::ConvertToCanonical` 转规范值**一次**；可配性由数值来源自身声明
	 * （`AllowsValueConvention()` 能力位），能力位为假的源 MUST NOT 转换。
	 */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Numeric Param Modifier",
		Meta = (Bitmask, BitmaskEnum = "/Script/TcsNotation.ETcsValueConventionFlag"))
	ETcsValueConventionFlag ValueConvention = ETcsValueConventionFlag::VCF_None;

#pragma endregion


// 归属与排座次
#pragma region Origin

public:
	/**
	 * 同带内展示/审计位（折叠按 Op 分桶——带序唯一真相在 Op，折叠器 MUST NOT 读本字段）
	 */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Numeric Param Modifier")
	int32 SortKey = 0;

	/**
	 * **覆盖优先级**（只在上面 `Op` 选了 `覆盖` 时才有意义）。
	 *
	 * 同一个参数上挂多条 `覆盖` 时，**这个数大的那条胜出**，其余的整个作废。数字小的不会"叠加"、
	 * 也不会"排队等候" —— 游戏里改一下、下帧就按新的算。典型用法：普通装备给 `覆盖 0`、
	 * 特殊天赋给 `覆盖 10` ⇒ 天赋在时天赋说了算，天赋掉了装备的自动接管（**不需要任何唤醒动作**）。
	 *
	 * ---
	 *
	 * **以下为实现（策划可略）**：Override 带的强弱排座次（**仅 `Op == TAO_Override` 有意义**，
	 * 物化时原样进账本）：同键多条 Override 时**大者胜**；优先级打平才落到折叠器的同优先级策略
	 * 比较数值。**MUST NOT 与 `SortKey` 合并**（两者是两个独立字段、各归其位）：折叠器只读本字段
	 * （`TcsAttributeBandFold.h` 的三级比较），`SortKey` 明文"不参与折叠"。合并会让 M5 的 Override
	 * 胜负判据与 M2 **不同**——那就违反了"与 AttributeModifier 同样的聚合流程"。
	 */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Numeric Param Modifier",
		Meta = (EditCondition = "Op == ETcsAttributeOp::TAO_Override", EditConditionHides))
	int32 OverridePriority = 0;

	/**
	 * **竞争组**（可留空；留空 = 不参与竞争，本行照常生效）。
	 *
	 * 用途：让**同一个参数上的几条修正互相"选一个"**，而不是全部叠加。给它们填**同一个组名**
	 * （任意键），组内**最终数值最大的那条**才生效。
	 *
	 * 典型用法 = 多个 buff 都给"移速加成"，希望**只有最强那个生效**（而不是八个 buff 叠加成超人）：
	 * 给这八条填同一个竞争组名。某个 buff 掉了，**第二强的自动顶上**，不需要额外配置。
	 *
	 * ---
	 *
	 * **以下为实现（策划可略）**：竞争组（D5-19 读侧竞争组）：同组在**求值时**按账本全量重算、
	 * 只有组内解析值最大者进折叠。**无休眠池**：竞争是求值时的重算 ⇒ 最大者撤销后次大者
	 * **自动递补**，不需要任何"唤醒"调用。无效 tag = 不参与分组（该条单独进折叠）。
	 */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Numeric Param Modifier")
	FGameplayTag CompeteGroup;

#pragma endregion
};



/**
 * **形状预留（`R6.5-e`，只记约束、不建字段——MUST NOT 现在就建空数组）**
 *
 * 布尔修正器（设计名 `FBoolSwitchModifier{SwitchKey, Value, Source}`，D5-5 v2 命名统一，原
 * `FLogicGateModifier`）将来落账本时，其条目集 MUST 是**并列的第二个数组**（`TArray<FTcsBoolSwitchModInstance> BoolSwitchModInstances`），
 * **MUST NOT 挤进 `NumericParamModInstances` 同一数组**。
 *
 * **判据（代数不同，不是风格偏好）**：`NumericParamModInstances` 的五个带（`Add`/`Override`/`PercentAdd`/`Mul`/
 * `FlatAdd`）**同属一个代数**——它们有共同的折叠公式与可比较的数值，故 M2 的单一
 * `AttrModInstances` 形状成立。布尔**没有** Σ/Π 代数、无可比大小、无值约定 ⇒ 与数值**不同代数**；
 * 混装会让折叠必须**按元素类型分派**（M2 无此概念），并使"一个键上数值与布尔并存"的语义无法定义。
 *
 * **为什么现在不建**：本轮 `R6.5-e` 未开工，布尔条目集没有任何写入者与读取者 ⇒
 * 建一个空数组正是"零消费者预建"。本条只把**形状判据**钉在注释与规格里，防止将来顺手合并。
 *
 * **★ 布尔侧的命名（2026-10-09 订正：本注初稿写错过，MUST NOT 照抄旧稿）**：账本侧类型应是
 * **`FTcsBoolSwitchModInstance`**，字段应是 **`BoolSwitchModInstances`**——规则有两条，两条都要满足：
 * ① **`[表词] + Mod + 面后缀`**，两张表各有表词：数值侧 = `NumericParam`
 * （`FTcsNumericParamRow` / `FTcsNumericParamModifier` / `FTcsNumericParamModInstance`），
 * 布尔侧 = `BoolSwitch`（`FTcsBoolSwitchRow` / `FTcsBoolSwitchModifier` / `FTcsBoolSwitchModInstance`）；
 * ② **容器字段名 = 元素类型名的复数形式**（`FTcsBoolSwitchModInstance` → `BoolSwitchModInstances`）。
 * **本注初稿曾写 `FTcsBoolSwitchParamInstance`，那是错的**：它把布尔表词与数值表词 `Param` 混装，
 * 两头都不沾（`FTcsBoolSwitchRow` 本身也不带 `Param`——设计管它叫"布尔**开关**表"）。
 */



/**
 * 技能参数修正器实例（**账本侧**，D2-13 双形状的账本面）：纯 C++ struct——账本只为聚合热路径服务，
 * 不进反射面。
 *
 * **`ResolvedValue` 恒为已解析规范值**（物化器单点转换保证，账本不做二次猜测——零膨胀）：
 * 求值发生在物化边界（`Operand` 经 `FTcsParamEvaluateContext` 求值 + 值约定转一次），
 * 此后一切读取（折叠 / 竞争组比较）只读本字段，**MUST NOT 在读路径重新求值参数源**
 * （那会让读数值漂移，并绕开"值约定只转一次"的单一收口）。
 *
 * **零对象引用**是本形状的存在理由与判据（见定义侧注释）：本结构体**无任何** `FInstancedStruct` /
 * `TScriptInterface` / 对象指针成员 ⇒ `FTcsLearnedSkillEntry` 满足"账本 MUST NOT 持 `UObject` 引用"
 * 的既有纪律，门面 `AddReferencedObjects` **无需**为账本新增遍历。
 *
 * **导出宏纪律**：全内联值类型不加模块导出宏（加了会在消费方 `LNK2019`——见 `TcsSourceHandle.h` 注记）。
 */
struct FTcsNumericParamModInstance
{
	// 被修正的参数键（与定义侧同键空间）
	FGameplayTag ParamKey;

	// 运算带（带序唯一真相；SortKey 不参与折叠）
	ETcsAttributeOp Op = ETcsAttributeOp::TAO_Add;

	// 已解析的规范值（物化器求值 + 值约定转换的唯一产物）
	double ResolvedValue = 0.0;

	// 归属来源（级联撤销锚点；本模块的摘除按它全量命中）
	FTcsSourceHandle Source;

	// Override 带的强弱排座次（**仅 `TAO_Override` 读**，其余带忽略；打平按折叠器策略比数值）
	int32 OverridePriority = 0;

	// 同带内展示/审计位（折叠器 MUST NOT 读它）
	int32 SortKey = 0;

	// 竞争组（无效 = 不分组；组内只有解析值最大者进折叠）
	FGameplayTag CompeteGroup;

	/**
	 * 显式构造入口（**"定义侧行 → 账本侧条目"的字段映射唯一声明处**——同 M2
	 * `FTcsAttrModInstance::MakeFromDef` 的分工）。
	 *
	 * 为什么要有它：把字段映射留在这里而不是散在物化器里，是因为**两个形状都归 TcsSkill**——
	 * 物化器只该负责"求值 + 转规范值"，不该逐字段知道账本条目需要哪些位。
	 *
	 * **`InSource` 是形参而不是定义侧字段**（见本文件定义侧的订正说明）：来源**因实例而异**，
	 * 属账本面。两个调用点各传各的：
	 * - 定义侧物化（`MaterializeParamChainRows`）⇒ 传 **`FTcsCastRun.RunSource`**（这一次施法）；
	 * - 外部施加（`ApplyParamModifiers`）⇒ 传**该入口的 `Source` 形参**（调用方声明"这次是谁给的"）。
	 *
	 * @param Row 定义侧行（字段形状的唯一声明处）。
	 * @param InResolvedValue 已解析的规范值（本函数**不做**求值、**不做**值约定转换——那是物化边界的事）。
	 * @param InSource 归属来源（级联撤销锚点；由**施加方**给出，不来自定义侧行）。
	 * @return 返回装配好的账本条目。
	 */
	static FTcsNumericParamModInstance MakeFromModifier(
		const FTcsNumericParamModifier& Row,
		double InResolvedValue,
		FTcsSourceHandle InSource)
	{
		FTcsNumericParamModInstance Instance;
		Instance.ParamKey = Row.ParamKey;
		Instance.Op = Row.Op;
		Instance.ResolvedValue = InResolvedValue;
		Instance.Source = InSource;
		Instance.OverridePriority = Row.OverridePriority;
		Instance.SortKey = Row.SortKey;
		Instance.CompeteGroup = Row.CompeteGroup;
		return Instance;
	}
};
