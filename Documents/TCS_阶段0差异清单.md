# TCS 阶段0差异清单（技能修改器 & 状态管理）

> **范围**  
> - 对照以下文档与当前实现：  
>   1. `Documents/技能修改器/TCS_技能修改器深度设计.md`（简称“深度设计”）  
>   2. `Documents/技能修改器/TCS_技能修改器_触发模型与事件对接.md`（简称“触发模型”）  
>   3. `Documents/技能修改器/TCS_技能修改器_类型与接口声明.md`（简称“类型声明”）  
>   4. `Documents/技能修改器/TCS_参数键并立设计_FName与GameplayTag.md`（简称“键并立设计”）  
>   5. `Documents/状态树与状态管理/TCS_阶段划分_即将开发_非策略部分.md`（简称“状态阶段文档”）  
> - 暂缓内容（`TCS_阶段划分_暂缓设计_策略解析_免疫_净化.md`）不在本清单中。

---

## 1. 技能修改器体系

| 文档引用 | 目标要点 | 当前实现差距 | 受影响代码 |
| --- | --- | --- | --- |
| 深度设计 §2~§4 | 运行态由 `UTcsSkillComponent` 持有，聚合缓存（Name/Tag）、脏队列、事件入口完整 | ✅ 已实现（阶段1）：组件新增 `ActiveSkillModifiers`、聚合缓存、Dirty 集合及 Apply/Remove/Update 接口 | `Source/TireflyCombatSystem/Private/Skill/TcsSkillComponent.cpp` |
| 深度设计 §6 | 默认执行器/合并器/筛选器/条件的最小集 | ✅ 已实现（阶段1）：`Public/Skill/Modifiers/` 下补充 Definition/Instance/Execution/Merger/Filter/Condition 骨架 | `Source/TireflyCombatSystem/Public/Skill/Modifiers/` |
| 深度设计 §8 & 触发模型 §3~§5 | SkillInstance 内部 Add/Remove 修正器逻辑被替换为组件聚合；事件驱动打脏 | ✅ 已实现（阶段1）：`UTcsSkillInstance` 改为通过组件聚合查询，移除加/乘数组；组件 Tick 触发重算 | `Source/TireflyCombatSystem/Private/Skill/TcsSkillInstance.cpp` |
| 触发模型 §2 | 修改器生命周期与上下文事件触发队列化 | ⏳ 仍需完善：事件仍以 Tick 驱动，条件/触发器待事件化（后续阶段覆盖） | `Source/TireflyCombatSystem/Private/Skill/TcsSkillComponent.cpp` |
| 类型声明 §1~§4 | 定义/实例/执行/合并等类声明及接口 | ✅ 已实现（阶段1）：新增 `FTcsSkillModifierDefinition` 等类型并提供默认实现骨架 | `Source/TireflyCombatSystem/Public/Skill/Modifiers/` |
| 类型声明 §5 | `UTcsSkillManagerSubsystem` 门面新增 `ApplySkillModifier` 等接口 | ✅ 已实现（阶段1）：子系统提供 Apply/Remove/Update 门面方法 | `Source/TireflyCombatSystem/Private/Skill/TcsSkillManagerSubsystem.cpp` |
| 类型声明 §6 | Settings 暴露 `SkillModifierDefTable` | ✅ 已实现（阶段1）：在 `UTcsCombatSystemSettings` 增加 `SkillModifierDefTable` | `Source/TireflyCombatSystem/Public/TcsCombatSystemSettings.h` |

---

## 2. 参数键并立与同步机制

| 文档引用 | 目标要点 | 当前实现差距 | 受影响代码 |
| --- | --- | --- | --- |
| 键并立设计 §1~§3 | `FTcsStateDefinition` / `UTcsStateInstance` / `UTcsSkillInstance` 维护 FName + GameplayTag 双 Map 与 API | ✅ 已实现（阶段2）：状态定义新增 `TagParameters`，实例层提供 ByTag 读写接口与容器缓存 | `Source/TireflyCombatSystem/Public/State/TcsState.h:158`，`Source/TireflyCombatSystem/Public/Skill/TcsSkillInstance.h:104` |
| 键并立设计 §4 | 聚合缓存、脏集按 Name/Tag 分离 | ✅ 已实现（阶段2）：SkillComponent 聚合、脏集与查询同步扩展 Tag 通道 | `Source/TireflyCombatSystem/Private/Skill/TcsSkillComponent.cpp:482` |
| 键并立设计 §5 | 快照 & 实时同步覆盖 Name/Tag | ✅ 已实现（阶段2）：SkillInstance 快照、实时同步与差异检测覆盖 ByTag 流程 | `Source/TireflyCombatSystem/Private/Skill/TcsSkillInstance.cpp:322-600` |
| 键并立设计 §6 | 配置校验/日志 | 无预检查入口 | `Source/TireflyCombatSystem/Public/Skill`（待新增） |

