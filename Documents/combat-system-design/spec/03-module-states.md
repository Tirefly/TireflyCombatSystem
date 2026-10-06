# [SPEC-02-states](../spec/03-module-states.md) — TcsState 状态层设计（v2 定稿重写）

- **文档 ID**：`SPEC-02-states`
- **类型**：SPEC / 模块规格
- **状态**：ACTIVE（**R5 核心已落地，2026-10-05 收束**——Task 0~7 全部完成，落地范围与切分见 §12；**本轮不落**的条款逐条带归属于 §12.2，不落不等于未定）
- **权威范围**：TcsState（M3）Def 层级、实例存储、五轴堆叠、关系表、快照；理由住 LOG-01-states
- **最后更新**：2026-10-05（R5 收束：Task 7 落地口径新增 §12.11 + R5 落地总账 §12.12；§3.3 事件词订正为新根名；§11 验收钩子改为"已验 / 未验"对账表）

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
- **Def 资产（2026-09-14 命名批；身份 2026-09-22 tag 化；**资产类名 2026-10-04 修**）**：`UTcsStateDef`（基类——对应 `FStateDefBase` 家族，持 `DefTag: FGameplayTag` + `IsDataValid` 校验挂点）→ ~~`UTcsBuffDef`~~ **`UTcsBuffDefAsset`**（本模块；UHT 按去前缀引擎名判重——`UTcsBuffDef` 与数据 struct `FTcsBuffDef` 同名即编译失败，见 §12.3）/ `UTcsSkillDef`（TcsSkill）；DataTable 双轨行 `FTcsBuffDefTableRow`。**范围澄清**：DefLibrary 管辖的只是 `FStateDefBase` 家族；修正器模板（`UTcsAttrModDef`/`UTcsSkillModDef`）与属性词表（AttributeDef）是并列另一族，**不共用此资产基类**（故基类名不用 `UTcsDefAssetBase`——会暗示覆盖全部 Def）。**基类统一 `UPrimaryDataAsset`（2026-09-17 定）**：`UTcsStateDef` 家族 / `UTcsAttrModDef` / `UTcsSkillModDef` 同此——Def 引用语义本是"**身份 tag + DefLibrary 解析**"，主资产身份让解析与按类型发现/加载归引擎（族内混用两套基类会让 DefLibrary 发现逻辑分叉）；`PrimaryAssetTypes` 注册属 M6 DefLibrary 轮。**双轨制语义（2026-09-17 用户口径，全 Def 族适用）**：**表 = 编辑期载体、资产 = 运行期载体**——DataTable（`FTcsBuffDefTableRow` / `FTcsAttributeDefTableRow` 等）只服务策划批量编辑与编辑器即时响应，**不作为运行期加载源**；运行期一律按 `DefTag` 解析资产（资产制扩展性更好，加 Fragment 等只动资产与载荷）。两轨一致性由 08 §5 的编辑器同步器维护。**身份字段与主资产身份（2026-09-22 改造）**：`DefTag` 取代原 `FName DefId`；主资产身份 = `[PrimaryAssetType, DefTag.GetTagName()]`（`FPrimaryAssetId` 的 name 位是引擎硬约束的 `FName`，tag 经 `GetTagName()` 转换）。表行侧 **`RowName` 降为编辑期定位**、行内 `DefTag` 才是内容身份，二者 **MUST NOT 被要求同名**（分工而非双真相；口径见 02 §2.1）。

## 3. 类型词汇（对外）

### 3.1 实例与存储（D3-1/D3-12）
- `FStateInstance`（USTRUCT，池化）：`DefTag / Handle / Source / Stacks / Level / Phase / ParamSnapshot`（**快照类型 `FTcsParamSnapshot`，2026-09-14 命名批**；**v3：FragmentSet 移除**——Fragment 配置归 Def，实例零策略/载荷/订阅句柄，D3-7 v2）。**实例-定义引用规范（D3-18；身份 2026-09-22 tag 化）**：权威=`DefTag`（`FGameplayTag`）；热路径=`GetDef()` 解析缓存（const 指针+DefLibrary 版本校验；重定向/热重载自动失效）；**运行期零回查 Def**（数值类基础信息在快照构建时消化——发现"执行中查 Def"即设计漏洞）。无 UObject、无每实例 StateTree（D3-5）。
- **`FStateInstance.ParamSnapshot`（D3-12）**：apply 时把全部生效参数一次性求值冻结（强度/Duration/Period/Level——施加上下文覆盖值优先，Def 默认兜底）；实例生命周期内读快照；**修改 = 按刷新政策重新施加（快照重建，新 payload 覆盖）**。
- **live 通道分界**：实例施加的持续修正器（Source=状态句柄，走 M2 账本）实时生效、随驱散/强化级联——与快照通道正交（灼烧幅度=快照；灼烧附带的-20% 火抗=live）。修正器来自 Def 的 ModifierRows 模板引用，apply 物化时可变值从 ParamSnapshot 解析（D3-19）——账本内纯规范值。
- ~~`UCombatStateRegistry`~~ → **`UTcsStateSubsystem : UWorldSubsystem`**（2026-10-04 R5 收窄轮定，见 §12.3；原命名与 06 文档统一待办就此结案）：中央注册表 per-unit 桶（`FCombatEntityHandle → 桶`）；军官组件与 Mass 桶只是访问适配器（桶指针缓存+代际校验）；桶只存数据与索引，操作全在引擎函数 `FStateOps`。

### 3.2 堆叠与刷新（D3-4，FBuffDef 字段）
`FStateStackPolicy` 五轴（每轴枚举**值 0 = 默认**；需要逃逸的轴以**高位置位**作 Custom——Custom 选中→编辑器暴露**裸 `FInstancedStruct`** 决策策略载荷（手写 `BaseStruct` 元数据收窄选择器，D3-7 v3））：
- `GroupBy`：None / PerSource / PerInstigator / Custom（~~PerTag(自定义分组 Tag)~~ **2026-10-05 裁撤**：组键基座含 `DefTag`，而分组词取自定义自身 ⇒ 同一 `DefTag` 内恒定 ⇒ 该档与 `None` 完全同义；其唯一有意义的读法"不同定义共享一个组"要求**组内多条实例**，属另一套层模型 ⇒ 台账 `STAT-7`）
- `MaxStacks`：int（组内最大层数，≤0=无限；2026-09-02 由 Capacity 改回——用户拍板回归 Stack 词汇）
- `Overflow`：RejectNew / ReplaceExisting（~~ReplaceOldest / ReplaceNewest~~ **2026-10-05 裁撤**："一组一条实例"下两者同义；**升级替换不内建**——StackChanged 满仓触发行→Apply 强力→Remove 原，D3-14；状态级重定向留未来）
- `ValueStack`：KeepMax / AddValues / PerStackValue（`PerStackValue` **2026-10-05 标记为本轮零行为**：合规实现需"层数读口 + 按层参数源"，否则退化成 §3.5 明文撤回的 FollowStacks ⇒ 台账 `STAT-6`）
- `StackDurationPolicy`：None / RefreshRemainingToTotal（**D3-17 终定轴名与值域**——TCS 原样；RefreshRemaining/Add 撤出 v1，无业务场景，需求出现再加）
组合覆盖 TCS 四 Merger 行为（NoMerge ≈ None+∞（**近似**：得到的是"一条实例、层数累加、默认轴下数值与时长都不变"；"每次施加独立成实例"改用 Custom 决策 Fragment 表达）；UseNewest=1+ReplaceExisting；UseOldest=1+RejectNew；StackByInstigator=PerInstigator+层叠）；Merger 策略类**删除**。

**层模型（2026-10-05 定，本节与 §5 的读法前提）**：**层数记在一条实例内**（`FTcsStateInstance.Stacks`），**组内至多一条实例**；"续杯 / 叠层"的判据 = **施加方换没换**（同来源 ⇒ 刷新、异来源 ⇒ 叠一层；**未声明来源按同来源处理**）。完整决策树、事件面与三条如实边界见 `openspec/specs/state-stacking-policies` 能力与 §12.9。

### 3.3 Duration 与 Period（FBuffDef 字段，2026-09-02 细化）
- **Duration 拆分（D3-13）**：`EDurationPolicy{Finite, Infinite}` + `DurationTime`（Finite 时有效，FTcsParamValue——原"字面量|Param"，PV 系列 2026-09-11 换型）；快照于 apply/refresh（D3-12）；Infinite=无到期条目永不过期（≤0 魔法值废除）；暂停/变速由泵统一。
- **Period**：`0/未设=无周期（默认）`；机制 = M3 注册**重复到期条目**到堆 → 每周期发 `TcsEvent.State.Periodic` 事件（payload 带 StateHandle + **当前 Stacks + Level**）；默认**等首个周期**，"施加即生效"写进 apply 响应链；**`PeriodRefresh{Keep(默认)/Reset(重置周期计时)/Immediate(立即执行一次并重置)}`**——堆叠刷新对周期计时器的影响（D3-13）。**落地态（2026-10-05）**：词为原生声明 `TcsStateEvents.h` 的 `Tag_TcsEvent_State_Periodic`（换根后新名，正文旧名 `Combat.State.Periodic` 已作废）；产生者 = Task 3 的周期条目、消费实证 = Task 7 检查 S11/S12（`PeriodicDelta=1` / 累计 3 次）。

### 3.4 关系表（裁决 1 + D3-3 + 形状扩展）
- 字段形状（裁决 1 增补）：`{ Status, Blocks, Requires, Priority, Cancels }`——FBuffDef 字段（约束施加态共存）；**FSkillDef 同形状字段**（语义映射：Block=禁止激活、Require=激活前提、Priority=顶替优先级、Cancels=激活时取消指定运行/状态——姿态切换），共享通用关系检查器（主体类型化：状态实例/施法运行）。
- 执行时机：apply 时校验（Blocks 拒绝 / Requires 未满足拒绝或排队）；**任何状态移除后对依赖它的存活状态做事件驱动重评**（脏标记，批量移除合并一次）。
- 槽位竞争（PriorityOnly 抢占 + PreemptionPolicy + 同优先级策略，TCS 报告 06（库外，已不在库内）已验证）并入 Priority 语义。
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

