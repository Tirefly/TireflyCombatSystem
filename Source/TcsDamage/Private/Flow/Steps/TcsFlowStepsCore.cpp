// Copyright Tirefly. All Rights Reserved.

#include "Flow/Steps/TcsFlowSteps.h"

#include "Clock/TcsClockSubsystem.h"
#include "EventBus/TcsEventBusSubsystem.h"
#include "Flow/TcsDamageFlowCollectEvent.h"
#include "Flow/TcsDamageRecord.h"
#include "Flow/TcsFlowKeys.h"
#include "Flow/TcsFlowStepConditions.h"
#include "Flow/TcsFlowStepExecutor.h"
#include "Parameter/TcsParamSource_Literal.h"
#include "TcsAttributeSubsystem.h"
#include "TcsDamageLogChannel.h"
#include "TcsDamageSubsystem.h"



// ===== 核心四步执行器（默认模板所用；其余六步与两个数据步骤见 TcsFlowStepsRest.cpp）=====
namespace
{
	// 黑板保留键（标准步骤库的契约键名——M8 校验/Explain 认识；项目自定义键自由 FName）
	// **伤害量只有一个契约键 `BaseDamage`**：基值以 Add 落在它上面、收集到的 PercentAdd/Mul 也叠在它上面
	// （不再有 `FinalDamage` 键——记录里的"最终值"直接读该键的折叠结果）
	// 黑板契约键 = **框架词汇**（步骤之间的接口），由本模块原生声明（`Flow/TcsFlowKeys.h`）——
	// 零项目配置依赖、编译期一致（不再有跨文件裸字面量耦合）。
	//
	// **MUST NOT 写 `const FGameplayTag& Alias = Tag_X;`**（2026-09-22 实测踩坑）：
	// `FNativeGameplayTag::operator FGameplayTag()` **按值返回**，绑定 const 引用 = 延长一个
	// **静态初始化期创建的临时量**的寿命——那时 tag 尚未加载，别名会**永远持空 tag**，
	// 而 `FTcsFlowAttributes::Submit` 拒绝空键（`ensureMsgf(Key.IsValid())`）→ 写入被静默拒绝。
	// 正确做法 = **每次使用点直接用原生全局量**（隐式转换发生在运行期，那时 tag 已就绪）。

	// 取宿主门面（步骤取世界/子系统的唯一通路——句柄化后上下文里没有 Actor 可借道）
	UTcsDamageSubsystem* ResolveFlowOwner(const FTcsDamageFlowContext& Context)
	{
		return Context.Owner.Get();
	}

	// 取属性门面（M2）
	UTcsAttributeSubsystem* ResolveAttributeSubsystem(const FTcsDamageFlowContext& Context)
	{
		UTcsDamageSubsystem* Owner = ResolveFlowOwner(Context);
		UWorld* World = Owner ? Owner->GetWorld() : nullptr;
		return World ? World->GetSubsystem<UTcsAttributeSubsystem>() : nullptr;
	}

	// 黑板写入两个口（语义分离）：
	// - **输入/基值类走 `Add`**：折叠式 `(0 + ΣAdd) × (1 + ΣPercentAdd) × ΠMul + ΣFlatAdd` 里
	//   "基值"天然就是 `ΣAdd` 的一员（与 M2 属性"基础值 + Add 带"同式同值）。
	//   **MUST NOT 用覆盖带**——覆盖带盖掉一切，会把收集到的修正（如"受火伤 +20%"）整笔抹掉。
	// - **结果类走 `Override`**：`Executed`/`Absorbed`/`Kill` 是本次执行的事实值，不含修正带语义。
	void SubmitAddValue(FTcsFlowAttributes& Blackboard, FGameplayTag Key, double Value)
	{
		FTcsParamValue Operand;
		Operand.Source.GetMutable<FTcsParamSource_Literal>().Value = Value;
		Blackboard.Submit(Key, ETcsAttributeOp::TAO_Add, Operand);
	}

	void SubmitOverrideValue(FTcsFlowAttributes& Blackboard, FGameplayTag Key, double Value)
	{
		FTcsParamValue Operand;
		Operand.Source.GetMutable<FTcsParamSource_Literal>().Value = Value;
		Blackboard.Submit(Key, ETcsAttributeOp::TAO_Override, Operand);
	}

