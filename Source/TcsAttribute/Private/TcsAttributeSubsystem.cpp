// Copyright Tirefly. All Rights Reserved.

#include "TcsAttributeSubsystem.h"

#include "Attribute/TcsAttributePipeline.h"
#include "TcsAttributeLogChannel.h"



UTcsAttributeSubsystem::UTcsAttributeSubsystem()
{
	// 建聚合管线（持本门面引用：数据宿主与实体注册表）
	Pipeline = MakeUnique<FTcsAttributePipeline>(*this);
}

UTcsAttributeSubsystem::~UTcsAttributeSubsystem() = default;

bool UTcsAttributeSubsystem::DoesSupportWorldType(const EWorldType::Type WorldType) const
{
	// 仅游戏世界实例化：编辑器预览/检查器世界不创建（对齐时钟/总线门面）
	return WorldType == EWorldType::Game ||
		WorldType == EWorldType::PIE ||
		WorldType == EWorldType::GamePreview;
}

void UTcsAttributeSubsystem::Deinitialize()
{
	// 确定性清理：属性实例与冻结暂存区随容器释放（零外部资源持有）
	Pipeline.Reset();
	Stores.Empty();
	EntityNames.Empty();

	Super::Deinitialize();
}

UTcsAttributeSubsystem* UTcsAttributeSubsystem::Resolve(const UWorld* World)
{
	// 唯一查找点：两个消费模块的白名单薄壳都以它为实现（将来补注入契约只换这里）
	return World ? World->GetSubsystem<UTcsAttributeSubsystem>() : nullptr;
}

FTcsCombatEntityHandle UTcsAttributeSubsystem::RegisterEntity(FName EntityName)
{
	const FTcsCombatEntityHandle Entity = EntityRegistry.Allocate();

	// 建空容器 + 登记调试名（句柄即唯一键，同名实体允许共存）
	Stores.Add(Entity, MakeUnique<FTcsAttributeStore>());
	EntityNames.Add(Entity, EntityName);

	UE_LOG(LogTcsAttribute, Log, TEXT("UTcsAttributeSubsystem: 实体注册 Entity=%llu Name=%s"),
		Entity.Id, *EntityName.ToString());

	return Entity;
}

void UTcsAttributeSubsystem::UnregisterEntity(FTcsCombatEntityHandle Entity)
{
	if (!ensureMsgf(Stores.Contains(Entity),
		TEXT("UTcsAttributeSubsystem::UnregisterEntity: 未注册或已注销的实体句柄（Id=%lld）"), Entity.Id))
	{
		return;
	}

	// 属性实例随容器整块释放（无需逐条摘除：来源级联只针对实体存活期的修正器）
	Stores.Remove(Entity);
	EntityNames.Remove(Entity);

	UE_LOG(LogTcsAttribute, Log, TEXT("UTcsAttributeSubsystem: 实体注销 Entity=%llu"), Entity.Id);
}

FName UTcsAttributeSubsystem::GetEntityName(FTcsCombatEntityHandle Entity) const
{
	const FName* FoundName = EntityNames.Find(Entity);
	return FoundName ? *FoundName : NAME_None;
}

bool UTcsAttributeSubsystem::RegisterAttributeDef(
	const FGameplayTag& Attribute,
	const FTcsAttributeDefData& DefData)
{
	if (!ensureMsgf(Attribute.IsValid(),
		TEXT("UTcsAttributeSubsystem::RegisterAttributeDef: 属性名为空（属性名即定义表的键）")))
	{
		return false;
	}

	if (!ensureMsgf(!DefTable.Contains(Attribute),
		TEXT("UTcsAttributeSubsystem::RegisterAttributeDef: 属性定义重复登记（属性 %s）——词表重名属加载期错误"),
		*Attribute.GetTagName().ToString()))
	{
		return false;
	}

	DefTable.Add(Attribute, DefData);

	UE_LOG(LogTcsAttribute, Log, TEXT("UTcsAttributeSubsystem: 属性定义登记 Attribute=%s BaseValue=%.6f Domain=%d"),
		*Attribute.GetTagName().ToString(), DefData.BaseValue, static_cast<int32>(DefData.ValueDomain));

	return true;
}

