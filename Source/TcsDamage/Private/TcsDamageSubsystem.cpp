// Copyright Tirefly. All Rights Reserved.

#include "TcsDamageSubsystem.h"

#include "Engine/Engine.h"
#include "Engine/World.h"
#include "EventBus/TcsEventBusSubsystem.h"
#include "Flow/Steps/TcsFlowSteps.h"
#include "Flow/TcsDamageFlowCollectEvent.h"
#include "Flow/TcsDamageRecord.h"
#include "Flow/TcsFlowStepExecutor.h"
#include "HAL/IConsoleManager.h"
#include "TcsDamageLogChannel.h"



// 流程收集事件 Tag 全集（原生声明——不进项目 Tag 表）
UE_DEFINE_GAMEPLAY_TAG(Tag_Tcs_Event_Damage_FlowStarted, "Tcs.Event.Damage.FlowStarted");
UE_DEFINE_GAMEPLAY_TAG(Tag_Tcs_Event_Damage_PreHit, "Tcs.Event.Damage.PreHit");
UE_DEFINE_GAMEPLAY_TAG(Tag_Tcs_Event_Damage_Hit, "Tcs.Event.Damage.Hit");
UE_DEFINE_GAMEPLAY_TAG(Tag_Tcs_Event_Damage_Crit, "Tcs.Event.Damage.Crit");
UE_DEFINE_GAMEPLAY_TAG(Tag_Tcs_Event_Damage_Element, "Tcs.Event.Damage.Element");
UE_DEFINE_GAMEPLAY_TAG(Tag_Tcs_Event_Damage_AfterDamage, "Tcs.Event.Damage.AfterDamage");
UE_DEFINE_GAMEPLAY_TAG(Tag_Tcs_Event_Damage_PreExecute, "Tcs.Event.Damage.PreExecute");
UE_DEFINE_GAMEPLAY_TAG(Tag_Tcs_Event_Damage_Completed, "Tcs.Event.Damage.Completed");

// 伤害记录事件 Tag（原生声明；载荷 = FTcsDamageRecord）
UE_DEFINE_GAMEPLAY_TAG(Tag_Tcs_Event_Damage_Recorded, "Tcs.Event.Damage.Recorded");

// 记录环形缓冲容量（R3 常量；改容量随统计需求轮）
namespace
{
	constexpr int32 GTcsDamageRecordBufferSize = 128;
}



// 生命期
void UTcsDamageSubsystem::Initialize(FSubsystemCollectionBase& Collection)
{
	Super::Initialize(Collection);

	// 官方默认模板（D7-5：标准十阶段只是插件自带的标准件，可整表替换）
	// R3 组装四步：CollectStart → BaseDamage → Execute → Completed
	// （Hit/Crit/Element/PreHit/AfterDamage/PreExecute 的 struct 与执行器已就位，本模板不组装）
	FTcsFlowTemplate DefaultTemplate;
	DefaultTemplate.TemplateId = FName(TEXT("Default"));
	DefaultTemplate.Steps.AddDefaulted_GetRef().InitializeAs<FTcsFlowCollectStart>();
	DefaultTemplate.Steps.AddDefaulted_GetRef().InitializeAs<FTcsFlowBaseDamage>();
	DefaultTemplate.Steps.AddDefaulted_GetRef().InitializeAs<FTcsFlowExecute>();
	DefaultTemplate.Steps.AddDefaulted_GetRef().InitializeAs<FTcsFlowCompleted>();

	if (RegisterTemplate(DefaultTemplate))
	{
		UE_LOG(LogTcsDamage, Log, TEXT("UTcsDamageSubsystem: 官方默认模板已登记（Default：4 步）"));
	}
}

bool UTcsDamageSubsystem::DoesSupportWorldType(const EWorldType::Type WorldType) const
{
	// 仅游戏世界实例化：编辑器预览/检查器世界不创建（对齐时钟/总线/属性/效果链门面）
	return WorldType == EWorldType::Game ||
		WorldType == EWorldType::PIE ||
		WorldType == EWorldType::GamePreview;
}

