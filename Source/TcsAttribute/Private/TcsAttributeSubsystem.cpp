// Copyright Tirefly. All Rights Reserved.

#include "TcsAttributeSubsystem.h"

#include "TcsAttributeLogChannel.h"



bool UTcsAttributeSubsystem::DoesSupportWorldType(const EWorldType::Type WorldType) const
{
	// 仅游戏世界实例化：编辑器预览/检查器世界不创建（对齐时钟/总线门面）
	return WorldType == EWorldType::Game ||
		WorldType == EWorldType::PIE ||
		WorldType == EWorldType::GamePreview;
}

void UTcsAttributeSubsystem::Deinitialize()
{
	// 确定性清理：属性实例随容器释放（零外部资源持有）
	Stores.Empty();
	UnitNames.Empty();

	Super::Deinitialize();
}

FTcsCombatEntityHandle UTcsAttributeSubsystem::RegisterUnit(FName UnitName)
{
	const FTcsCombatEntityHandle Unit = EntityRegistry.Allocate();

	// 建空容器 + 登记调试名（句柄即唯一键，同名单位允许共存）
	Stores.Add(Unit, MakeUnique<FTcsAttributeStore>());
	UnitNames.Add(Unit, UnitName);

	UE_LOG(LogTcsAttribute, Log, TEXT("UTcsAttributeSubsystem: 单位注册 Unit=%llu Name=%s"),
		Unit.Id, *UnitName.ToString());

	return Unit;
}

void UTcsAttributeSubsystem::UnregisterUnit(FTcsCombatEntityHandle Unit)
{
	if (!ensureMsgf(Stores.Contains(Unit),
		TEXT("UTcsAttributeSubsystem::UnregisterUnit: 未注册或已注销的单位句柄（Id=%llu）"), Unit.Id))
	{
		return;
	}

	// 属性实例随容器整块释放（无需逐条摘除：来源级联只针对单位存活期的修正器）
	Stores.Remove(Unit);
	UnitNames.Remove(Unit);

	UE_LOG(LogTcsAttribute, Log, TEXT("UTcsAttributeSubsystem: 单位注销 Unit=%llu"), Unit.Id);
}

FName UTcsAttributeSubsystem::GetUnitName(FTcsCombatEntityHandle Unit) const
{
	const FName* FoundName = UnitNames.Find(Unit);
	return FoundName ? *FoundName : NAME_None;
}

bool UTcsAttributeSubsystem::RegisterAttributeDef(
	const FTcsAttributeName& Attribute,
	const FTcsAttributeDefTableRow& DefRow)
{
	if (!ensureMsgf(!Attribute.IsNone(),
		TEXT("UTcsAttributeSubsystem::RegisterAttributeDef: 属性名为空（属性名即定义表的键）")))
	{
		return false;
	}

	if (!ensureMsgf(!DefTable.Contains(Attribute),
		TEXT("UTcsAttributeSubsystem::RegisterAttributeDef: 属性定义重复登记（属性 %s）——词表重名属加载期错误"),
		*Attribute.Name.ToString()))
	{
		return false;
	}

	DefTable.Add(Attribute, DefRow);

	UE_LOG(LogTcsAttribute, Log, TEXT("UTcsAttributeSubsystem: 属性定义登记 Attribute=%s BaseValue=%.6f Domain=%d"),
		*Attribute.Name.ToString(), DefRow.BaseValue, static_cast<int32>(DefRow.ValueDomain));

	return true;
}

const FTcsAttributeDefTableRow* UTcsAttributeSubsystem::FindAttributeDef(const FTcsAttributeName& Attribute) const
{
	return DefTable.Find(Attribute);
}