const FTcsAttributeDefData* UTcsAttributeSubsystem::FindAttributeDef(const FGameplayTag& Attribute) const
{
	return DefTable.Find(Attribute);
}

bool UTcsAttributeSubsystem::AddAttribute(
	FTcsCombatEntityHandle Entity,
	const FGameplayTag& Attribute)
{
	// 直接查表而非走 GetStore：拒绝面只在此处报一次 ensure（避免同一违规双站点触发）
	FTcsAttributeStore* Store = ResolveStore(Entity);
	if (!ensureMsgf(Store != nullptr,
		TEXT("UTcsAttributeSubsystem::AddAttribute: 实体未注册（Id=%lld）"), Entity.Id))
	{
		return false;
	}

	if (!ensureMsgf(Attribute.IsValid(),
		TEXT("UTcsAttributeSubsystem::AddAttribute: 属性名为空（实体 Id=%lld）"), Entity.Id))
	{
		return false;
	}

	if (!ensureMsgf(!Store->Attributes.Contains(Attribute),
		TEXT("UTcsAttributeSubsystem::AddAttribute: 属性重复添加（实体 Id=%lld，属性 %s）"),
		Entity.Id, *Attribute.GetTagName().ToString()))
	{
		return false;
	}

	// 解冻优先：暂存区已有同名实例 → 整条搬回（基础值/边界/值域模式/修正器条目原样保留）
	if (FTcsAttributeInstance* FrozenInstance = Store->FrozenAttributes.Find(Attribute))
	{
		const int32 SlotCount = FrozenInstance->AttrModInstances.Num();
		Store->Attributes.Add(Attribute, MoveTemp(*FrozenInstance));
		Store->FrozenAttributes.Remove(Attribute);

		UE_LOG(LogTcsAttribute, Log,
			TEXT("UTcsAttributeSubsystem: 属性解冻 Entity=%llu Attribute=%s 修正器条目=%d（基础值取回冻结前的值）"),
			Entity.Id, *Attribute.GetTagName().ToString(), SlotCount);

		return true;
	}

	// 定义解析在门面内部完成（2026-09-17 用户口径：实体侧只认属性名，不传定义数据）
	const FTcsAttributeDefData* DefData = DefTable.Find(Attribute);
	if (!ensureMsgf(DefData != nullptr,
		TEXT("UTcsAttributeSubsystem::AddAttribute: 属性定义未登记（属性 %s——宿主须先 RegisterAttributeDef）"),
		*Attribute.GetTagName().ToString()))
	{
		return false;
	}

	// 动态边界自引用禁止（D2-4：以自身为边界 = 循环依赖）
	const bool bSelfReferencedBound =
		(DefData->Bounds.Min.Mode == ETcsAttributeBoundMode::ABM_Dynamic && DefData->Bounds.Min.DynamicAttribute == Attribute) ||
		(DefData->Bounds.Max.Mode == ETcsAttributeBoundMode::ABM_Dynamic && DefData->Bounds.Max.DynamicAttribute == Attribute);
	if (!ensureMsgf(!bSelfReferencedBound,
		TEXT("UTcsAttributeSubsystem::AddAttribute: 动态边界自引用（实体 Id=%lld，属性 %s 以自身为边界）"),
		Entity.Id, *Attribute.GetTagName().ToString()))
	{
		return false;
	}

	// 定义字段在实例内展开（实例不持定义行引用——热路径不回查定义，D2-1/D2-9）
	FTcsAttributeInstance Instance;
	Instance.Attr = Attribute;
	Instance.BaseValue = DefData->BaseValue;
	Instance.CachedCurrent = DefData->BaseValue;	// 占位值（结算前不对外承诺——缓存值的唯一生产者仍是聚合管线）
	Instance.bDirty = true;	// 新建即脏：初值在批外读取/提交时由管线结算——值域收口、既有修正器、动态边界都在那一刻生效
	Instance.Bounds = DefData->Bounds;
	Instance.ValueDomain = DefData->ValueDomain;
	Instance.OverrideTieBreak = DefData->OverrideTieBreak;
	Store->Attributes.Add(Attribute, MoveTemp(Instance));

	UE_LOG(LogTcsAttribute, Log, TEXT("UTcsAttributeSubsystem: 属性新建 Entity=%llu Attribute=%s BaseValue=%.6f"),
		Entity.Id, *Attribute.GetTagName().ToString(), DefData->BaseValue);

	return true;
}

