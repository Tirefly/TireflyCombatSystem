// Copyright Tirefly. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Subsystems/WorldSubsystem.h"

#include "Attribute/TcsAttributeDef.h"
#include "Attribute/TcsAttributeInstance.h"
#include "Attribute/TcsAttrModInstance.h"
#include "GameplayTagContainer.h"
#include "Attribute/TcsAttributeStore.h"
#include "Handle/TcsCombatEntityHandle.h"

#include "TcsAttributeSubsystem.generated.h"

// 聚合管线（模块内部类型；仅前向声明——公开头不暴露 Private 头）
class FTcsAttributePipeline;



/**
 * 属性系统门面（M2 数据宿主，02 §2.2a）：世界级子系统，持**属性定义表**、单位注册表与每单位属性容器。
 *
 * 非 Tickable（D2-8）：M2 不认识时间——聚合由调用方驱动（聚合管线），本门面只做存取与定义加载，
 * 自身不推进任何状态（零自 tick、零定时器）。
 *
 * 调用面纪律（2026-09-17 用户口径）：单位侧只按**属性名**操作（`AddAttribute` / `RemoveAttribute`），
 * 定义一律由本门面内部按名解析——调用方 MUST NOT 传定义数据（那是宿主单位不该知道的内部流程）。
 */
UCLASS()
class TCSATTRIBUTE_API UTcsAttributeSubsystem : public UWorldSubsystem
{
	GENERATED_BODY()

// 生命期
#pragma region Lifetime

public:
	// 构造函数：建聚合管线（管线持本门面引用——数据宿主与单位注册表）
	UTcsAttributeSubsystem();

	// 析构函数：显式定义（管线为前向声明的不完整类型，销毁点须在 .cpp 内——PIMPL 约束）
	virtual ~UTcsAttributeSubsystem() override;

	// 世界类型过滤：仅游戏世界（Game/PIE/GamePreview）实例化——属性是运行时状态（对齐时钟/总线门面）
	virtual bool DoesSupportWorldType(const EWorldType::Type WorldType) const override;

	// 反初始化：确定性清空全部单位与属性实例（容器自持数据，无外部资源待释放）
	virtual void Deinitialize() override;

#pragma endregion


// 单位注册
#pragma region Unit

public:
	/**
	 * 注册单位：发放实体身份句柄并建空属性容器。
	 * 发号说明：R3 由本门面发号；M6 世界注册表落地后移交发号（句柄类型与消费者签名不变）。
	 *
	 * **反射面（2026-09-24，提案 `add-scripting-reflection-surface`）**：`UFUNCTION()` 无 specifier
	 * 是有意的——形参/返回全反射，但同批口径统一（避免蓝图参数校验带来的隐式承诺面）。
	 * 本方法是**脚本层起链的必要前置**：没有注册实体就没有合法的 `Caster` 句柄，
	 * 伤害流程会因"单位未注册"而 ensure（实测先例）。
	 *
	 * @param UnitName 单位名（调试/屏显用，不参与键控——同名单位可共存）。
	 * @return 返回新单位的实体句柄（永不为无效值）。
	 */
	UFUNCTION()
	FTcsCombatEntityHandle RegisterUnit(FName UnitName);

	/**
	 * 注销单位：释放该单位的全部属性实例与注册记录。
	 *
	 * **反射面（2026-09-24）**：`UFUNCTION()` 无 specifier（口径同上）。
	 *
	 * @param Unit 单位句柄；未注册或已注销的句柄 ensure 拦截并忽略。
	 */
	UFUNCTION()
	void UnregisterUnit(FTcsCombatEntityHandle Unit);

	/**
	 * 读取单位名（调试/屏显用）。
	 *
	 * @param Unit 单位句柄。
	 * @return 返回注册时登记的单位名；未知句柄返回 NAME_None。
	 */
	FName GetUnitName(FTcsCombatEntityHandle Unit) const;

#pragma endregion


// 属性定义表
#pragma region Definition

public:
	/**
	 * 登记属性定义（宿主 / DefLibrary 加载定义资产后调用——`UTcsAttributeDef::Def` 即行载荷、
	 * `DefTag` 即属性身份）。
	 * 属性身份由调用方显式给出：定义行内**不再单独存一份"行名 id"**（2026-09-17 用户口径 + 2026-09-22
	 * tag 化——`RowName` 降为编辑期定位，内容身份是资产的 `[PrimaryAssetType, DefTag.GetTagName()]`）。
	 * 拒绝面（ensure 提示 + 返回 false）：属性身份无效、同身份重复登记
	 * （D2-1：词表重名 = 加载期错误，不得静默覆写）。
	 *
	 * @param Attribute 属性身份 tag（= 定义资产 DefTag）。
	 * @param DefData 属性定义数据（字段形状唯一声明处；2026-09-22 改造：行与数据分离，本口只收数据）。
	 * @return 返回是否登记成功。
	 */
	bool RegisterAttributeDef(
		const FGameplayTag& Attribute,
		const FTcsAttributeDefData& DefData);