---

## 3. 状态槽与 StateTree 联动

| 文档引用 | 目标要点 | 当前实现差距 | 受影响代码 |
| --- | --- | --- | --- |
| 状态阶段文档 §1 “尚未实现”1) | 合并器接入 `AssignStateToStateSlot` | ✅ 已实现（阶段4）：按同/异发起者选择 `UTcsStateMerger`，合并结果驱动槽位刷新 | `Source/TireflyCombatSystem/Private/State/TcsStateComponent.cpp:542-714` |
| 同文档 2) | `ApplyStateToSpecificSlot` 管理器接口 | 子系统仅有 `ApplyState` | `Source/TireflyCombatSystem/Private/State/TcsStateManagerSubsystem.cpp:60-110` |
| 同文档 3) | 槽位排队/延迟应用 | ✅ 已实现（阶段4）：新增排队队列、TTL、Gate 打开重试 | `Source/TireflyCombatSystem/Public/State/TcsStateComponent.h:153`<br>`Source/TireflyCombatSystem/Private/State/TcsStateComponent.cpp:360-481` |
| 同文档 4) | `ETcsDurationTickPolicy` | ✅ 已实现（阶段4）：数据表配置策略，Tick 中按策略递减持续时间 | `Source/TireflyCombatSystem/Public/State/TcsStateSlot.h:29`<br>`Source/TireflyCombatSystem/Private/State/TcsStateComponent.cpp:140-201` |
| 同文档 5) | Gate 关闭/抢占策略、挂起复用 | ✅ 已实现（阶段4）：支持 Gate 关闭暂停/取消、优先级抢占挂起/清除、StateTree Pause/Resume | `Source/TireflyCombatSystem/Public/State/TcsStateSlot.h:43`<br>`Source/TireflyCombatSystem/Private/State/TcsStateComponent.cpp:256-359`<br>`Source/TireflyCombatSystem/Private/State/TcsState.cpp:300-360` |
| 同文档 6) | 顶层 Slot Debug Evaluator | ✅ 已实现（阶段4）：新增 `FTcsStateSlotDebugEvaluator` 输出调试快照 | `Source/TireflyCombatSystem/Public/StateTree/TcsStateSlotDebugEvaluator.h`<br>`Source/TireflyCombatSystem/Private/StateTree/TcsStateSlotDebugEvaluator.cpp` |
| 同文档 新增 7) | 技能施放统一走槽位管线 | `UTcsSkillComponent::TryCastSkill` 添加状态后未调用 `AssignStateToStateSlot`，只手动启动 StateTree | `Source/TireflyCombatSystem/Private/Skill/TcsSkillComponent.cpp:198-270` |
| 文档：后续改进细节 | StateTree Gate 事件驱动 | ✅ 已落地（阶段4）：添加事件驱动刷新请求与低频兜底的 Gate 同步 | `Source/TireflyCombatSystem/Public/State/TcsStateComponent.h:156-165`<br>`Source/TireflyCombatSystem/Private/State/TcsStateComponent.cpp:108-199, 482-518` |
| 同条 | 状态到期/移除通知 | `HandleStateInstanceRemoval` / `OnStateInstanceDurationExpired` 仅留 TODO | `Source/TireflyCombatSystem/Private/State/TcsStateManagerSubsystem.cpp:111-162` |

