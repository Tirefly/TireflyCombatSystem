// Copyright Tirefly. All Rights Reserved.

#include "Skill/TcsCostOps.h"

#include "EventBus/TcsEventBusSubsystem.h"
#include "Parameter/TcsParamValue.h"
#include "Parameter/TcsParamValueSource.h"
#include "StructUtils/InstancedStruct.h"

#include "Def/TcsParamRow.h"
#include "Def/TcsSkillDefData.h"
#include "Param/TcsSkillParamKeys.h"
#include "Skill/TcsCostEvents.h"
#include "Skill/TcsParamChain.h"
#include "Skill/TcsSkillRegistry.h"
#include "TcsAttributeSubsystem.h"

#include "TcsSkillLogChannel.h"
#include "TcsSkillSubsystem.h"



// 生效值求值（内建策略：属性资源）
double FTcsSkillCostPolicy_ResourceAttr::ResolveEffectiveAmount(
	const FTcsSkillCostContext& Context,
	double* OutBaseAmount) const
{
	// **① 基数 = `Amount` 自身的求值**（2026-10-12 换型为 `FTcsParamValue`）。
	//
	// 求值上下文**与参数链其余各点逐字同款**（`Subject`/`Instigator` = 单位、
	// `EffectiveLevel` = 本次生效等级）——**这是"按等级配消耗"能成立的唯一原因**：
	// 等级表参数源（`StateLevelArray` / `StateLevelMap`）读的正是**基类**的
	// `Context.EffectiveLevel`（`TcsParamSource_StateLevel.h:70`），而不是状态域派生字段。
	//
	// **参数表恒空**（同 `MakeContext` 的既有边界）：技能侧引用类源（`ParamRef`）在
	// 这个时点没有"更外层参数表"可查 ⇒ 落各自兜底（不报错、不崩溃）。
	FTcsParamEvaluateContext Ctx;
	Ctx.Subject = Context.Unit;
	Ctx.Instigator = Context.Unit;
	Ctx.EffectiveLevel = Context.Level;
	Ctx.ParamTable = TScriptInterface<ITcsParamTableReader>();

	const double BaseAmount = Amount.Evaluate(Ctx);

	// **无条目 ⇒ 无修正器可折叠 ⇒ 基数即结果**。
	//
	// **这条支路存在的理由（MUST 保留）**：装置与宿主会**直接构造一份配置**
	// 去测配置边界（无效 `ResourceAttr` / 负值）——那种场景**没有账本条目**，
	// 而"配置边界"的判据本来就只依赖配置自身。强行要求传条目会迫使那些调用点
	// 造一个假条目（那就成了"为测试造真相"）。
	if (!Context.Entry)
	{
		// 无减免 ⇒ 基数即结果（**仍钳 0**：负基数的拦截归 `CanAfford`，
		// 它经 `OutBaseAmount` 拿到**未钳**的原值，见下方收口处的判据）
		if (OutBaseAmount)
		{
			*OutBaseAmount = BaseAmount;
		}

		return FMath::Max(0.0, BaseAmount);
	}

	// **② 减免 = `CostAmount` 键上的修正器折叠**（初值取①）。
	//
	// **无修正器时本调用是恒等的**（`Fold` 对空集：`((B+0)×1×1)+0 = B`）
	// ⇒ **不需要**先探测"有没有修正器"，也不必为此多一次遍历。
	//
	// **两种减免形态各占一带**（`FoldTcsAttributeBands` 的既有算术，零新机制）：
	// `TAO_PercentAdd` = 百分比、"全部缩放之后"的 `TAO_FlatAdd` = 固定值。
	// **符号方向由作者给出**：`Amount` 是"要付多少"，减免是**减少**它
	// ⇒ 作者写 `PercentAdd = -0.3` / `FlatAdd = -10`（负数即减免，与属性侧同款直觉）。
	const double Folded = FTcsParamChainOps::FoldParamKeyWithBase(
		*Context.Entry, Tag_TcsSkillParam_CostAmount, BaseAmount);

	// **★ 一处 MUST 分开的两件事（本装置拒绝面检查 `d` 实测抓到的设计缺陷，MUST 保留本注）**：
	//
	// 初版把"钳 0"当成唯一的收口，于是**"负基数"被钳成 0 ⇒ `CanAfford` 的
	// 负值守卫永远不触发** ⇒ 检查 `d`（`Amount = -50` 应恒不可付）
	// 从 `false` 变成 `true`（**红字当场报出，见证据档**）。两者语义完全不同：
	// - **负基数**（作者把 `Amount` 直接填成 -50）= **配置错误**，是"倒给资源"的漏洞形态
	//   ⇒ MUST 拦住（判据在 `CanAfford`）；
	// - **折叠后转负**（基数 30、减免 -100）= **减免过度**，是**合法的配置结果**
	//   （"这个 buff 让技能免费"）⇒ MUST 钳 0、MUST NOT 报错。
	// ⇒ 收口因此**不能只留一个数**：调用方要能区分"基数本来是多少"与"减免后剩多少"。
	if (OutBaseAmount)
	{
		*OutBaseAmount = BaseAmount;
	}

	return FMath::Max(0.0, Folded);
}