	// —— ① CollectStart：发流程开始事件 + 重置收集 ——
	bool ExecuteFlowCollectStart(const FInstancedStruct& StepData, FTcsDamageFlowContext& Context)
	{
		const FTcsFlowCollectStart* Step = StepData.GetPtr<FTcsFlowCollectStart>();
		if (!Step || !ShouldRunFlowStep(Step->Conditions, Context))
		{
			return true;
		}

		Context.Blackboard.Reset();

		if (UTcsDamageSubsystem* Owner = ResolveFlowOwner(Context))
		{
			Owner->PublishCollectEvent(Tag_TcsEvent_Damage_FlowStarted, Context);
		}
		return true;
	}

	// —— ⑥ BaseDamage：接收输入值 → delegate 逃生口 → 写黑板（流程零计算）——
	bool ExecuteFlowBaseDamage(const FInstancedStruct& StepData, FTcsDamageFlowContext& Context)
	{
		const FTcsFlowBaseDamage* Step = StepData.GetPtr<FTcsFlowBaseDamage>();
		if (!Step || !ShouldRunFlowStep(Step->Conditions, Context))
		{
			return true;
		}

		// 输入值来自**上下文请求字段**（链步骤解算结果）——不是黑板键：
		// 黑板会被 `CollectStart` 重置（收集语义），而输入是"请求"的一部分、不随收集清除；
		// 若从黑板读输入，链侧预写 + 本步提交会造成**重复计数**（实测：10 + 20 = 30）。
		const double IncomingBase = Context.BaseDamageInput;

		double BaseDamage = IncomingBase;
		if (Step->Delegate && Step->Delegate.GetObject())
		{
			// 降级逃生口：宿主特殊公式（默认实现即原样返回）
			// **走 Execute_ 而非虚表直调**（2026-09-24，台账 SCRIPT-8）：脚本层实现走 ProcessEvent，
			// 虚表直调会静默跳过它（表现为"公式不生效"而非崩溃）；Execute_ 内部查不到脚本覆写时
			// 回落原生 _Implementation ⇒ C++ 实现与脚本实现双轨并存。
			// 上下文经**反射视图**传入（`FTcsDamageFlowContext` 是纯 C++ struct，不能作 UFUNCTION 形参）。
			const FTcsDamageFlowContextView ContextView = Context.MakeView();
			BaseDamage = ITcsDamageFlowDelegate::Execute_CalculateBaseDamage(
				Step->Delegate.GetObject(), IncomingBase, Context.Attacker,
				Context.Targets.Num() > 0 ? Context.Targets[0] : FTcsCombatEntityHandle(), ContextView);
		}

		// 基值以 **Add** 提交（见上：基值 = ΣAdd 的一员；后续收集到的 PercentAdd/Mul 自然叠在它上面）
		// 键无效 = 未配置 -> 落契约键 `BaseDamage`（原默认值语义，2026-09-22 tag 化改造后显式化）
		const FGameplayTag OutputKey = Step->OutputKey.IsValid() ? Step->OutputKey : FGameplayTag(Tag_DamageFlowKey_BaseDamage);
		SubmitAddValue(Context.Blackboard, OutputKey, BaseDamage);
		return true;
	}

