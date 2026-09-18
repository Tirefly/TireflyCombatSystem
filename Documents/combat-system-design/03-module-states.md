# 03-module-states.md — TcsState 状态层设计（v2 定稿重写）

- 日期：2026-09-02
- 状态：**v2 定稿**——全部增补（D3-10~12 等）已折入正文；修订记录见文末
- 职责一句话：**管理"施加态"的全生命周期——应用/刷新/叠层/到期/移除，执行关系表约束；实例是轻量 struct，行为全部外置。**

## 1. 模块边界

- 消费者：M4（生命周期事件驱动行为链）、M5（技能施加状态）、M6（Entity 适配）、M8（编辑器）。
- 依赖：TcsCore（句柄/池/总线/时钟/到期堆/Source）、TcsNotation（参数行 ValueConvention 与快照构建转换——D5-18 v2）、TcsAttribute（修正器账本）、**TcsEffect（FEffectStep 基类型+ExecuteChain——D4-14 注册制分派）**。
- **FStepApplyState 步骤类型+执行器住本模块（D4-14）**；Buff 行为=原语链（D3-5）经 TcsEffect 解释器执行，链的触发仍由触发行订阅 M3 生命周期事件挂接（订阅方向不变，M4→M3 事件流）。

## 2. Def 类层级（D3-10 终定）

```
FStateDefBase（抽象，编辑器隐藏）          ← 本模块定义
 ├─ FBuffDef : FStateDefBase              ← 本模块定义（施加态语义）
 └─ FSkillDef : FStateDefBase             ← 定义于 TcsSkill 模块（施法语义，见 05 文档）
```

- **FStateDefBase（抽象）**：词表条目（StatusTag）、FragmentSet 配置、**通用默认参数（含 LevelBase/MaxLevel；参数行 {Key, Base: FTcsParamValue, Mode: Snapshot(默认)|Live, ValueConvention}——D5-18 v2 镜像 SkillDef 行形状、**实现名 `FTcsNumericParamRow` / `ETcsParamMode`（2026-09-14 命名批，FSkillDef 继承白拿）**，快照构建写入点求值转换；Base 载体 PV 系列 2026-09-11 换型 TInstancedStruct<FTcsParamValueSource>，StateLevelArray/Map 源落点——"buff 每级+10HP"直接配置）**、**描述视图配置 `Descriptions: TArray<FTcsDescriptionEntry{DescriptionId, TextKey, Views[SlotName, View]}>`（D5-17 v3——视图策略化：多描述入口 + `TInstancedStruct<FTcsParamView>` 槽位，buff tooltip 刚需；StringTable 只含槽名，配置空间与编辑器预警见 11 文档）**、生命周期事件词汇、**修正器引用行 ModifierRows（D3-19：UTcsAttrModDef/UTcsSkillModDef 模板引用，apply/激活物化）**。不含任何时值/堆叠/施法配置。
- **FBuffDef（施加态语义）**：Duration/Period、堆叠五轴、关系表字段。
- **FSkillDef（施法语义，详见 05）**：施法时段表、冷却、Cost、主链、关系字段（同形状语义映射）。**无堆叠、无时值**。
- 编辑器只暴露 FBuffDef / FSkillDef（用户 TCS 决策复用：基类不该有派生类的配置）。
- **Def 资产（2026-09-14 命名批）**：`UTcsStateDef`（基类——对应 `FStateDefBase` 家族，持 `DefId` + `IsDataValid` 校验挂点）→ `UTcsBuffDef`（本模块）/ `UTcsSkillDef`（TcsSkill）；DataTable 双轨行 `FTcsBuffDefTableRow`。**范围澄清**：DefLibrary 管辖的只是 `FStateDefBase` 家族；修正器模板（`UTcsAttrModDef`/`UTcsSkillModDef`）与属性词表（AttributeDef）是并列另一族，**不共用此资产基类**（故基类名不用 `UTcsDefAssetBase`——会暗示覆盖全部 Def）。**基类统一 `UPrimaryDataAsset`（2026-09-17 定）**：`UTcsStateDef` 家族 / `UTcsAttrModDef` / `UTcsSkillModDef` 同此——Def 引用语义本是"FName Id + DefLibrary 解析"，主资产身份让解析与按类型发现/加载归引擎（族内混用两套基类会让 DefLibrary 发现逻辑分叉）；`PrimaryAssetTypes` 注册属 M6 DefLibrary 轮。**双轨制语义（2026-09-17 用户口径，全 Def 族适用）**：**表 = 编辑期载体、资产 = 运行期载体**——DataTable（`FTcsBuffDefTableRow` / `FTcsAttributeDefTableRow` 等）只服务策划批量编辑与编辑器即时响应，**不作为运行期加载源**；运行期一律按 `DefId` 解析资产（资产制扩展性更好，加 Fragment 等只动资产与载荷）。两轨一致性由 08 §5 的编辑器同步器维护。