bool UTcsAttributeSubsystem::RemoveAttribute(
	FTcsCombatEntityHandle Entity,
	const FGameplayTag& Attribute)
{
	FTcsAttributeStore* Store = ResolveStore(Entity);
	if (!ensureMsgf(Store != nullptr,
		TEXT("UTcsAttributeSubsystem::RemoveAttribute: 实体未注册（Id=%lld）"), Entity.Id))
	{
		return false;
	}

	if (!ensureMsgf(Attribute.IsValid(),
		TEXT("UTcsAttributeSubsystem::RemoveAttribute: 属性名为空（实体 Id=%lld）"), Entity.Id))
	{
		return false;
	}

	// 冻结：整条实例搬入暂存区（不销毁、不丢修正器条目——装备穿脱不丢等级加成、buff 效果不丢）
	FTcsAttributeInstance* Instance = Store->Attributes.Find(Attribute);
	if (!ensureMsgf(Instance != nullptr,
		TEXT("UTcsAttributeSubsystem::RemoveAttribute: 该实体未持有此属性（实体 Id=%lld，属性 %s）"),
		Entity.Id, *Attribute.GetTagName().ToString()))
	{
		return false;
	}

	const int32 SlotCount = Instance->AttrModInstances.Num();
	Store->FrozenAttributes.Add(Attribute, MoveTemp(*Instance));
	Store->Attributes.Remove(Attribute);

	UE_LOG(LogTcsAttribute, Log,
		TEXT("UTcsAttributeSubsystem: 属性冻结 Entity=%llu Attribute=%s 修正器条目=%d（可被同名添加解冻恢复）"),
		Entity.Id, *Attribute.GetTagName().ToString(), SlotCount);

	return true;
}

double UTcsAttributeSubsystem::EvaluateCurrent(FTcsCombatEntityHandle Entity, const FGameplayTag& Attribute)
{
	return Pipeline.IsValid() ? Pipeline->EvaluateCurrent(Entity, Attribute) : 0.0;
}

bool UTcsAttributeSubsystem::TryGetBaseValue(
	FTcsCombatEntityHandle Entity,
	const FGameplayTag& Attribute,
	double& OutBaseValue) const
{
	// **非确保解析**（`ResolveStore` 而非 `GetStore`）：读取是正常查询路径，
	// "实体未注册"在这里不是契约违规——调用方（技能侧 Cost 门禁）只要拿到 false 就能如实报出
	// "该实体没有这个属性"，而**不该**为此留红字（那会把常态路径变成红字噪音，
	// 同 `ResolveStore` 自述的"供管线与内部读取路径使用"口径）。
	const FTcsAttributeStore* Store = ResolveStore(Entity);
	if (!Store || !Attribute.IsValid())
	{
		return false;
	}

	// **持有性判据 = `FindInstance` 命中**，MUST NOT 依赖 `GetBaseValue` 的返回值来判
	// ——它对未持有返回 0.0，与"确实持有且基值为 0"**不可区分**（本口存在的理由）。
	const FTcsAttributeInstance* Instance = Store->FindInstance(Attribute);
	if (!Instance)
	{
		return false;
	}

	OutBaseValue = Instance->BaseValue;
	return true;
}