- `ApplyState(目标, DefTag, Source, Overrides?) -> EApplyResult`（Overrides = 施加方传入的参数覆盖，进快照）
- `ExtendDuration(handle, Δ) / SetRemaining(handle, T)`（D3-15：运行实例生命周期操作——"灼烧延长"，到期堆同步）；**MaxStacks 无运行时修改**（政策常量；成长=变体+重定向）
- `RemoveState / ExpireState(handle, Cause)`、`GetState(handle)`、`ForEachState(unit, 谓词)`
- `SetLevel / GetLevel`（Custom 增长模式的项目入口；FollowStacks 之类由项目实现）
- 关系检查器：`CheckRelations(主体, 动作) -> 结果`（通用，主体=状态实例/施法运行）

## 5. 关键流程与机制

- **Apply**：校验（词表/就绪态）→ 关系表检查 → 五轴共存决策（含 Custom 策略）→ 池分配 → **快照构建（D3-12）** → **行为 Fragment 订阅挂接**（按 `Interests` 向总线订阅，**每兴趣 Tag 一条 + 计数配对**、共享 Handler 由门面 `UPROPERTY` 强持有；`Source` = 状态实例句柄，remove/expire 自动退订——D3-7 v2）→ **内联触发行登记**（`Source` = 实例级联锚点 `CascadeAnchor`；**排在 `Applied` 广播之前**）→ 挂 Duration/Period 条目（到期堆）→ **修正器物化（D3-19）**：解析 ModifierRows 模板引用——属性行 ParamKey 从快照解析后挂 M2（`Source` = 实例级联锚点），技能参数行经注册制分派到 TcsSkill 执行器挂 M5 → `OnStateApplied`（提交尾 flush，立即通道）。**三处接线（订阅 / 触发行 / 修正器）全部 MUST 排在 `Applied` 广播之前**（订阅者要能在自己的 `Applied` 回调里读到挂载后的数值与自己的行为）。
- **Remove/Expire**：原因标记（Expired/Removed/Cancelled）→ M2 `RemoveBySource` → **行为 Fragment 退订**（与触发行退订同处，**排在广播之前**——"状态走了，行为先走"）→ 周期条目惰性摘除 → 关系表级联重评（批量合并）→ `OnStateRemoved`。
- **Phase 转移矩阵**：阶段×允许迁移 = 数据表声明（可查不可改）；非法迁移 = ensure。
- **生命周期事件全集（D3-7）**：`OnStateApplied / OnStateRefreshed / OnStackChanged / OnStateExpired / OnStateRemoved(+原因)`——全阶段总线事件，核心词汇 FStruct。
- **Fragment 机制（v4——策略载体 D3-7 v3）**：全部策略统一 **FInstancedStruct 载体**——策略 = USTRUCT 反射基类 + C++ 虚函数分派（StateTree 同构；配置即成员，Resolve/Pass/决策方法 const 纯），Def 资产以**裸 `FInstancedStruct`** 成员持有策略（+ 手写 `meta=(BaseStruct=…)`；策略实例归 Def 资产持有；单字段暴露——类/载荷配对 bug 类消失）。**实例零策略**（FStateInstance 不持策略/载荷/订阅句柄——池化 struct 纯度与操作复制地基；策略调用点在引擎流程，Def 解析注入）。两种深度：**决策 Fragment**（按决策种类各自抽象接口：五轴共存决策/IValueDomainPolicy/ICostPolicy 等——签名强类型，流程调用点注入；**五轴共存决策已于 2026-10-05 落地**）；**行为 Fragment**（`FTcsStateBehaviorFragment`：兴趣 Tag 列表 + 单一泛化虚回调 `OnStateEvent(FGameplayTag, FInstancedStruct, Ctx)`——**✅ 已于 2026-10-05 落地**：apply 按**每兴趣 Tag 一条订阅**挂接 / remove 自动退订，复用 D4-1 订阅配对 + 实例级联机制；载荷回调内自取、状态载荷按实例身份自筛。宿主样本证明见 `EVID-2026-10-05-state-behavior-fragments`，口径见 §12.10）。per-instance 扩展状态 = 实例 Variables 通道按需（今天零消费者；回合语义等未来 Fragment 需求出现时裁）。回合制等领域能力落位：回合本体论不进核心（= ITimeSource + 回合语义 Fragment）。BP 策略扩展通道放弃（R0"蓝图不承诺"承责）；编辑器校验由 Def 资产 IsDataValid 承担。
- **到期驱动（D3-6）**：集中最小堆（M0 泵），到期才出队；惰性删除+代际校验；帧成本=到期项数。

## 6. 依赖方向声明（铁律 1，D4-14 后更新）

**M3 的 Def 不持有行为链**——buff 行为由 M4 触发行订阅 M3 生命周期事件挂接（事件流 M4→M3 方向不变）。M3 只负责忠实广播生命周期。

**编译依赖（D4-14 注册制分派后）**：TcsState 依赖 TcsEffect（FEffectStep 基类型+ExecuteChain——ApplyState 步骤+执行器是领域步骤，住本模块并自注册；Buff 链在本模块发起、经注册表分派到各领域执行器）。**TcsEffect 不依赖 TcsState**——机制层对领域零感知；运行时对状态实例的查询经 ICombatEntityQuery 注入。

## 7. 网络姿态落点（NET-1/2）

- 权威侧跑全流程；**操作复制**：状态操作流（Apply{DefTag,Source,ParamSnapshot}/Refresh/StackChange/Remove{Handle,Reason}）复制到客户端镜像桶重放——**ParamSnapshot 随操作流走 = 回放输入完备**；客户端到期推算仅作表现。
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
- v2 增补 9（2026-09-15，D5-17 v3 描述视图策略化）：§2 DescriptionTextKey 单字段 → **Descriptions 配置组**（`FTcsDescriptionEntry` 多描述入口 + `TInstancedStruct<FTcsParamView>` 视图槽位——~~类型住 TcsNotation，本模块已有 Notation 边零新边~~ → **类型住本模块 TcsState**：`DEC-02-fold-display` v3 命名批的"住哪"表明写"描述视图槽位 / 配置条目 → TcsState（Def 形状所在）"，TcsNotation 只拥有词汇约定与视图机制。**2026-10-04 订正**：上述"住 TcsNotation"的父注作废、改判 TcsState（见 §12.5 第 4 条）；
    §3.6 tooltip 索引口径改 Series 视图措辞（口径本身不变：实例快照 Level）。
- v2 增补 10（2026-09-23，标识体系 tag 化改造回写）：§2 Def 资产身份 `FName DefId` → **`FGameplayTag DefTag`**（主资产身份 = `[PrimaryAssetType, DefTag.GetTagName()]`；表行 `RowName` 降为编辑期定位，与 `DefTag` 分工而非双真相）；§3.1 实例权威字段、§4 `ApplyState` 签名、§7 操作复制载荷同步改 tag 口径。落点 = 提案 `switch-identifiers-to-gameplay-tags`（2026-09-22 归档）。
- v2 增补 11（2026-10-04，**R5 开工收窄轮**）：新增 **§12「R5 实施范围与切分」**（落地对照 / 不落及归属 / 两处实现口径 / 四处口径待澄清）；身份块状态改 `PENDING（R5 实施中）`；**两处实现口径改名**记录在 §12.3——宿主类 `UCombatStateRegistry` → `UTcsStateSubsystem`（§3.1 正文加注）、资产类 `UTcsBuffDef` → `UTcsBuffDefAsset`（§2 正文加注），两处正文改名的**规格提案随 `PLN-R5` Task 1 一并落地**。计划见 [`PLN-R5`](../plans/plan-r5-state-layer.md)。
- v2 增补 12（2026-10-04，**R5 Task 1 落地**）：模块与 Def 资产族物化——`TcsState` 进 `.uplugin`（第 8 个模块）、`FTcsStateDefBase` / `FTcsBuffDef` / `FTcsBuffDefTableRow` / `UTcsStateDef` / `UTcsBuffDefAsset` / `FTcsNumericParamRow` / `FTcsDescriptionEntry`(+`FTcsDescriptionViewSlot`) 落地、定义库第三条发现路径 `DiscoverStateDefs` / `ResolveStateDef` 接通（第三份缓存 + GC 引用收集）。
  **订正 v2 增补 9 的"描述载体类型住 TcsNotation"**：改判 **TcsState**（依据 `DEC-02-fold-display` v3 命名批"住哪"表）；新增 **§12.5** 记四条落地口径（两处类型提前落 / 状态词根归属挂账 / 描述载体归属订正）。规格提案 = `2026-10-04-add-tcs-state-module`（四份 delta）。
- v2 增补 13（2026-10-04，**R5 Task 2 落地**）：运行态骨架物化——`FTcsStateHandle` / `FTcsStateInstance` / `FTcsStateRegistry`（per-unit 桶 + 槽位代际）/ `FTcsStateOps`（引擎函数，三拆）/ `UTcsStateSubsystem`（世界门面 + 定义登记口）/ `EStatePhase`+`EStateRemoveCause`+`EApplyResult` 三枚举 / 六枚生命周期事件 tag + 载荷 `FTcsStateEventPayload` 落地；**§3.1 的实现名与 §3.3 的 tag 名就此定型**（见 **§12.6**）。
  **§3.1 正文的两处落地订正**：① `FStateInstance` 的实现名是 **`FTcsStateInstance`**（成族）且**非反射结构体**（只在 C++ 侧流转；进总线的载荷是 `FTcsStateEventPayload`）；② 实例**自持 `Unit`（宿主单位）**并**只持 `DefTag`（定义身份）**——`GetDef()` 解析缓存在本轮**不落**（热路径不回查 Def 的承诺登记在 §12.6 第 3 条，快照构建落地后才有意义）。
  **§4 的门面归位**：`ApplyState` / `RemoveState` / `ExpireState` / `GetState` / `ForEachState` 已落；`ExtendDuration` / `SetRemaining` 本轮只落**签名 + 拒绝面**（堆同步归 Task 3）；`SetLevel` / `GetLevel` / `CheckRelations` 仍归后续（§12.2）。
