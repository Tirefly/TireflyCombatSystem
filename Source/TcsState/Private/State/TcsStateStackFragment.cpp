// Copyright Tirefly. All Rights Reserved.

#include "State/TcsStateStackFragment.h"

#include "State/TcsStateInstance.h"



bool FTcsStateStackDecisionFragment::IsSameGroup(
	const FTcsStateStackRequest& Request,
	const FTcsStateInstance& Existing) const
{
	// 默认 = 内置"不分组"语义：组键基座（同单位 + 同定义），不含来源与发起者
	return Existing.DefTag == Request.DefTag && Existing.Unit == Request.Unit;
}



bool FTcsStateStackDecisionFragment::ShouldAccept(
	const FTcsStateStackRequest& /*Request*/,
	const FTcsStateInstance& /*Existing*/) const
{
	// 默认恒接受：容量与溢出政策在引擎侧照常生效
	return true;
}



int32 FTcsStateStackDecisionFragment::ResolveStacks(
	const FTcsStateStackRequest& Request,
	const FTcsStateInstance& Existing) const
{
	// 默认 = 内置规则：同来源续杯（层数不变）、异来源叠一层。
	// 与引擎侧同一处口径：**未声明来源按同来源处理**（否则门面每次发新号都会被判成"换了施加方"）。
	const bool bSameSource = !Request.Source.IsValid() || Request.Source == Existing.Source;
	return bSameSource ? Existing.Stacks : Existing.Stacks + 1;
}
