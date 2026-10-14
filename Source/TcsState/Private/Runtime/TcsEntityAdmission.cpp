// Copyright Tirefly. All Rights Reserved.

#include "Runtime/TcsEntityAdmission.h"



// 统一重申请决策接口（中性默认实现）
bool FTcsEntityAdmission::IsSameGroup(
	const FTcsEntityAdmissionRequest& Request,
	const FTcsEntityAdmissionExisting& Existing) const
{
	// 默认 = 内置"不分组"语义：组键基座（同实体 + 同定义），不含来源与发起者
	return Existing.DefTag == Request.DefTag && Existing.Entity == Request.Entity;
}



bool FTcsEntityAdmission::ShouldAccept(
	const FTcsEntityAdmissionRequest& /*Request*/,
	const FTcsEntityAdmissionExisting& /*Existing*/) const
{
	// 默认恒接受：容量与溢出政策在引擎侧照常生效
	return true;
}



int32 FTcsEntityAdmission::ResolveStacks(
	const FTcsEntityAdmissionRequest& Request,
	const FTcsEntityAdmissionExisting& Existing) const
{
	// 默认 = 内置规则：同来源续杯（层数不变）、异来源叠一层。
	// 与引擎侧同一处口径：**未声明来源按同来源处理**（否则门面每次发新号都会被判成"换了施加方"）。
	const bool bSameSource = !Request.Source.IsValid() || Request.Source == Existing.Source;
	return bSameSource ? Existing.Stacks : Existing.Stacks + 1;
}



// 技能侧默认策略：每次执行一实例（`CI_InstancePerExecution`）
bool FTcsEntityAdmissionAlwaysNew::IsSameGroup(
	const FTcsEntityAdmissionRequest& /*Request*/,
	const FTcsEntityAdmissionExisting& /*Existing*/) const
{
	// 恒不同组 ⇒ 每次申请都建新实例。**该档的语义由本字段表达，不靠算子里的特例分支**。
	return false;
}



// 技能侧默认策略：每实体一实例 + 驳回（`CI_InstancePerEntity` + `bRetriggerOnActive = false`）
bool FTcsEntityAdmissionPerEntityReject::ShouldAccept(
	const FTcsEntityAdmissionRequest& /*Request*/,
	const FTcsEntityAdmissionExisting& /*Existing*/) const
{
	// 同组（同实体 + 同定义）且已有在飞实例 ⇒ 驳回。
	// 本条只在"同组已命中"时被问到 ⇒ 恒 false 即"已有在飞就驳回"。
	return false;
}



// 技能侧默认策略：每实体一实例 + 顶替（`CI_InstancePerEntity` + `bRetriggerOnActive = true`）
bool FTcsEntityAdmissionPerEntityReplace::ShouldAccept(
	const FTcsEntityAdmissionRequest& /*Request*/,
	const FTcsEntityAdmissionExisting& /*Existing*/) const
{
	// 接受这次申请，而同组 ⇒ 调用方按 `Replace` 档"终止旧的、建新的"。
	return true;
}
