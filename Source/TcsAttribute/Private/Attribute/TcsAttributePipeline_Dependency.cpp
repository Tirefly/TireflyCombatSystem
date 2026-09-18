// Copyright Tirefly. All Rights Reserved.

#include "Attribute/TcsAttributePipeline.h"

#include "Attribute/TcsAttrModInstance.h"
#include "TcsAttributeLogChannel.h"
#include "TcsAttributeSubsystem.h"



// 依赖图工具（Tarjan SCC：环检测）
namespace
{
	// Tarjan 强连通分量递归（属性图很小，递归安全；分量大小 > 1 即为环）
	void StrongConnect(
		const FTcsAttributeName& Node,
		const TMap<FTcsAttributeName, TArray<FTcsAttributeName>>& Graph,
		TMap<FTcsAttributeName, int32>& Index,
		TMap<FTcsAttributeName, int32>& LowLink,
		TArray<FTcsAttributeName>& Stack,
		TSet<FTcsAttributeName>& OnStack,
		int32& NextIndex,
		bool& bOutHasCycle)
	{
		Index.Add(Node, NextIndex);
		LowLink.Add(Node, NextIndex);
		++NextIndex;
		Stack.Add(Node);
		OnStack.Add(Node);

		if (const TArray<FTcsAttributeName>* Readers = Graph.Find(Node))
		{
			for (const FTcsAttributeName& Next : *Readers)
			{
				if (!Index.Contains(Next))
				{
					StrongConnect(Next, Graph, Index, LowLink, Stack, OnStack, NextIndex, bOutHasCycle);
					LowLink[Node] = FMath::Min(LowLink[Node], LowLink[Next]);
				}
				else if (OnStack.Contains(Next))
				{
					LowLink[Node] = FMath::Min(LowLink[Node], Index[Next]);
				}
			}
		}

		if (LowLink[Node] == Index[Node])
		{
			// 弹出一个强连通分量：大小 > 1 即存在环（自环由 RegisterDependency 提前排除）
			int32 ComponentSize = 0;
			FTcsAttributeName Popped;
			do
			{
				Popped = Stack.Pop(EAllowShrinking::No);
				OnStack.Remove(Popped);
				++ComponentSize;
			}
			while (!(Popped == Node) && Stack.Num() > 0);

			if (ComponentSize > 1)
			{
				bOutHasCycle = true;
			}
		}
	}

	// 环检测（严格模式）：图中存在环 → true
	bool HasDependencyCycle(const TMap<FTcsAttributeName, TArray<FTcsAttributeName>>& Graph)
	{
		TMap<FTcsAttributeName, int32> Index;
		TMap<FTcsAttributeName, int32> LowLink;
		TArray<FTcsAttributeName> Stack;
		TSet<FTcsAttributeName> OnStack;
		int32 NextIndex = 0;
		bool bHasCycle = false;

		for (const TPair<FTcsAttributeName, TArray<FTcsAttributeName>>& Pair : Graph)
		{
			if (!Index.Contains(Pair.Key))
			{
				StrongConnect(Pair.Key, Graph, Index, LowLink, Stack, OnStack, NextIndex, bHasCycle);
				if (bHasCycle)
				{
					break;
				}
			}
		}

		return bHasCycle;
	}
}



void FTcsAttributePipeline::RegisterDependency(
	FTcsAttributeStore& Store, const FTcsAttributeName& ReadAttribute, const FTcsAttributeName& ReaderAttribute)
{
	// 自引用已在属性添加期拒绝（D2-4）；此处静默跳过（防御）
	if (ReadAttribute == ReaderAttribute || ReadAttribute.IsNone())
	{
		return;
	}

	TArray<FTcsAttributeName>& Readers = Store.Dependents.FindOrAdd(ReadAttribute);
	if (Readers.Contains(ReaderAttribute))
	{
		return;	// 边已存在
	}

	Readers.Add(ReaderAttribute);

	// 成环检测（严格模式）：成环则**拒绝刚登记的这条边**并 ensure
	// （读者在本次重算中使用被读者的上一缓存值——确定性优先于未定义语义）
	if (HasDependencyCycle(Store.Dependents))
	{
		Readers.Remove(ReaderAttribute);
		if (Readers.Num() == 0)
		{
			Store.Dependents.Remove(ReadAttribute);
		}

		ensureMsgf(false,
			TEXT("属性依赖成环：已拒绝该边（读者 %s 读取者 %s）——请检查 AttributeScaled 配置"),
			*ReaderAttribute.Name.ToString(), *ReadAttribute.Name.ToString());
	}
}

void FTcsAttributePipeline::MarkDependentsDirty(
	FTcsAttributeStore& Store, const FTcsAttributeName& ChangedAttribute)
{
	const TArray<FTcsAttributeName>* Readers = Store.Dependents.Find(ChangedAttribute);
	if (!Readers)
	{
		return;
	}

	// 只标直接读者：多级传播由提交期的多轮扫描承担（环已被拒，收敛有界）
	for (const FTcsAttributeName& Reader : *Readers)
	{
		if (FTcsAttributeInstance* ReaderInstance = Store.FindInstance(Reader))
		{
			ReaderInstance->bDirty = true;
		}
	}
}