- v2 增补 14（2026-10-04，**R5 Task 3 落地**）：数值与时间两条腿——`FTcsParamSnapshot` 与快照构建（`ConvertToCanonical` 首次点亮）、四个等级源 + `ITcsEntityLevelProvider`（**接口住本模块**、走派生上下文，非 Core）、Duration-Period 重复到期条目与 `PeriodRefresh` 三态、`ExtendDuration` / `SetRemaining` 的堆同步。落地口径 = **§12.7**；证据 `EVID-2026-10-04-state-param-snapshot-and-level-sources`。
- v2 增补 15（2026-10-04，**R5 Task 4 落地**）：**修正器物化（D3-19）**——`ModifierRows` 模板 → M2 账本条目（`Source` = 实例来源句柄）、引用类操作数**从该实例自己的快照取值**（反射壳 + RAII 栈式绑定）、施加/刷新同批内挂载、移除/到期按同一来源一次摘净。落地口径 = **§12.8**；证据 `EVID-2026-10-04-state-modifier-materialization`。
- v2 增补 16（2026-10-05，**R5 Task 5 落地**）：**五轴堆叠与刷新政策**——**层模型裁定甲案**（层数记在一条实例内、组内至多一条实例）、组键三态 + Custom 逃逸位、`MaxStacks` 与溢出两档、数值叠加两档、时长刷新两档；**两处裁档**（`EGB_PerTag`+`GroupTag`；`EOP_ReplaceOldest`/`ReplaceNewest`）与 `EVS_PerStackValue` 标记零行为。落地口径 = **§12.9**；证据 `EVID-2026-10-05-state-stacking-policies`。
- v2 增补 17（2026-10-05，**R5 Task 6 落地，6a + 6b 两半**）：6a——`FTcsStepApplyState` / `FTcsStepModifyAttribute` 自注册、内联触发行登记与按锚点退订、`CascadeAnchor` 与 `Source` 解耦、链运行态 `RunSource` + `CausedBy` 与两个只读读数口、`AttributeCompare` 第三条内置条件；6b——行为 Fragment 契约（每兴趣 Tag 一条订阅 + 共享 Handler）、首个状态载荷读取器与泛型主体匹配、`FTcsStateRegistry` 出线进程唯一发号（身份修复）、`STAT-5` 有限重入守卫。落地口径 = **§12.10**（6a 十一条 + 6b 四条）；证据 `EVID-2026-10-05-state-chain-primitives` / `-state-behavior-fragments`。
- v2 增补 18（2026-10-05，**R5 Task 7 落地 + R5 收束**）：端到端竖切（`Tcs.Test.State.Run` / `.Reject`）+ **首批真内容资产**（4 枚）+ `AttrModDef` 根落地（10 → 11，`ATTR-1` 闭合）+ 定义库第四条发现路径与第四计数。**正文订正**：§3.3 的 `Combat.State.Periodic` 改为换根后新名 `TcsEvent.State.Periodic`（并标注产生者与消费实证）；§11 验收钩子改为"已验 / 未验"对账表；身份块状态 `PENDING` → **`ACTIVE`**。落地口径 = **§12.11**；R5 全轮总账 = **§12.12**；证据 `EVID-2026-10-05-state-layer-pie`。规格提案 = `verify-state-layer-e2e`（1 新能力 + 3 改能力）**已于 2026-10-06 归档**为 `changes/archive/2026-10-06-verify-state-layer-e2e/`（**归档日为 10-06 机器日期，非计划预写的 10-05**）；归档后 `specs/state-layer-e2e-validation/`（四需求）落盘、`gameplay-tag-governance` 的 `## Purpose` 根数手工 10 → 11 补正，`validate --all --strict` = **34 / 0**、能力数 33 → **34**、`changes/` 零活动提案。

## 11. 验收钩子

竖切剧本第 6 项（**新增纯数据链资产零 C++**——M9 收尾轮后 buff 验证改测试 Source/链数据，M3 状态桶不在 R3）由 M2 兑现；关系表级联重评、到期堆、Custom 策略、快照重建（refresh）在 M3 轮人工检查路径。

**R5 收束时的对账（2026-10-05）**——本节各钩子逐条改为"已验 / 未验"两态，**未验的仍是留白，不得读成已验**；证据 = `EVID-2026-10-05-state-layer-pie`（端到端竖切，含 15 条实测检查）与 R5 Task 1~6 的六份单元证据。

| 验收钩子 | 状态 | 证据 / 留白 |
|---|---|---|
| **同一实例串起六个能力面**（定义解析 → 快照冻结 → 真资产修正器物化 → 内联触发行起链 → 周期回调 → 到期回收与属性复原） | ✅ **已验** | `Tcs.Test.State.Run` 两轮 **15/0**，零非预期红字、复现性摘要逐字相同；真内容资产驱动（`DA_State_E2E` + `DA_ModDef_E2E` + `DA_Chain_E2E_Apply` / `DA_Chain_E2E_Behavior`）；`EVID-2026-10-05-state-layer-pie` §1 |
| **属性复原与"常驻"两面读数互证**（状态修正器按锚点摘净 / 链条目按设计常驻） | ✅ **已验** | S14 `Armor` 复原 `5.000`（+25 覆写到期摘净）；S15 `Attack` 停在 `25.000`（−5 链写入常驻）——两面读数同轮取得，证明"状态挂 `CascadeAnchor`、链挂 `RunSource`"的落点差异**可观测**；`CHAIN-7`（链挂条目无框架侧回收触发点）据此登记 |
| **内联触发行按锚点退订** | ✅ **已验** | S13 效果门面触发行计数回到施加前读数；实现侧 = `UnregisterTriggerRowsBySource(CascadeAnchor)`（§12.10 第 7 条） |
| **可复现性** | ✅ **已验（口径已订正）** | 两轮比对的判据 = **复现性摘要行逐字相同 + 各检查结论相同**，**MUST NOT** 落在区段字节上（进程内一次性惰性初始化日志只写一次；见 §12.11 第 1 条） |
| **关系表级联重评** | ⛔ **未验** | 关系表检查器整体归 R5.5-e（字段语义未定稿，§12.2）；本轮只落字段形状、零消费者 |
| **Custom 策略** | ✅ **已验（限堆叠决策一支）** | 宿主自定义决策 Fragment 被真实调用（样本恒不同组 ⇒ 每次新建）；Task 5 检查 22；**关系表/槽位竞争侧的 Custom 仍未落**（R5.5-e） |
| **快照重建（refresh）** | ✅ **已验** | Task 4 检查 21e（重复施加 ⇒ `Refreshed`，同批内摘旧挂新、快照按新 payload 重建）；Task 5 检查 22 叠层刷新路径 |
| **`PeriodRefresh` 三态** | 🟡 **部分** | 只验了 `Keep`（默认档）；`Reset` / `Immediate` 本轮无内容消费者（证据 §6 边界）；`ExtendDuration` 的**延长上限未定**（D3-15 原文未定，实现为不钳到总时长） |
| **技能参数行分派（`UTcsSkillModDef`）** | ⛔ **未验** | 类型与 `TcsSkill` 今天都不存在（§12.2，归 R5.5-f / R6） |
| **操作复制（网络姿态）** | ⛔ **未验** | 网络姿态整体未开工（§12.2，归 R7 `WAIT-3`） |

## 增补：时值拆分/Overflow/生命周期操作/关系表组织（2026-09-02，v5 增补）

- **Duration 拆分（用户提案采纳）**：`EDurationPolicy{Finite, Infinite}` + `DurationTime`（Finite 时有效，FTcsParamValue——原"字面量|Param"，PV 系列 2026-09-11 换型）——替换"≤0=永久"魔法值；None 场景收敛：瞬发走链直接表达、永久=Infinite（GAS 三值习惯不适用——伤害系统已独立成模块）。
- **Period 联动**：Period 独立可配（Finite+Period=标准 DoT、Infinite+Period=光环、四组合全合法；Policy 联动仅为编辑器显示逻辑）；**`PeriodRefresh{Keep(默认)/Reset(重置周期计时)/Immediate(立即执行一次并重置)}`**——堆叠刷新对周期计时器的影响（用户提案采纳）。
- **Overflow 升级替换：引擎不内建**（用户确认）：事件组合三步表达（`StackChanged 满仓` 触发行 → `ApplyState{更强}` + `RemoveState{原}`）；状态级重定向（DefTag 存在期解析选路）留未来扩展；引擎保证 StackChanged 时序与同帧合并。
- **运行实例生命周期操作**：`FStateOps::ExtendDuration(handle, Δ) / SetRemaining(handle, T)` 进 API（"灼烧延长"类）；**MaxStacks 无运行时修改**（政策常量；"层数上限+2"成长 = 变体 BuffDef + 技能/状态级重定向）。
- **关系表/槽位竞争表组织（用户提案采纳）**：两者**并存**（槽位竞争管施法运行/动作状态，关系表管 Buff 施加态，互相影响）；组织 = **统一 DataTable/DataAsset 行**（行 = 单个 StateDef **或一类**——StatusTag 匹配）；**解析栈**：全局默认表（插件）→ 游戏模式表 → 角色覆盖表（Boss）——让渡点模式再次应用；关系字段引用走 Tag（StateTag/SkillTag 子域）；**槽位竞争不用 StateTree**（数据行 + 引擎抢占规则；StateTree 只做单位级决策）。
- **堆叠轴命名**：~~DurationMerge~~ 撤回——值名回 TCS（{None, RefreshRemainingToTotal}），**轴名 StackDurationPolicy 已终定（D3-17，已回写 §3.2）**。

