# M3（状态/Buff 层）决策点提案 v1（草稿，待拍板）

- 日期：2026-09-02
- 状态：**草稿**——提前一轮起草（顺序无关、只依赖已定裁决）；若 MD-1/MD-2 拍板有变仅重编号
- 证据核验：2026-09-02 已对照 TCS 调研报告 01/02/06/07/08 逐条核验（要点见各决策点标注）
- 前置：裁决1（Tag 关系表）已定**数据形状**；本文只裁**执行时机与机制**

## 核验带来的三个认知修正（读决策点前先看）

1. **"策略类爆炸"在 Buff 域不成立**：BuffDef 上仅 2 个 TSubclassOf（Merger + InstanceClass）、0 个 FInstancedStruct；爆炸是全插件口径（TSubclassOf×17）。Buff 行为的真正主载体是**每个 buff 实例内嵌一棵 StateTree**——这是重建要显式重审的重磅选择。
2. **TCS 没有任何 Tag relation / 免疫机制**：约束只有"apply 时条件门禁（且默认不求值）+ 槽位优先级抢占"。裁决1 的关系表是**净新增设计**，其执行时机是真决策点。
3. **实例存储是 per-component 双账本**：槽位数组 + 实例索引两套记账靠每帧刷新收敛（结构性隐患）；且 BuffComponent 连索引都没有，查询靠 Cast 走 State 主链。

---

## 决策点

### D3-1 状态实例存储位置（两级单位架构的落点）
- **A 推荐：中央注册表**——世界子系统内 per-unit 句柄桶（`FUnitHandle → 实例存储`），军官与小兵同一实现；单一代码路径服务两级单位（Mass 小兵无组件也能挂状态）。
- **B TCS 现状式（per-carrier）**：军官=组件内槽位、小兵=Mass fragment 内存储，共享操作引擎代码。存储贴近实体、缓存友好，但**两套载体必然发散**（核验：TCS 的 BuffComponent 缺索引、双账本收敛靠每帧刷新，就是这种发散的实证）。
- **风险与代价**：A 的中央 store 要防 god-object——只存数据与索引，逻辑放操作引擎（复用 TCS"引擎函数 + 数据容器"的分工）；下标稳定性交给 D0-2 句柄。

### D3-2 定义就绪语义（高风险三连之一在 M3 的落点）
- **A TCS 现状**：一次性 `bIsRuntimeReady` + `OnRuntimeReady` 广播，**部分加载失败不阻止 ready**（核验：02 报告 147 行），无中间态、无重验证。
- **B 推荐：显式状态机**——`Unloaded → Loading → Ready / Failed(带失败清单)`；Failed 可重试、可增量补载；Ready 广播至多一次但**必须以全量成功为前提**；编辑器期提供重验证入口。修高风险三连之"ready 不等预加载"。

### D3-3 关系表（裁决1）的执行时机
- **A 仅 apply 时检查**（TCS 条件门禁模式，条件永不重评——核验：全插件唯一求值点在 TryApplyState，且 `bCheckWhenApplying=false` 的条目永不求值）。缺陷：`Requires` 的前置状态过期后，依赖它的状态成为孤儿。
- **B 推荐：apply 拒绝 + 移除级联重评**——apply 时校验 Blocks/Requires/Priority 拒绝或排队；**任何状态移除后，对依赖它的存活状态做一次事件驱动的重评**（脏标记，非每帧）。补齐 TCS 缺失的"约束失效传播"。
- **C 持续重评**：每帧/每变更全量检查。表达力最强但成本与语义噪音大，YAGNI。
- 继承：槽位竞争（PriorityOnly 抢占 + PreemptionPolicy + 同优先级策略，TCS 06 已验证）并入裁决1 的 Priority 语义，不再独立造机制。

### D3-4 堆叠/刷新/共存的数据化
- **A TCS 现状**：维度是 Def 数据字段（MaxStackCount、DurationPolicy、ExpirationPolicy，已验证好用）；共存/替换靠 Merger 策略类（4 个实现：NoMerge/UseNewest/UseOldest/StackByInstigator）；边界行为硬编码（叠数=0 移除等）。
- **B 推荐：维度保持数据字段 + 共存策略数据化**——一个 `FStackCoexistPolicy` 数据行（规则枚举 + 分组键：按来源/按施加者/全局），枚举组合覆盖 TCS 四实现；边界行为留引擎代码。**删除 Merger 策略类**（四个都是单一行为枚举，正是"该数据化的不该做成类"的判据）；未来真出现表达不了的策略再加类型。
- 数据字段集（继承 TCS 已验证的）：MaxStackCount、DurationPolicy（None/RefreshRemainingToTotal）、ExpirationPolicy（ClearEntire/RemoveSingleStack/RemoveSingleStackAndRefresh）。