// 门禁
bool FTcsSkillCostPolicy_ResourceAttr::CanAfford(
	const FTcsSkillCostContext& Context,
	FString* OutReason) const
{
	// ── 三类"配错"（一律 fail-closed） ──
	//
	// **注意本函数不再处理"不消耗"**：那个判定（`Timing == ECT_None`）**住在分派层**
	// （`FTcsCostOps::CanAffordCost`）——策略只在"确实要付"时被调到。
	// **判据**：`None` 是**时机**的属性（框架认识），不是**策略**的属性
	// （策略不该被问到"要不要付"）。

	if (!ResourceAttr.IsValid())
	{
		// "想配消耗但没填消耗什么" ⇒ 拦住并报出（**MUST NOT** 静默按"不消耗"放过）
		if (OutReason)
		{
			*OutReason = TEXT("ResourceAttr 无效（想配资源消耗但未填属性）");
		}

		UE_LOG(LogTcsSkill, Warning,
			TEXT("消耗配置错误：属性资源策略的 ResourceAttr 无效（单位=%lld）——判定为不可付"),
			Context.Unit.Id);
		return false;
	}

	// **★ 求本次生效的消耗量（基数求值 ⊕ `CostAmount` 键上的减免折叠）**。
	//
	// **为什么求值排在这里**：上面那条"无效 tag"分支恒不可付且不看数值
	// ⇒ 为它求值是白算（同"门禁不过就别建快照"的既有口径）。
	//
	// **等级从哪来**：上下文里的 `Level` 由分派层填（条目等级；无条目时为 0）。
	//
	// **`OutBaseAmount` MUST 取**：负**基数**的拦截依赖它（见 `ResolveEffectiveAmount` 的注释
	// ——"钳 0"会把"负基数"与"减免过度"压成同一个值，而前者必须拦）。
	double BaseAmount = 0.0;
	const double EffectiveAmount = ResolveEffectiveAmount(Context, &BaseAmount);

	// **负值判据看的是"未钳的基数"（★ 装置拒绝面检查 `d` 抓到的设计缺陷的修法）**：
	// 负的**基数**（作者把 `Amount` 直接填成 -50）是"倒给资源"的漏洞形态 ⇒ 在这里拦。
	// 而"减免过度导致折叠后转负"已被钳到 0 ⇒ **不会**在这里被误拦
	//（两者用 `BaseAmount` 区分，MUST NOT 合成一个判据——那正是初版的错）。
	if (BaseAmount < 0.0)
	{
		// **负数量可被利用**：扣减式是 `Base - Amount` ⇒ 负值会让技能**倒给**资源
		// （`Base - (-50)` = `Base + 50`）。这是**漏洞形态**，不是"配置不生效" ⇒ 必须拦。
		if (OutReason)
		{
			*OutReason = FString::Printf(TEXT("Amount 为负（%.3f）——会让技能倒给资源"),
				BaseAmount);
		}

		UE_LOG(LogTcsSkill, Warning,
			TEXT("消耗配置错误：Amount = %.3f < 0（单位=%lld 属性=%s）——判定为不可付"),
			BaseAmount, Context.Unit.Id, *ResourceAttr.ToString());
		return false;
	}

	if (EffectiveAmount == 0.0)
	{
		// "消耗 0 点"**就是不消耗**——正确声明，非错误。零日志。
		// **本条也是"减免到免费"的落点**：减免把量折到 0 ⇒ 恒可付、零扣减、零日志
		// （不需要为"免费"单独开一条分支——它与"配了 0 点"在业务上是同一件事）。
		return true;
	}

	// ── 正常判定：**先确认持有，再比基值** ──
	//
	// **★ 为什么 MUST 先确认持有**（本函数因此走 `TryGetBaseValue` 而不是"取值再比较"）：
	// 底层 `GetBaseValue` 对**未持有**返回 `0.0`，与"确实持有且基值为 0"**不可区分** ⇒
	// 不先问持有，就会把"属性配错 / 压根没加"表现成"**付不起**"
	// ——而"属性没加"与"资源不足"是两种含义完全不同的红字
	// （`TcsEffectAttributeAccess.h:72` 是同一条判据的既有表述："条件求值要区分二者才能把
	// '键写错'报出来"）。
	UTcsAttributeSubsystem* Attribute = Context.Subsystem->GetWorld()
		? Context.Subsystem->GetWorld()->GetSubsystem<UTcsAttributeSubsystem>() : nullptr;

	if (!Attribute)
	{
		// 世界无属性门面（世界拆解 / 未装配）⇒ 无法判定"付得起吗"。
		// **判为不可付**（fail-closed）：资源都没法查就放技能，等于免费放——
		// 那是"配置了消耗却静默不生效"的最坏形态。
		if (OutReason)
		{
			*OutReason = TEXT("属性门面不可得（世界未装配属性子系统）");
		}

		UE_LOG(LogTcsSkill, Warning,
			TEXT("消耗判定失败：属性门面不可得（单位=%lld 属性=%s）——判定为不可付"),
			Context.Unit.Id, *ResourceAttr.ToString());
		return false;
	}

	double BaseValue = 0.0;
	if (!Attribute->TryGetBaseValue(Context.Unit, ResourceAttr, BaseValue))
	{
		// **未持有 ≠ 基值为 0**：如实报出"该单位没有这个属性"（最常见的原因是宿主忘了
		// `AddAttribute`，或属性 tag 写错）——这与"资源不足"是两种不同的红字。
		if (OutReason)
		{
			*OutReason = FString::Printf(TEXT("该单位未持有属性 %s"),
				*ResourceAttr.ToString());
		}

		UE_LOG(LogTcsSkill, Warning,
			TEXT("消耗判定失败：单位未持有该属性（单位=%lld 属性=%s 需要 %.3f）"
				"——若这是意料之外，检查宿主是否 AddAttribute 了该属性"),
			Context.Unit.Id, *ResourceAttr.ToString(), EffectiveAmount);
		return false;
	}

	if (BaseValue < EffectiveAmount)
	{
		// **正常业务路径**（资源真的不够）⇒ `Log` 级，**不是红字**——这是玩家最常遇到的
		// "蓝不够"，不是缺陷。
		if (OutReason)
		{
			*OutReason = FString::Printf(TEXT("资源不足（基值 %.3f < 需要 %.3f）"),
				BaseValue, EffectiveAmount);
		}

		UE_LOG(LogTcsSkill, Log,
			TEXT("消耗判定：资源不足（单位=%lld 属性=%s 基值=%.3f 需要=%.3f）"),
			Context.Unit.Id, *ResourceAttr.ToString(), BaseValue, EffectiveAmount);
		return false;
	}

	return true;
}