---

## 12. R5 实施范围与切分（2026-10-04 收窄轮）

> **本节回答"本轮把本文的哪些条款变成代码、哪些不落"**——它是实施视角的收窄结论，实施计划见 [`PLN-R5`](../plans/plan-r5-state-layer.md)。

### 12.1 本轮落地（R5）

| § | 条款 | 落点 |
|---|---|---|
| §2 | `FStateDefBase` / `FBuffDef` 形状 + `UTcsStateDef` / `UTcsBuffDefAsset` 资产 + `FTcsBuffDefTableRow` | PLN-R5 Task 1 |
| §3.1 | `FStateInstance`（池化、纯数据）+ per-unit 桶 + `FStateOps` | Task 2（**✅ 2026-10-04**，落地口径见 §12.6——实现名 `FTcsStateInstance`、类名 `FTcsStateRegistry` / `FTcsStateOps`） |
| §3.2 | `FStateStackPolicy` 五轴（含 Custom 决策 Fragment） | Task 5 |
| §3.3 | Duration 两态 + Period + `PeriodRefresh` 三态 | Task 3（**✅ 2026-10-04**，落地口径见 §12.7） |
| §3.5 | `LevelBase` / `MaxLevel` + **四型等级源** + `ITcsEntityLevelProvider` | Task 3（**✅ 2026-10-04**，落地口径见 §12.7——接口住 **TcsState**、读口走派生上下文） |
| §3.6 | `FTcsParamSnapshot` / `FTcsParamSnapshotEntry` + 快照构建（含 ValueConvention 转换） | Task 3（**✅ 2026-10-04**，落地口径见 §12.7——实现名沿用本节命名批、`ConvertToCanonical` 首次点亮）；读取适配器的**反射壳**由 Task 4 补（**✅ 2026-10-04**，见 §12.8） |
| §4 | 门面 `ApplyState` / `RemoveState` / `ExpireState` / `GetState` / `ForEachState` / `ExtendDuration` / `SetRemaining` | Task 2 / 3 |
| §5 | Apply / Remove / Expire 流程 + 生命周期事件全集 + 行为 Fragment 订阅挂接 + **修正器物化（D3-19）** | Task 2（**✅**）/ **Task 4（✅ 2026-10-04：修正器物化落地，口径见 §12.8）** / **Task 6（✅ 2026-10-05）**：内联触发行接线（6a）+ 行为 Fragment 订阅挂接（6b）**两半均已落地**，口径见 §12.10 |
| §6 | `FStepApplyState` 领域步骤（自注册）+ Buff 内联触发行（施加时登记、`Source` = 实例句柄） | Task 6（**✅ 2026-10-05**）：步骤落 `TcsState/Public|Private/Chain/TcsStepApplyState.*`、接线落 `TcsStateOps_Trigger.cpp`；**两处口径订正**：① 步骤 `Source` = **链运行态的 `RunSource`**（起链时发号的来源锚点），不是 `RunState.Self`——`FTcsChainRun` / `FTcsEffectContext` 本无来源位；② 内联触发行登记的 `Source` = **实例的 `CascadeAnchor`**（级联撤销锚点），不是"实例来源句柄"——`Source` 已退回"施加方身份"（一个来源可施加多个定义，按它撤销会互相误摘，见 §12.10） |

### 12.2 本轮**不落**（带归属）

| 条款 | 为何不落 | 归属 |
|---|---|---|
| §3.4 关系表**检查器** | **字段语义从未定稿**（`DEC-2026-08-31-state-relation` 的"Priority 语义 / 互斥组结构 / 族规则表达式"至今待在办行；本文 §3.4 只给了形状与执行时机）⇒ 不满足"设计已按实施视角收窄" | R5.5-e（**先拍板语义**）。本轮**只落字段形状**（`FBuffDef` 的 `Blocks` / `Requires` / `Priority` / `Cancels`），零消费者如实登记 |
| §3.4 级联重评 / 槽位竞争 / 解析栈组织 | 依赖上面的语义拍板；槽位竞争主体（施法运行）属 R6 | R5.5-e |
| §3.7 就绪状态机（`Unloaded→Loading→Ready/Failed`） | 它是 **DefLibrary（M6/R7）**的性质，不是状态层的 | R7 `INTEG-3` |
| §5 修正器物化的**技能参数行**分派 | `UTcsSkillModDef` 与 `TcsSkill` 今天都不存在 | R5.5-f / R6 |
| §7 操作复制（`Apply{DefTag,Source,ParamSnapshot}` 等） | 网络姿态整体未开工 | R7（`WAIT-3`） |
| §8 非目标各项 | 性质不变（不做行为链解释器 / 回合本体论 / 每实例 StateTree / 值复制 / Instancing / Level 增长策略） | — |

### 12.3 收窄轮定下的两处实现口径（与本文正文的差异）

1. **宿主类名**：§3.1 的 `UCombatStateRegistry` **作废**——本轮定为 **`UTcsStateSubsystem : UWorldSubsystem`**（per-unit 状态桶；与既有 `UTcsAttributeSubsystem` 同构）。`SPEC-05-integration` §2.1 的 `UCombatWorldRegistrySubsystem`（实体状态机 + 泵接线 + DefLibrary 门禁）**仍有效但属 R7/M6**——两者分工：**状态桶归 TcsState，实体注册与门禁归集成层**。
2. **资产类名**：§2 的 `UTcsBuffDef` → **`UTcsBuffDefAsset`**（UHT 按"去前缀引擎名"判重：`UTcsBuffDef` 与数据 `FTcsBuffDef` 同名即编译失败，先例 `UTcsEffectTriggerDefAsset`；`openspec/project.md` 既定补救条款）。`UTcsStateDef` 不变。

### 12.4 口径待澄清（本轮登记，不阻塞）

- §2 的 `FStateDefBase` "**生命周期事件词汇**"字段：**零消费者**（行为 Fragment 的 `Interests` 已是声明面）⇒ 本轮不预建，等真实消费者。
- §2 的 `Descriptions` 描述视图配置：本轮只落字段与作者侧校验，**渲染归 R8**（`SPEC-09-editor`）。
- §3.3 正文里的 `Combat.State.Periodic` 是**换根前的旧 tag 名**——新名 `TcsEvent.State.Periodic` **已于 2026-10-04（Task 2）落地**、其**产生者（周期条目）于 Task 3 落地**（`TcsStateEvents.h` 的 `Tag_TcsEvent_State_Periodic`，原生声明 + 导出宏）；**该词本轮只有声明、零广播**（周期计时的产生者归 Task 3）。
- §3.4 的"槽位竞争并入 Priority 语义"一节引用的 `TCS 报告 06`（库外，已不在库内）**不可复核**，语义拍板时 MUST 重新论证。

### 12.5 Task 1 落地口径（2026-10-04，R5）

| # | 口径 | 依据 / 后果 |
|---|---|---|
| 1 | `EDurationPolicy` / `ETcsPeriodRefresh` **随 Task 1 落**（住 `Public/State/TcsStateEnums.h`）；Task 2 在同一文件补 `EStatePhase` / `EStateRemoveCause` / `EApplyResult` | `FTcsBuffDef` 的时值字段在 Task 1 就要编译 ⇒ 类型必须先于字段存在；计划 Task 2 交付物列表里的这两个枚举名随之让位（免两处定义） |
| 2 | `FStateStackPolicy` **随 Task 1 落形状**（住 `Public/State/TcsStateStackPolicy.h`：四轴枚举 + `MaxStacks`）；Task 5 只补决策 Fragment 与行为 | 同上：`FTcsBuffDef.StackPolicy` 字段需要类型；本轮**不带** Custom 位的 `TInstancedStruct` 载荷与任何策略基类（零消费者不预建） |
| 3 | **状态词根归属挂账**：新增 `StateDef` 根只承载"状态定义资产的身份解析"（`DiscoverStateDefs` / `ResolveStateDef`）；`Def.StatusTag` 的根归属**本轮不裁定** | 一角色一根 + 零消费者不预建根——解析消费者为零（关系表检查器与按它匹配的槽位竞争表整体归 R5.5-e）；验收资产里 `StatusTag` 复用同一个 `StateDef.*` 词，不构成对归属的裁定 |
| 4 | **描述配置载体住本模块**（订正 §10 v2 增补 9 的"类型住 TcsNotation"父注） | `DEC-02-fold-display` v3 命名批"住哪"表 + `SPEC-07-notation` §1 边界（字段归各 Def、词汇约定归 Notation）；Task 1 落 `FTcsDescriptionEntry` / `FTcsDescriptionViewSlot` 两个纯数据载体，视图策略族仍归 R8 |

### 12.6 Task 2 落地口径（2026-10-04，R5）

