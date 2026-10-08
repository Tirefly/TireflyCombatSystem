// Copyright Tirefly. All Rights Reserved.

#include "TcsSkillSubsystem.h"

#include "TcsSkillLogChannel.h"



// 定义登记
bool UTcsSkillSubsystem::RegisterSkillDef(FGameplayTag DefTag, const FTcsSkillDefData& Def)
{
	// 身份来自资产（`UTcsSkillDef::DefTag`）——由调用方传入，不从内容里读（理由见头文件）
	if (!DefTag.IsValid())
	{
		UE_LOG(LogTcsSkill, Error, TEXT("技能定义登记被拒：DefTag 无效"));
		return false;
	}

	if (RegisteredDefs.Contains(DefTag))
	{
		// 不静默覆写（口径同链 / 触发 / 状态定义登记）——同一身份两个定义是加载期错误
		UE_LOG(LogTcsSkill, Error, TEXT("技能定义登记被拒：%s 在本世界已登记（不静默覆写）"),
			*DefTag.ToString());
		return false;
	}

	// 按值拷入（TUniquePtr 持有使解析返回的指针地址稳定，且内容不与定义库共享生命周期）
	RegisteredDefs.Add(DefTag, MakeUnique<FTcsSkillDefData>(Def));

	return true;
}

bool UTcsSkillSubsystem::UnregisterSkillDef(FGameplayTag DefTag)
{
	// 已建的条目持有的是 `DefTag`（身份）而非定义指针 ⇒ 注销只影响"新授予能否解析到定义"
	if (RegisteredDefs.Remove(DefTag) > 0)
	{
		return true;
	}

	UE_LOG(LogTcsSkill, Warning, TEXT("技能定义注销：%s 未登记（正常清理路径）"), *DefTag.ToString());
	return false;
}

const FTcsSkillDefData* UTcsSkillSubsystem::GetRegisteredSkillDef(FGameplayTag DefTag) const
{
	// 未登记返回 nullptr——正常查询路径，不 ensure（同族的 ResolveSkillDef / GetRegisteredStateDef 同款）
	const TUniquePtr<FTcsSkillDefData>* Found = RegisteredDefs.Find(DefTag);
	return Found ? Found->Get() : nullptr;
}

int32 UTcsSkillSubsystem::GetRegisteredSkillDefCount() const
{
	return RegisteredDefs.Num();
}