## 3. 类型词汇（对外）

### 3.1 实例与存储（D3-1/D3-12）
- `FStateInstance`（USTRUCT，池化）：`DefId / Handle / Source / Stacks / Level / Phase / ParamSnapshot`（**快照类型 `FTcsParamSnapshot`，2026-09-14 命名批**；**v3：FragmentSet 移除**——Fragment 配置归 Def，实例零策略/载荷/订阅句柄，D3-7 v2）。**实例-定义引用规范（D3-18）**：权威=DefId；热路径=`GetDef()` 解析缓存（const 指针+DefLibrary 版本校验；重定向/热重载自动失效）；**运行期零回查 Def**（数值类基础信息在快照构建时消化——发现"执行中查 Def"即设计漏洞）。无 UObject、无每实例 StateTree（D3-5）。
- **`FStateInstance.ParamSnapshot`（D3-12）**：apply 时把全部生效参数一次性求值冻结（强度/Duration/Period/Level——施加上下文覆盖值优先，Def 默认兜底）；实例生命周期内读快照；**修改 = 按刷新政策重新施加（快照重建，新 payload 覆盖）**。
- **live 通道分界**：实例施加的持续修正器（Source=状态句柄，走 M2 账本）实时生效、随驱散/强化级联——与快照通道正交（灼烧幅度=快照；灼烧附带的-20% 火抗=live）。修正器来自 Def 的 ModifierRows 模板引用，apply 物化时可变值从 ParamSnapshot 解析（D3-19）——账本内纯规范值。
- `UCombatStateRegistry`（命名与 06 文档统一待办，见 06 `UCombatWorldRegistrySubsystem`）：中央注册表 per-unit 桶（`FCombatEntityHandle → 桶`）；军官组件与 Mass 桶只是访问适配器（桶指针缓存+代际校验）；桶只存数据与索引，操作全在引擎函数 `FStateOps`。

### 3.2 堆叠与刷新（D3-4，FBuffDef 字段）
`FStateStackPolicy` 五轴（每轴枚举**值 0=默认/None，值 1=Custom**——Custom 选中→编辑器暴露 `TInstancedStruct` 决策策略（struct 单字段，D3-7 v3））：
- `GroupBy`：None / PerSource / PerInstigator / PerTag(自定义分组 Tag)
- `MaxStacks`：int（组内最大层数，≤0=无限；2026-09-02 由 Capacity 改回——用户拍板回归 Stack 词汇）
- `Overflow`：RejectNew / ReplaceOldest / ReplaceNewest（**升级替换不内建**——StackChanged 满仓触发行→Apply 强力→Remove 原，D3-14；状态级重定向留未来）
- `ValueStack`：KeepMax / AddValues / PerStackValue
- `StackDurationPolicy`：None / RefreshRemainingToTotal（**D3-17 终定轴名与值域**——TCS 原样；RefreshRemaining/Add 撤出 v1，无业务场景，需求出现再加）
组合覆盖 TCS 四 Merger 全部行为（NoMerge=None+∞；UseNewest=1+ReplaceOldest；UseOldest=1+RejectNew；StackByInstigator=PerInstigator+层叠）；Merger 策略类**删除**。

