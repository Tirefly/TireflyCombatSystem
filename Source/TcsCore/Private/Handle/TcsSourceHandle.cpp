// Copyright Tirefly. All Rights Reserved.

#include "Handle/TcsSourceHandle.h"

#include <atomic>



// 来源 Id 发号计数器（进程唯一）
//
// 只此一份的原因：本文件编进 TcsCore.dll，而 `Allocate()` 是导出符号——TcsIntegration / TcsDamage /
// 宿主 TcsDev 等镜像全部调到同一个它。反之若把计数器留在头里（实例成员或 `static` 成员/内联变量），
// MSVC 只在单个镜像内做 COMDAT 折叠，每个 DLL 各留一份计数器、各自从 1 开始（台账 `WAIT-6`）。
//
// 命名带文件前缀：unity 构建会把同模块多个 .cpp 并进同一 TU、匿名 namespace 随之合并
// （2026-09-18 实证：同名 file-local 符号报 C2084"已有主体"）——file-local 符号一律带文件前缀。
namespace
{
	std::atomic<uint64> GTcsSourceHandleNextId{0};
}



// 分配新来源 Id（原子递增；0 保留为无效值）
FTcsSourceHandle FTcsSourceHandleRegistry::Allocate()
{
	FTcsSourceHandle Handle;
	Handle.Id = GTcsSourceHandleNextId.fetch_add(1, std::memory_order_relaxed) + 1;
	return Handle;
}