// 提交
bool FTcsSkillCostPolicy_ResourceAttr::CommitCost(
	const FTcsSkillCostContext& Context,
	double* OutApplied) const
{
	if (OutApplied)
	{
		*OutApplied = 0.0;
	}

	// **注意本函数不再处理"不消耗"与"策略缺失"**：那两项判定住在**分派层**
	// （`FTcsCostOps::CommitCost`）——策略只在"确实要付且策略存在"时被调到。

	if (!ResourceAttr.IsValid())
	{
		// "想配消耗但没填消耗什么" ⇒ 零提交 + `Warning`（与 `CanAfford` 同判据）
		UE_LOG(LogTcsSkill, Warning,
			TEXT("消耗提交跳过：属性资源策略的 ResourceAttr 无效（单位=%lld）"), Context.Unit.Id);
		return false;
	}

	// **★ 求本次生效的消耗量**——与 `CanAfford` **调同一个函数**（MUST NOT 各算一份：
	// "门禁判的数"与"真扣的数"必须一致，那正是本仓 TOCTOU 防护的实质）。
	//
	// **本行 MUST NOT 被优化成"沿用门禁那次的结果"**：门禁与提交之间隔着建 run / 快照 /
	// 物化等步骤，期间修正器可能已变（`ApplyParamModifiers` 是宿主可随时调的口）。
	double CommitBaseAmount = 0.0;
	const double EffectiveAmount = ResolveEffectiveAmount(Context, &CommitBaseAmount);

	// **负基数 MUST 在此也拦**（与 `CanAfford` 同判据）：
	// 正常路径下门禁已拦过，但本函数**可被独立调用**（`PayCost` 公开口 / 装置验边界）
	// ⇒ 缺这道守卫时负基数会**照扣**（`Base - (-50)` = 倒给资源）。
	// **判据取未钳的基数**（同门禁：把"负基数"与"减免过度"分开）。
	if (CommitBaseAmount < 0.0)
	{
		UE_LOG(LogTcsSkill, Warning,
			TEXT("消耗提交被拒：Amount 为负（%.3f）——会让技能倒给资源（单位=%lld 属性=%s）"),
			CommitBaseAmount, Context.Unit.Id, *ResourceAttr.ToString());
		return false;
	}

	if (EffectiveAmount == 0.0)
	{
		// "消耗 0 点" = 不消耗 ⇒ 零动作、零日志（与 `CanAfford` 同口径）。
		// **本条也是"减免到免费"的落点**：减免折到 0 ⇒ 不扣、零日志、不发事件。
		return true;
	}

	UTcsAttributeSubsystem* Attribute = Context.Subsystem->GetWorld()
		? Context.Subsystem->GetWorld()->GetSubsystem<UTcsAttributeSubsystem>() : nullptr;
	if (!Attribute)
	{
		UE_LOG(LogTcsSkill, Warning,
			TEXT("消耗提交失败：属性门面不可得（单位=%lld 属性=%s）"),
			Context.Unit.Id, *ResourceAttr.ToString());
		return false;
	}

	// **读基值 → 减 → 写回**（**MUST NOT** 用 `ApplyModifier` 挂修正器——三条判据见头文件）。
	//
	// **为什么读的是基值而不是 `EvaluateCurrent`**：后者是**折叠后**的当前值
	// （基值 + 全部修正器）。拿它减会在有修正器时**多减**（扣掉的是"当前值 − Amount"，
	// 写回的却是基值 ⇒ 修正器那部分的量被一起扣掉了）。判据 = 写入点与读取点 MUST 是同一层。
	double BaseValue = 0.0;
	if (!Attribute->TryGetBaseValue(Context.Unit, ResourceAttr, BaseValue))
	{
		// 门禁那一侧已按"未持有 ⇒ 不可付"拦住；走到这里说明**门禁之后属性被移除了**
		// （TOCTOU 的真实形态）⇒ 如实报出并拒绝提交。
		UE_LOG(LogTcsSkill, Warning,
			TEXT("消耗提交失败：单位已不持有该属性（单位=%lld 属性=%s）——门禁后属性被移除"),
			Context.Unit.Id, *ResourceAttr.ToString());
		return false;
	}

	const double NewBaseValue = BaseValue - EffectiveAmount;

	// **提交失败 MUST 被报出**：`SetBaseValue` 自身对"单位未注册 / 属性名为空 / 未持有"返回 false
	// （含 `ensureMsgf` 红字）⇒ MUST NOT 假设它一定成功，否则 `PayCost` 会报成功而资源没扣。
	if (!Attribute->SetBaseValue(Context.Unit, ResourceAttr, NewBaseValue))
	{
		UE_LOG(LogTcsSkill, Warning,
			TEXT("消耗提交失败：属性写回被拒（单位=%lld 属性=%s 目标基值=%.3f）"),
			Context.Unit.Id, *ResourceAttr.ToString(), NewBaseValue);
		return false;
	}

	if (OutApplied)
	{
		*OutApplied = EffectiveAmount;
	}

	UE_LOG(LogTcsSkill, Log,
		TEXT("消耗已提交：单位=%lld 属性=%s %.3f → %.3f（扣 %.3f）"),
		Context.Unit.Id, *ResourceAttr.ToString(), BaseValue, NewBaseValue, EffectiveAmount);

	// **消耗事件不在此发**——`CanAfford` 失败**不发**（`D5-16` 明文："CanAfford 失败不发
	// ——`TryActivate` 返回值覆盖"）：门禁拒绝面已有 `SAR_CannotAfford` 这个具名读数，
	// 再发一枚"没扣成"的事件纯属噪音（订阅者还得自己判"这枚事件是真的扣了还是没扣"）。
	//
	// **本函数只拿得到 `Unit` 与 `Context`，拿不到条目身份** ⇒ 载荷的 `EntryHandle` / `DefTag`
	// 由调用方补发；此处先发一枚"单位级"的部分载荷会造出两种载荷形状
	// ⇒ 由拿到条目的调用方统一发（见 `PayCostForEntry` / `TcsCastOps`），本函数**只负责扣减**。
	// **这是一处刻意的职责切分**：策略的 `CommitCost` 是"纯扣减"（它连技能都不需要知道——
	// 激活路径的 `OnCastStarted` 档就是直调它），发事件需要条目身份，属调用方的事。
	return true;
}