bool UTcsAttributeSubsystem::AddAttribute(
	FTcsCombatEntityHandle Unit,
	const FTcsAttributeName& Attribute)
{
	// 直接查表而非走 GetStore：拒绝面只在此处报一次 ensure（避免同一违规双站点触发）
	FTcsAttributeStore* Store = ResolveStore(Unit);
	if (!ensureMsgf(Store != nullptr,
		TEXT("UTcsAttributeSubsystem::AddAttribute: 单位未注册（Id=%llu）"), Unit.Id))
	{
		return false;
	}

	if (!ensureMsgf(!Attribute.IsNone(),
		TEXT("UTcsAttributeSubsystem::AddAttribute: 属性名为空（单位 Id=%llu）"), Unit.Id))
	{
		return false;
	}

	if (!ensureMsgf(!Store->Attributes.Contains(Attribute),
		TEXT("UTcsAttributeSubsystem::AddAttribute: 属性重复添加（单位 Id=%llu，属性 %s）"),
		Unit.Id, *Attribute.Name.ToString()))
	{
		return false;
	}

	// 定义解析在门面内部完成（2026-09-17 用户口径：单位侧只认属性名，不传定义数据）
	const FTcsAttributeDefTableRow* DefRow = DefTable.Find(Attribute);
	if (!ensureMsgf(DefRow != nullptr,
		TEXT("UTcsAttributeSubsystem::AddAttribute: 属性定义未登记（属性 %s——宿主须先 RegisterAttributeDef）"),
		*Attribute.Name.ToString()))
	{
		return false;
	}

	// 动态边界自引用禁止（D2-4：以自身为边界 = 循环依赖）
	const bool bSelfReferencedBound =
		(DefRow->Bounds.Min.Mode == ETcsAttributeBoundMode::ABM_Dynamic && DefRow->Bounds.Min.DynamicAttribute == Attribute) ||
		(DefRow->Bounds.Max.Mode == ETcsAttributeBoundMode::ABM_Dynamic && DefRow->Bounds.Max.DynamicAttribute == Attribute);
	if (!ensureMsgf(!bSelfReferencedBound,
		TEXT("UTcsAttributeSubsystem::AddAttribute: 动态边界自引用（单位 Id=%llu，属性 %s 以自身为边界）"),
		Unit.Id, *Attribute.Name.ToString()))
	{
		return false;
	}

	// 定义字段在实例内展开（实例不持定义行引用——热路径不回查定义，D2-1/D2-9）
	FTcsAttributeInstance Instance;
	Instance.Attr = Attribute;
	Instance.BaseValue = DefRow->BaseValue;
	Instance.CachedCurrent = DefRow->BaseValue;	// 添加期初值即基础值（此后缓存值的唯一生产者是聚合管线）
	Instance.bDirty = false;
	Instance.Bounds = DefRow->Bounds;
	Instance.ValueDomain = DefRow->ValueDomain;
	Store->Attributes.Add(Attribute, MoveTemp(Instance));

	UE_LOG(LogTcsAttribute, Log, TEXT("UTcsAttributeSubsystem: 属性添加 Unit=%llu Attribute=%s BaseValue=%.6f"),
		Unit.Id, *Attribute.Name.ToString(), DefRow->BaseValue);

	return true;
}

bool UTcsAttributeSubsystem::RemoveAttribute(
	FTcsCombatEntityHandle Unit,
	const FTcsAttributeName& Attribute)
{
	FTcsAttributeStore* Store = ResolveStore(Unit);
	if (!ensureMsgf(Store != nullptr,
		TEXT("UTcsAttributeSubsystem::RemoveAttribute: 单位未注册（Id=%llu）"), Unit.Id))
	{
		return false;
	}

	if (!ensureMsgf(!Attribute.IsNone(),
		TEXT("UTcsAttributeSubsystem::RemoveAttribute: 属性名为空（单位 Id=%llu）"), Unit.Id))
	{
		return false;
	}

	// 实例连同其修正器槽位一并移除（来源方的级联撤销按其自身生命周期走 RemoveBySource，二者不互相替代）
	if (!ensureMsgf(Store->Attributes.Remove(Attribute) > 0,
		TEXT("UTcsAttributeSubsystem::RemoveAttribute: 该单位未持有此属性（单位 Id=%llu，属性 %s）"),
		Unit.Id, *Attribute.Name.ToString()))
	{
		return false;
	}

	UE_LOG(LogTcsAttribute, Log, TEXT("UTcsAttributeSubsystem: 属性移除 Unit=%llu Attribute=%s"),
		Unit.Id, *Attribute.Name.ToString());

	return true;
}

FTcsAttributeStore* UTcsAttributeSubsystem::GetStore(FTcsCombatEntityHandle Unit)
{
	// 悬空表现为"查不到"（句柄无代际段）：返回 nullptr + ensure 提示——Development 期暴露传错句柄
	if (!ensureMsgf(Stores.Contains(Unit),
		TEXT("UTcsAttributeSubsystem::GetStore: 未注册或已注销的单位句柄（Id=%llu）"), Unit.Id))
	{
		return nullptr;
	}

	return ResolveStore(Unit);
}

const FTcsAttributeStore* UTcsAttributeSubsystem::GetStore(FTcsCombatEntityHandle Unit) const
{
	if (!ensureMsgf(Stores.Contains(Unit),
		TEXT("UTcsAttributeSubsystem::GetStore: 未注册或已注销的单位句柄（Id=%llu）"), Unit.Id))
	{
		return nullptr;
	}

	return ResolveStore(Unit);
}

FTcsAttributeStore* UTcsAttributeSubsystem::ResolveStore(FTcsCombatEntityHandle Unit)
{
	// 解引用外层表的间接层：容器指针因此不随"新增其他单位"失效（TMap 元素地址会随扩容搬移）
	const TUniquePtr<FTcsAttributeStore>* Found = Stores.Find(Unit);
	return Found ? Found->Get() : nullptr;
}

const FTcsAttributeStore* UTcsAttributeSubsystem::ResolveStore(FTcsCombatEntityHandle Unit) const
{
	const TUniquePtr<FTcsAttributeStore>* Found = Stores.Find(Unit);
	return Found ? Found->Get() : nullptr;
}