	/**
	 * 查询已登记定义（未登记返回 nullptr；正常查询路径，不触发 ensure）。
	 *
	 * @param Attribute 属性名。
	 * @return 返回该属性的定义数据；未登记返回 nullptr。
	 */
	const FTcsAttributeDefData* FindAttributeDef(const FGameplayTag& Attribute) const;

#pragma endregion


// 单位属性
#pragma region Attribute

public:
	/**
	 * 添加属性（单位侧唯一入口）：按**属性名**在该单位属性容器内建实例——定义由本门面
	 * 从定义表内部解析，调用方不需要也不应该知道定义数据。
	 * **解冻优先**：暂存区已有同名实例 → 整条搬回（基础值/边界/值域模式/槽位原样保留，输出解冻日志）；
	 * 否则按定义行新建（输出新建日志）。
	 * 拒绝面（ensure 提示 + 返回 false）：单位未注册、属性名为空、该单位已持有同名属性、
	 * 定义未登记（仅新建路径需要）、定义行的动态边界自引用（以自身为边界 = 循环依赖，D2-4）。
	 *
	 * @param Unit 单位句柄。
	 * @param Attribute 属性身份 tag（= 定义资产 `DefTag`）。
	 * @return 返回是否添加成功。
	 */
	UFUNCTION()
	bool AddAttribute(
		FTcsCombatEntityHandle Unit,
		const FGameplayTag& Attribute);

	/**
	 * 改写属性基础值（等级成长等宿主升级事务的落点，02 §2.2a）：标脏并按事务纪律重算/广播
	 * （未开批 = 隐式批，立即生效）——属事务的"改基值"写操作类。
	 * 拒绝面（ensure 提示 + 返回 false）：单位未注册、属性名为空、该单位未持有此属性。
	 *
	 * @param Unit 单位句柄。
	 * @param Attribute 属性名。
	 * @param NewBaseValue 新的基础值。
	 * @return 返回是否改写成功。
	 */
	UFUNCTION()
	bool SetBaseValue(
		FTcsCombatEntityHandle Unit,
		const FGameplayTag& Attribute,
		double NewBaseValue);

	/**
	 * 移除属性（整条属性下线）= **冻结**：把整条实例（基础值/边界/值域模式/修正器槽位）从属性容器
	 * 搬入暂存区 `FrozenAttributes`——**不销毁、不丢弃槽位内容**，输出冻结日志。
	 * 与来源撤销的关系：来源方（buff/装备等）的级联撤销仍按其自身生命周期走 `RemoveBySource`；
	 * 被冻结实例的槽位也在 `RemoveBySource` 的扫描面内（来源在冻结期结束不会被遗留）。
	 * 拒绝面（ensure 提示 + 返回 false）：单位未注册、属性名为空、该单位未持有此属性。
	 *
	 * @param Unit 单位句柄。
	 * @param Attribute 属性名。
	 * @return 返回是否移除成功。
	 */
	bool RemoveAttribute(
		FTcsCombatEntityHandle Unit,
		const FGameplayTag& Attribute);

#pragma endregion


// 聚合管线（门面转发——消费者只认识门面）
#pragma region Pipeline

public:
	/**
	 * 求值属性当前值（脏则惰性重算；事务期读旧值）。
	 * 单位未注册或属性未定义时返回 0（读取是正常查询路径，不 ensure）。
	 *
	 * @param Unit 单位句柄。
	 * @param Attribute 属性名。
	 * @return 返回当前值。
	 * **反射面（2026-09-24）**：`UFUNCTION()` 无 specifier（口径同 `RegisterUnit`）——
	 * 脚本层读属性当前值是常见需求（写 Buff 逻辑要按血量分支）。
	 */
	UFUNCTION()
	double EvaluateCurrent(FTcsCombatEntityHandle Unit, const FGameplayTag& Attribute);