// ── 分派层（框架只做"要不要付 / 付哪一个策略"的判断，具体扣减归策略） ──────────────

const FTcsSkillCostPolicy* FTcsCostOps::ResolvePolicy(const FTcsSkillCostConfig& Config)
{
	// **单参数 `GetPtr<T>()`**（判据见头文件）：空载荷与类型不符**都**返回 `nullptr`
	// ⇒ 一个判据覆盖两种降级。**MUST NOT 用双模板参数重载**（Shipping 下完全不校验类型）。
	return Config.CostMode.GetPtr<FTcsSkillCostPolicy>();
}



FTcsSkillCostContext FTcsCostOps::MakeContext(
	UTcsSkillSubsystem& Subsystem,
	FTcsCombatEntityHandle Unit,
	const FTcsLearnedSkillEntry* Entry)
{
	FTcsSkillCostContext Context;
	Context.Subsystem = &Subsystem;
	Context.Unit = Unit;
	Context.Entry = Entry;
	// **等级从条目取**（等级表参数源读的就是它）；无条目 = 装置验配置边界场景，取 0。
	Context.Level = Entry ? Entry->Level : 0;
	return Context;
}



bool FTcsCostOps::CanAffordCost(
	UTcsSkillSubsystem& Subsystem,
	FTcsCombatEntityHandle Unit,
	const FTcsSkillCostConfig& Config,
	FString* OutReason,
	const FTcsLearnedSkillEntry* Entry)
{
	// ── `ECT_None`：**正确声明"不消耗"** ⇒ 恒可付、**零日志、不看策略** ──
	// 这是"不消耗"的**唯一正确表达**（2026-10-12 用户裁定：Timing 为 None ⇒ 后面一概不管）。
	// **MUST NOT** 顺手去看 `CostMode`（那会把"声明不消耗"与"策略没接上"混成一类）。
	if (Config.Timing == ETcsSkillCostTiming::ECT_None)
	{
		return true;
	}

	// ── `Timing != None` 但策略缺失/类型不符 ⇒ **配置错误，fail-closed** ──
	//
	// **MUST NOT 静默按"不消耗"放过**：那会让"想配消耗但没配清"表现成"一切正常"
	//（本仓反复在拦的那类 bug，见头文件的"三类分开处置"表）。
	// **与换型前的行为差异 = 收紧**：换型前的 `ECP_CustomFragment` 降级是**免费放**，
	// 现在空策略**拦住**（判据见头文件）。
	const FTcsSkillCostPolicy* Policy = ResolvePolicy(Config);
	if (!Policy)
	{
		if (OutReason)
		{
			*OutReason = TEXT("CostMode 为空或类型不符（想配消耗但没接上策略）");
		}

		UE_LOG(LogTcsSkill, Warning,
			TEXT("消耗配置错误：Timing 非 None 但 CostMode 为空/类型不符（单位=%lld）——判定为不可付"),
			Unit.Id);
		return false;
	}

	// ── 分派给策略（**门禁与提交点在这里走同一个权威**） ──
	const FTcsSkillCostContext Context = MakeContext(Subsystem, Unit, Entry);
	return Policy->CanAfford(Context, OutReason);
}



