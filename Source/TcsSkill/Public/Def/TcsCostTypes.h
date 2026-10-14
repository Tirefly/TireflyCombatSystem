// Copyright Tirefly. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "GameplayTagContainer.h"
#include "StructUtils/InstancedStruct.h"

#include "Handle/TcsCombatEntityHandle.h"
#include "Parameter/TcsParamValue.h"

#include "TcsCostTypes.generated.h"

class UTcsSkillSubsystem;
struct FTcsLearnedSkillEntry;



/**
 * **★ 已删除：`ETcsCostPolicy`（三档 `ECP_None` / `ECP_ResourceAttr` / `ECP_CustomFragment`）**
 * —— 2026-10-12 用户改点**策略模式**，该枚举**整体退役**（MUST NOT 补回）。
 *
 * **判据 = "同一件事两个入口"**：档位枚举答"走哪个"，策略对象答"怎么做"——两者并存时
 * "到底以哪个为准"就成了必须裁定的问题。改点后只剩**策略对象**一个入口。
 *
 * **原字面留痕（历史现场，MUST NOT 改写）**：
 * > 消耗策略 —— "**这个技能消耗什么**"（`D5-10` 的 `FCostConfig.Policy`）。
 * > 命名沿 R6 已立的短前缀惯例（`ECastInstancing` / `ECastQueryMode` / `EMainChainStart` 三处
 * > 全部带前缀）⇒ `ECost*` 族的首词即域。
 *
 * **三档各自的去向（逐条交代，防"删了没下文"）**：
 * - `ECP_None` → **`ETcsSkillCostTiming::ECT_None`**（"不消耗"的唯一入口）；
 * - `ECP_ResourceAttr` → **`FTcsSkillCostPolicy_ResourceAttr`**（内建策略，**同样遵循策略方案**）；
 * - `ECP_CustomFragment` → **策略对象本身**（宿主派生 `FTcsSkillCostPolicy` 或走脚本转发器）。
 */



/**
 * 消耗时机 —— "**什么时候付**"（`D5-10` 的 Cost Timing）。
 *
 * **★ 四档（2026-10-12 用户改点：策略模式）**：
 * - `ECT_None` = **一定没有消耗**（默认档）——**它取代了原来的 `ECP_None` 枚举档**，
 *   是"不消耗"的**唯一**入口。判据：同一件事两个入口会让"到底看哪个"变成需要裁定的问题。
 * - 其余三档 = 框架在两个**施法事务自身的边界**上自动提交，或完全不自动（`Manual`）。
 *
 * **`OnEvent` 档 MUST NOT 补回**（2026-10-12 用户裁定）：见下方留痕。
 *
 * **本枚举刻意没有 `OnCastInterrupted`**（2026-10-11 裁定，MUST NOT 补回）——两条硬判据：
 *
 * ① `D5-4` 已把 `bRefundCostOnInterrupt` 砍除，理由原文是"**返还是政策不是引擎语义**"
 *    ⇒ 框架层面"打断 ⇒ 退钱"这条路已被明确关闭；
 * ② 于是"打断时扣"这一档**没有任何独立含义**：说"施法开始时扣" ⇒ 被打断**也扣了**
 *    （那就是 `ECT_OnCastStarted`）；说"打断时扣" ⇒ 施法**成功完成就不扣**——**荒谬**。
 *
 * ⇒ 本枚举与 `ETcsSkillCooldownTiming` **各有自己的档位集**，MUST NOT 共用、MUST NOT 用一个
 * "覆盖"另一个（冷却侧无 `None`，本侧无 `OnCastInterrupted`）。
 *
 * **原名留痕（2026-10-12，`PLN-R6.5-CC` 裁定 R12）**：本类型原名 **`ETcsCostTiming`**，
 * 与冷却侧同批加域前缀以保持这对姊妹枚举对称；**只改类型名，语义不动**。
 *
 * **原字面留痕（`OnEvent` 档的删除理由，MUST NOT 改写）**：
 * > **★ 三档（2026-10-12 用户裁定：`OnEvent` 档整体删除，MUST NOT 补回）**：
 * > "由某个**特定事件**触发的消耗"属于**宿主项目自己的玩法逻辑**，不是框架该内置的档位——
 * > 判据：框架只能认识"施法这个事务自身的边界"（开始 / 完成），而"某个特定事件"是
 * > **项目词汇**（挨打时付费、连击到第 3 段付费、血低于 30% 付费……），
 * > 内置等于让框架去猜项目的玩法。**宿主实现成本极低**：监听自己的事件 → 调 `PayCost`
 * > （那正是 `ECT_Manual` 档 + 公开口 `PayCost` 的组合，**不需要新的框架面**）。
 * >
 * > **同批删除的伴随面（MUST 一起删，否则留下悬空契约）**：
 * > ① `FTcsCostConfig::TriggerEventTag` 字段；② `UTcsCostEventRelay` 整个类（它**只**为该档服务）；
 * > ③ 门面的 `GetCostEventRelay()` 与 `CostEventRelay` 成员；④ 三处撤销路径的中继退订。
 *
 * **枚举值上方 MUST NOT 放 `//` 行注释**（UHT 会转成 `ToolTip` 并与显式 `UMETA(ToolTip=...)` 冲突，
 * 直接编译失败——本仓实测）。语义一律写进 `UMETA`。
 */
