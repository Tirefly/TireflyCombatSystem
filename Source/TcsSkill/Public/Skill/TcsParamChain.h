// Copyright Tirefly. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "GameplayTagContainer.h"

#include "Handle/TcsCombatEntityHandle.h"
#include "Handle/TcsSourceHandle.h"
#include "Parameter/TcsParamTableReader.h"

#include "Skill/TcsSkillEntrySelector.h"
#include "Skill/TcsNumericParamModifier.h"
#include "Skill/TcsSkillEntryHandle.h"
#include "Skill/TcsSkillRegistry.h"

class UTcsSkillSubsystem;
struct FTcsParamEvaluateContext;
struct FTcsSkillDefData;

/**
 * 技能参数链引擎函数（设计文档 §3.1 的既定分工）：**桶只存数据与索引，操作全在这里**。
 *
 * **它兑现用户的两条硬需求**：
 * 1. **"计算过程与 AttributeModifier 有同样的聚合流程"** —— 求值一律把条目摊平成
 *    `FTcsAttributeBandEntry` 后调**共享折叠器** `FoldTcsAttributeBands`（`STAT-1` 收束：
 *    M2 属性聚合 / TcsDamage 流程属性 / M5 参数链三处共用**同一份**五带实现）；
 * 2. **"Apply 与 Removal 准确无误"** —— 施加与摘除**成对**：凡写入 `NumericParamModInstances` 的条目
 *    MUST 带 `Source`，且该 `Source` MUST 能被 `RemoveParamModifiersBySource` 摘除
 *    （判据 = 不存在"摘不掉的条目"）。
 *
 * **全部成员为静态函数**：本类**无实例**（不持状态）——状态一律经 `Subsystem` 参数传入
 * （同 `FTcsSkillOps` / `FTcsCastOps` 的形状）。
 *
 * **导出宏是必需项**（判据 = "该符号是否被跨模块引用"）：宿主侧装置若直接调本族静态成员，
 * 不带 `TCSSKILL_API` 必 `LNK2019`（同族实测）。
 */
class TCSSKILL_API FTcsParamChainOps
{
// 施加与摘除
#pragma region ApplyRemove

public:
	/**
	 * 外部施加：把一批参数修正器物化后追加进**选择器命中的**每个条目的 `NumericParamModInstances`。
	 *
	 * **单位未注册 / 无匹配条目 ⇒ 忽略 + 日志 + 返回 false，MUST NOT ensure**
	 * （同 M2 `ApplyModifier` 的 D2-14 口径：框架允许宿主动态增删，且不做来源追溯）。
	 *
	 * **`Source` 形参 = 本次施加的来源锚点**（2026-10-09 用户质疑后订正：原稿只在定义侧行上找它）。
	 * **为什么是形参而不是"修正器行自带的来源"**：来源**因实例而异**——同一批修正器可以是装备给的、
	 * 也可以是天赋给的 ⇒ 它属于**这一次施加**，不属于那条声明。这同时让"按来源摘除"的成对性成立：
	 * 凡写入 `NumericParamModInstances` 的条目，其 `Source` 都来自本形参（或定义侧物化时的 `RunSource`）。
	 *
	 * **两个未实现档（`ESS_ByCategoryTags` / `ESS_Custom`）MUST 具名报出且零施加**（返回 false，
	 * `Warning` 里带档名）——MUST NOT 静默当 `ESS_All`、MUST NOT 静默无操作
	 * （静默会让宿主以为已施加）。前者归台账 `STAT-10`，后者归 `R6.5-g`。
	 *
	 * **物化边界单点**：每条修正器的 `Operand` 在此求值一次、按行 `ValueConvention` 转一次规范值，
	 * 写进账本侧 `ResolvedValue`；账本**不二次转换**（同 M2 物化器的职责划分）。
	 *
	 * @param Subsystem 技能门面。
	 * @param Unit 单位实体句柄。
	 * @param Selector 条目选择器（决定作用到哪些在册条目）。
	 * @param Modifiers 定义侧修正器行（元素类型与 `ParamChainRows` 相同）。
	 * @param Source 本次施加的来源锚点（级联摘除用，由调用方声明"这次是谁给的"）。
	 * @return 返回是否至少命中一个条目并施加（无命中 / 未实现档 / 单位未注册均为 false）。
	 */
	static bool ApplyParamModifiers(
		UTcsSkillSubsystem& Subsystem,
		FTcsCombatEntityHandle Unit,
		const FTcsSkillEntrySelector& Selector,
		TConstArrayView<FTcsNumericParamModifier> Modifiers,
		FTcsSourceHandle Source);