| # | 口径 | 依据 / 后果 |
|---|---|---|
| 1 | **身份归资产、登记口显式收身份**：`UTcsStateSubsystem::RegisterStateDef(FGameplayTag DefTag, const FTcsBuffDef& Def)` 收两个形参；**不往 `FTcsStateDefBase` 加 `DefTag`** | 身份是**资产**属性（`UTcsStateDef::DefTag`：资产身份、`GetPrimaryAssetId()` 取值来源、作者期校验对象），`FTcsBuffDef` 是**定义内容**。为"从内容读身份"再加一份 = 策划在同一资产里手填两遍同一个 tag（**双真相**）。运行期副本的身份由登记口一次对齐（调用方 = 定义库，其缓存键本就是 `DefTag`） |
| 2 | **实例自持 `Unit`（宿主单位）** | 句柄里没有单位段 ⇒ 不自持则 `GetState(Handle)` 只能遍历所有桶（O(桶数)）；且 Task 4 的修正器物化要用它把修正器挂到该单位的属性账本上（同一份信息，不是重复真相） |
| 3 | **注册表自建槽位、不复用 `TTcsInstancePool`**；`GetDef()` 解析缓存**本轮不落** | 那个池是"一池一句柄空间"，状态要"**每单位**一个句柄空间"（桶）；套用只有"一桶一池"（同一件事两套身份）或"全局一池"（句柄与单位解耦 ⇒ `GetState` 无法直接定位）两条更差的路。槽位语义**刻意与池一致**（分配优先复用空闲槽 / 释放使代际 +1 / 新槽从 1 起 / 奇偶簿记）。`GetDef()` 缓存**等快照构建**：本轮"热路径不回查 Def"由"实例只用 `DefTag` 做身份比较、数值面尚不存在"自然满足，缓存与版本失效判据纯属预建（零消费者） |
| 4 | **事件载荷的 `Source` 不是 `UPROPERTY`**；六枚 tag 落在既有 `TcsEvent` 根下（**零新增根**） | `FTcsSourceHandle` 是**非反射**纯 C++ struct（"纯内联类型不得带导出宏"那条纪律的另一面）——需要脚本层可达时须先反射化它（台账 `SCRIPT` 系列连带项）。事件词属**框架契约**（广播面漏配即静默失效），故由本模块原生声明并带导出宏 |
| 5 | **阶段机三态与"先广播后释放"** | `EStatePhase{Inactive/Active/Expiring}`：`Expiring` 是"已进入移除流程、槽位尚未归还"的窗口——**它是"订阅者在移除回调里仍能 `GetState` 读到实例"能成立的前提**；非法迁移 `ensure`（配置错误语义），与脏句柄的时序竞态口径（只 `Warning`）刻意分开。四条合法边含 `Expiring → Active`（刷新是"过渡后挂回"） |
| 6 | **共存决策的暂用位**：本轮"同组"判据 = **同单位 + 同 `DefTag`**（`GroupBy = None` 语义） | 它是 Task 5 五轴共存决策的**替换点**（代码注释已就地标明）；`EApplyResult::EAR_Stacked` 本轮不可达，`EStateRemoveCause::ESRC_Cancelled` 本轮无内建产生者（归 R5.5-e）——两处均已在枚举注释里写明归属 |

### 12.7 Task 3 落地口径（2026-10-04，R5）

| # | 口径 | 依据 / 后果 |
|---|---|---|
| 1 | **等级读口不进 Core 上下文**：`ITcsEntityLevelProvider` 住 `TcsState/Public/Host/`，字段挂**派生上下文** `FTcsStateEvaluateContext`（唯一新增字段 `LevelProvider`），域侧源按 `GetScriptStruct()->IsChildOf(...)` 判定后取值 | `FTcsParamEvaluateContext` 住 `TcsCore`，而该读口的唯一消费者（四个等级源）住 `TcsState`——把域读口放进 Core 会让 Core 持有只有上层使用的类型，且日后每个域都想往那里塞自己的读口。**先例 = 同族 `FTcsAttributeEvaluateContext` 持 `ITcsAttributeProvider`**（PV-1 明文支持多层派生）。R-1 调研原设想的"加成 Core 第三字段"**不采用** |
| 2 | **`Instigator` 反而进基础上下文**（`FTcsParamEvaluateContext` 的第三个主体字段） | **分界判据**：字段本身是**跨域通用的主体身份**（谁施加的、谁被施加、生效等级）⇒ 进 Core 基础上下文；字段是**某域的读口**（属性读口、等级读口）⇒ 进该域派生上下文。它是台账 `DAMAGE-5` 的 **+1 字段增量**（Task 0 只补了 `Subject` 与 `EffectiveLevel`） |
| 3 | **快照实现名取设计名** `FTcsParamSnapshot` / `FTcsParamSnapshotEntry` | 快照是**参数域**概念（修正器物化、R6 技能侧参数行都要读它），不是状态模块专属；`TcsState` 前缀会让"技能侧复用同一类型"在名字上说不通。Task 2 在 `FTcsStateInstance` 里预留的 `FTcsStateParamSnapshot` 注释**随之作废** |
| 4 | **读取适配器是纯 C++ 类**（`FTcsStateParamTableReader`：持快照只读指针 + 键查询），本轮**不派生 `UObject`、不实现 `ITcsParamTableReader`** | 唯一消费方（Task 4 物化器）是 C++ 直调，而该接口**不是 `Blueprintable`** ⇒ 造 `UObject` 壳属"零消费者预建"，且会把快照指针寿命绑到 GC 上。继承接口的时机 = 出现第一个要把快照当参数表挂进上下文的反射消费方 |
| 5 | **周期与时值各持一个条目锚点**（`PeriodEntry` / `ExpiryEntry`）；周期走**重复到期条目**（回调广播 `Periodic` 后重新入堆）；**撤销顺序前伸一步** = 先撤该实例两个条目再走 `Expiring → 广播 → 归还槽位` | `Finite` 与 `Period > 0` 是可同时成立的两个语义（标准 DoT）；挤一个锚点会让"撤销旧条目"分不清撤哪条（`ExtendDuration` 只该动时值条目、`PeriodRefresh` 只该动周期条目）。条目持句柄 ⇒ 槽位复用后回调仍会到达，先撤条目才不产生无谓回调 |
| 6 | **`PeriodRefresh` 的 `Immediate` = 同步执行一次**（不是排一个 0 延迟条目）；`ExtendDuration` **不钳到总时长** | "立即"的语义就是本次调用内发生，排 0 延迟会把它变成"下一泵点"（与语义不符）。延长上限在 D3-15 原文**未定**；若改为"钳到总时长"那是另一条决策，代码注释已标明改一处即可 |
| 7 | **`EvaluateTotalDuration` 实时求值、不设"时值的参数键"** | `DurationTime` 是**时值字段**（不在 `Def.Params` 里）；把它塞进快照就要给时值编一个参数表键，而**键空间归宿主声明**（`gameplay-tag-governance`）。求值上下文与快照构建共用同一装配点（`FTcsStateOps::MakeContext`），故"时值与参数行不同档"的双口径不会出现 |
| 8 | **宿主参数源插槽（R4.5-b）**：接口与转发器**同住一个头** `TcsCore/Public/Parameter/TcsParamSourceHost.h`；连带给基类补 `virtual ~FTcsParamValueSource() = default;` | 同族先例 `TcsParamSource_AttributeScaled.h` 把接口 / 派生上下文 / 源三者同处一文件——"同一机制的两半"拆开只多一跳。虚析构是**多态基类的正确性要求**：等级表两族源持 `TArray` / `TMap`（非平凡析构），非虚析构下"经基类指针删除"是未定义行为（`C4265` 正是这一点）。**两个虚函数都要转发**（漏能力位会让宿主源在白名单校验上拿到错误的默认位） |

### 12.8 Task 4 落地口径（2026-10-04，R5）

| # | 口径 | 依据 / 后果 |
|---|---|---|
| 1 | **物化求值的参数表 = 该实例的快照，经反射壳装进 `Ctx.ParamTable`（甲案）**——纯类本体 `FTcsStateParamTableReader` 不动（仍是查找语义的唯一实现），**另加薄壳** `UTcsStateParamTableReader`（只委托）+ 门面单例（`UPROPERTY` 持有）+ `FTcsStateSnapshotScope`（**RAII 栈式绑定**，嵌套按栈恢复） | **修订 §12.7 第 4 条的推论**（本体"不派生 `UObject`"仍成立，被推翻的是"因此物化器不需要反射壳"）。理由：物化求值要经**反射上下文**把快照交给参数源，而"读参数表"**已有反射契约**；为它另开一条纯 C++ 表通道会让"本次求值的参数表是哪张"出现**两个不一致的答案**（反射位说没有、旁路说有）。栈式绑定是为了让嵌套物化按栈恢复而不是互相覆盖（先例 = 属性管线的 `PushEvalStack` / `PopEvalStack`）。**被否方案**：乙案（Core 加纯 C++ 表端口 `ITcsNumericParamTable` + `EvaluateWithTable` 虚函数）、丙案（物化点自解析 `ParamRef`——ParamRef 语义出现第二份实现）。理由与取舍见 `LOG-DECISIONS` |
| 2 | **物化器收"实例"而不是散字段**：`Materialize(Subsystem, Def, Instance, Out)` | `Unit` / `Instigator` / `Level` / `Source` / `ParamSnapshot` 全是实例已有信息（实例自持 `Unit` 正是为了免去"遍历桶找单位"）——多形参只会让"某处少填一个"成为可能，而**少填在源侧表现为静默落兜底**（最坏的一类错，因为不报错） |
| 3 | **同一批内"摘旧 + 挂新"**（刷新路径）：`BeginBatch` → `RemoveBySource(来源)` → 逐条 `ApplyModifier` → `Commit` | 三条后果都要：① 多个修正器只触发**一次**重算 + 一次广播；② 订阅者看不到"摘了一半"的中间态；③ 逐条挂载若不批，重入窗口放大 N 倍。刷新不这么做就会出现"快照是新值、账本还是旧值"的静默不一致 |
| 4 | **撤销顺序再前伸一步**：**摘除修正器 → 撤时间条目 → `Expiring` → 广播 → 归还槽位**；且**摘除 MUST 排在"取实例指针"之前** | §12.7 第 5 条把撤销顺序前伸到"先撤时间条目"；本条再前伸一步（属性面还清）。顺序中的"取指针之后"是硬约束：摘除的批提交会广播，订阅者可在其中重入状态操作 ⇒ 之后 MUST 重新定位桶与实例（既有守卫接住失效）。完整重入纪律见第 5 条 |
| 5 | **重入纪律（本轮新增的一条通用纪律）**：**任何"可能广播"的调用之后，调用方手里的实例指针与桶引用都可能失效** ⇒ 施加/刷新路径在挂载提交后 MUST 重新定位实例与桶；重新定位后发现实例已不在册时 MUST **不再补播施加事件**（对已不在册的实例广播是错语义）+ Warning | 提交/广播会同步派发到订阅者（`PublishImmediate`），订阅者能改状态（移除本实例 / 注销单位）⇒ 那会让实例指针与桶引用双双失效，症状从"数值错"到"槽位复用后写坏别人的实例"都可能。本轮闭合了**自己引入的两处**；既有的"`Removed` 广播窗口内重入移除同一句柄"**未修**，入册台账 **`STAT-5`**（触发条件 = Task 6 起回调里能改状态的消费者出现） |
| 6 | **属性访问解析点是"薄包装 + 白名单"**：`FTcsStateAttributeAccess`（住 `Private/State/TcsStateAttributeAccess.h`，实现体落 `Private/State/TcsStateOps_Modifier.cpp`——该文件是 TcsState 内**唯一** include 属性门面头处）；暴露面 = 语义面三个（`ApplyModifier` / `RemoveBySource` / `EvaluateCurrent`）+ **事务对** + **一个存在性判据** `IsLedgerReady` | Q-2 缓解①的落地：把"直接依赖属性门面"的影响面钉死在可替换的最小范围内（将来补注入契约只换这一处）。**上限用结构表达而不是靠记性**（不把门面指针交出去 ⇒ 越界在编译期不可达）。**事务对进上限**的理由见第 3 条；**存在性判据**的理由：状态与属性是**两套登记**（状态可施加到没有属性账本的单位上，合法），而账本侧 `BeginBatch` 对未注册单位会 **ensure** ⇒ 挂点先问一句，"没有可改的属性"按配置状态静默处理（`Log` 级、不留红字） |
| 7 | **账本条目的 `Source` = 状态实例的来源句柄**（`FTcsStateInstance::Source`），刷新 MUST NOT 更换；模板行 → 账本条目的字段映射的唯一声明处 = `FTcsAttrModInstance::MakeFromDef(Row, Operand, Source)`（实现住 `TcsAttrModDef.h` 文件末——行类型在那里才完备；header-only `inline` ⇒ 不需要模块导出宏） | "**同一个状态实例 = 同一个来源**"是 Task 6 内联触发行退订要复用的同一条锚点（一次按来源摘除清掉该实例的**两族**条目）。字段映射放在**同时拥有两个形状**的模块里，上层只负责"求值 + 转规范值"，R6 技能侧物化复用同一处 |
| 8 | **`TSoftObjectPtr` 取值顺序 = 先 `Get()`、未加载才 `LoadSynchronous()`** | 命中已加载对象时 MUST NOT 发加载请求；且该顺序使**装置瞬态模板**成为可行夹具（瞬态对象无资产路径，`LoadSynchronous()` 解不出）。GC 可达性由状态门面对登记表副本里 `TSoftObjectPtr` 的 ARO 承担 |
| 9 | **"零红字"的准确读法 = 零*非预期*红字**：`Tcs.Test.Slice.Run` 固有 3 条预期 Warning（19f 未登记定义 ×1 / 20j 无限时值时长操作 ×2），装置头部 MUST 逐条列出预期红字与归属 | 本轮实测发现装置头部原写"红字都在 `.Reject`"与事实不符（会让复核者把预期当故障）。**验收报告引用"零红字"时 MUST 同时给出预期红字清单** |