UENUM(BlueprintType)
enum class ETcsSkillCostTiming : uint8
{
	ECT_None = 0			UMETA(DisplayName = "无消耗", ToolTip = "该技能一定没有消耗——框架不看策略、不扣减、零日志（默认档）"),

	ECT_OnCastStarted = 1	UMETA(DisplayName = "施法开始", ToolTip = "激活事务内提交：门禁⑤通过后、建施法运行态之前"),

	ECT_OnCastCompleted = 2	UMETA(DisplayName = "施法完成", ToolTip = "施法走完全部时段、或瞬发当刻完成时提交"),

	ECT_Manual = 3			UMETA(DisplayName = "手动", ToolTip = "框架不自动提交，由宿主在自己的逻辑处调 PayCost（含'某个特定事件触发'这类项目玩法）"),
};



/**
 * 消耗策略求值上下文（**纯 C++ 结构，非反射**）。
 *
 * **为什么可以非反射**（★ 这是本轮两处分层的**立足点**，MUST 读）：
 * UHT 的签名面约束（"形参类型必须可蓝图化"）**只作用于 `BlueprintNativeEvent`**；
 * **`USTRUCT` 的 C++ 虚函数不受该约束** ⇒ 策略的 `CanAfford` / `CommitCost` 可以取富类型
 * （`FTcsLearnedSkillEntry` 是**非反射** struct——它作 `BlueprintNativeEvent` 形参是 UHT 硬错误）。
 * ⇒ **刻意分成两层**：
 * - **C++ 面**（`FTcsSkillCostPolicy` 虚分派）：用本结构，可持非反射类型、零翻译成本；
 * - **脚本面**（`ITcsSkillCostPolicyHost`）：**MUST** 只用已反射类型 ⇒ 传 `FTcsSkillEntryHandle`，
 *   由转发器 `FTcsSkillCostPolicy_HostDelegate` 负责"句柄 → 解析条目 → 装本结构"的翻译。
 *
 * **MUST NOT 升格为 `USTRUCT`/`UPROPERTY`**（策略实例归 Def 资产、求值瞬时不跨帧）。
 * 若日后要过网，那是另一项需求。
 */
struct FTcsSkillCostContext
{
	// 技能门面（策略解析属性子系统等一切世界级服务都从这里走）
	UTcsSkillSubsystem* Subsystem = nullptr;

	// 被扣资源的实体
	FTcsCombatEntityHandle Entity;

	// 账本条目（**减免修正器与等级都从这里取**；可为空 = 无条目场景，见 `FTcsCostOps`）
	const FTcsLearnedSkillEntry* Entry = nullptr;

	// 本次生效等级（等级表参数源读它 ⇒ "按等级配消耗"的落点）
	int32 Level = 0;
};