### 3.3 Duration 与 Period（FBuffDef 字段，2026-09-02 细化）
- **Duration 拆分（D3-13）**：`EDurationPolicy{Finite, Infinite}` + `DurationTime`（Finite 时有效，FTcsParamValue——原"字面量|Param"，PV 系列 2026-09-11 换型）；快照于 apply/refresh（D3-12）；Infinite=无到期条目永不过期（≤0 魔法值废除）；暂停/变速由泵统一。
- **Period**：`0/未设=无周期（默认）`；机制 = M3 注册**重复到期条目**到堆 → 每周期发 `Combat.State.Periodic` 事件（payload 带 StateHandle + **当前 Stacks + Level**）；默认**等首个周期**，"施加即生效"写进 apply 响应链；**`PeriodRefresh{Keep(默认)/Reset(重置周期计时)/Immediate(立即执行一次并重置)}`**——堆叠刷新对周期计时器的影响（D3-13）。

### 3.4 关系表（裁决 1 + D3-3 + 形状扩展）
- 字段形状（裁决 1 增补）：`{ Status, Blocks, Requires, Priority, Cancels }`——FBuffDef 字段（约束施加态共存）；**FSkillDef 同形状字段**（语义映射：Block=禁止激活、Require=激活前提、Priority=顶替优先级、Cancels=激活时取消指定运行/状态——姿态切换），共享通用关系检查器（主体类型化：状态实例/施法运行）。
- 执行时机：apply 时校验（Blocks 拒绝 / Requires 未满足拒绝或排队）；**任何状态移除后对依赖它的存活状态做事件驱动重评**（脏标记，批量移除合并一次）。
- 槽位竞争（PriorityOnly 抢占 + PreemptionPolicy + 同优先级策略，TCS 06:117-119 已验证）并入 Priority 语义。
- **组织与解析栈（D3-16）**：关系表/槽位竞争表**并存**（槽位=施法运行/动作状态，关系=Buff 施加态，互相影响）；统一 DataTable/DataAsset 行（行=单个或**一类** StateDef——StatusTag 匹配）；**解析栈**：全局默认表（插件）→ 游戏模式表 → 角色覆盖表（Boss）——让渡点模式应用；关系引用走 Tag（StateTag/SkillTag 子域）；**槽位竞争不用 StateTree**（数据行+引擎抢占规则）。

### 3.5 Level（D3-11 终定）
- StateDef 通用默认参数含 `LevelBase / MaxLevel`；`FStateInstance.Level`（实例携带）。
- **增长/修改策略完全交给项目业务侧**（引擎零内置——FollowStacks 枚举撤回）；**等级→强度映射 = 参数域数据（PV-4 2026-09-11 修订 D3-11；评判轮收束为四源）**：`FTcsParamSource_StateLevelArray/Map`（状态自身等级）与 `FTcsParamSource_InstigatorLevelArray/Map`（经 `ITcsEntityLevelProvider::GetEntityLevel`——**接口定义于本模块**、宿主实现；TargetLevel 系暂不提供——用户拍板）源住本模块（Def 数据直配等级表），宿主升级事务照旧。
- 对比说明：Skill 侧 Level 持久（Entry+CastRun 双侧，见 05）；Buff 强度=施加时快照、临时——本质差异非缺失。

### 3.6 StateParamSnapshot（D3-12 终定）
- **apply 那一刻**：全部生效参数一次性求值冻结进 `ParamSnapshot`（施加上下文覆盖值优先，Def 默认兜底；参数行 ValueConvention 转换在快照构建写入点——D5-18 v2，快照内永远规范值）。
- 实例生命周期内读快照；**修改 = 重新施加（快照重建）**；持续修正器通道保持 live。
- **快照条目形状（PV-3，2026-09-11；实现名 `FTcsParamSnapshotEntry`，2026-09-14 命名批）**：已解析规范值 + **源引用位**双字段——Debug 可见取值来源，Live 化（已承诺基建）零结构迁移。**注意禁用 `Resolved*` 命名**（动词纪律：Resolve 仅句柄/Id→对象）。
- **tooltip 索引口径（D5-17 v2/v3，2026-09-14/15）**：实例（buff/状态）tooltip 的 Series 视图（整表+当前档高亮）当前档索引取**该实例快照的 Level**，不是查询时单位等级——"运行中升级不追溯"的既定语义（D3-12），用现等级高亮会出现"显示 5 级、实际按 3 级生效"。技能面板侧口径不同（Entry 当前 EffectiveLevel，见 05）。
- 收益：客户端预演、可预测（施法/施加中途换装备不追溯）、回放输入完备。