### 12.9 Task 5 落地口径（2026-10-05，R5）

| # | 口径 | 依据 / 后果 |
|---|---|---|
| 1 | **层模型 = 甲案**（用户 2026-10-05 拍板）：**层数记在一条实例内**（`FTcsStateInstance.Stacks`）、**组内至多一条实例**；替换 = 先走完整移除链摘掉旧条、再建新条 | 三条代价如实登记（§3.2 与台账）：legacy `NoMerge` 只能**近似**；`Overflow` 的后两档在"一组一条实例"下**同义**（裁撤）；"按分组词分组"**不可表达**（裁撤）。**被否的乙案**（层 = 独立实例）能让四行等价表逐字成立，但 `KeepMax` 必须补"代表层"选举机制（否则 N 条实例各挂一份修正器、账本天然累加 ⇒ `KeepMax` 与 `AddValues` 观测不可分），且 `Stacks` 变层序、`Applied`/`Expired` 按层各广播一次——新机制最多而当时零内容消费者 |
| 2 | **四档回执判据**：同组**同来源**（续杯）⇒ `EAR_Refreshed`（层数不变）；同组**异来源** → 未满仓 ⇒ `EAR_Stacked`（+1）；满仓 ⇒ `EAR_Rejected`（无广播）或按 `Overflow` 替换后 `EAR_Applied` | **只有这一读法**让计划 Step 2 的"未满仓则 `Stacks++`"与 Step 3 的"同组**同来源**的重新施加 = `Refresh`"同时成立，且四档全部可达（否则 `Refreshed` 或 `Stacked` 必有一档成为死档）。实测四档全可达（`EVID-2026-10-05-state-stacking-policies` §1） |
| 3 | **未声明来源（无效句柄）视为"同来源"**（通配） | 来源句柄在门面处按无效值发号（每次一枚新号）⇒ 若"未声明"被判成"换了施加方"，任何不关心来源的调用方在默认策略（层数无限）下**每次施加都叠一层**——一个只在长跑里才显形的错误。**副作用（好消息）**：既有装置检查 19c / 21e（"重复施加 ⇒ `Refreshed`"）无需改动即可继续成立。⇒ **生命周期长于一次施加的调用方 MUST 传稳定的 `Source`**（它既是级联锚点也是"续杯 / 叠层"判据） |
| 4 | **两处裁档**（零消费者时裁，无迁移面）：`EGB_PerTag` + `GroupTag` 字段；`EOP_ReplaceOldest` / `EOP_ReplaceNewest` | 前者见 §3.2（基座含 `DefTag` ⇒ 与 `None` 同义）；后者在"一组一条实例"下是同义动作。**同族第四例**：`EVS_PerStackValue` **不裁但标记零行为**（它的合规实现是新增机制、不是死档；且 §3.5 明文撤回它的退化形态）⇒ 台账 `STAT-6` |
| 5 | **`StackChanged` 的广播判据 = 层数真的变了**；叠层路径顺序 = **`StackChanged` 先于 `Refreshed`**（两笔载荷的 `Stacks` 均为新层数） | 计划 Step 2 的括号注（替换路径也播 `StackChanged`）与事件词的既定语义（"层数变化"）冲突 ⇒ 以词义为准（替换的观测量 = `Removed` + `Applied` 两笔）。定序判据用**同一订阅者的处理序号**（计数器只能证"都到了"） |
| 6 | **Custom 决策 Fragment**：`USTRUCT(meta = (Hidden))` + 三个**中性默认**方法（`IsSameGroup` / `ShouldAccept` / `ResolveStacks`，禁 `= 0` 与 `PURE_VIRTUAL`）；载体 = **裸 `FInstancedStruct`** + 手写 `meta = (BaseStruct = ...)`；框架**零具体策略**（宿主样本住 LAC `TcsDev`） | 计划 Step 4 的 `TInstancedStruct<…>` 是全仓 2026-09-24 已换型的旧写法（该载体在宿主脚本层导出为空壳 ⇒ 宿主配不了策略）。**"解析一次"纪律**：把"找成员 + 出决策"拆成两次调用会让**同一次施加**刷 2 条重复 Warning ⇒ 合成单一入口 `ResolveStackDecision(Request, Bucket, Policy)` |
| 7 | **数值叠加在物化边界按层成倍**（`OPK_Literal` ⇒ `Literal × Stacks`；`OPK_AttributeScaled` ⇒ `Coefficient × Stacks`） | 物化边界是"值"的唯一收口处（值约定转换也在那里），且每次物化**从模板重算**（`Out.Reset()` + 逐行重建）⇒ 乘层数不自我累积、条数不随层变。**顺序硬约束**：调用方 MUST **先写层数再物化**（"层数先落、数值后算"），反过来会让本次叠层的数值停在旧层数——无编译期保护，靠注释与装置读数钉住 |
| 8 | **时长刷新轴在"时间重挂点"读**：`ScheduleTime` 在刷新路径上按 `StackDurationPolicy` 决定"不覆写 `DurationRemaining`（保留剩余）"还是"回满额"；首挂恒回满额 | 原实现**无条件回满额** ⇒ `ESD_None` 这一档**在实现上从未存在**（Task 3 落 `ScheduleTime` 时该轴还是"只落形状"，无人读它）。判据：这是**实现缺陷**（规格 §3.2/§3.3 与计划 Step 3 都要求按轴），不是"计划没写"。保留那一支靠"不覆写"实现——`PushExpiry` 按字段值排到期时刻 |
| 9 | **满仓拒绝 = 业务结果（`Log`）**；**载荷缺失/类型不符 = 配置错误（`Warning`）**；后者只住拒绝命令 | 拒绝是策略的正常结局之一（不是故障）；"选了逃逸位却没挂策略"是作者错误。且"任何**故意触发失败输出**的检查 MUST 独立成 opt-in 命令"（`MEM-20261004-27`）⇒ 该检查住 `Tcs.Test.Slice.Reject`（检查 K） |
| 10 | **组键基座含 `DefTag`** ⇒ **不同 `DefTag` 的实例永不共组** | 一条实例 = 一个定义的数据（快照、修正器、事件载荷全按该定义来）⇒ 跨定义共组在甲案下会退化成"B 定义去顶 A 定义的实例"，语义是坏的。它同时是第 4 条裁撤的根据 |

### 12.10 Task 6 落地口径（2026-10-05，R5；链原语与行为面接线——6a 十一条 + 6b 四条）