/**
 * 消耗策略抽象基类（**宿主与内建共用同一套方案**，`D5-10` 的 `ICostPolicy`）。
 *
 * **★ 2026-10-12 用户改点（策略模式，取代档位枚举）**：原设计的
 * `ETcsCostPolicy{None/ResourceAttr/CustomFragment}` 把"消耗什么"写死成**枚举档位**，
 * 结果内建路径与自定义路径**是两条会分叉的路**。改点后：
 * - **框架自带的属性资源策略也遵循本方案**（`FTcsSkillCostPolicy_ResourceAttr`），不写死在代码里；
 * - 宿主自定义消耗 = 派生本类（或走脚本面转发器），**与内建路径同一套分发**；
 * - `ETcsCostPolicy` 枚举**整体删除**（枚举与策略并存 = 同一件事两个入口）。
 *
 * **为什么是 `USTRUCT` 虚分派基类（而非注册表）**：与 `FTcsParamValueSource` 同款——
 * **"配置即成员"**（策略自己的字段就住在派生结构体里），Def 资产以 `FInstancedStruct` 持有实例，
 * 值类型本身可序列化进资产。**注册表形态**（同 `FTcsTriggerConditionRegistry`）适合"条件是纯数据、
 * 行为外置"的场景；而消耗策略**需要携带配置字段**，故取本形态。
 *
 * **"抽象"由约定达成**（StateTree `FStateTreeConditionBase` 同款，**MUST NOT 改回 `=0`**）：
 * UHT 为每个 USTRUCT **无条件**生成 `TCppStructOps<T>`（构造路径 `new T` 需可默认构造）
 * ⇒ **纯虚 `=0` 会报 `C2259` 无法实例化抽象类**；而 **`PURE_VIRTUAL` 在
 * `CHECK_PUREVIRTUALS` 开启时展开为 `=0`、关闭时才是致命错误体**——同一份代码在 Development
 * 编不过、Shipping 能过（**最坏的一类失败**）。
 * ⇒ 故：**中性默认实现 + `meta=(Hidden)`**（picker 按 `Hidden` 过滤 ⇒ 基类不可被作者选中）。
 *
 * **虚析构是必需项（多态基类的正确性要求，非洁癖）**：本族是多态基类，派生策略持
 * `TArray` / `FTcsParamValue` 一类**非平凡析构**的成员；若非虚析构，任何"经基类指针删除"的写法
 * 都是未定义行为（MSVC 的 `C4265` 正是这一点）。先例 = `FTcsParamValueSource`。
 *
 * **纯函数性纪律（`D0-1`）**：`CanAfford` MUST 是**纯查询**（不扣任何东西、不发事件、不改账本）；
 * 两者 MUST NOT 自取随机数（同输入须同输出，否则回放失效）。
 */
USTRUCT(meta = (Hidden))
struct TCSSKILL_API FTcsSkillCostPolicy
{
	GENERATED_BODY()

	// 虚析构（见类注释：多态基类的正确性要求）
	virtual ~FTcsSkillCostPolicy() = default;

// 检查
#pragma region Afford

public:
	/**
	 * 该实体**现在**付得起吗（门禁⑤ 的判据）。
	 *
	 * **纯查询语义（MUST）**：MUST NOT 扣任何资源、MUST NOT 改账本、MUST NOT 发事件。
	 *
	 * **拒绝面形态**：返回 `bool`，**MUST NOT ensure**——"付不起"是**正常业务路径**，
	 * 不是契约违规（与 `AttributeCompare` 条件的处置同款："不通过 + 一条日志"）。
	 *
	 * @param Context 求值上下文（只读）。
	 * @param OutReason 输出"为什么付不起"的可读原因（可空）——宿主排查最需要的读数。
	 * @return 返回是否付得起。
	 */
	virtual bool CanAfford(const FTcsSkillCostContext& Context, FString* OutReason = nullptr) const
	{
		// 中性默认：可付（派生策略 MUST 覆写；基类被选中是配置事故，故本默认只保证"不崩"）
		return true;
	}

#pragma endregion


// 提交
#pragma region Commit

public:
	/**
	 * 提交消耗（**本策略自己的扣减实现**）。
	 *
	 * **调用方已保证 `CanAfford` 通过**（门禁⑤ 与每个提交点各自判一次）——但实现方
	 * **MUST NOT 依赖"提交一定成功"**：底层写口（如 `SetBaseValue`）对"实体未注册 / 属性未持有"
	 * 会失败 ⇒ 本函数 MUST 检查并向上报失败（否则 `PayCost` 会报成功而资源没扣）。
	 *
	 * @param Context 求值上下文（只读）。
	 * @param OutApplied 输出实际扣减的数量（0 = 零动作；仅返回 true 时有意义）。
	 * @return 返回是否提交成功。
	 */
	virtual bool CommitCost(const FTcsSkillCostContext& Context, double* OutApplied = nullptr) const
	{
		// 中性默认：零动作、成功（派生策略 MUST 覆写）
		return true;
	}

#pragma endregion
};