	/**
	 * 读未提交候选值（预览；不落账、不广播）。无进行中的批时等同求值当前值。
	 *
	 * **反射面（2026-09-24）**：`UFUNCTION()` 无 specifier（口径同上）。
	 *
	 * @param Unit 单位句柄。
	 * @param Attribute 属性名。
	 * @return 返回候选值。
	 */
	UFUNCTION()
	double PeekPending(FTcsCombatEntityHandle Unit, const FGameplayTag& Attribute);

	/**
	 * 挂修正器（标脏；未开批时立即重算 + 广播 = 隐式批）。
	 * 单位未注册或目标属性无实例时忽略并留日志（返回 false，不 ensure——D2-14）。
	 *
	 * @param Unit 单位句柄。
	 * @param Modifier 修正器（Target 指向目标属性）。
	 * @return 返回是否挂载成功。
	 */
	bool ApplyModifier(FTcsCombatEntityHandle Unit, const FTcsAttrModInstance& Modifier);

	/**
	 * 按来源级联摘除（扫描面含实例槽位与冻结暂存区）。
	 *
	 * @param Unit 单位句柄。
	 * @param Source 来源句柄。
	 * @return 返回摘除条数（0 = 无匹配，正常路径）。
	 */
	int32 RemoveBySource(FTcsCombatEntityHandle Unit, const FTcsSourceHandle& Source);

	// 开始变更批（嵌套计数；最外层提交才统一重算 + 广播）
	void BeginBatch(FTcsCombatEntityHandle Unit);

	// 提交变更批（唯一提交点；批内多次变更只重算一次、每属性最多广播一次）
	void Commit(FTcsCombatEntityHandle Unit);

#pragma endregion


// 存取
#pragma region Access

public:
	/**
	 * 取单位属性容器：消费者（聚合管线/战斗实体组件）可长期缓存该指针——**外层表存的是
	 * `TUniquePtr`，故新增其他单位不会让既有容器指针失效**（引擎事实 2026-09-17：`TMap`/`TSet`
	 * 元素存在连续缓冲里，扩容即搬移，直接按值存值类型会搬走指针）。
	 * 缓存纪律：句柄注销后指针失效（该单位容器被释放）——注销与缓存不可交叉。
	 * 容器**内部**实例指针不保证跨插入稳定（同一 TMap 语义）——实例按属性名查询，不缓存指针。
	 *
	 * @param Unit 单位句柄。
	 * @return 返回该单位的属性容器；未注册或已注销句柄返回 nullptr（ensure 提示）。
	 */
	FTcsAttributeStore* GetStore(FTcsCombatEntityHandle Unit);

	// 取单位属性容器（只读）
	const FTcsAttributeStore* GetStore(FTcsCombatEntityHandle Unit) const;

#pragma endregion


// 存储
#pragma region Storage

private:
	// 属性定义表（键 = 属性身份 tag；值 = 定义数据——AddAttribute 的解析源）
	TMap<FGameplayTag, FTcsAttributeDefData> DefTable;

	// 单位注册表（键 = 实体身份句柄；值 = 该单位的属性容器）
	// 存 TUniquePtr：容器指针因此不随"新增其他单位"失效（TMap 元素地址会随扩容搬移——引擎事实）
	TMap<FTcsCombatEntityHandle, TUniquePtr<FTcsAttributeStore>> Stores;

	// 单位名表（仅调试/屏显用，不参与键控）
	TMap<FTcsCombatEntityHandle, FName> UnitNames;

	// 实体身份发号器（R3 由本门面发号，见 RegisterUnit 说明）
	FTcsCombatEntityHandleRegistry EntityRegistry;

	// 聚合管线（模块内部类型，仅前向声明持有——公开头不暴露 Private 头；构造建、Deinitialize 清）
	TUniquePtr<FTcsAttributePipeline> Pipeline;

#pragma endregion


// 内部查询（门面与管线共用）
#pragma region Internal

public:
	/**
	 * 非确保解析（句柄 → 容器）：命中返回容器指针，未命中返回 nullptr。
	 * 与 `GetStore` 的区别：**不 ensure**——供管线与内部读取路径使用（读取是正常查询路径，
	 * 单位未注册不算契约违规）。
	 *
	 * @param Unit 单位句柄。
	 * @return 返回该单位的属性容器；未注册返回 nullptr。
	 */
	FTcsAttributeStore* ResolveStore(FTcsCombatEntityHandle Unit);

	// 非确保解析（只读）
	const FTcsAttributeStore* ResolveStore(FTcsCombatEntityHandle Unit) const;

#pragma endregion
};