	// —— ⑨ Execute：裁决 → 护盾 hook → M2 事务扣血 → 成功才消费 ——
	bool ExecuteFlowExecute(const FInstancedStruct& StepData, FTcsDamageFlowContext& Context)
	{
		const FTcsFlowExecute* Step = StepData.GetPtr<FTcsFlowExecute>();
		if (!Step || !ShouldRunFlowStep(Step->Conditions, Context))
		{
			return true;
		}

		// 键无效 = 未配置 -> 落契约键（同上）
		const FGameplayTag DamageKey = Step->DamageKey.IsValid() ? Step->DamageKey : FGameplayTag(Tag_DamageFlowKey_BaseDamage);
		const FGameplayTag CandidateKey = Step->CandidateKey.IsValid() ? Step->CandidateKey : FGameplayTag(Tag_DamageFlowKey_ExecuteCandidates);
		const double Candidate = Context.Blackboard.Read(DamageKey);

		// 裁决：候选中按消耗策略的 SortKey 选一（收集 ≠ 消费——未被选中者完全不动）
		int32 BestIndex = INDEX_NONE;
		int32 BestSortKey = TNumericLimits<int32>::Lowest();
		if (const TArray<FTcsFlowAttributeSubmit>* Submits = Context.Blackboard.FindSubmits(CandidateKey))
		{
			for (int32 Index = 0; Index < Submits->Num(); ++Index)
			{
				if ((*Submits)[Index].Consume.SortKey > BestSortKey)
				{
					BestSortKey = (*Submits)[Index].Consume.SortKey;
					BestIndex = Index;
				}
			}
		}

		// 护盾 hook（宿主；默认 0 = 无护盾）
		// **走 Execute_**（同 BaseDamage 步的理由：脚本层实现必须经反射分发抵达）
		// 上下文经**反射视图**传入（`FTcsDamageFlowContext` 是纯 C++ struct，不能作 UFUNCTION 形参）
		// ——视图一次提取、供本节三处委托调用共用（护盾 / 吸收映射 / 致死判定）
		UObject* DelegateObject = Step->Delegate ? Step->Delegate.GetObject() : nullptr;
		const FTcsDamageFlowContextView ContextView = Context.MakeView();

		double Absorbed = 0.0;
		if (DelegateObject)
		{
			for (const FTcsCombatEntityHandle& Target : Context.Targets)
			{
				Absorbed += ITcsDamageFlowDelegate::Execute_ModifyShield(
					DelegateObject, Target, Candidate, ContextView);
			}
		}

		// 吸收 → 执行量的映射：**经宿主委托**（中性默认 = 候选减吸收且不为负，即整肃前行为）
		// 判据（2026-10-02 边界整肃）：零下限只引用数值、方向收敛到"不介入" ⇒ 合法契约默认；
		// 但"候选 − 吸收"的**减法模型**宿主无法替换 ⇒ 框架内定。框架内定的处置**不是删**，
		// 而是**给它一个覆盖点**——故该表达式移入 `ResolveExecutedDamage`。
		// 多目标时与 `BaseDamage` 步同款取首个目标：`Absorbed` 本就是全目标之和
		// ⇒ 本映射是**流程级**的一次取值，不是逐目标取值。
		double Executed = FMath::Max(0.0, Candidate - Absorbed);
		if (DelegateObject)
		{
			Executed = ITcsDamageFlowDelegate::Execute_ResolveExecutedDamage(
				DelegateObject,
				Context.Targets.Num() > 0 ? Context.Targets[0] : FTcsCombatEntityHandle(),
				Candidate, Absorbed, ContextView);
		}

		// M2 事务扣血：**改基值**（伤害是"生命被削减"，不是可被来源级联撤销的修正器——见 plan2 Task 4 注记）
		// 属性键解析：**步骤级 AttrKey 优先**（模板可覆盖）；否则用调用方在上下文里指定的请求键
		// （插件组装的官方默认模板不可能知道项目词表——`Health` 是项目侧的）
		// 两处都是 FGameplayTag（2026-09-22 tag 化改造）——不再有 FName 与包装互转
		const FGameplayTag ResolvedAttrKey = Step->AttrKey.IsValid() ? Step->AttrKey : Context.TargetAttrKey;

		bool bApplied = false;
		if (UTcsAttributeSubsystem* AttributeSubsystem = ResolveAttributeSubsystem(Context))
		{
			if (ResolvedAttrKey.IsValid())
			{
				for (const FTcsCombatEntityHandle& Target : Context.Targets)
				{
					AttributeSubsystem->BeginBatch(Target);
					const double Current = AttributeSubsystem->EvaluateCurrent(Target, ResolvedAttrKey);
					// **值域收口归属性层的宿主配置**（`ETcsAttributeBoundMode::ABM_None` 是默认 =
					// 该侧不设边界；`TcsAttributePipeline.cpp` 的 `ApplyValueDomain` 只在宿主显式配了
					// 边界时才钳制）。本步 MUST NOT 自设下限——框架自设的零下限会**抢在宿主已有的
					// 配置点之前**下结论，形成"框架与宿主各有一套值域结论"的双真相
					// （判据 = **契约覆盖**，比"框架内定"更重的一档；2026-10-02 边界整肃删除）。
					const double NewValue = Current - Executed;
					AttributeSubsystem->SetBaseValue(Target, ResolvedAttrKey, NewValue);
					AttributeSubsystem->Commit(Target);
					bApplied = true;
				}
			}
			else
			{
				UE_LOG(LogTcsDamage, Warning, TEXT("FlowExecute: 步骤未配 AttrKey 且上下文未指定 TargetAttrKey——本次不扣血（仅记录）"));
			}
		}

		SubmitOverrideValue(Context.Blackboard, FGameplayTag(Tag_DamageFlowKey_Executed), bApplied ? Executed : 0.0);
		SubmitOverrideValue(Context.Blackboard, FGameplayTag(Tag_DamageFlowKey_Absorbed), Absorbed);

		// 记账语义：本次事务提交后目标生命 ≤ 0（死亡规则仍归宿主）
		// ——阈值经 `ITcsDamageFlowDelegate::IsLethal` 可被宿主覆盖（中性默认 = `<= 0.0`，
		// 即整肃前行为）；框架**仍只记账**、MUST NOT 杀实体（2026-10-02 边界整肃）。
		double bKill = 0.0;
		if (UTcsAttributeSubsystem* AttributeSubsystem = ResolveAttributeSubsystem(Context))
		{
			if (ResolvedAttrKey.IsValid())
			{
				for (const FTcsCombatEntityHandle& Target : Context.Targets)
				{
					const double CurrentValueAfterApply = AttributeSubsystem->EvaluateCurrent(Target, ResolvedAttrKey);
					const bool bLethal = DelegateObject
						? ITcsDamageFlowDelegate::Execute_IsLethal(DelegateObject, Target, CurrentValueAfterApply, ContextView)
						: CurrentValueAfterApply <= 0.0;
					if (bLethal)
					{
						bKill = 1.0;
						break;
					}
				}
			}
		}
		SubmitOverrideValue(Context.Blackboard, FGameplayTag(Tag_DamageFlowKey_Kill), bKill);

		return true;
	}