/**
 * 内建消耗策略：**属性资源**（走 M2 属性账本：按 `ResourceAttr` 键读基值并扣减 `Amount`）。
 *
 * **★ 它也走同一套策略方案**（2026-10-12 用户裁定的核心之一，MUST 读）：
 * "TCS 自带一个 ResourceAttr，但也遵循这套方案，而不是直接把 ResourceAttr 写死在代码里。"
 * 判据 = **消除两条会分叉的路**：若内建路径写死、自定义另开一条，则"门禁判的数"与"真扣的数"
 * 会分两个实现漂移——而那正是 TOCTOU 防护要防的形态。走同一套 ⇒ 门禁与提交点
 * **在同一个策略对象上**调用，一致性由构造保证。
 *
 * **实现住 `Private/Skill/TcsCostOps.cpp`**（本文件只给形状；UHT 不解析 .cpp 的反射，
 * 故实现必须出线）。
 *
 * **★ `BlueprintType` 是必需项、不是装饰（2026-10-13 实测抓出的缺陷，MUST 读）**：
 * 本类原先只有裸 `USTRUCT()`，结果**没有导出到脚本层**——Python 侧 `hasattr(unreal,
 * "TcsSkillCostPolicy_ResourceAttr")` 为 **False**，而**姊妹类**
 * `FTcsSkillCostPolicy_HostDelegate`（带 `BlueprintType`）**在**。
 * **判据（引擎源码）**：`IsScriptExposedStruct`（`PythonScriptPlugin/…/PyGenUtil.cpp:1560-1581`）
 * **沿父链**逐个查 `BlueprintType` 元数据，**任一环有才导出**；裸 `USTRUCT()` 三者皆无 ⇒ 不导出。
 * 基类 `FTcsSkillCostPolicy` 带 `meta=(Hidden)` 却出现在脚本层，**正是"作为已导出子类的父类被顺带发出"**，
 * 不是它自己够格。
 * **同族先例（逐个核对过）**：`FTcsParamValueSource` / `FTcsParamEnumerableSource` 两个基类都是
 * `USTRUCT(meta=(Hidden))`，而 `FTcsParamSource_Literal` / `_ParamRef` / `_HostDelegate` /
 * `_AttributeScaled` / `_InstigatorLevelArray` 等**全部具体类都带 `BlueprintType`** ⇒ 全在脚本层。
 * **⇒ 具体策略类 MUST 带 `BlueprintType`；基类 MUST 只带 `Hidden`**（前者决定"脚本能不能配"，
 * 后者决定"picker 里能不能选到基类"——两件事，别互相顶替）。
 * **本缺陷的实质影响**：内建消耗策略**脚本侧配不了**（Python / C# 都取不到该类型），
 * 与"内建也走同一套策略方案"（`R4`）以及本轮的脚本面插槽目标**直接冲突**。
 * **注意它不影响 C++ 面与编辑器 picker**：`FInstancedStructFilter::IsStructAllowed`
 * 只看 `Hidden` / `AllowedClasses` 等元数据、**不读 `BlueprintType`**（见
 * `unreal-development-workflow` 的"细节面板可编辑性"条）⇒ 策划在细节面板里一直选得到它
 * ——**这正是它潜伏至今的原因**（三条路里只有脚本那条断）。
 */