### 3.7 就绪状态机（D3-2）
`Unloaded → Loading → Ready / Failed(失败清单)`；Ready 广播至多一次且**以全量成功为前提**；Failed 可重试/增量补载；编辑器重验证入口（修高风险三连之"ready 不等预加载"）。

## 4. 入口服务

- `ApplyState(目标, DefId, Source, Overrides?) -> EApplyResult`（Overrides = 施加方传入的参数覆盖，进快照）
- `ExtendDuration(handle, Δ) / SetRemaining(handle, T)`（D3-15：运行实例生命周期操作——"灼烧延长"，到期堆同步）；**MaxStacks 无运行时修改**（政策常量；成长=变体+重定向）
- `RemoveState / ExpireState(handle, Cause)`、`GetState(handle)`、`ForEachState(unit, 谓词)`
- `SetLevel / GetLevel`（Custom 增长模式的项目入口；FollowStacks 之类由项目实现）
- 关系检查器：`CheckRelations(主体, 动作) -> 结果`（通用，主体=状态实例/施法运行）

## 5. 关键流程与机制

- **Apply**：校验（词表/就绪态）→ 关系表检查 → 五轴共存决策（含 Custom 策略）→ 池分配 → **快照构建（D3-12）** → 行为 Fragment 订阅挂接（兴趣 Tag→总线订阅，Source=状态句柄，remove/expire 自动退订——D3-7 v2）→ 挂 Duration/Period 条目（到期堆）→ **修正器物化（D3-19）**：解析 ModifierRows 模板引用——属性行 ParamKey 从快照解析后挂 M2（Source=状态句柄），技能参数行经注册制分派到 TcsSkill 执行器挂 M5 → `OnStateApplied`（提交尾 flush，立即通道）。
- **Remove/Expire**：原因标记（Expired/Removed/Cancelled）→ M2 `RemoveBySource` → 周期条目惰性摘除 → 关系表级联重评（批量合并）→ `OnStateRemoved`。
- **Phase 转移矩阵**：阶段×允许迁移 = 数据表声明（可查不可改）；非法迁移 = ensure。
- **生命周期事件全集（D3-7）**：`OnStateApplied / OnStateRefreshed / OnStackChanged / OnStateExpired / OnStateRemoved(+原因)`——全阶段总线事件，核心词汇 FStruct。
- **Fragment 机制（v4——策略载体 D3-7 v3）**：全部策略统一 **FInstancedStruct 载体**——策略 = USTRUCT 反射基类 + C++ 虚函数分派（StateTree 同构；配置即成员，Resolve/Pass/决策方法 const 纯），Def 资产以 `TInstancedStruct<Base>` 成员持有策略（策略实例归 Def 资产持有；单字段暴露——类/载荷配对 bug 类消失）。**实例零策略**（FStateInstance 不持策略/载荷/订阅句柄——池化 struct 纯度与操作复制地基；策略调用点在引擎流程，Def 解析注入）。两种深度：**决策 Fragment**（按决策种类各自抽象接口：五轴共存决策/IValueDomainPolicy/ICostPolicy 等——签名强类型，流程调用点注入）；**行为 Fragment**（`FTcsStateBehaviorFragment`：兴趣 Tag 列表 + 单一泛化虚回调 `OnStateEvent(FGameplayTag, FInstancedStruct, Ctx)`——apply 订阅挂接/remove 自动退订，复用 D4-1 订阅配对+Source 级联机制；载荷回调内自取，Tag 匹配保证来源）。per-instance 扩展状态 = 实例 Variables 通道按需（今天零消费者；回合语义等未来 Fragment 需求出现时裁）。回合制等领域能力落位：回合本体论不进核心（= ITimeSource + 回合语义 Fragment）。BP 策略扩展通道放弃（R0"蓝图不承诺"承责）；编辑器校验由 Def 资产 IsDataValid 承担。
- **到期驱动（D3-6）**：集中最小堆（M0 泵），到期才出队；惰性删除+代际校验；帧成本=到期项数。

## 6. 依赖方向声明（铁律 1，D4-14 后更新）

