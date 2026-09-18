// Copyright Tirefly. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Subsystems/WorldSubsystem.h"

#include "Attribute/TcsAttributeDef.h"
#include "Attribute/TcsAttributeInstance.h"
#include "Attribute/TcsAttrModInstance.h"
#include "Attribute/TcsAttributeName.h"
#include "Attribute/TcsAttributeStore.h"
#include "Handle/TcsCombatEntityHandle.h"

#include "TcsAttributeSubsystem.generated.h"



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
	 * @param UnitName 单位名（调试/屏显用，不参与键控——同名单位可共存）。
	 * @return 返回新单位的实体句柄（永不为无效值）。
	 */
	FTcsCombatEntityHandle RegisterUnit(FName UnitName);

	/**
	 * 注销单位：释放该单位的全部属性实例与注册记录。
	 *
	 * @param Unit 单位句柄；未注册或已注销的句柄 ensure 拦截并忽略。
	 */
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
	 * `DefId` 即属性名）。
	 * 属性名由调用方显式给出：定义行内**不带 id**（2026-09-17 用户口径——DataTable 的身份是
	 * 行名、运行期身份是资产的 `[PrimaryAssetType, DefId]`，行内再存一份只会造成双真相）。
	 * 拒绝面（ensure 提示 + 返回 false）：属性名为空、同属性名重复登记
	 * （D2-1：词表重名 = 加载期错误，不得静默覆写）。
	 *
	 * @param Attribute 属性名（= 定义资产 DefId = 词表行名）。
	 * @param DefRow 属性定义行（字段形状唯一声明处）。
	 * @return 返回是否登记成功。
	 */
	bool RegisterAttributeDef(
		const FTcsAttributeName& Attribute,
		const FTcsAttributeDefTableRow& DefRow);

	/**
	 * 查询已登记定义（未登记返回 nullptr；正常查询路径，不触发 ensure）。
	 *
	 * @param Attribute 属性名。
	 * @return 返回该属性的定义行；未登记返回 nullptr。
	 */
	const FTcsAttributeDefTableRow* FindAttributeDef(const FTcsAttributeName& Attribute) const;

#pragma endregion


// 单位属性
#pragma region Attribute

public:
	/**
	 * 添加属性（单位侧唯一入口）：按**属性名**在该单位属性容器内建实例——定义由本门面
	 * 从定义表内部解析，调用方不需要也不应该知道定义数据。
	 * 拒绝面（ensure 提示 + 返回 false）：单位未注册、属性名为空、同单位重复添加同属性、
	 * 定义未登记、定义行的动态边界自引用（以自身为边界 = 循环依赖，D2-4）。
	 *
	 * @param Unit 单位句柄。
	 * @param Attribute 属性名（= 定义行 DefId）。
	 * @return 返回是否添加成功。
	 */
	bool AddAttribute(
		FTcsCombatEntityHandle Unit,
		const FTcsAttributeName& Attribute);

	/**
	 * 移除属性（整条属性下线）：销毁该属性实例及其修正器槽位内容。
	 * 语义说明：槽位内修正器随实例一并丢弃——来源方（buff/装备等）的级联撤销仍按其自身生命周期
	 * 走 `RemoveBySource`（聚合管线轮），二者不互相替代；调用方需保证移除时机不与来源回收竞态。
	 * 拒绝面（ensure 提示 + 返回 false）：单位未注册、属性名为空、该单位未持有此属性。
	 *
	 * @param Unit 单位句柄。
	 * @param Attribute 属性名。
	 * @return 返回是否移除成功。
	 */
	bool RemoveAttribute(
		FTcsCombatEntityHandle Unit,
		const FTcsAttributeName& Attribute);

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
	// 属性定义表（键 = 属性名 = DefId；值 = 定义行——AddAttribute 的解析源）
	TMap<FTcsAttributeName, FTcsAttributeDefTableRow> DefTable;

	// 单位注册表（键 = 实体身份句柄；值 = 该单位的属性容器）
	// 存 TUniquePtr：容器指针因此不随"新增其他单位"失效（TMap 元素地址会随扩容搬移——引擎事实）
	TMap<FTcsCombatEntityHandle, TUniquePtr<FTcsAttributeStore>> Stores;

	// 单位名表（仅调试/屏显用，不参与键控）
	TMap<FTcsCombatEntityHandle, FName> UnitNames;

	// 实体身份发号器（R3 由本门面发号，见 RegisterUnit 说明）
	FTcsCombatEntityHandleRegistry EntityRegistry;

	// 解引用外层表的间接层（命中返回容器指针；未命中返回 nullptr，不 ensure——调用方已判定）
	FTcsAttributeStore* ResolveStore(FTcsCombatEntityHandle Unit);
	const FTcsAttributeStore* ResolveStore(FTcsCombatEntityHandle Unit) const;

#pragma endregion
};