	/**
	 * 级联摘除：扫该单位**全部条目的 `NumericParamModInstances`**、摘掉 `Source` 匹配者，返回摘除条数。
	 *
	 * **`0` 条命中 = 正常路径，MUST NOT ensure**（来源可能只挂过已撤销的修正器；
	 * 同 M2 `RemoveBySource` 的措辞与口径）。
	 *
	 * **边界登记（MUST NOT 读成"已覆盖全部摘除面"）**：M2 的 `RemoveBySource` 还要扫
	 * **冻结暂存区** `FrozenAttributes`（那里躺着"来源在冻结期结束"的修正器，只扫条目集会让它
	 * 永久滞留、解冻时凭空多出数值）。**技能账本今天没有对应的冻结机制** ⇒ 本函数**只扫在册条目**。
	 * **若将来出现"技能条目冻结"，该扫描面 MUST 同批补上**。
	 *
	 * @param Subsystem 技能门面。
	 * @param Unit 单位实体句柄。
	 * @param Source 归属来源句柄（无效 = 0 命中，返回 0）。
	 * @return 返回摘除的修正器条数（跨全部条目合计）。
	 */
	static int32 RemoveParamModifiersBySource(
		UTcsSkillSubsystem& Subsystem,
		FTcsCombatEntityHandle Unit,
		FTcsSourceHandle Source);

#pragma endregion


// 物化边界
#pragma region Materialize

public:
	/**
	 * **物化边界的唯一收口**：把定义侧一行解析成"进账本的那个数"。
	 *
	 * 两件事，且只做一次：
	 * ① 求值 `Operand`（经 `FTcsParamEvaluateContext`——引用类源即从 `Ctx.ParamTable` 取键值）；
	 * ② 按该行 `ValueConvention` 经 `FTcsValueConvention::ConvertToCanonical` 转规范值——
	 *    **能力位为假的源不转**（判据由源自身 `AllowsValueConvention()` 声明，
	 *    **MUST NOT** 建"源类型 × 可配约定"的中心名单——同 `FTcsCastOps_Snapshot.cpp` 的既有写法）。
	 *
	 * 外部施加（`ApplyParamModifiers`）与定义侧物化（`MaterializeParamChainRows`）**共用本函数**，
	 * 保证"哪个值进账本"全模块只有一套规则（D5-19 的"与声明式共用同一物化器"落实在**边界函数**上）。
	 *
	 * @param Row 定义侧修正器行。
	 * @param Ctx 求值上下文（装配方决定主体、等级与参数表）。
	 * @return 返回已解析的规范值（账本侧 `ResolvedValue` 的取值）。
	 */
	static double ResolveModifierValue(const FTcsNumericParamModifier& Row, const FTcsParamEvaluateContext& Ctx);

