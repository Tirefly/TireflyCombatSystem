// Copyright Tirefly. All Rights Reserved.

#include "State/TcsStateStackFragment.h"

// 本文件原为状态侧决策 Fragment 的**三方法实现处**；那三个方法已于 2026-10-14
// **上提到共享层的统一决策接口** `FTcsEntityAdmission`
// （见 `Runtime/TcsEntityAdmission.cpp` 的中性默认实现）。
//
// **本类型现只剩"继承 + 反射身份"**：`FTcsStateStackDecisionFragment : public FTcsEntityAdmission`
// 不再声明任何决策方法 ⇒ 本翻译单元**不再需要任何实现**。
// 保留本文件是为了让"状态侧的决策实现已上提"这件事在目录结构上可见（空实现体不是笔误）。
//
// **行为不变（可核）**：共享层默认实现与原先这三条**逐字相同**——
// `IsSameGroup` = 同实体 + 同 `DefTag`；`ShouldAccept` = 恒 true；
// `ResolveStacks` = 未声明来源按同来源、同来源不变、异来源 +1。
// 差异只有一处且是**修 bug 性质**：原先比较的是 `FTcsStateStackRequest` / `FTcsStateInstance`
// 两个域类型（后者当时装全部 14 字段），现经适配层转成通用视图（字段直取、零加工）⇒ 同输入同结论。
// **★ 2026-10-14 拆解后（`PLN-REFACTOR-1` Task 2）**：当时的 `FTcsStateInstance` 已拆为
// 基底 `FTcsStateInstance`（公共 6 字段）+ `FTcsBuffInstance`（Buff 专属 8 字段）；
// 本句涉及的 `DefTag` / `Entity` / `Source` **三个字段跨拆解位置不变**（前两个在基底、`Source` 在派生）
// ⇒ **上述"同输入同结论"的结论不受拆解影响**。