| # | 口径 | 依据 / 后果 |
|---|---|---|
| 1 | **级联锚点与施加方身份解耦**（用户 2026-10-05 拍板）：实例新增 `CascadeAnchor`（**每实例恒发新号、刷新不换**）作级联撤销锚点；`Source` 退回"**谁施加的**"（共存判据 + 事件载荷），**MUST NOT 当撤销锚点** | 现状下**一个来源施加两个不同定义**（一条技能给同一目标挂两个 buff）时，撤一个会按来源值把另一条的修正器与触发行一并摘掉，而那条实例还活着 ⇒ "状态在，修正器没了"的不一致态。**代价**：实例 +1 字段；`state-instance-lifecycle` / `state-modifier-materialization` 各 1~2 条 MODIFIED。**它同时修正了一句失效的规格**（旧文写"`Source` MUST 每次施加取新值"，而代码在调用方声明时是沿用） |
| 2 | **链运行态 = "身份 + 因果边"两个字段**（用户 2026-10-05 拍板）：`FTcsEffectContext::RunSource`（**身份**：`ExecuteChain` 在黑板未自带时发一枚新号；**子链另发**）+ `CausedBy`（**因果边**：触发行起链 = 该行的 `Source`、子链 = 父链的 `RunSource`、宿主直调可无） | **为什么不是一个字段**：身份要"每次运行唯一"才能让异源叠层可达；成因要"指回上游"才能答"这条链是谁触发的"。合一即互斥——定义库登记的系统级触发行**全部共用一枚**来源句柄（`TcsDefinitionSubsystem` 装配期只发一次），把该值当身份用会让所有系统级规则起出的链互为同源 ⇒ 链路径下**只能续杯、无法叠层**。**三处独立证据一致指向"边落在对象上"**：① `SPEC-01`（`01-module-m0-core.md` §2）定案逐字"只做归属，**不承载因果链**——因果链由**对象图天然边**承载（…每层对象自带 `Source` 字段，查询从明确起点沿对象逐级走）"；② 原始版自己的结论（`master:Documents/后续优化内容/AbilityKit设计借鉴/05-溯源树Trace与explain.md:136`）"**不要**把血缘树塞进 `FTcsSourceHandle` 结构体本身…血缘树是上层诊断数据，应分开"；③ **原始版实践反证**（2026-10-05 `master` 分支考古）：`CausalityChain` 是"只写不读"的字段（命中只有工厂拷贝+追加 / `NetSerialize` / `DebugString`；`GetRootSource` / `GetParentSource` 有伪码无实现；`operator==` 只比 `Id`），且**建边路径在插件内从未被执行**（`CreateChildSourceHandle` 零调用方，`ApplyBuff` 传空 `FPrimaryAssetId()`；`master` 的 `Source/` 下连伤害模块都没有）⇒ "技能 → Buff → 持续伤害"那条链在原始版里从未真实运行过一次 |
| 3 | **因果边 MUST NOT 成为撤销或正确性依赖**：不得按 `CausedBy` 级联撤销、不得用 `CausedBy` 判共存；撤销一律按各实例自己的锚点 | 原始版自己的否决理由（`master:…调研：SourceHandle机制扩展优化.md:203-204` 逐字："父级 SourceHandle 的生命周期可能先于子级结束…**已确认 State 之间不应根据 SourceHandle 互相影响生命周期**"）。**如实边界**：定义库那一路的行共用一枚来源句柄 ⇒ 系统级行的 `CausedBy` 只能精确到"定义库那一批规则"；精确到"哪一行"需将来把发号改成**每行一枚**（登记台账） |
| 4 | **`FTcsStepApplyState` 的取值**：`Source` = 黑板 `RunSource`、`Instigator` = 黑板 `Instigator`（无效时门面退化为目标）、`ParamTable` = **空**（引用类参数源落兜底）、`Overrides` = 步骤字段 | 计划 Step 1 原写"来源句柄 = 本次链运行态（`Run.Self` 或链级来源位）"——**没有落点**：`FTcsChainRun`（11 字段）与 `FTcsEffectContext` 本无来源位，`Self` 是运行态自身句柄、类型也换不成 `FTcsSourceHandle`。链侧参数表载体不存在（`ITcsParamTableReader` 的链侧实现零消费者）⇒ 如实登记边界 |
| 5 | **步骤不能断链**：`ApplyState`（与 `ModifyAttribute`）的全部失败面统一为 `Warning` / `Error` + **`TSR_Completed`**；四档回执（含 `Rejected`）一律 `Log` 级 | `ETcsStepResult` 只有 `Completed` / `Running` 两档，**步骤无法中断链**（断链只发生在"执行器未登记"时，由门面自己做）。计划 Step 1 原写"仅 `DefTag` 不可解析时 `Error` + 断链"**不可表达**；真正的软失败接管归 `OnError`（R5.5-a） |
| 6 | **内联触发行接线时机（两侧都是硬约束）**：施加路径**登记排在 `Applied` 广播之前**（新登记的行要能看见自己的 `Applied`）；移除 / 到期路径**退订排在 `Removed` / `Expired` 广播之前**（该实例自己的行 MUST NOT 因自己的死亡事件起链） | 撤销顺序由"摘修正器 → 撤时间条目 → `Expiring` → 广播 → 归还槽位"扩为 **"摘修正器 → 退订内联触发行 → 撤时间条目 → `Expiring` → 广播 → 归还槽位"**。"状态在，行为就在"的对偶是"**状态走了，行为先走**" |
| 7 | **内联触发行不存句柄**：实例 MUST NOT 持触发行句柄（D3-7 v2 纪律：订阅句柄归注册表）⇒ 退订口径 = **按实例锚点级联**（`UnregisterTriggerRowsBySource(CascadeAnchor)`） | 登记 `Source` 用锚点 ⇒ 一次摘净且不误伤别条实例（与第 1 条同源）。`TcsStateOps_Trigger.cpp` 是 TcsState 内**唯一** include `TcsEffectSubsystem.h` 处（与属性解析点同款单点纪律） |
| 8 | **`AttributeCompare`（第三条内置条件）**：比较枚举 `ETcsAttributeComparison` 住 **`TcsAttribute`**（属性值域词汇）；条件 struct 住 `TcsEffect`（与 `HasAllTags` / `Chance` 同处）；`FTcsTriggerContext` 补 **`World`**（**非 `UPROPERTY`** 的裸指针） | **求值器签名拿不到世界**（`FTcsTriggerConditionTest` 只收"条件数据 + 上下文 + 随机值"），而读账本必须先解析世界 ⇒ 世界进上下文（由求值器填入）。**失败面两档**：不可求值（键无效 / 主体无效 / 无世界 / 无账本）⇒ 不通过且**零红字**；**键在账本上不存在** ⇒ 不通过 + `Warning`——`EvaluateCurrent` 对"键不存在"与"值恰为 0"给同一读数（0.0），不区分就会让"键写错"表现成"按 0 比较"、可能与阈值比较后**静默通过**（该红字属拒绝面） |
| 9 | **属性门面查找收敛为单载具**（用户 2026-10-05 拍板）：`UTcsAttributeSubsystem::Resolve(const UWorld*)` 是**唯一查找点**（将来补 `ITcsAttributeAccess` 只换这一处）；两模块各留自己的**白名单薄壳**（`FTcsStateAttributeAccess` / `FTcsEffectAttributeAccess`），**白名单不合并** | 各自的审计面不同（TcsState 侧窄；TcsEffect 侧另需取值口 `EvaluateCurrent` 与只读判据 `HasAttribute`）。**顺手订正一处"注释说谎"**：`FTcsStateAttributeAccess` 头注释原声称允许面含 `EvaluateCurrent`，而该类**从未暴露**它（TcsState 侧零消费者）⇒ 措辞收回 |
| 10 | **链运行态的只读观测面**：`UTcsEffectSubsystem::GetRunSource(Handle)` / `GetRunCausedBy(Handle)`（**非 `UFUNCTION`**，句柄无效或运行态已释放时返回默认构造，不 ensure） | 两个新字段若无读数就是"只写不读"（原始版的教训）⇒ 给出观测面：宿主装置与将来的 Explain 面板读它；**脚本可达面归 R8 Explain 轮**（故不加 `UFUNCTION`，不动 `effect-interpreter` 的反射面规格） |
| 11 | **溯源树（血缘遍历 / Explain / 按血缘回放）归 R8**，本轮只落"链运行态的第一段边" | 台账新增一行登记它（含原始版 AbilityKit 借鉴调研的三阶段路线：A 只维护父子链接 / B explain 窗口 / C Snapshot 回放；建议的新能力名 `add-source-lineage`）。**判据**：原始版的教训是"边不可读就等于没有"——本轮先把边做成**可读**，成树与可视化等真实消费者（R8） |
| 12 | **行为 Fragment 的订阅承接形态**（6b）：**每兴趣 Tag 一条订阅 + 共享 `UTcsStateBehaviorHandler`**（`UPROPERTY` 由门面强持有、对门面弱引用）；**MUST NOT** 每实例各订一次 | 总线 `Subscribe` 只收 `UTcsEventHandler*`、派发只传 `(EventTag, Payload)` ⇒ 回调**无法自辨身份**（拿不到订阅句柄）⇒ 每实例各订一次收益为零、成本为正（订阅表随实例数膨胀、退订易漏）。**先例 = `FTcsChainEventWaitRegistry`**（同款"每 Tag 一条 + 计数配对"）。**一条实测事实**：总线订阅表对 Handler 持**弱引用** ⇒ 不加 `UPROPERTY` 强持有会被 GC 掉、订阅**静默失效**（无红字、无日志） |
| 13 | **状态载荷自筛 + 泛型主体匹配**（6b，`TRIG-6` 甲案）：补**首个状态**载荷读取器（`Caster` = 有效 `Instigator` 否则新增字段 `Unit`；`Subject` = `FTcsStateHandle`）；载荷信息与触发行实例各持反射 `FInstancedStruct Subject`，**两侧均有效**时须精确 `UScriptStruct` 一致 + `CompareScriptStruct(A, B, 0)` 值相等才命中；**未绑定的全局行与外部无 `Subject` 事件保留原 Tag 路由** | `TcsEffect` MUST NOT 反向依赖 `TcsState`（铁律）⇒ 泛型主体装在**反射 `FInstancedStruct`** 里、匹配用"脚本结构同一性 + 反射值相等"，**不引入对上层类型的依赖**。**行为 Fragment 侧的对应语义**：只对状态载荷按单参数 `GetPtr<FTcsStateEventPayload>()` + `Handle` 相等自筛；**非状态载荷（含自定义 reader 给出 `Subject` 者）仍按兴趣向全部存活实例扇出**（`Subject` 只约束 effect-trigger 的绑定行）。**验收读数** = 23g **恢复"先主后次"顺序**后仍 `+25.000 / 计数增量 1`（6a 首轮 `30 / 2` 正是该顺序的症状）⇒ 不再依赖夹具顺序规避 |
| 14 | **身份修复（6b，裁定 8 甲案）**：`FTcsStateRegistry` 的分配代际改由**出线（模块级）进程唯一发号器**发出——每次分配取**不复用的正奇数**、释放 `+1` 置偶数、**世界拆解不复位**、`int32` 正值空间耗尽 `Fatal`（不回绕）；句柄**仍是** `(Index, Generation)` 两反射字段，API 不变 | **缺陷**：原实现**各桶各自从代际 1 起发** ⇒ 两个单位的**首个**实例都得到 `{Index=0, Generation=1}`，而 `GetState(Handle)` **扫描所有桶**且句柄里**没有单位段** ⇒ 拿 A 单位首实例的句柄会命中 B 单位的首实例（错单位 / 错移除 / 事件误路由）。**被否乙案**（句柄加 `Unit` 段）需迁移反射面与全部消费者 ⇒ `Unit` 本轮只加到**事件载荷**上。**读数** = 23j `旧句柄0/57拒绝`（同 PIE 内）+ 23l 第二轮 `旧句柄拒绝=是`（跨世界不复位） |
| 15 | **`STAT-5` 有限重入守卫的宽度**（6b）：`UTcsStateSubsystem::InRemovalBroadcastHandles` **只**在**真正的移除广播窗口**成对标记 / 解除（进入广播前加入、广播返回后移除）⇒ 递归 `Remove` 同句柄返回 `false` 静默、不重复广播与释放。**MUST NOT** 用"`Phase == ESP_Expiring`"当"正在移除"的判据 | `Expiring` **不是**"正在移除"的同义词——`RefreshStacked` 在刷新期间**也临时用 `Expiring`**（`TcsStateOps_Stack.cpp:235–249`）⇒ 按阶段判会**静默拒绝**合法的 `Refreshed` 自移除（最坏的一类错：不报错、只少做事）。**同批闭合**六处"广播 / 副作用后仍读旧指针"（`Apply` / `RefreshStacked` / `EPR_Immediate` / `PushPeriod` / `TcsStepApplyState` / `TcsStepModifyAttribute`）⇒ 统一"**副作用前取值快照、副作用后按句柄重查**"。**边界**：本条**只**承诺"递归 `Remove` 同句柄"这一支；终止广播内**同定义重施**那一支未修（台账 `STAT-9`） |

