// Copyright Tirefly. All Rights Reserved.


#include "Buff/BuffMerger/TcsBuffMerger.h"



ETcsBuffMergeDependencyFlags UTcsBuffMerger::GetDependencyFlags_Implementation() const
{
	return ETcsBuffMergeDependencyFlags::MemberSet
		| ETcsBuffMergeDependencyFlags::ApplyTimestamp
		| ETcsBuffMergeDependencyFlags::Instigator
		| ETcsBuffMergeDependencyFlags::RuntimeStack
		| ETcsBuffMergeDependencyFlags::ExecutionStage
		| ETcsBuffMergeDependencyFlags::SlotGateState;
}