bool FTcsCostOps::CommitCost(
	UTcsSkillSubsystem& Subsystem,
	FTcsCombatEntityHandle Unit,
	const FTcsSkillCostConfig& Config,
	double* OutApplied,
	const FTcsLearnedSkillEntry* Entry)
{
	if (OutApplied)
	{
		*OutApplied = 0.0;
	}

	// ── `ECT_None` ⇒ 零动作、零日志（不看策略） ──
	if (Config.Timing == ETcsSkillCostTiming::ECT_None)
	{
		return true;
	}

	// ── 策略缺失 ⇒ 零提交 + `Warning`（与 `CanAffordCost` 同判据） ──
	const FTcsSkillCostPolicy* Policy = ResolvePolicy(Config);
	if (!Policy)
	{
		UE_LOG(LogTcsSkill, Warning,
			TEXT("消耗提交跳过：Timing 非 None 但 CostMode 为空/类型不符（单位=%lld）"),
			Unit.Id);
		return false;
	}

	const FTcsSkillCostContext Context = MakeContext(Subsystem, Unit, Entry);

	// **★ 原子性：框架把策略的整个提交包进属性批（2026-10-12 用户裁定）**。
	//
	// **批给什么、不给什么（MUST 读，防误判——源码核实 2026-10-12）**：
	// - **给**：重算与广播推迟到最外层 `Commit`（`BatchDepth` 嵌套计数）+ **订阅者看不到中间态**
	//   ⇒ **关闭重入窗口**（"第一条扣减广播 → 订阅者花掉资源 → 第二条扣不动"）。
	// - **不给**：**回滚**。`FTcsAttributePipeline::SetBaseValue` 的写入是**立即**的
	//   （`Instance->BaseValue = NewBaseValue;`），批只推迟 `Recalculate`。
	// - ⇒ **"全有全无"的实际保障 = 先判后扣**（调用方 MUST 先过 `CanAffordCost`），
	//   **MUST NOT** 把批当作事务/回滚来依赖。
	//
	// **★ 一处必须的守卫（本框架包批引入的新问题）**：`BeginBatch` / `Commit` 对**未注册单位**
	// 会触发 `ensureMsgf`（红字）。内建属性策略不可能走到那里（它先要 `TryGetBaseValue` 成功
	// ⇒ 单位必然已注册），但**宿主自定义策略**可能"没注册单位也判可付" ⇒ 框架包批会**替宿主
	// 多报一条 ensure**。故包批前用**不 ensure** 的 `ResolveStore` 探测；未注册时**不包批**
	// （策略照常提交，其自身失败面自行上报）。
	UTcsAttributeSubsystem* Attribute = Subsystem.GetWorld()
		? Subsystem.GetWorld()->GetSubsystem<UTcsAttributeSubsystem>() : nullptr;

	const bool bCanWrapInBatch = (Attribute != nullptr)
		&& (Attribute->ResolveStore(Unit) != nullptr);

	if (bCanWrapInBatch)
	{
		// **批的边界 = 紧邻本策略的提交**，MUST NOT 圈进施法激活的其它属性改动
		// （如 `MaterializeParamChainRows`）——圈的边界就是"这一次消耗"。
		Attribute->BeginBatch(Unit);
	}

	double Applied = 0.0;
	const bool bCommitted = Policy->CommitCost(Context, &Applied);

	if (bCanWrapInBatch)
	{
		// **提交点唯一**：批内多次变更只重算一次、每属性最多广播一次（`TcsAttributeSubsystem.h:287`）。
		Attribute->Commit(Unit);
	}

	if (OutApplied)
	{
		*OutApplied = Applied;
	}

	return bCommitted;
}



