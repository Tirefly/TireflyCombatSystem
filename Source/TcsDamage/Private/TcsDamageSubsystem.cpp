// Copyright Tirefly. All Rights Reserved.

#include "TcsDamageSubsystem.h"

#include "EventBus/TcsEventBusSubsystem.h"
#include "Flow/TcsFlowStepExecutor.h"
#include "TcsDamageLogChannel.h"



// 流程收集事件 Tag（原生声明——不进项目 Tag 表）
UE_DEFINE_GAMEPLAY_TAG(Tag_TcsEvent_Damage_FlowStarted, "Tcs.Event.Damage.FlowStarted");



// 生命期
bool UTcsDamageSubsystem::DoesSupportWorldType(const EWorldType::Type WorldType) const
{
	// 仅游戏世界实例化：编辑器预览/检查器世界不创建（对齐时钟/总线/属性/效果链门面）
	return WorldType == EWorldType::Game ||
		WorldType == EWorldType::PIE ||
		WorldType == EWorldType::GamePreview;
}

void UTcsDamageSubsystem::Deinitialize()
{
	// 确定性清理：模板随登记表释放（模板自持数据，零外部资源持有）
	Templates.Empty();

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