**M3 的 Def 不持有行为链**——buff 行为由 M4 触发行订阅 M3 生命周期事件挂接（事件流 M4→M3 方向不变）。M3 只负责忠实广播生命周期。

**编译依赖（D4-14 注册制分派后）**：TcsState 依赖 TcsEffect（FEffectStep 基类型+ExecuteChain——ApplyState 步骤+执行器是领域步骤，住本模块并自注册；Buff 链在本模块发起、经注册表分派到各领域执行器）。**TcsEffect 不依赖 TcsState**——机制层对领域零感知；运行时对状态实例的查询经 ICombatEntityQuery 注入。

## 7. 网络姿态落点（NET-1/2）

- 权威侧跑全流程；**操作复制**：状态操作流（Apply{DefId,Source,ParamSnapshot}/Refresh/StackChange/Remove{Handle,Reason}）复制到客户端镜像桶重放——**ParamSnapshot 随操作流走 = 回放输入完备**；客户端到期推算仅作表现。
- 士兵层服务器权威、无本地预测；镜像接口位本期不实现。

## 8. 非目标

不做行为链解释器（M4）；不做回合本体论；不做每实例 StateTree；不做值复制；不做状态机编辑器（M8）；**不做 Instancing**（运行实例语义归 Skill/05）；不做 Level 增长策略（项目侧）。

## 9. 依据

- 拍板：D3-1~D3-12（2026-09-02 三轮问答框；用户贡献：中央注册表性能模型、正交轴+Custom 逃逸位、MaxStacks 改名、Def 类层级、Level 定位、Mass/Instancing 归属指正、StateParamSnapshot 参照引入）。
- 证据：TCS 06/07/08 核验（双账本、就绪缺陷、Merger 四实现、逐帧递减、死事件、DefManager 策略）；AbilityKit 取证（中央管理器+薄适配器、BuffRuntime 池化）；裁决 1/2a；MEM-20260819-07 三问标尺；MEM-20260902-04 缺失 vs 有意。

## 10. 修订记录

- v1（2026-09-02）：初版决策折入。
- v2（2026-09-02）：折入 D3-10（Def 类层级）、D3-11（Level 终定）、D3-12（StateParamSnapshot）、关系表 +Cancels、MaxStacks 改名、Duration/Period 细化；**移除 ECastInstancing（归 05/Skill）**。
- v2 增补 3（2026-09-02，R3 计划审阅轮 4）：折入 D3-19 修正器模板与引用行（FStateDefBase.ModifierRows、apply 物化、覆写=参数传值、物化执行器注册制分派）。
- v2 增补 4（2026-09-02，R3 计划审阅轮 5）：折入 D5-18 v2——参数行 {Key, Base, ValueConvention} 镜像 SkillDef 形状、DescriptionTextKey 补录（buff tooltip）、依赖加 TcsNotation（快照构建写入点转换；§3.6 同步）。
- v2 增补 5（2026-09-02，策略形态一元化轮）：折入 D3-7 v2——Fragment 形态统一 EditInlineNew Instanced UObject（配置即成员）；FStateInstance 去 FragmentSet 字段（实例零策略/载荷/订阅句柄）；行为 Fragment 订阅契约（兴趣 Tag 数据列表 + 单一泛化回调 + Source 级联退订）；决策 Fragment 按决策种类各自抽象接口不变；per-instance 扩展状态 = Variables 通道按需（零消费者不预建）。
- v2 增补 6（2026-09-10，D3-7 v3 载体修订）：Fragment 载体 EditInlineNew Instanced UObject → **USTRUCT 反射基类 + C++ 虚函数分派 + `TInstancedStruct<Base>` 持有**（可行性调研后用户拍板——引擎事实与 StateTree 先例见 README 决策日志同日条目）；行为 Fragment UTcs→FTcs；BP 策略扩展通道放弃；编辑器校验挪至 Def 资产 IsDataValid；实例零策略/泛化订阅契约不变。
- v2 增补 7（2026-09-11，PV 系列）：参数行 Base 换型 **FTcsParamValue{TInstancedStruct<FTcsParamValueSource>}**（D2-12 FTcsParamScalar 被取代）；**等级源住本模块**（D3-11 修订——等级表 def 数据进引擎）：StateLevelArray/Map + InstigatorLevelArray/Map（`ITcsEntityLevelProvider::GetEntityLevel` **定义于本模块**、宿主实现；TargetLevel 系暂不提供——评判轮用户拍板）；快照条目留源引用位（Debug+Live 预留）；DurationTime 等时值字段同批换型。
- v2 增补 8（2026-09-14，参数折叠与展示轮）：折入 **D5-17 v2**（§2 描述绑定词法族、§3.6 tooltip 索引口径 = 实例快照 Level）、**D5-18 v3**（参数行 Mode 列与约定白名单同参 11 文档）、**命名批**（§2 Def 资产层级 `UTcsStateDef`/`UTcsBuffDef` + 范围澄清、`FTcsNumericParamRow`/`ETcsParamMode`、§3.1 `FTcsParamSnapshot`、§3.6 `FTcsParamSnapshotEntry`）；**PV-10**（等级源后续实现可枚举能力——`FTcsParamEnumerableSource` 基类住 Core，索引解析唯一真相在源，随本模块等级源同批落地）。
- v2 增补 9（2026-09-15，D5-17 v3 描述视图策略化）：§2 DescriptionTextKey 单字段 → **Descriptions 配置组**（`FTcsDescriptionEntry` 多描述入口 + `TInstancedStruct<FTcsParamView>` 视图槽位——类型住 TcsNotation，本模块已有 Notation 边零新边）；§3.6 tooltip 索引口径改 Series 视图措辞（口径本身不变：实例快照 Level）。