void UTcsDamageSubsystem::Deinitialize()
{
	// 确定性清理：模板与记录缓冲随容器释放（自持数据，零外部资源持有）
	Templates.Empty();
	RecordBuffer.Reset();
	NextRecordSequence = 0;

	Super::Deinitialize();
}



// 模板登记表
bool UTcsDamageSubsystem::RegisterTemplate(const FTcsFlowTemplate& Template)
{
	ensure(IsInGameThread());

	if (!ensureMsgf(!Template.TemplateId.IsNone(), TEXT("UTcsDamageSubsystem::RegisterTemplate: 模板 id 为空——拒绝登记")))
	{
		return false;
	}

	if (Templates.Contains(Template.TemplateId))
	{
		ensureMsgf(false, TEXT("UTcsDamageSubsystem::RegisterTemplate: 模板 %s 已登记——拒绝重复登记（不得静默覆写）"),
			*Template.TemplateId.ToString());
		return false;
	}

	Templates.Add(Template.TemplateId, MakeUnique<FTcsFlowTemplate>(Template));

	UE_LOG(LogTcsDamage, Log, TEXT("UTcsDamageSubsystem: 流程模板登记 Template=%s 步骤数=%d"),
		*Template.TemplateId.ToString(), Template.Steps.Num());
	return true;
}

bool UTcsDamageSubsystem::UnregisterTemplate(FName TemplateId)
{
	ensure(IsInGameThread());

	if (TemplateId.IsNone() || !Templates.Contains(TemplateId))
	{
		UE_LOG(LogTcsDamage, Warning, TEXT("UTcsDamageSubsystem::UnregisterTemplate: 模板 %s 未登记——忽略"), *TemplateId.ToString());
		return false;
	}

	Templates.Remove(TemplateId);

	UE_LOG(LogTcsDamage, Log, TEXT("UTcsDamageSubsystem: 流程模板注销 Template=%s"), *TemplateId.ToString());
	return true;
}

const FTcsFlowTemplate* UTcsDamageSubsystem::FindTemplate(FName TemplateId) const
{
	const TUniquePtr<FTcsFlowTemplate>* Found = Templates.Find(TemplateId);
	return (Found && Found->IsValid()) ? Found->Get() : nullptr;
}



// 执行
bool UTcsDamageSubsystem::RunTemplate(FName TemplateId, FTcsDamageFlowContext& Context)
{
	ensure(IsInGameThread());

	const FTcsFlowTemplate* Template = FindTemplate(TemplateId);
	if (!Template)
	{
		UE_LOG(LogTcsDamage, Error, TEXT("UTcsDamageSubsystem::RunTemplate: 模板 %s 未登记——拒绝执行"), *TemplateId.ToString());
		return false;
	}

	// 每流程唯一的来源锚点（作用域修改器经 RemoveBySource 级联摘除的锚点——09 §2.1/§3）
	Context.FlowSource = FlowSourceRegistry.Allocate();

	// 宿主门面回填：步骤取世界/子系统的唯一通路（句柄化后上下文里没有 Actor 可借道）
	Context.Owner = this;

	UE_LOG(LogTcsDamage, Log, TEXT("UTcsDamageSubsystem: 流程 %s 起（步骤 %d，FlowSource=%llu）"),
		*TemplateId.ToString(), Template->Steps.Num(), Context.FlowSource.Id);

	for (int32 StepIndex = 0; StepIndex < Template->Steps.Num(); ++StepIndex)
	{
		const FInstancedStruct& Step = Template->Steps[StepIndex];
		const UScriptStruct* StepStruct = Step.GetScriptStruct();
		const FTcsFlowStepExecute* Executor = FTcsFlowStepExecutorRegistry::Get().Find(StepStruct);
		if (!Executor)
		{
			// 未知步骤类型：中止剩余步骤（不静默跳过——静默会让"配置写错"表现成"流程少跑了几步"）
			UE_LOG(LogTcsDamage, Error, TEXT("UTcsDamageSubsystem::RunTemplate: 模板 %s 第 %d 步类型 %s 无执行器——中止流程"),
				*TemplateId.ToString(), StepIndex, StepStruct ? *StepStruct->GetName() : TEXT("<空>"));
			return false;
		}

		if (!(*Executor)(Step, Context))
		{
			// 流程失败/打断：中止剩余步骤，已产生的副作用不回滚（止于未来）
			UE_LOG(LogTcsDamage, Warning, TEXT("UTcsDamageSubsystem::RunTemplate: 模板 %s 第 %d 步中止流程"),
				*TemplateId.ToString(), StepIndex);
			return false;
		}
	}

	UE_LOG(LogTcsDamage, Log, TEXT("UTcsDamageSubsystem: 流程 %s 完成（%d 步）"), *TemplateId.ToString(), Template->Steps.Num());
	return true;
}