> **阶段3差异焦点（技能施放 ↔ 槽位联动）**
>
> | 关注项 | 当前状态 | 备注 | 受影响代码 |
> | --- | --- | --- | --- |
> | `UTcsSkillComponent::TryCastSkill` 走槽位管线 | ✅ 已改造（阶段3）：通过 StateManagerSubsystem/StateComponent 分配槽位，失败时回退直接激活 | `Source/TireflyCombatSystem/Private/Skill/TcsSkillComponent.cpp:199-320` |
> | `UTcsStateComponent` 合并/激活逻辑缺口 | ✅ 已闭环（阶段3）：槽位分配沿用合并/排队策略，并在阶段变更时统一广播事件 | `Source/TireflyCombatSystem/Private/State/TcsStateComponent.cpp:291-1384` |
> | `UTcsStateManagerSubsystem` 指定槽位接口 | ✅ 已补充：新增 `ApplyStateToSpecificSlot / ApplyStateInstanceToSlot` | `Source/TireflyCombatSystem/Private/State/TcsStateManagerSubsystem.cpp:60-170` |
> | 槽位事件/Active 列表同步 | ✅ 已完成（阶段3）：`OnStateStageChanged` 事件驱动 `ActiveSkillStateInstances`，移除手工维护 | `Source/TireflyCombatSystem/Public/State/TcsStateComponent.h:20`<br>`Source/TireflyCombatSystem/Private/Skill/TcsSkillComponent.cpp:38-120, 518-575` |
> | 数据配置校验 | ✅ 已补充（阶段3）：Settings 数据校验 & 运行时告警同步约束技能 `StateSlotType` | `Source/TireflyCombatSystem/Public/TcsCombatSystemSettings.h:41-118`<br>`Source/TireflyCombatSystem/Private/Skill/TcsSkillComponent.cpp:262-276` |

---

## 4. 阶段验收与责任

| 阶段 | 状态 | 验收标准草案 | 责任人（建议） | 协作范围 | 文档依据 |
| --- | --- | --- | --- | --- | --- |
| 阶段0：基线梳理 | ✅ 完成 | 差异清单成稿，阶段目标与责任划分同步输出 | Combat Runtime 文档维护（Owner） | 产品 & 系统设计复核 | TCS 插件开发计划 |
| 阶段1：技能修改器运行态重构 | ✅ 完成 | SkillComponent 持有运行态、默认执行/合并/筛选/条件落地、旧 SkillInstance 修正器逻辑删除 | Combat Runtime 运行态负责人（Owner） | 系统架构 & QA | 深度设计 §2~§8；类型声明 §1~§4 |
| 阶段2：参数键并立 | ✅ 完成 | Name/Tag 双命名空间落地，快照/实时同步与聚合缓存均支持 Tag | 参数通道负责人（Owner） | 状态系统、数据表维护、小队 QA | 键并立设计 §1~§5 |
| 阶段3：技能施放与槽位联动 | ✅ 完成 | TryCastSkill -> AssignStateToStateSlot；ActiveStateInstances 与槽位事件对齐 | 战斗槽位负责人（Owner） | StateTree 设计、Gameplay 程序 | 状态阶段文档 §1(7) |
| 阶段4：状态管理事件化与槽位增强 | ✅ 完成 | 合并器接入、排队/持续时间策略、Gate 事件化与抢占、Slot 调试 Evaluator 全量落地 | 状态系统负责人（Owner） | Combat Runtime、工具链、QA | 状态阶段文档 §1(1~6)，文档：后续改进细节 |
| 阶段5：验证与回归 | 🔲 规划中 | 自动化测试 & 构建通过，文档/README 更新 | QA / Tech Writer 联合（Owner） | 全模块提测支持 | TCS 插件开发计划 |

> 责任说明：若后续在项目管理工具中指派到具体成员，请同步更新 Owner 字段以保持文档准确性。

---

## 5. 后续动作建议
1. 阶段2收尾：同步策划/数据表 Owner，梳理 `TagParameters` 配置准则并安排一次聚合/同步功能回归测试。  
2. 阶段3验收：组织技能施放→槽位→StateTree 回归，覆盖阶段事件广播与 Active 列表同步。  
3. 阶段4验收：延续状态系统回归案例，验证排队、挂起/抢占、持续时间策略与 Gate 事件链路。  
4. 阶段5（验证/回归）：QA Owner 准备自动化测试矩阵，在阶段3/4 验收完成后补充脚本与文档。