USTRUCT(BlueprintType)
struct TCSSKILL_API FTcsSkillCostPolicy_ResourceAttr : public FTcsSkillCostPolicy
{
	GENERATED_BODY()

// 资源
#pragma region Resource

public:
	/**
	 * 消耗哪个属性（取值是**宿主本体论**——"蓝条 / 怒气 / 弹药"都是项目词，
	 * 故框架只给空槽位、不填默认值）。
	 *
	 * **无效 tag 的处置 = 恒不可付 + `Warning`（fail-closed）**，MUST NOT 静默按"不消耗"放过。
	 * 判据 = 本仓既有惯例（`TcsFlowStepConditions.h:56` / `TcsStepModifyFlow.cpp:36` /
	 * `TcsEffectAttributeAccess.h:72` 三处措辞几乎逐字相同）："**配置写错**"与"**正确声明不消耗**"
	 * 是两件事，混成一类会让前者**表现成一切正常**、日志还清白。
	 */
	UPROPERTY(EditAnywhere, Category = "Cost Policy|Resource")
	FGameplayTag ResourceAttr;

	/**
	 * 消耗数量的**基数**（参数源载体，2026-10-12 `R6.5-1` 参数化批次换型）。
	 *
	 * **判据 = 一条 `double` 同时承担了三件事，而三者都需要"可配置"**：
	 * ① **按等级配数值**（"1 级 10 蓝 / 2 级 15 蓝"）——靠等级表参数源
	 *   （`FTcsParamSource_StateLevelArray` / `_StateLevelMap`，它们读**基类**的
	 *   `Context.EffectiveLevel`，而策略上下文已填该字段 ⇒ 直接可用）；
	 * ② **按来源配数值**（`HostDelegate` / `ParamRef` 等既有源）；
	 * ③ **减免**——靠**契约键** `Tag_TcsSkillParam_CostAmount` 上的修正器折叠
	 *   （**初值 = 本字段的求值结果**）。
	 *
	 * **为什么减免不直接挂在本字段上、而要经一个契约键**（MUST 读，防误改）：
	 * 修正器住在**键**上（`FTcsNumericParamModInstance::ParamKey`），而本字段是**字段**不是键。
	 * ⇒ 修正是"往某个键挂一条 `{Op, Value}`"，再由折叠把它作用到该键的初值上。
	 * **这同时是"两种减免形态"的落点**：`TAO_PercentAdd` = 百分比减免、
	 * `TAO_FlatAdd` = 固定值减免（见 `Tag_TcsSkillParam_CostAmount` 的注释）。
	 *
	 * **求值时点**：**门禁⑤ 与每个提交点各自求值一次**（MUST NOT 跨点复用——
	 * 那会让"门禁判的数"与"真扣的数"漂移，正是 TOCTOU 防护要防的形态）。
	 *
	 * **取值边界（三条）**：
	 * - `0` = 正确声明"消耗 0 点" ⇒ 恒可付、**零日志**（不是错误）；
	 * - `< 0` = **恒不可付 + `Warning`**。判据：扣减式是 `Base - Amount`，负值会让技能
	 *   **倒给**资源（`Base - (-50)` = `Base + 50`）——这是**可被当作漏洞利用**的形态；
	 * - `> 0` = 正常判定 `基值 >= Amount`，**且**该实体 MUST 持有该属性。
	 *
	 * **命名（为何带 `Amount` 而非就叫 `Value`）**：同族参数源字段一律以**它语义上的角色**命名
	 * （`FTcsPhaseSpan::Duration` / `FTcsCooldownTrack::Duration`），保留 `Amount` 让
	 * "这个载体的语义是消耗量"在字段名上仍然可读。
	 *
	 * **默认值**：`FTcsParamValue` 默认构造是 **`Literal(0)`** ⇒ 默认 = 消耗 0 点 = 不消耗
	 * （与换型前的 `double Amount = 0.0` **逐字同义**；且全仓 `.uasset` 扫 `Cost` 的 `Amount`
	 * **零命中**，无存量资产需迁移）。
	 */
	UPROPERTY(EditAnywhere, Category = "Cost Policy|Resource")
	FTcsParamValue Amount;

#pragma endregion


// 策略实现（出线到 .cpp——UHT 不解析 .cpp，反射只需本声明）
#pragma region Policy

public:
	// 检查：无效 tag / 负基数 / 未持有 / 基值不足 → 恒不可付 + 具名原因
	virtual bool CanAfford(const FTcsSkillCostContext& Context, FString* OutReason = nullptr) const override;

