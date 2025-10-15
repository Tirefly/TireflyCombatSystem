# TCS 插件阶段性开发计划

> **范围说明**  
> - 本计划仅覆盖 `Documents/技能修改器/` 下的设计文档以及 `Documents/状态树与状态管理/TCS_阶段划分_即将开发_非策略部分.md` 中列出的任务。  
> - `Documents/状态树与状态管理/TCS_阶段划分_暂缓设计_策略解析_免疫_净化.md` 所述内容视为“暂缓”，本阶段绝对不开展相关实现。

---

## 阶段 0：规划确认与基线梳理（1 周）
- 整理现有实现差异：对照  
  - 《技能修改器深度设计》（`技能修改器/TCS_技能修改器深度设计.md`）  
  - 《技能修改器：触发模型与事件对接》（`技能修改器/TCS_技能修改器_触发模型与事件对接.md`）  
  - 《技能修改器：类型与接口声明》（`技能修改器/TCS_技能修改器_类型与接口声明.md`）  
  - 《技能/状态参数：FName 与 GameplayTag 并立设计》（`技能修改器/TCS_参数键并立设计_FName与GameplayTag.md`）  
  - 《TCS 阶段划分：即将开发的实现文档（非策略部分）》（`状态树与状态管理/TCS_阶段划分_即将开发_非策略部分.md`）
- 输出差异清单与责任/验收草案：见《TCS 阶段0差异清单》（`Documents/TCS_阶段0差异清单.md`），后续在项目管理工具补充负责人。
- 输出差异清单与责任分配，锁定阶段性目标与验收标准。

## 阶段 1：技能修改器运行态重构（2~3 周）
- **目标**：让技能修改器的运行态由 `UTcsSkillComponent` 统一管理，完成文档中定义的运行数据结构、聚合缓存与事件管线。  
  - 实施 `ActiveSkillModifiers/NameAggCache/TagAggCache/DirtyNames/DirtyTags` 等结构，并对照《技能修改器深度设计》（第 2~4 章）。  
  - 补齐默认执行器、合并器、过滤器、条件，实现最小可用集合（参照《类型与接口声明》第 1~4 节）。  
  - 实现 `ApplySkillModifiers / RemoveSkillModifierById / UpdateSkillModifiers` 等接口，并与 `UTcsSkillManagerSubsystem` 门面对齐。
- **实施步骤建议**：  
  1. 新增 `Skill/Modifiers` 目录下的核心类型（Definition、Instance、Execution、Merger、Filter、Condition）及默认实现骨架。  
  2. 扩展 `UTcsCombatSystemSettings`，增加 `SkillModifierDefTable`，并提供装载/校验辅助接口。  
  3. 重构 `UTcsSkillComponent` 数据成员与生命周期：引入运行态缓存、Dirty 队列、事件入口（参见深度设计 §2.2 与触发模型 §3）。  
  4. 调整 `UTcsSkillManagerSubsystem`，增加 Apply/Remove/Update 门面函数，读取数据表创建实例。  
  5. 迁移 `UTcsSkillInstance` 与组件交互：删除旧的 `Additive/MultiplicativeModifiers`，改为通过组件查询聚合结果。  
  6. 自测与回归：构建临时自动化测试或示例蓝图，验证 Apply/Remove/Update 及聚合缓存命中情况。

- **同步事项**：调整旧的 `UTcsSkillInstance` 加法/乘法数组，迁移到组件查询模式（参考《深度设计》第 8 章）。

## 阶段 2：参数键并立与快照/实时同步改造（1~2 周）
- **目标**：为技能与状态参数实现 FName 与 GameplayTag 双命名空间。  
  - 扩展 `FTcsStateDefinition`、`UTcsStateInstance`、`UTcsSkillInstance` 数据结构和 API（详见《参数键并立设计》）。  
  - 改造快照与实时同步逻辑，使 `UTcsSkillComponent::UpdateActiveSkillRealTimeParameters` 与新聚合缓存协同。  
  - 更新技能修改器执行器，确保同时支持 Name/Tag（参照《类型与接口声明》执行器参数声明部分）。

## 阶段 3：技能施放与状态槽联动（1~2 周）
- **目标**：确保技能状态实例统一走槽位与激活流程。  
  - 修改 `UTcsSkillComponent::TryCastSkill`：创建状态实例后立即调用 `AssignStateToStateSlot`，并触发阶段事件。  
  - 修复 `UTcsStateComponent::AddStateInstance` 与 `ActiveStateInstances` 维护逻辑，落实《TCS 阶段划分：即将开发…》新增的任务 7。  
  - 对 `UTcsStateManagerSubsystem::ApplyState` 做适配，保证技能与普通状态共用入口。

## 阶段 4：状态管理事件化与槽位增强（2 周）
- **目标**：兑现状态系统文档中“事件驱动”与相关待办。  
  - 将 `UTcsStateComponent::CheckAndUpdateStateTreeSlots` 改为事件触发，具体参见《文档：后续改进细节.md》中对应条目。  
  - 按《TCS 阶段划分：即将开发…》的优先级 1~6 逐项推进（合并器接入、ApplyStateToSpecificSlot、槽位排队、Duration 策略、抢占策略、调试 Evaluator）。  
  - 确保所做改动不触碰“暂缓设计”文档中的策略/免疫/净化内容。

## 阶段 5：验证、文档与回归（1 周）
- 编写/更新自动化测试（属性修改器、技能修改器、槽位激活等），覆盖新接口。  
- 补充 README/开发文档中的接口示例与使用说明。  
- 进行一次完整的 `TireflyGameplayUtilsEditor Win64 Development` 构建与核心自动化测试跑通，准备阶段验收报告。

---

## 附：阶段出口检查清单
| 阶段 | 验收要点 |
| ---- | -------- |
| 0 | 差异清单、阶段时间表、责任人确定 |
| 1 | 技能修改器运行态迁移完毕，新旧接口切换，基础执行/合并/筛选/条件可用 |
| 2 | 参数 Name/Tag 双命名空间生效，快照/实时同步与聚合缓存联通 |
| 3 | 技能施放必经槽位，状态索引与事件广播正确 |
| 4 | StateTree Gate 事件化、槽位增强项逐条落地且无越权开发 |
| 5 | 测试与文档更新完成，构建/自动化验证通过 |
