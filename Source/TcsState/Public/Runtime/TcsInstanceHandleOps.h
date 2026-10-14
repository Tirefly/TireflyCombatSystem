// Copyright Tirefly. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Handle/TcsInstanceHandle.h"
#include "Templates/TypeHash.h"



/**
 * 运行态句柄的**语义单点**（共享层公共件，2026-10-14 立）。
 *
 * **它是什么**：三个展平句柄（`FTcsStateHandle` / `FTcsSkillInstanceHandle` / `FTcsSkillEntryHandle`）
 * 的**判据 / 身份比较 / 哈希 / 池位模式转换**的唯一实现处；三个薄型各自**委托**它。
 *
 * **它不是什么（MUST NOT 误读）**：**它不是第三个句柄类型，也不是"统一句柄"**。
 * 三个句柄类型 **MUST 保持各自独立**（裁决④ 的 2026-10-14 改述 =「**3 个反射声明 + 1 处逻辑单点**」）。
 * 判据三条（本节是它们的唯一载体，MUST NOT 在别处复述第二份）：
 * 1. **合并为一个类型会移除一道现存的编译期护栏**：各型持有**幻影标签**
 *    （`FTcsStateTag` / `FTcsSkillInstanceTag` / …），使"把状态句柄传给施法运行 API"在**编译期失败**。
 *    合并后该误用降为**运行期查不到**——正是本仓反复拦的"静默错配 + 误导性症状"形态。
 *    `instance-handle-pool` 能力为此**立了两条**判据（「不同池的句柄类型不可互换」+
 *    Scenario「句柄类型隔离」→ **编译失败**，原文"跨池误用在编译期拦截，无运行期成本"）。
 * 2. **单点转换必然退化**：一个类型的 `GetInner()` **只能返回一个** `TTcsInstanceHandle<Tag>`，
 *    无法服务三个标签 ⇒ 要么拆成三个方法（等于没合并）、要么调用点各自 `static_cast`
 *    （违反"唯一转换点"）。
 * 3. **三个反射身份本就必须各自声明**：三份不同的 `USTRUCT` 反射身份**不是可消除的重复**；
 *    实测三型各自的**代码**约 15 行（其余皆为注释）⇒ 真正的重复是**逻辑**，收敛逻辑即达成 DRY。
 *
 * **★ 同时否决的绕过方案（"一个宏生成三个类型"）MUST 记录以免后人重试**：【引擎源码】实测 ——
 * 引擎 `Runtime/` 内**宏展开出 `USTRUCT(` 的用例零命中**；引擎自带的四个 wrapper-struct 宏
 * 全部只生成普通 C++ struct（无 `USTRUCT`、无 `GENERATED_BODY`）；`USTRUCT(...)` 在
 * `ObjectMacros.h` 里被定义成**空**（"给编译器看的占位"），**不是** UHT 的展开入口。
 * **且存在结构性障碍**：`ObjectMacros.h:GENERATED_BODY` = `BODY_MACRO_COMBINE(CURRENT_FILE_ID,_,__LINE__,_GENERATED_BODY)`
 * ⇒ **按 `__LINE__` 拼符号名**，而 UHT **按扫描到的行**生成对应符号 ⇒ 外层宏包裹后
 * **行号语义是否仍对齐，本机无法确认** ⇒ 依本仓既有纪律（同类"本机不可核实"的依赖一律去掉，
 * 先例 `FGameplayTagQuery`），**MUST NOT** 把本能力建在未核实的 UHT 行为上。
 *
 * **为什么是纯静态函数族**：它**不是反射类型**（不参与序列化、不进细节面板、不做脚本契约），
 * 故全内联、**不带模块导出宏**（导出宏只给"有 out-of-line 成员或反射符号"的类型——
 * 全内联值类型加宏反而会让消费方去别的 DLL 导入不存在的符号，见 `unreal-development-workflow`
 * 的「引擎机制事实」）。
 */