bool UTcsAttributeSubsystem::SetBaseValue(
	FTcsCombatEntityHandle Entity,
	const FGameplayTag& Attribute,
	double NewBaseValue)
{
	FTcsAttributeStore* Store = ResolveStore(Entity);
	if (!ensureMsgf(Store != nullptr,
		TEXT("UTcsAttributeSubsystem::SetBaseValue: 实体未注册（Id=%lld）"), Entity.Id))
	{
		return false;
	}

	if (!ensureMsgf(Attribute.IsValid(),
		TEXT("UTcsAttributeSubsystem::SetBaseValue: 属性名为空（实体 Id=%lld）"), Entity.Id))
	{
		return false;
	}

	FTcsAttributeInstance* Instance = Store->FindInstance(Attribute);
	if (!ensureMsgf(Instance != nullptr,
		TEXT("UTcsAttributeSubsystem::SetBaseValue: 该实体未持有此属性（实体 Id=%lld，属性 %s）"),
		Entity.Id, *Attribute.GetTagName().ToString()))
	{
		return false;
	}

	// 改基值 = 事务的写操作类之一；标脏与重算/广播交给管线（批内由提交统一处理）
	return Pipeline.IsValid() ? Pipeline->SetBaseValue(Entity, Attribute, NewBaseValue) : false;
}

double UTcsAttributeSubsystem::PeekPending(FTcsCombatEntityHandle Entity, const FGameplayTag& Attribute)
{
	return Pipeline.IsValid() ? Pipeline->PeekPending(Entity, Attribute) : 0.0;
}

bool UTcsAttributeSubsystem::ApplyModifier(FTcsCombatEntityHandle Entity, const FTcsAttrModInstance& Modifier)
{
	return Pipeline.IsValid() ? Pipeline->ApplyModifier(Entity, Modifier) : false;
}

int32 UTcsAttributeSubsystem::RemoveBySource(FTcsCombatEntityHandle Entity, const FTcsSourceHandle& Source)
{
	return Pipeline.IsValid() ? Pipeline->RemoveBySource(Entity, Source) : 0;
}

void UTcsAttributeSubsystem::BeginBatch(FTcsCombatEntityHandle Entity)
{
	if (Pipeline.IsValid())
	{
		Pipeline->BeginBatch(Entity);
	}
}

void UTcsAttributeSubsystem::Commit(FTcsCombatEntityHandle Entity)
{
	if (Pipeline.IsValid())
	{
		Pipeline->Commit(Entity);
	}
}

FTcsAttributeStore* UTcsAttributeSubsystem::GetStore(FTcsCombatEntityHandle Entity)
{
	// 悬空表现为"查不到"（句柄无代际段）：返回 nullptr + ensure 提示——Development 期暴露传错句柄
	if (!ensureMsgf(Stores.Contains(Entity),
		TEXT("UTcsAttributeSubsystem::GetStore: 未注册或已注销的实体句柄（Id=%lld）"), Entity.Id))
	{
		return nullptr;
	}

	return ResolveStore(Entity);
}

const FTcsAttributeStore* UTcsAttributeSubsystem::GetStore(FTcsCombatEntityHandle Entity) const
{
	if (!ensureMsgf(Stores.Contains(Entity),
		TEXT("UTcsAttributeSubsystem::GetStore: 未注册或已注销的实体句柄（Id=%lld）"), Entity.Id))
	{
		return nullptr;
	}

	return ResolveStore(Entity);
}

FTcsAttributeStore* UTcsAttributeSubsystem::ResolveStore(FTcsCombatEntityHandle Entity)
{
	// 解引用外层表的间接层：容器指针因此不随"新增其他实体"失效（TMap 元素地址会随扩容搬移）
	const TUniquePtr<FTcsAttributeStore>* Found = Stores.Find(Entity);
	return Found ? Found->Get() : nullptr;
}

const FTcsAttributeStore* UTcsAttributeSubsystem::ResolveStore(FTcsCombatEntityHandle Entity) const
{
	const TUniquePtr<FTcsAttributeStore>* Found = Stores.Find(Entity);
	return Found ? Found->Get() : nullptr;
}