	// —— ⑩ Completed：组装并发布伤害记录（每目标一条）——
	bool ExecuteFlowCompleted(const FInstancedStruct& StepData, FTcsDamageFlowContext& Context)
	{
		const FTcsFlowCompleted* Step = StepData.GetPtr<FTcsFlowCompleted>();
		if (!Step || !ShouldRunFlowStep(Step->Conditions, Context))
		{
			return true;
		}

		UTcsDamageSubsystem* Owner = ResolveFlowOwner(Context);
		if (!Owner)
		{
			UE_LOG(LogTcsDamage, Warning, TEXT("FlowCompleted: 门面不可得——记录未产出"));
			return true;
		}

		const UTcsClockSubsystem* Clock = Owner->GetWorld() ? Owner->GetWorld()->GetSubsystem<UTcsClockSubsystem>() : nullptr;
		const double Now = Clock ? Clock->GetClock().Elapsed : 0.0;

		for (const FTcsCombatEntityHandle& Target : Context.Targets)
		{
			FTcsDamageRecord Record;
			Record.FlowId = Context.FlowSource.Id;
			Record.Source = Context.Attacker;
			Record.Target = Target;
			// 命中 / 暴击结果**从契约键读回**（两者同款取值来源）——框架 MUST NOT 硬编码：
			// "本次是否命中"是宿主判定（`DamageFlowKey.HitRate` 由宿主响应方改写、
			// `FTcsFlowHit` 步按 `>= 1.0` 折算）。该步**不在默认模板** ⇒ 无提交 ⇒ 折叠初值 0
			// ⇒ `bHit = false`（整肃前是恒 `true` 的硬编码常量；2026-10-02 边界整肃改正）。
			// 元素**不进记录**：由 `FTcsFlowElement` 步写进流程分类 Tag 集（`DamageCategory` 根）。
			Record.bHit = Context.Blackboard.Read(FGameplayTag(Tag_DamageFlowKey_Hit)) > 0.0;
			Record.bCrit = Context.Blackboard.Read(FGameplayTag(Tag_DamageFlowKey_Crit)) > 0.0;
			// Base = 调用方输入（原始解算结果）；Final = 该键折叠后的最终值（基值 + 收集到的修正）
			Record.Base = Context.BaseDamageInput;
			Record.Final = Context.Blackboard.Read(FGameplayTag(Tag_DamageFlowKey_BaseDamage));
			Record.Executed = Context.Blackboard.Read(FGameplayTag(Tag_DamageFlowKey_Executed));
			Record.Absorbed = Context.Blackboard.Read(FGameplayTag(Tag_DamageFlowKey_Absorbed));
			Record.bKill = Context.Blackboard.Read(FGameplayTag(Tag_DamageFlowKey_Kill)) > 0.0;
			Record.Timestamp = Now;

			Owner->AppendRecord(Record);
		}

		// 完成收集事件（宿主挂点：结算后表现/统计——记录事件在 AppendRecord 内已发）
		if (UTcsDamageSubsystem* CompletedOwner = ResolveFlowOwner(Context))
		{
			CompletedOwner->PublishCollectEvent(Tag_TcsEvent_Damage_Completed, Context);
		}

		return true;
	}
}

UE_DEFINE_FLOW_STEP_EXECUTOR(FTcsFlowCollectStart, ExecuteFlowCollectStart)
UE_DEFINE_FLOW_STEP_EXECUTOR(FTcsFlowBaseDamage, ExecuteFlowBaseDamage)
UE_DEFINE_FLOW_STEP_EXECUTOR(FTcsFlowExecute, ExecuteFlowExecute)
UE_DEFINE_FLOW_STEP_EXECUTOR(FTcsFlowCompleted, ExecuteFlowCompleted)