bool FTcsCostOps::PayCostForEntry(
	UTcsSkillSubsystem& Subsystem,
	FTcsCombatEntityHandle Unit,
	FTcsSkillEntryHandle EntryHandle)
{
	FSkillBucket* Bucket = Subsystem.Registry.FindBucket(Unit);
	FTcsLearnedSkillEntry* Entry = Bucket ? Bucket->Find(EntryHandle) : nullptr;
	if (!Entry)
	{
		UE_LOG(LogTcsSkill, Warning, TEXT("提交消耗被拒：账本条目悬空（单位=%lld 条目=%d/%d）"),
			Unit.Id, EntryHandle.Index, EntryHandle.Generation);
		return false;
	}

	const FTcsSkillDefData* Def = Subsystem.GetRegisteredSkillDef(Entry->DefTag);
	if (!Def)
	{
		UE_LOG(LogTcsSkill, Warning, TEXT("提交消耗被拒：定义取不到（单位=%lld 定义=%s）"),
			Unit.Id, *Entry->DefTag.ToString());
		return false;
	}

	// **★ TOCTOU 防护：提交点重查一次**（判据与门禁⑤ **同一函数**，MUST NOT 各写一份）。
	// 门禁通过到本提交点之间，资源可能已被花掉（同一帧内的另一次施法、或宿主的直接扣减）。
	//
	// **★ 传 `Entry`（2026-10-12 B 形状）**：不传 ⇒ `CostAmount` 上的减免不生效，
	// 于是"门禁算的数"与"提交扣的数"不一致——正是本仓 TOCTOU 防护要防的那类自相矛盾。
	FString Reason;
	if (!CanAffordCost(Subsystem, Unit, Def->Cost, &Reason, Entry))
	{
		UE_LOG(LogTcsSkill, Log,
			TEXT("提交消耗被拒[重查未过]：单位=%lld 定义=%s 原因=%s"),
			Unit.Id, *Entry->DefTag.ToString(), *Reason);
		return false;
	}

	double Applied = 0.0;
	if (!CommitCost(Subsystem, Unit, Def->Cost, &Applied, Entry))
	{
		return false;
	}

	// **扣成功才发事件**（`CanAfford` 失败不发——`D5-16` 明文；零动作档也不发，见 `BroadcastCostApplied`）。
	// **载荷的 `Amount` 取"实际扣掉的量"**（= 求值 ⊕ 减免后的结果），不是 `Config.Amount` 的旧字段值
	// ——后者在换型后已不是数字，"发出扣了 30"而实际扣了 21 会直接误导订阅者的 UI。
	BroadcastCostApplied(Subsystem, *Entry, Def->Cost, Applied);
	return true;
}