### 12.11 Task 7 落地口径（2026-10-05，R5；端到端竖切与首批真内容资产）

| # | 口径 | 依据 / 后果 |
|---|---|---|
| 1 | **可复现性判据 MUST NOT 落在区段字节上**：判据改为"**复现性摘要行逐字相同 + 各检查结论相同**"，摘要只留不变量（`Decoupled=1` / `PerOK=1` 一类） | 两轮 `State.Run` 区段 **85 vs 84 行** / **13,726 vs 13,620 字节**，逐行归一化求差集得**唯一**差异 = `FTcsEffectStepExecutorRegistry: 步骤执行器登记表已解析（共 10 类）`（只第一轮出现）。真因 = **进程内一次性惰性初始化** ⇒ 与 `FTcsSourceHandle.Id`（进程内单调计数器，实测 `2/3` → `25/26` → `48/49`）同族、**按构造不可复现**。**掩蔽坑**：宽 pattern 会同时命中 Damage 侧的 `FTcsFlowStepExecutorRegistry`（`L3387`）⇒ grep MUST 带模块前缀 |
| 2 | **"现象只在重建二进制后消失"本身就是陈旧 DLL 的判据**：取证期 MUST NOT 用 Live Coding | S8 / S14 的残留旧算术**只在重编后**消失；Live Coding 能给"编译通过"的假象而**不更新交付二进制**。故本轮口径 = **先构建、后取证**（先关编辑器跑完整双配置 UBT，再进 PIE 取数） |
| 3 | **跨快照的行号 MUST 按内容重定位、MUST NOT 按偏移量推算** | 证据 §1.1/§1.2 的"日志行"列曾**整体偏移一行**——起因是从作废快照按**区段端点做算术偏移**重映射，而两份快照的区段基址不同。修法 = 全部按**内容正则**重定位（`检查 S\d+` / `检查 [a-g]`，42 处引用逐条复核） |
| 4 | **"检查编号 ↔ 标题"对照表 MUST 从日志行逐字提取**（`检查 S\d+：`） | 一份交付伴随物（提交信息）曾按 `plan.md` Task 7 Step 2 的**措辞**与既往轮次的编号**习惯**推断该对照表 ⇒ 11/15 条标题与实测不符，且其中一条声称的读数**本轮根本不存在**。**判据**：`plan.md` / `design.md` / 既往提交信息**都不是**合法来源——**计划描述"要验什么"，不等于"实测标题是什么"**。真值以证据（标题逐字取自日志、四列齐备）为准 |
| 5 | **周期面判据 MUST 是下界、MUST NOT 是等式** | S11 在 0.60s 拍采样得 `PeriodicDelta=1`，到 S12（2.60s）累计已 **3** 次，同一物理量在两个采样点读数不同（tick ≈ 0.65 / 1.31 / 1.98s）⇒ 判据 = `PeriodicDelta >= 1` |
| 6 | **行为链落点 = `Attribute.Attack`（−5，常驻）；状态修正器模板保持 `Attribute.Armor`（+Y，到期摘净）**（用户拍板甲案） | 判据 = 状态修正器挂 `Instance.CascadeAnchor`（到期按锚点摘净）、链条目挂 `Context.RunSource`（无人回收）⇒ **两读数互为对照、各自可判**（S14 `Armor` 复原 `5.000` / S15 `Attack` 停在 `25.000`）。**否决乙案**（把链改到会被状态到期一起回退的属性上）——那会让验收变成对实现的**追认**（既有场景逐字不改即已满足） |
| 7 | **`AttrModDef` 根落地（10 → 11）——"触发条件型"条目第一次真的被触发** | 本条要求的**真内容资产**修正器模板使"资产身份必须有 tag 载体"成立 ⇒ `ATTR-1` 原判"零解析消费者 ⇒ 不开根"的**前提消失**。落法 = 按"一角色一根"定根 + 定义库**第四条按类发现路径**（`DiscoverAttrModDefs` / `ResolveAttrModDef`）+ 就绪行第四计数。**残留边界（如实）**：`ResolveAttrModDef` 运行期调用者**为零**（物化仍走 `ModifierRows` 资产直引用）；该根下只有 `Check` 形态一个词 |
| 8 | **首个状态载荷读取器与内联触发行接线在端到端路径上的验证** = S13（按锚点退订）；**链挂条目常驻** = S15 ⇒ 台账 `CHAIN-7` | 见 §12.10 第 6/7 条（实现口径）与证据 §1.2、§6 边界 8 |

### 12.12 R5 落地总账（2026-10-05 收束）

**八个 Task 全部完成**，逐 Task 的落地口径 = §12.5~§12.11。本轮的**证据面**为 **七份**：`EVID-2026-10-04-tcs-state-def-asset`（Task 1）/ `-state-instance-lifecycle`（Task 2）/ `-state-param-snapshot-and-level-sources`（Task 3）/ `-state-modifier-materialization`（Task 4）/ `EVID-2026-10-05-state-stacking-policies`（Task 5）/ `-state-chain-primitives` + `-state-behavior-fragments`（Task 6 6a/6b）/ **`EVID-2026-10-05-state-layer-pie`（Task 7 端到端）**。

**本轮落地面（按 §12.1 逐条兑现）**：§2 Def 形状与资产族、§3.1 实例与 per-unit 桶、§3.2 五轴堆叠、§3.3 Duration/Period、§3.5 四型等级源与读口、§3.6 参数快照、§4 门面、§5 生命周期 + **修正器物化** + **行为 Fragment 订阅挂接**、§6 `ApplyState` 步骤与内联触发行。**一处扩张**：`AttrModDef` 根（10 → 11）与定义库第四发现路径——它不是原计划条款，而是**验收要求真内容资产**倒逼出的实现（§12.11 第 7 条）。

**本轮不落面**（逐条归属见 §12.2）：§3.4 关系表检查器与级联重评、§3.7 就绪状态机、技能参数行分派、网络操作复制。

**如实边界（二十条，逐条见证据 §6）**：单机/单世界/单 PIE 进程；Shipping **只验编译、从未运行**；`StateLevel*` 只验**数组型**；`LevelProvider` 读口本轮未被消费；内容资产 `Fragments` 留空（D2 口径）；`PeriodRefresh` 只验 `Keep`；`ExtendDuration` 上界未定；`STAT-9` 刻意未修；`ResolveAttrModDef` 零调用者；`AttrModDef` 根下词薄；模板操作数配等级源未验；链挂条目常驻（`CHAIN-7`）；`ModifyAttribute` 不等同 Instant（`CHAIN-8`）；行为刷新路径；空片段 Warning；`Expired` 行为退订；全局触发行兼容；typed 自定义载荷；`Interests` 层级；脚本可达为零；递归终止性。

**收束门禁**：`openspec validate --all --strict --no-interactive` = **34 passed / 0 failed**；`openspec/changes/` **零活动提案**——`verify-state-layer-e2e` **已于 2026-10-06 归档**为 `changes/archive/2026-10-06-verify-state-layer-e2e/`（**归档日 = 10-06 机器日期，非计划预写的 10-05，已就地留痕**）；归档后能力数 33 → **34**。计划状态 = **已完成**；R5.5 批次表移交**保持 `ACTIVE`** 承载（同 `PLN-R4` 先例）。