## 11. 验收钩子

竖切剧本第 6 项（**新增纯数据链资产零 C++**——M9 收尾轮后 buff 验证改测试 Source/链数据，M3 状态桶不在 R3）由 M2 兑现；关系表级联重评、到期堆、Custom 策略、快照重建（refresh）在 M3 轮人工检查路径。

## 增补：时值拆分/Overflow/生命周期操作/关系表组织（2026-09-02，v5 增补）

- **Duration 拆分（用户提案采纳）**：`EDurationPolicy{Finite, Infinite}` + `DurationTime`（Finite 时有效，FTcsParamValue——原"字面量|Param"，PV 系列 2026-09-11 换型）——替换"≤0=永久"魔法值；None 场景收敛：瞬发走链直接表达、永久=Infinite（GAS 三值习惯不适用——伤害系统已独立成模块）。
- **Period 联动**：Period 独立可配（Finite+Period=标准 DoT、Infinite+Period=光环、四组合全合法；Policy 联动仅为编辑器显示逻辑）；**`PeriodRefresh{Keep(默认)/Reset(重置周期计时)/Immediate(立即执行一次并重置)}`**——堆叠刷新对周期计时器的影响（用户提案采纳）。
- **Overflow 升级替换：引擎不内建**（用户确认）：事件组合三步表达（`StackChanged 满仓` 触发行 → `ApplyState{更强}` + `RemoveState{原}`）；状态级重定向（DefId 存在期解析选路）留未来扩展；引擎保证 StackChanged 时序与同帧合并。
- **运行实例生命周期操作**：`FStateOps::ExtendDuration(handle, Δ) / SetRemaining(handle, T)` 进 API（"灼烧延长"类）；**MaxStacks 无运行时修改**（政策常量；"层数上限+2"成长 = 变体 BuffDef + 技能/状态级重定向）。
- **关系表/槽位竞争表组织（用户提案采纳）**：两者**并存**（槽位竞争管施法运行/动作状态，关系表管 Buff 施加态，互相影响）；组织 = **统一 DataTable/DataAsset 行**（行 = 单个 StateDef **或一类**——StatusTag 匹配）；**解析栈**：全局默认表（插件）→ 游戏模式表 → 角色覆盖表（Boss）——让渡点模式再次应用；关系字段引用走 Tag（StateTag/SkillTag 子域）；**槽位竞争不用 StateTree**（数据行 + 引擎抢占规则；StateTree 只做单位级决策）。
- **堆叠轴命名**：~~DurationMerge~~ 撤回——值名回 TCS（{None, RefreshRemainingToTotal}），**轴名 StackDurationPolicy 已终定（D3-17，已回写 §3.2）**。