	/**
	 * 定义侧 `ParamChainRows` 的**激活期物化**（PV-9）：逐行求值 + 转规范值后追加进该条目的
	 * `NumericParamModInstances`，且 `Source` 一律取**本次施法运行态的来源句柄**（`FTcsCastRun.RunSource`）
	 * ⇒ 随施法终结**级联摘除**，生命周期语义与基类 `ModifierRows` 同款。
	 *
	 * **为什么在这里做（而不是照搬状态侧的独立物化器类）**：状态侧的
	 * `FTcsStateModifierMaterializer::Materialize` 之所以独立，是因为它的模板行要**解析资产引用**
	 * （`TSoftObjectPtr<UTcsAttrModDef>`）。技能侧本轮**只落内联形态**
	 * （`UTcsSkillModDef` 全仓 `Source/` 零命中 ⇒ 无引用目标，模板引用路径整体归 `R6.5-f`）
	 * ⇒ 没有可解析的外部引用，独立一层只会得到"零逻辑的转发壳"。
	 *
	 * @param Subsystem 技能门面。
	 * @param Def 技能定义内容（提供 `ParamChainRows`）。
	 * @param Entry 目标账本条目（就地追加）。
	 * @param ParamTable 物化求值用的参数表（= 本次 run 的 `ParamSnapshot`，经壳绑定；
	 *        引用类操作数即从它取值）。可空——空时引用类源落各自兜底。
	 * @param RunSource 本次施法运行态的来源句柄（全部物化条目的 `Source`）。
	 * @return 返回物化出的条数（0 = 该定义未配链行）。
	 */
	static int32 MaterializeParamChainRows(
		UTcsSkillSubsystem& Subsystem,
		const FTcsSkillDefData& Def,
		FTcsLearnedSkillEntry& Entry,
		const TScriptInterface<ITcsParamTableReader>& ParamTable,
		FTcsSourceHandle RunSource);

#pragma endregion


// 求值
#pragma region Evaluate

public:
	/**
	 * 折叠某键的参数链（**需求①"与 AttributeModifier 同样的聚合流程"的落点**）。
	 *
	 * 求值流水：① 取该条目 `NumericParamModInstances` 中 `ParamKey` 匹配者 → ② `CompeteGroup` 分桶、
	 * **组内解析值最大者**进折叠（未分组者各自独立）→ ③ 摊平成
	 * `FTcsAttributeBandEntry{Op, Value = ResolvedValue, OverridePriority}` →
	 * ④ 调 **`FoldTcsAttributeBands(初值, Entries)`**（第三参取默认 `OTB_Max`——
	 * 策略在 M2 住在**属性定义**上，而技能参数**没有"属性定义"这一层**）。
	 *
	 * **初值 = 该键参数行的求值结果；该键无参数行则 0**（`9.6`）。
	 *
	 * **MUST NOT 自建第二份折叠**：`Source/TcsSkill/` 内 `FoldTcsAttributeBands` 调用**仅此一处**。
	 *
	 * **命中判据**：参数行存在**或**至少有一条同键条目 ⇒ 返回 true。两者皆无 = miss
	 * （**MUST NOT** 静默返回 0 当命中——调用方要能区分"命中且为 0"与"没配这个键"）。
	 *
	 * @param Subsystem 技能门面。
	 * @param Entry 账本条目（条目集来源）。
	 * @param Def 该条目的定义内容（参数行来源；可为空 = 视作无参数行）。
	 * @param Key 参数键。
	 * @param OutValue 输出折叠结果（miss 时内容未定义，调用方不得使用）。
	 * @return 返回是否命中。
	 */
	static bool EvaluateParamKey(
		UTcsSkillSubsystem& Subsystem,
		const FTcsLearnedSkillEntry& Entry,
		const FTcsSkillDefData* Def,
		FGameplayTag Key,
		double& OutValue);

	/**
	 * 求该条目 `Level` 键上的修正合计（`EffectiveLevel` 用）。
	 *
	 * **`Level` 是可折叠的参数键之一**：它走**同一条**五带折叠（`Add +2` ⇒ +2；
	 * `PercentAdd`/`Mul` 亦按同一公式作用在 `LevelBase` 上），再把结果累加进等级——
	 * 这与"特殊键"的本意一致：特殊之处在于**它改的是等级而不是参数表读数**，
	 * 而不在于它有一套自己的算术（那会造出第二份折叠语义）。
	 *
	 * @param Subsystem 技能门面。
	 * @param Entry 账本条目。
	 * @param Def 该条目的定义内容（提供 `LevelBase`；可为空 = 0）。
	 * @return 返回生效等级（已 `clamp` 下界到 0）。
	 */
	static int32 EvaluateEffectiveLevel(
		UTcsSkillSubsystem& Subsystem,
		const FTcsLearnedSkillEntry& Entry,
		const FTcsSkillDefData* Def);

#pragma endregion


// 选择器
#pragma region Selector

public:
	/**
	 * 条目是否被选择器命中（四档分派）。
	 *
	 * @param Selector 选择器。
	 * @param Entry 候选条目。
	 * @return 返回是否命中。
	 */
	static bool MatchesSelector(const FTcsSkillEntrySelector& Selector, const FTcsLearnedSkillEntry& Entry);

#pragma endregion
};