### D3-5 buff 行为载体（与 M4 强关联，先定方向）
- **A TCS 现状：每 buff 实例内嵌一棵 StateTree**（核验：06:11、08:285，经 STSchema 注入上下文）。成熟工具、策划熟悉；但**每实例一棵 StateTree 的内存与调度成本**在千人战场不可接受，且与"StateTree 只做决策/编排"的定位重叠模糊。
- **B 推荐：buff 行为 = M4 原语链（FEffectStep）**——实例是轻量 USTRUCT（句柄+池），行为是定义侧的数据链，由集中泵驱动；周期信号（TCS 的 STEvaluator_BuffPeriod）变为泵的周期事件。
- **C 折中**：轻 buff 走 B，"行为复杂的大 buff"允许挂 StateTree 编排（符合裁决定位：StateTree 做编排）。可作为 B 的后续扩展项，**不进第一版**。
- 明确：单位级决策（放什么 buff、何时放）仍归 StateTree（裁决定位不变）；本决策点只裁"buff 被挂上之后的自身行为"。

### D3-6 到期与计时驱动
- **A TCS 现状**：BuffComponent::TickComponent 逐帧递减（仅 Active/HangUp 递减、Pause 冻结、O(追踪数)）；变速零处理；通用状态无独立计时。
- **B 推荐：集中到期队列**——M0 时钟泵驱动一个按到期时间排序的最小堆（或时间轮），到期才出队处理；帧成本 = 到期项数而非存活状态数；ScaledDt/暂停/变速由泵统一裁决（D0-5 落点）。千人规模下 O(存活) 逐帧扫描不可接受。

### D3-7 生命周期扩展点覆盖
- **A TCS 现状**：apply/remove 事件齐全（含 Expired/Removed/Cancelled 原因）；refresh/expire 只有数据枚举无代码钩子；阶段转移矩阵不可插拔；存在死事件（OnStateParameterChanged 零监听、ResumeState 零调用方）。
- **B 推荐：三件套统一覆盖**——① 全生命周期阶段迁移发总线事件（apply/refresh/stack 变更/expire/remove+原因），修复覆盖缺口；② 常用行为保持数据枚举（D3-4 字段集）；③ 复杂行为扩展走 **Fragment 机制**（承接被冻结的 `add-state-fragment-mechanism` 提案思路，按三问标尺落位：机制原生/配置内聚/本体论不进核心）。死事件不迁移。
- 附加约束：阶段转移合法性矩阵进引擎代码但**以数据表声明**（阶段×允许迁移），可查不可改行为。

---

## 已定裁决落地清单（不重议）

| 裁决 | 落点 |
|---|---|
| 裁决1 数据形状 `FUnitStatusRule{Status,Blocks,Requires,Priority}` | D3-3 的数据基础 |
| 实例=USTRUCT+句柄+池 | D3-1 的形态前提 |
| StateTree 只做决策/编排 | D3-5 的边界依据 |
| 三问标尺（MEM-20260819-07） | D3-7 的 Fragment 落位 |

## 与其他模块的依赖标注

- D3-1 依赖 D0-2（句柄）；D3-6 依赖 D0-5（时钟泵）；D3-5 依赖 M4 原语集拍板（可先定方向、参数后补）。
- D3-2 同时是 M1 定义注册表的就绪语义（M1/M3 共用，按此拍板）。

| D3-10 | **Def 类层级终定**：StateDef 抽象（词表/FragmentSet/通用默认参数含 LevelBase/MaxLevel）编辑器隐藏；BuffDef 持 Duration/Period/堆叠五轴/关系表（+Cancels）；SkillDef 持施法/冷却/Cost/关系字段（语义映射）；SkillDef 无堆叠 | 已拍板 2026-09-02 |
| D3-11 | **Level 终定**：StateDef 默认参数含 LevelBase/MaxLevel；FStateInstance.Level；Skill 侧 Entry+FCastRun 双侧携带（快照）；增长/修改策略=项目业务侧（引擎零内置） | 已拍板 2026-09-02 |

| D3-12 | **StateParamSnapshot 终定**（用户参照 TCS/GAS 提出）：FStateInstance.ParamSnapshot——apply 时快照全部生效参数（强度/Duration/Period/Level），实例生命周期内读快照；持续修正器通道保持 live（Source 级联）；实时读取 opt-in；ECastInstancing 归 Skill（State 重复处理=Stack 政策） | 已拍板 2026-09-02 |

| D3-12 v2 | **双维度快照终定**（用户修正）：①参数 Mode 列——参数表逐行 {Snapshot(默认)/Live}（Live 仅 Skill 侧有意义；Buff 参数 immutable，实时缩放走属性维度）；②属性读取快照——**属性读默认 Live**、声明式 AttrCapture 列表捕获后读快照（见 D5-12 v2/09 文档） | 已拍板 2026-09-02 |