// 收集事件协议
void UTcsDamageSubsystem::PublishCollectEvent(FGameplayTag EventTag, FTcsDamageFlowContext& Context)
{
	ensure(IsInGameThread());

	UWorld* World = GetWorld();
	UTcsEventBusSubsystem* BusSubsystem = World ? World->GetSubsystem<UTcsEventBusSubsystem>() : nullptr;
	if (!BusSubsystem)
	{
		UE_LOG(LogTcsDamage, Warning, TEXT("UTcsDamageSubsystem::PublishCollectEvent: 事件总线不可得——收集事件 %s 丢弃"),
			*EventTag.ToString());
		return;
	}

	// 载荷 = 上下文指针包装（进程内瞬态：立即通道同步派发期有效，消费方不得跨帧持有）
	FTcsDamageFlowCollectEvent Payload;
	Payload.Context = &Context;

	FInstancedStruct PayloadStruct;
	PayloadStruct.InitializeAs<FTcsDamageFlowCollectEvent>(Payload);

	// 立即通道：同步派发——响应方在本次调用返回前已提交黑板修正（收集 ≠ 消费）
	BusSubsystem->PublishImmediate(EventTag, PayloadStruct);

	UE_LOG(LogTcsDamage, Verbose, TEXT("UTcsDamageSubsystem: 收集事件 %s 已派发（FlowSource=%llu）"),
		*EventTag.ToString(), Context.FlowSource.Id);
}



// 记录
void UTcsDamageSubsystem::AppendRecord(FTcsDamageRecord& Record)
{
	ensure(IsInGameThread());

	Record.Sequence = ++NextRecordSequence;

	// 环形缓冲：超容丢最旧（统计/回放只关心近况；全量归档归宿主）
	RecordBuffer.Add(Record);
	if (RecordBuffer.Num() > GTcsDamageRecordBufferSize)
	{
		RecordBuffer.RemoveAt(0, RecordBuffer.Num() - GTcsDamageRecordBufferSize, EAllowShrinking::No);
	}

	// 立即通道同步派发（记录是结果快照——消费方无须访问流程上下文）
	if (UWorld* World = GetWorld())
	{
		if (UTcsEventBusSubsystem* BusSubsystem = World->GetSubsystem<UTcsEventBusSubsystem>())
		{
			FInstancedStruct PayloadStruct;
			PayloadStruct.InitializeAs<FTcsDamageRecord>(Record);
			BusSubsystem->PublishImmediate(Tag_Tcs_Event_Damage_Recorded, PayloadStruct);
		}
	}

	UE_LOG(LogTcsDamage, Log, TEXT("UTcsDamageSubsystem: 伤害记录 #%llu（Flow=%llu 源=#%lld 目标=#%lld Base=%.3f Final=%.3f Executed=%.3f 暴击=%d 击杀=%d）"),
		Record.Sequence, Record.FlowId, Record.Source.Id, Record.Target.Id,
		Record.Base, Record.Final, Record.Executed, Record.bCrit ? 1 : 0, Record.bKill ? 1 : 0);
}

void UTcsDamageSubsystem::GetRecentRecords(TArray<FTcsDamageRecord>& OutRecords) const
{
	OutRecords = RecordBuffer;
}



// 记录浏览命令（**入库的正式调试口**——不是测试装置；R8-6 Explain 落地前的临时替代）
namespace
{
	// 屏显行键基址（固定行号——重跑覆盖同一批行）
	constexpr uint64 GTcsDamageDumpScreenKey = 0x7C5AB000;