struct FTcsInstanceHandleOps
{
// 判据
#pragma region Predicates

public:
	/**
	 * 句柄有效性判据（**三个句柄的唯一判据**）。
	 *
	 * **统一为最严**：`Index >= 0 && Generation > 0`。
	 * **被统一的真不一致**：`FTcsSkillInstanceHandle` 原判 `Index != -1`（**不看 `Generation`**），
	 * 而另两者判本式。
	 *
	 * **为什么连 `Generation` 一起判**（原注释自述"本结构只判『是否曾被赋值』"，此处改述）：
	 * 判"是否曾被赋值"不够——`Generation == 0` 的语义是"**该槽从未分配过**"，它与
	 * `Index >= 0` 的组合只可能出现在**手工拼装的畸形句柄**上，而那种句柄一旦被当作有效传下去，
	 * 就会在解析处才失败（症状离成因很远）。⇒ 判据 MUST 在**句柄自己身上**就拒绝它。
	 *
	 * **行为中性（MUST 记录，防误读为行为变更）**：对**良构句柄**（`Allocate` 产物
	 * `Generation >= 1`；默认构造产物 `Index = -1`）两套判据**结论逐例相同**；
	 * 仅"手工构造 `Index >= 0` 且 `Generation == 0`"这一**从不出现的形态**由"有效"变为"无效"。
	 *
	 * **`Index` 可能为 0**（首个槽位）⇒ 判据是"`Generation` 非 0"，**不是**"下标非 0"。
	 *
	 * @param Index 展平句柄的槽位下标。
	 * @param Generation 展平句柄的代际。
	 * @return 返回该句柄是否有效。
	 */
	static bool IsValid(int32 Index, int32 Generation)
	{
		return Index >= 0 && Generation > 0;
	}

	/**
	 * 身份比较判据（**身份 = `(Index, Generation)` 全等**）。
	 *
	 * 为什么不是"只比 `Index`"：槽位会被复用 ⇒ 只比下标会把**两代不同的实例**认成同一个
	 * （配对清理与容器键控都会因此误伤）。
	 *
	 * @return 返回两个句柄是否同身份。
	 */
	static bool IsSameIdentity(int32 IndexA, int32 GenerationA, int32 IndexB, int32 GenerationB)
	{
		return IndexA == IndexB && GenerationA == GenerationB;
	}

	// 哈希（`TMap` / `TSet` 键控用；与身份判据同源——只哈希这两个字段）
	static uint32 Hash(int32 Index, int32 Generation)
	{
		return HashCombine(GetTypeHash(Index), GetTypeHash(Generation));
	}

#pragma endregion


// 池位模式转换
#pragma region Conversion

public:
	/**
	 * 展平句柄 → 池句柄（**唯一转换点**——避免各处自行 `static_cast` 拼装出语义不一致的句柄）。
	 *
	 * **位模式无损的依据**：展平侧 `Index` 用 `int32`（UHT 不支持 `uint32` 作属性类型）、
	 * `-1` 与池的 `InvalidIndex`（`0xFFFFFFFF`）**位模式相同** ⇒ 两个字段的 `static_cast` 互逆。
	 *
	 * @param Index 展平句柄的槽位下标（`-1` = 无效）。
	 * @param Generation 展平句柄的代际。
	 * @return 返回位模式一致的池句柄。
	 */
	template <typename TTagType>
	static TTcsInstanceHandle<TTagType> ToInner(int32 Index, int32 Generation)
	{
		TTcsInstanceHandle<TTagType> Inner;
		Inner.Index = static_cast<uint32>(Index);
		Inner.Generation = static_cast<uint32>(Generation);
		return Inner;
	}

	/**
	 * 池句柄 → 展平句柄的两个字段（**唯一转换点**，与 `ToInner` 对称）。
	 *
	 * 采用"写回出参"而不是返回某个具体句柄类型：本函数族**不认识**那三个反射类型
	 * （认识它们就得 include 三个头，且会把共享层绑到具体域）⇒ 由各薄型在自己的
	 * `SetInner` 里把出参写进自己的字段。
	 *
	 * @param Inner 池句柄。
	 * @param OutIndex 出参：展平槽位下标。
	 * @param OutGeneration 出参：展平代际。
	 */
	template <typename TTagType>
	static void FromInner(const TTcsInstanceHandle<TTagType>& Inner, int32& OutIndex, int32& OutGeneration)
	{
		OutIndex = static_cast<int32>(Inner.Index);
		OutGeneration = static_cast<int32>(Inner.Generation);
	}

#pragma endregion
};