	// 提交：`SetBaseValue` 直写（读基值 → 减 → 写回）；**MUST NOT 用 `ApplyModifier`**
	virtual bool CommitCost(const FTcsSkillCostContext& Context, double* OutApplied = nullptr) const override;

	/**
	 * 求**本次生效的消耗量**（`Amount` 的求值 ⊕ `CostAmount` 键上的减免折叠）。
	 *
	 * **为什么是独立函数、而不是内联进 `CanAfford` / `CommitCost`**：门禁⑤ 与**每个提交点**
	 * 都要用同一个数（TOCTOU 防护的实质 = "门禁判的数与真扣的数一致"）⇒ 求值 MUST 只有一份实现。
	 *
	 * **两种减免形态**（`FoldTcsAttributeBands` 的既有算术，**零新机制**）：
	 * **百分比** ⇒ `TAO_PercentAdd`；**固定值** ⇒ `TAO_FlatAdd`（全部缩放之后的平坦加）。
	 * 作者写负数即减免。**能力边界（如实）**：`FlatAdd` 排在 `PercentAdd` 之后 ⇒
	 * "先打折再减固定值"可表达，反之不可（`STAT-1` 明令禁止第二份折叠）。
	 *
	 * **值域 = `[0, +∞)`**：折叠后钳下界到 0——"减免过度 ⇒ 免费"是**合法配置结果**。
	 *
	 * **★ `OutBaseAmount` 为什么必须存在（装置拒绝面检查 `d` 实测抓到的设计缺陷）**：
	 * "钳 0"把**两件语义完全不同的事**压成同一个值——
	 * ① **负基数**（作者把 `Amount` 填成 -50）= **配置错误**、"倒给资源"的漏洞形态 ⇒ MUST 拦；
	 * ② **折叠后转负**（基数 30、减免 -100）= **减免过度** = 合法的"免费" ⇒ MUST 钳 0、不报错。
	 * 若只返回钳过的结果，`CanAfford` 就**永远看不到①**（它已被钳成 0）
	 * ⇒ 检查 `d` 当场从 `false` 翻成 `true`。⇒ 调用方 MUST 能拿到**未钳的基数**来判①。
	 *
	 * @param Context 求值上下文。
	 * @param OutBaseAmount 输出**未钳的基数**（可空）。**`CanAfford` MUST 传它**——负基数的拦截依赖它。
	 * @return 返回本次生效的消耗量（已钳到 `[0, +∞)`）。
	 */
	double ResolveEffectiveAmount(const FTcsSkillCostContext& Context, double* OutBaseAmount = nullptr) const;

#pragma endregion
};