| D3-13 | Duration 拆分终定：EDurationPolicy{Finite,Infinite}+DurationTime；Period 独立可配+PeriodRefresh{Keep/Reset/Immediate} | 已拍板 2026-09-02 |
| D3-14 | Overflow 升级替换不内建终定：事件组合表达；状态级重定向留未来 | 已拍板 2026-09-02 |
| D3-15 | 生命周期操作终定：ExtendDuration/SetRemaining 进 API；MaxStacks 无运行时修改（成长=变体+重定向） | 已拍板 2026-09-02 |
| D3-16 | 关系表/槽位表组织终定：并存、DataTable 行（单/类 StateDef）、解析栈（全局→模式→角色）、引用走 Tag、槽位不用 StateTree | 已拍板 2026-09-02 |
| D3-18 | **实例-定义引用规范终定**：FStateInstance 权威=DefId；GetDef() 解析缓存（const+版本校验，重定向/热重载自动失效）；运行期零回查；**回归修复：D3-13/14/15/16/17 折入 v2 正文**（此前 v2 重写漏折） | 已拍板 2026-09-02 |
| D3-17 | 堆叠轴/分组命名终定：轴名 StackDurationPolicy、值 {None, RefreshRemainingToTotal}（TCS 原样；RefreshRemaining/Add 撤出 v1——无业务场景，需求出现再加）；分组枚举维持 {None, PerSource, PerInstigator, PerTag} | 已拍板 2026-09-02 |

| D3-19 | **修正器模板与引用行终定**（旧 TCS AttrModDef/SkillModDef 收编，R3 计划审阅轮 4）：独立模板资产**沿用旧名** `UTcsAttrModDef`/`UTcsSkillModDef`（纯模板=默认值+可选 FTcsParamScalar.ParamKey 声明——用户拍板保留资产形态）；`FStateDefBase.ModifierRows` = 模板引用行 `{TemplateId}`（**纯引用不内联、不做双形态**，内联留未来让渡点；旧"双轨字段手工镜像"缺陷无载体）；**覆写唯一通道=参数传值**（物化点从施加方 ParamSnapshot 解析，miss 兜底模板 Literal；**引用处零字段覆写**——PatchField 禁令同类理由，要变体=变体模板/参数化）；apply 物化 Source=状态句柄、注销级联；物化执行器注册制分派（D4-14 模式：属性行住 TcsState/TcsAttribute，技能参数行 TcsSkill 自注册——TcsState 不反向依赖 TcsSkill）；宿主命令式共用同一物化器 `MaterializeModifiers(TemplateIds, ParamContext, Source)`；物化期读模板=定义→实例边界合法，物化后零回查不变（D2-9/D3-18） | 已拍板 2026-09-02 |

| D3-7 v2 | **Fragment 形态与实例关系修订（策略形态一元化，2026-09-02 审阅轮 5 后，用户提案）**：①全部策略统一 **EditInlineNew Instanced UObject**（配置即成员、Resolve/Pass/决策方法 const 纯、策略实例归 Def 资产持有；单字段暴露——类/载荷配对 bug 类消失；此前"两字段是结构性要求"论断收回）；②**FStateInstance 去 FragmentSet 字段**——实例零策略/载荷/订阅句柄（池化 struct 纯度+操作复制地基；策略调用点在引擎流程，Def 解析注入）；③行为 Fragment 订阅契约 = **兴趣 Tag 数据列表 + 单一泛化回调** `OnStateEvent(FGameplayTag, FInstancedStruct, Ctx)`（订阅/退订复用 D4-1 Source 级联配对机制——作者零订阅管理代码）；决策 Fragment 按决策种类各自抽象接口不变（五轴/IValueDomainPolicy/ICostPolicy）；④per-instance 扩展状态 = 实例 Variables 通道按需（今天零消费者，YAGNI）；用户论据："Fragment 属于外接内容，真要废弃也方便，不会影响 TcsState 内部执行流程"（契约面窄、内核不依赖 → 形态可试错） | 已拍板 2026-09-02 |
| D3-7 v3 | **策略载体切换 FInstancedStruct（2026-09-10 可行性调研轮，用户拍板）**：D3-7 v2 的一元化决策不变、仅载体修订——策略 = **USTRUCT 反射基类 + C++ 虚函数分派（StateTree 同构）**，Def 资产以 `TInstancedStruct<Base>` 成员持有（策略实例归 Def 资产持有、实例零策略、行为 Fragment 泛化订阅契约均不变）。依据：UE5.8 StructUtils 已入 CoreUObject（编辑器开箱 picker/内嵌编辑/TArray details）+ StateTree/Mass/SmartObjects shipped 先例 + 与 D4-14"数据+分派"心智收敛 + 网络开箱（Iris FInstancedStructNetSerializer）+ 旧 TCS"TSubclassOf+Config 对"双形态不回归。**代价明确接受**：BP 策略扩展通道放弃（R0 §9"蓝图理论可行不承诺"承责）、编辑器校验钩子挪至 Def 资产 IsDataValid。时机窗口 = 策略 UObject 零实现（plan2 Task 2 未动工），零代码返工 | 已拍板 2026-09-10 |
