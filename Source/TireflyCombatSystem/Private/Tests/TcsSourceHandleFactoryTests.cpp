// Copyright Tirefly. All Rights Reserved.

#include "TcsSourceHandle.h"

#include "Misc/AutomationTest.h"
#include "Misc/FileHelper.h"
#include "Misc/Paths.h"



#if WITH_AUTOMATION_TESTS

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FTcsSourceHandleFactoryRootTest,
	"TireflyCombatSystem.SourceHandle.Factory.Root",
	EAutomationTestFlags_ApplicationContextMask | EAutomationTestFlags::EngineFilter)

bool FTcsSourceHandleFactoryRootTest::RunTest(const FString& Parameters)
{
	FTcsSourceHandleFactory::ResetForTests();

	const FTcsSourceHandle DefaultHandle;
	TestFalse(TEXT("Default SourceHandle is invalid"), DefaultHandle.IsValid());
	TestEqual(TEXT("Default SourceHandle starts with invalid ID"), DefaultHandle.Id, -1);

	const FTcsSourceHandle FirstHandle = FTcsSourceHandleFactory::CreateRootSourceHandle();
	TestTrue(TEXT("First SourceHandle is valid"), FirstHandle.IsValid());
	TestEqual(TEXT("First SourceHandle ID is zero"), FirstHandle.Id, 0);
	TestEqual(TEXT("Root SourceHandle has no causality chain"), FirstHandle.CausalityChain.Num(), 0);

	const FTcsSourceHandle SecondHandle = FTcsSourceHandleFactory::CreateRootSourceHandle();
	TestTrue(TEXT("Second SourceHandle is valid"), SecondHandle.IsValid());
	TestEqual(TEXT("Second SourceHandle ID increments"), SecondHandle.Id, 1);
	TestNotEqual(TEXT("Root SourceHandle IDs are unique"), FirstHandle.Id, SecondHandle.Id);

	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FTcsSourceHandleFactoryChildTest,
	"TireflyCombatSystem.SourceHandle.Factory.Child",
	EAutomationTestFlags_ApplicationContextMask | EAutomationTestFlags::EngineFilter)

bool FTcsSourceHandleFactoryChildTest::RunTest(const FString& Parameters)
{
	FTcsSourceHandleFactory::ResetForTests();

	const FPrimaryAssetId SkillSourceDefId(FPrimaryAssetType(TEXT("TcsTestSkill")), FName(TEXT("Skill_A")));
	const FPrimaryAssetId BuffSourceDefId(FPrimaryAssetType(TEXT("TcsTestBuff")), FName(TEXT("Buff_A")));

	const FTcsSourceHandle RootHandle = FTcsSourceHandleFactory::CreateRootSourceHandle();
	const FTcsSourceHandle ChildHandle = FTcsSourceHandleFactory::CreateChildSourceHandle(
		RootHandle,
		SkillSourceDefId);

	TestTrue(TEXT("Child SourceHandle is valid"), ChildHandle.IsValid());
	TestEqual(TEXT("Child SourceHandle receives next ID"), ChildHandle.Id, 1);
	TestEqual(TEXT("Child SourceHandle appends one direct parent source"), ChildHandle.CausalityChain.Num(), 1);
	TestTrue(TEXT("Child SourceHandle stores direct parent source"), ChildHandle.CausalityChain[0] == SkillSourceDefId);

	const FTcsSourceHandle GrandchildHandle = FTcsSourceHandleFactory::CreateChildSourceHandle(
		ChildHandle,
		BuffSourceDefId);

	TestTrue(TEXT("Grandchild SourceHandle is valid"), GrandchildHandle.IsValid());
	TestEqual(TEXT("Grandchild SourceHandle receives next ID"), GrandchildHandle.Id, 2);
	TestEqual(TEXT("Grandchild SourceHandle inherits and appends chain"), GrandchildHandle.CausalityChain.Num(), 2);
	TestTrue(TEXT("Grandchild SourceHandle preserves root parent source"), GrandchildHandle.CausalityChain[0] == SkillSourceDefId);
	TestTrue(TEXT("Grandchild SourceHandle appends direct parent source"), GrandchildHandle.CausalityChain[1] == BuffSourceDefId);

	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FTcsSourceHandleFactoryInvalidChildTest,
	"TireflyCombatSystem.SourceHandle.Factory.InvalidChild",
	EAutomationTestFlags_ApplicationContextMask | EAutomationTestFlags::EngineFilter)

bool FTcsSourceHandleFactoryInvalidChildTest::RunTest(const FString& Parameters)
{
	FTcsSourceHandleFactory::ResetForTests();

	const FPrimaryAssetId ValidSourceDefId(FPrimaryAssetType(TEXT("TcsTestSkill")), FName(TEXT("Skill_A")));
	const FTcsSourceHandle InvalidParentChild = FTcsSourceHandleFactory::CreateChildSourceHandle(
		FTcsSourceHandle(),
		ValidSourceDefId);
	TestFalse(TEXT("Child SourceHandle rejects invalid parent"), InvalidParentChild.IsValid());

	const FTcsSourceHandle FirstRootAfterInvalidParent = FTcsSourceHandleFactory::CreateRootSourceHandle();
	TestEqual(TEXT("Invalid parent does not consume an ID"), FirstRootAfterInvalidParent.Id, 0);

	const FTcsSourceHandle InvalidSourceIdChild = FTcsSourceHandleFactory::CreateChildSourceHandle(
		FirstRootAfterInvalidParent,
		FPrimaryAssetId());
	TestFalse(TEXT("Child SourceHandle rejects invalid source definition ID"), InvalidSourceIdChild.IsValid());

	const FTcsSourceHandle SecondRootAfterInvalidSourceId = FTcsSourceHandleFactory::CreateRootSourceHandle();
	TestEqual(TEXT("Invalid source definition ID does not consume an ID"), SecondRootAfterInvalidSourceId.Id, 1);

	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FTcsSourceHandleStateRemovalCleanupGuardTest,
	"TireflyCombatSystem.SourceHandle.StateRemoval.CleanupGuard",
	EAutomationTestFlags_ApplicationContextMask | EAutomationTestFlags::EngineFilter)

bool FTcsSourceHandleStateRemovalCleanupGuardTest::RunTest(const FString& Parameters)
{
	const FString LifecycleSourcePath = FPaths::ConvertRelativePathToFull(
		FPaths::ProjectPluginsDir() / TEXT("TireflyCombatSystem/Source/TireflyCombatSystem/Private/State/TcsStateComponent_Lifecycle.cpp"));

	FString LifecycleSource;
	if (!FFileHelper::LoadFileToString(LifecycleSource, *LifecycleSourcePath))
	{
		AddError(FString::Printf(TEXT("Failed to load lifecycle source file: %s"), *LifecycleSourcePath));
		return false;
	}

	TestTrue(
		TEXT("FinalizeStateRemoval uses SourceHandle.IsValid() so Id 0 is cleaned up"),
		LifecycleSource.Contains(TEXT("StateInstance->GetSourceHandle().IsValid()")));
	TestFalse(
		TEXT("FinalizeStateRemoval must not exclude Id 0"),
		LifecycleSource.Contains(TEXT("StateInstance->GetSourceHandle().Id > 0")));

	return true;
}

#endif