/**
 * 消耗配置 —— 一个技能的完整消耗定义（`D5-10` 的 `FCostConfig`）。
 *
 * **★ 2026-10-12 用户改点后的形状（策略模式）**：原 `FTcsCostConfig` 是
 * `{Policy 枚举, Timing, ResourceAttr, Amount}`——`Policy` 枚举把"消耗什么"写死成三档。
 * 改点后收敛为**两件事**：**何时付**（`Timing`，框架认识）+ **付什么 / 怎么付**
 * （`CostMode`，一个策略实例）。
 *
 * **"不消耗"的唯一表达 = `Timing == ECT_None`**（2026-10-12 用户裁定："只要 CostTiming 为 None，
 * 那就不用管后面了，因为一定证明这个技能没有消耗"）。
 *
 * **★ `CostMode` 为空但 `Timing != None` ⇒ 配置错误、fail-closed**（拦 + `Warning`）——
 * **MUST NOT** 静默按"不消耗"处理。判据：**"声明不消耗"（`Timing = None`，零日志）**与
 * **"想配消耗但没配清"（`Timing != None` 而策略为空，拦住 + `Warning`）**是两件事；
 * 混成一类会让后者**表现成一切正常**（本仓反复在拦的那类 bug）。
 *
 * **策略实例归 Def 资产持有**（同 `FStateStackPolicy::CustomDecision` 的既有口径：
 * 单字段暴露 ⇒ 类/载荷配对 bug 类消失）。**⇒ 运行期不可换策略**，这是**刻意的**
 * （配置即成员）；若宿主需"运行期换策略"，那是另一项需求。
 *
 * **原字面留痕（不对称判据的由来与更新，MUST NOT 改写）**：
 * > **与冷却策略的关键差别（MUST 读，防按对称性误改）**：冷却侧的"无冷却"由**空数组**表达
 * > （`FTcsCooldownPolicy::Tracks`），而消耗侧的"不消耗"由**本结构体的 `Policy` 枚举**表达。
 * > 判据：冷却的形状天然是"零到多条轨道"（数组），消耗的形状是"零或一种资源扣减"（标量 + 键），
 * > 后者没有可枚举的多个元素 ⇒ 只能用档位名表达"没有"。**这是一处刻意的不对称，不是遗漏。**
 *
 * **★ 该判据的更新（2026-10-12）**：原文以"消耗没有可枚举的多个元素"论证**不能用数组**——
 * 该论证在同日的"成本组"提案中一度过期，随后**用户裁定框架不做多资源组合**（宿主自己写策略）
 * ⇒ **不对称依旧成立**，但**理由改为**：消耗是"零或一次**策略分派**"（框架不认识策略内部的
 * 元素个数）⇒ 仍只能用档位名表达"没有"。**结论不变、理由已更新**，故原字面保留作痕迹。
 */
USTRUCT(BlueprintType)
struct TCSSKILL_API FTcsSkillCostConfig
{
	GENERATED_BODY()

// 时机
#pragma region Timing

public:
	// 什么时候付（`ECT_None` = 该技能一定没有消耗 ⇒ `CostMode` 一概不看；见 ETcsSkillCostTiming）
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Cost Config")
	ETcsSkillCostTiming Timing = ETcsSkillCostTiming::ECT_None;

#pragma endregion


// 策略
#pragma region Policy

public:
	/**
	 * 消耗策略实例（裸 `FInstancedStruct`；类型收窄由 `BaseStruct` 元数据给出）。
	 *
	 * **为什么是裸 `FInstancedStruct` 而不是 `TInstancedStruct<FTcsSkillCostPolicy>`**
	 * （2026-09-24 换型口径，MUST NOT 改回）：`TInstancedStruct<T>` 字段在宿主脚本层
	 * **导出为空壳**（无字段读写代码）⇒ 脚本层配不了任何策略；裸形态是引擎官方注释给出的写法，
	 * 且 StateTree 用的正是裸形态。**代价（明示接受）**：丢编译期 `enable_if` 限定 ⇒
	 * 异族赋值不再编译报错，改由**运行期**校验兜底。
	 *
	 * **取内层一律用单参数 `GetPtr<FTcsSkillCostPolicy>()`**：它在
	 * `StructMemory == nullptr`（空载荷）**或**类型不符时**返回 `nullptr`**
	 * ⇒ 一个 `if (!Ptr)` 同时覆盖"载荷缺失"与"类型不符"。
	 * **★ MUST NOT 用双模板参数重载** `GetPtr<T, BaseT>()`——它在 `Shipping` 下**完全不校验**
	 * （`StructUtils.h` 源码自认 "this may crash in shipping!"），同一份代码编辑器里安全、
	 * Shipping 里类型混淆，**编译查不出来**。
	 *
	 * **空值的两种含义 MUST 分开处置**（见结构体注释）：`Timing == None` 时空值合法（不看它）；
	 * `Timing != None` 时空值 = **配置错误 ⇒ fail-closed**。
	 */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Cost Config",
		meta = (BaseStruct = "/Script/TcsSkill.TcsSkillCostPolicy"))
	FInstancedStruct CostMode;

#pragma endregion
};
