TCS 技能/状态参数：FName 与 GameplayTag 并立设计

目标
- 同一套系统下，参数键支持 FName 与 GameplayTag 两条独立命名空间；设计师可任选其一或两者并用。
- 不做 Tag↔Name 的映射/折叠/优先级；两者并立、互不影响。
- 兼容现有数据与实现（FName 流程不变），在此基础上新增 Tag 流程的等价能力。

一、数据模型改动
1) 状态定义（FTcsStateDefinition）
- 现状：Parameters: TMap<FName, FTcsStateParameter>
- 新增：TagParameters: TMap<FGameplayTag, FTcsStateParameter>
- 语义：两套表各自独立，等价一等公民；当同语义参数既在 Parameters 又在 TagParameters 配置时，代表“存在两份独立参数”。

2) 状态实例（UTcsStateInstance）
- 存储与 API 成对出现：
  - ByName：保持现有 Numeric/Bool/Vector 的 Map 与 Get/Set 接口
  - ByTag：新增 Numeric/Bool/Vector 的 Map 与 Get/Set 接口（后缀 ByTag）

3) 技能实例（UTcsSkillInstance）
- 存储与 API 成对出现：
  - ByName：保持现有 Bool/Vector/NumericParameterConfigs/快照缓存/实时缓存/时间戳机制与 Get/Set/Calculate/Sync 接口
  - ByTag：新增同名的一整套容器与接口（后缀 ByTag），包含 CalculateNumericParameterByTag、Get/Set Bool/Vector ByTag、快照/实时 ByTag 等

二、技能修改器（Skill Modifier）改动
1) 参数载荷（Modifier Params）
- 在 Additive/Multiplicative 等载荷结构体中，新增 ParamTag 字段；ParamName 与 ParamTag 可同时出现：
  - 若 ParamName 有效 → 作用于 ByName 参数空间
  - 若 ParamTag 有效 → 作用于 ByTag 参数空间
  - 若两者同时有效 → 对两个参数空间分别执行一次（互不影响）

2) 执行器（Execution）
- 读取载荷后分别对 Name 与 Tag 两套空间执行逻辑；无需映射或折叠。

三、聚合与缓存（UTcsSkillComponent）
- 聚合缓存与脏集按两套空间分别维护：
  - NameAggCache: TMap<UTcsSkillInstance*, TMap<FName, FAggregatedParamEffect>>
  - TagAggCache: TMap<UTcsSkillInstance*, TMap<FGameplayTag, FAggregatedParamEffect>>
  - DirtyNames: TMap<UTcsSkillInstance*, TSet<FName>>
  - DirtyTags: TMap<UTcsSkillInstance*, TSet<FGameplayTag>>
- 对外查询接口提供 ByName 与 ByTag 版本：GetAggregatedParamEffect / GetAggregatedParamEffectByTag。
- 合并/优先级/覆盖等规则在各自空间内独立生效，不做跨空间合并。

四、快照与实时
- TryCast 前：分别对 ByName 与 ByTag 的快照参数拍快照并写入 StateInstance（SetNumericParam / SetNumericParamByTag）。
- 实时：UpdateActiveSkillRealTimeParameters 中分别处理 Name 与 Tag 空间的实时参数同步（引入 ByTag 的最近值缓存与时间戳）。

五、校验与 UX
- Definition/载荷校验：至少填一个键（Name 或 Tag）；两者同时填写即表示“双份生效”。
- 蓝图 API：ByName 与 ByTag 分组展示，便于策划选择；载荷里的 ParamTag 字段 meta=(Categories="Param")，便于筛选。

六、示例
- Damage（ByName）+ Cooldown（ByTag）并存：
  - SkillDef.Parameters.Add("Damage", ...)
  - SkillDef.TagParameters.Add(Param.Cooldown, ...)
  - ModA：AdditiveParam(ParamName="Damage", +10)
  - ModB：CooldownMultiplier(ParamTag=Param.Cooldown, 0.8)
- 同时作用两套空间：
  - ModC：MultiplicativeParam(ParamName="Damage", ParamTag=Param.Damage, 1.2)
  - 效果：Name 空间与 Tag 空间的“伤害”各自×1.2，互不干涉。

七、落地顺序（无实现细节，便于排期）
1) FTcsStateDefinition：增加 TagParameters，并在参数计算/同步流程中遍历两套表
2) UTcsStateInstance：ByTag 的三类参数 Map 与 Get/Set API
3) UTcsSkillInstance：ByTag 的容器、Calculate/快照/实时 API 与缓存
4) UTcsSkillComponent：NameAggCache/TagAggCache/DirtyNames/DirtyTags 与相应查询/重算
5) Modifier Params：Additive/Multiplicative 增加 ParamTag；执行器按 Name/Tag 分别执行
6) 预检查工具/日志：校验配置与执行载荷键有效性

八、与已有设计文档的关系
- 深度设计（TCS_技能修改器深度设计.md）：作为顶层蓝图，简述“参数键并立”，并指向本文件。
- 触发模型（TCS_技能修改器_触发模型与事件对接.md）：增加“Name/Tag 双空间各自打脏与聚合”的说明。
- 类型与接口声明（TCS_技能修改器_类型与接口声明.md）：补充 ByTag 的结构与接口声明。