void FTcsCostOps::BroadcastCostApplied(
	UTcsSkillSubsystem& Subsystem,
	const FTcsLearnedSkillEntry& Entry,
	const FTcsSkillCostConfig& Config,
	double AppliedAmount)
{
	// **只对"真的扣了一笔"发**：扣减量 `<= 0` 不是"扣了一笔"（"消耗 0 点"与"不消耗"
	// 都不是）——发它们会让订阅者收到无意义的通知（且 `CanAfford` 失败也已经不发）。
	//
	// **判据取实际扣减量而不是配置基数**：配置里的 `Amount` 是**参数源载体**、不是数字；
	// 而"有没有真扣"只有**实际扣减量**知道（减免到 0 的那一刻恰好就是"没扣"）。
	if (AppliedAmount <= 0.0)
	{
		return;
	}

	UTcsEventBusSubsystem* Bus = Subsystem.GetEventBus();
	if (!Bus)
	{
		// 世界无总线（拆解期）⇒ 静默跳过（不 ensure、不红字——时序而非契约违规）
		return;
	}

	FTcsCostEventPayload Payload;
	Payload.EntryHandle = Entry.Handle;
	Payload.Unit = Entry.Unit;
	Payload.DefTag = Entry.DefTag;

	// **★ 载荷的 `ResourceAttr` 是内建策略专属字段（如实登记）**：只有内建属性资源策略
	// 才有一个明确的"被扣属性"；**宿主自定义策略**扣什么框架不认识 ⇒ 该字段留无效。
	// **MUST NOT** 为此去猜（那等于让框架认识宿主的资源语义）。
	if (const FTcsSkillCostPolicy_ResourceAttr* ResourcePolicy =
		Config.CostMode.GetPtr<FTcsSkillCostPolicy_ResourceAttr>())
	{
		Payload.ResourceAttr = ResourcePolicy->ResourceAttr;
	}

	// **`Amount` 取"实际扣掉的量"**：它是**求值 ⊕ 减免之后**的结果
	// ——订阅者（UI 扣血飘字 / 资源条动效）要的正是"这一发花了多少"，而不是配置里的基数。
	Payload.Amount = AppliedAmount;

	// **立即通道**：与冷却事件同口径（状态查询面立刻自洽）
	Bus->PublishImmediate(Tag_TcsEvent_Cast_CostApplied, FInstancedStruct::Make(Payload));
}