	// 打印最近记录（旧 → 新序；每行含输入值 / 最终值 / 差额 = 修改器净影响）
	// 签名顺序按 `FAutoConsoleCommandWithWorldAndArgs`：**Args 在前、World 在后**
	void DumpDamageRecords(const TArray<FString>& Args, UWorld* World)
	{
		UTcsDamageSubsystem* Subsystem = World ? World->GetSubsystem<UTcsDamageSubsystem>() : nullptr;
		if (!Subsystem)
		{
			UE_LOG(LogTcsDamage, Display, TEXT("Tcs.Damage.DumpRecords: 未找到伤害流程门面（请确认世界类型为 PIE/Game）"));
			return;
		}

		// 参数：条数（默认 10；钳到 [1, 缓冲容量]——非正/超大值不报错）
		int32 Count = 10;
		if (Args.Num() > 0)
		{
			Count = FCString::Atoi(*Args[0]);
		}
		Count = FMath::Clamp(Count, 1, 128);

		TArray<FTcsDamageRecord> Records;
		Subsystem->GetRecentRecords(Records);
		if (Records.Num() == 0)
		{
			UE_LOG(LogTcsDamage, Display, TEXT("Tcs.Damage.DumpRecords: 尚无记录（先跑一次伤害流程）"));
			if (GEngine)
			{
				GEngine->AddOnScreenDebugMessage(GTcsDamageDumpScreenKey, 30.0f, FColor::Yellow,
					TEXT("Tcs.Damage.DumpRecords: 尚无记录"));
			}
			return;
		}

		const int32 FirstIndex = FMath::Max(0, Records.Num() - Count);
		UE_LOG(LogTcsDamage, Display, TEXT("Tcs.Damage.DumpRecords: 共 %d 笔，显示最近 %d 笔（旧 → 新）"),
			Records.Num(), Records.Num() - FirstIndex);
		UE_LOG(LogTcsDamage, Display, TEXT("  序号  Flow  源     目标   Input     Final     差额      执行      吸收   暴击 击杀"));
		if (GEngine)
		{
			GEngine->AddOnScreenDebugMessage(GTcsDamageDumpScreenKey, 30.0f, FColor::Cyan,
				FString::Printf(TEXT("伤害记录：共 %d 笔，最近 %d 笔"), Records.Num(), Records.Num() - FirstIndex));
		}

		for (int32 Index = FirstIndex; Index < Records.Num(); ++Index)
		{
			const FTcsDamageRecord& Record = Records[Index];
			const double Delta = Record.Final - Record.Base;

			const FString Line = FString::Printf(
				TEXT("  #%-4lld %-5lld #%-5lld #%-5lld %-9.3f %-9.3f %+-9.3f %-9.3f %-9.3f %-4d %d"),
				Record.Sequence, Record.FlowId, Record.Source.Id, Record.Target.Id,
				Record.Base, Record.Final, Delta, Record.Executed, Record.Absorbed,
				Record.bCrit ? 1 : 0, Record.bKill ? 1 : 0);

			UE_LOG(LogTcsDamage, Display, TEXT("%s"), *Line);
			if (GEngine)
			{
				GEngine->AddOnScreenDebugMessage(GTcsDamageDumpScreenKey + 1 + static_cast<uint64>(Index - FirstIndex),
					30.0f, FColor::White, Line);
			}
		}

		UE_LOG(LogTcsDamage, Display, TEXT("  说明：Input = 链步骤解算输入；Final = 收集到的修正全部生效后的最终值；差额 = Final − Input（正=增强，负=削弱）"));
	}
}

static FAutoConsoleCommandWithWorldAndArgs GTcsDamageDumpRecordsCommand(
	TEXT("Tcs.Damage.DumpRecords"),
	TEXT("打印最近若干笔伤害记录（默认 10；含输入值 / 最终值 / 差额 = 修改器净影响 / 执行量）——Explain（R8-6）落地前的临时浏览口"),
	FConsoleCommandWithWorldAndArgsDelegate::CreateStatic(&DumpDamageRecords));
