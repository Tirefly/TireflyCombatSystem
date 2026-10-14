# skill-registry Specification

## Purpose
定义 M5 技能层的**已学技能账本**：per-entity 条目记录与代际句柄、授予 / 撤销 / 按来源级联、账本层参数读取面（生效等级与参数双表）、技能定义的**逐世界登记口**，以及门禁第一道「实体可操作」的判据来源与降级语义——让"某个实体会哪些技能"成为可查询、可增删、可判定能否施放的运行事实，而不是只有一份资产形状。

## Requirements

### Requirement: 已学技能账本（per-entity 条目与代际句柄）

`TcsSkill` MUST 提供**已学技能账本**——把"某个实体会哪些技能"变成**可查询、可增删**的 per-entity 登记表，且它是技能层唯一的"学习事实"来源（`SPEC-04-skill` §3.1 的 `FLearnedSkillEntry`）：

- 账本 MUST 以 `UTcsSkillSubsystem`（`UWorldSubsystem`）为门面，持 **per-entity 桶**（一个实体一个句柄空间）；桶 MUST 经**共享层的统一桶模板**（`TTcsEntityRuntimeBucket`，见 `entity-runtime-mechanism` 能力）承载——其内部载体 = `TTcsInstancePool`，**且 MUST 注入进程级代际发号器**（`FTcsStateGenerationIssuer`：每实体一个句柄空间、槽位索引在实体内定位，而代际由**进程内**发号器分配不复用的奇数，释放 +1 置偶数）。**★ 本句订正于 2026-10-14**：原文写"MUST NOT 复用 `TTcsInstancePool`（该池是『一池 = 一个句柄空间』，与 per-entity 桶的形状不符）"——该禁令**依据不足**：池是**每实体各持一个实例**（不是全局一池），形状本就符合；真约束是"代际 MUST 进程唯一"，故已改为"用池 + 注入进程级发号器"（判据见 `instance-handle-pool` 能力的「实例池的分配释放与解析」需求）；
- 条目句柄类型 MUST 为 `FTcsSkillEntryHandle{int32 Index, int32 Generation}` 并 **MUST 反射**（`USTRUCT(BlueprintType)`）——展平两字段（**不内嵌模板句柄**：模板类型无法作 `UPROPERTY`，脚本层往返会得到空壳 ⇒ 读不到值、传回全零 ⇒ 代际失配），`Index` 取 `int32`（UHT 不支持 `uint32` 作属性类型；`-1` 与池的 `0xFFFFFFFF` 位模式相同）；**有效性判据 = `Generation` 非 0**，MUST NOT 判"下标非 0"（首个槽位的 `Index` 就是 0）；
- 条目记录类型 MUST 为 `FTcsLearnedSkillEntry`，字段**恰好**为八项 `{FGameplayTag DefTag, FTcsSkillEntryHandle Handle, FTcsCombatEntityHandle Entity, FTcsSourceHandle LearnSource, int32 Level, TArray<FTcsCastRunHandle> RunHandles, TArray<FTcsNumericParamModInstance> NumericParamModInstances, TArray<FTcsCooldownTrackState> CooldownTracks}`：
  - **非反射结构体**（无 `USTRUCT`）——它是桶内池化纯数据、本轮零过网与零脚本消费面（同 `FTcsStateInstance` 的处置与理由）；脚本/蓝图侧的读写面另属 `SCRIPT-10`（触发条件型），**MUST NOT** 在本轮为此反射化；
  - `DefTag` = **权威内容身份**（`EffectiveDefId` 概念已整体移除——D5-9）；`Handle` = **桶内句柄**（条目自身身份；`Index` / `Generation` 的语义由所属桶定义）；`Entity` = **条目所属实体**（门面按句柄取条目时句柄里没有实体段 ⇒ 自持后是 O(1) 定位，否则只能遍历所有桶找句柄；且"这个技能在谁身上"本就是条目身份的一部分，非重复真相——同 `FTcsStateInstance::Entity` 的理由）；`LearnSource` = 学习来源（级联撤销锚点）；`Level` = **持久等级**（运行中升级不追溯，快照时点在激活期）；`RunHandles` = 在飞施法运行句柄集（**激活期回填、run 终结时摘除**——回填与摘除 MUST 成对，否则账本会永久持有已归还的句柄，表现为"技能显示在飞但实际放完了"；写入者 = 施法运行态能力）；
  - **`Entity` 缺失时的后果（MUST 记录，防后人误删）**：施法终结路径（打断 / 顶替）只拿得到**运行句柄**，需经 `Entity` 才能定位条目并摘除 `RunHandles`、按来源回收；缺它会让终结路径要么无法定位，要么退化为全桶扫描；
  - **`NumericParamModInstances` 字段已回补（2026-10-09，Task 4 按上条裁定自己的约定回补）**：本字段原被裁定"归参数链轮"而**从条目中移除**（元素类型 `FTcsNumericParamModifier` / `FTcsNumericParamModInstance` 当时不存在 ⇒ 写进来即**未声明标识符**）。Task 4 交付后，字段随之下落为**第七条** `TArray<FTcsNumericParamModInstance> NumericParamModInstances`——它是参数账本的**槽位容器**（形如 M2 `FTcsAttributeInstance::AttrModInstances`），也是"激活期物化 + 外部施加 + 按来源级联摘除"三者的共同落点（缺它则三者**无字段可写**）；
    - **元素类型是账本侧的纯 C++ `FTcsNumericParamModInstance`（无 `USTRUCT`、无对象引用）** ⇒ 本条**依然**满足下方 GC 纪律（"账本条目 MUST NOT 持任何 `UObject` 引用"），门面 `AddReferencedObjects` **MUST NOT** 为此新增遍历。**判据**：若有人把这里换成定义侧的 `FTcsNumericParamModifier`（含 `FTcsParamValue` ⇒ 可持 `TScriptInterface`），则账本容器（`TArray`、非 `UPROPERTY`、元素非反射）会 **GC 不可见** ⇒ 那些对象被静默回收（症状 = 参数链里的源变空引用，不崩溃）；
  - **`CooldownTracks` 字段已回补（2026-10-11，`R6.5-1` Task 0；MUST 记录理由以免后人误读为"当初漏写"）**：本字段原被明确裁定"归 `R6.5-a`"，**本条当时的原文即"MUST NOT 声明任何冷却轨道状态字段"**，理由是"其元素类型仍不存在，而 `TArray<T>` 成员要求 `T` 可见 ⇒ 提前声明即**未声明标识符**（硬编译错误）"。`R6.5-1` Task 0 交付 `FTcsCooldownTrackState`（`Skill/TcsCooldownState.h`）、Task 1 交付其消费者（`TcsCooldownOps`）⇒ **类型与消费者同时到位**，字段随之下落为**第八条** `TArray<FTcsCooldownTrackState> CooldownTracks`。**⇒ 与上一条同款：不是"当初漏了"，是"当初不能写"。**
    - **冷却的真值住本字段**（`D5-15` 明文："**真值在 Entry，UI 轮询**"）——**不住施法运行态**。判据：冷却是**条目级**的持久状态（"这个技能还要等多久"跨施法存在），而 run 是**一次施法**的运行态、随终结归还池槽位 ⇒ 冷却若挂 run 上会随施法结束一起消失；
    - **与 Def 侧 `FTcsCooldownPolicy::Tracks` 一一对应、同序**（同序是硬契约：查询口按**下标**寻址 —— `GetCooldownRemaining(Entity, Entry, TrackIndex)`）；
    - **空数组 = 该条目此刻没有任何冷却在跑**，与"该技能配了空 `Tracks`（永远可放）"是**两件事**（前者是运行态此刻为空、后者是定义上不存在轨道）⇒ **MUST NOT** 用"本数组是否为空"反推"这个技能有没有冷却"，那要读 Def；
    - **元素类型 `FTcsCooldownTrackState` 是纯 C++ 值类型（无 `USTRUCT`、无对象引用）** ⇒ 本条**依然**满足下方 GC 纪律，门面 `AddReferencedObjects` **MUST NOT** 为此新增遍历（同 `NumericParamModInstances` 的判据）；
    - **实体级组槽不住本字段**：`GroupTag` 有效的轨道（公共冷却）共用一个**实体级**槽（住门面），因为公共冷却的作用域是**实体**而非条目 —— 若按条目各存一份，同组两个条目会各起各的线，那就**不是 GCD 了**（症状：放完 A 之后 B 立刻能放）；
- 门面 MUST 提供 `GrantSkill(FTcsCombatEntityHandle Entity, FGameplayTag DefTag, FTcsSourceHandle Source)` / `RevokeSkill(...)`，并 MUST 支持**按来源级联撤销**（摘该来源在该实体的全部条目）；`LearnSource` 语义同状态侧的施加方来源句柄（**不是**状态侧的 `CascadeAnchor`——账本条目不挂修正器与触发行，无独立级联锚点需求，MUST NOT 照抄状态侧的两个句柄）；
- **授予前置门禁**：`DefTag` 在本世界**未登记**时 MUST **拒绝授予**并为 `Warning`（**不 ensure**——内容缺口不是契约违规）；MUST NOT 建出一个取不到定义内容的条目（那会让参数读取面全部落空，而失败面表现为"读数为 0"而非"没学到"，属静默错误）；
- 门面 MUST 提供 `DoesSupportWorldType`（仅游戏世界实例化）、`Initialize` / `Deinitialize`，且在 `Deinitialize` MUST 清空**登记表 / 账本注册表 / 注入的接口**三者（跨世界确定性清理——旧句柄一律凭代际失配失效，同状态门面的清理口径）；
- **GC 纪律**：本类持**非 `UPROPERTY` 容器**且元素含对象引用时 MUST 覆写 `AddReferencedObjects` 补引用；注入的 `TScriptInterface` MUST 以 `UPROPERTY` 持有；判据是"**容器是否 GC 可见**"，与值语义/指针语义无关。

#### Scenario: 授予后可查询、撤销后归零

- **WHEN** 对某实体 `GrantSkill` 一个已登记定义的技能，随后 `RevokeSkill` 该条目
- **THEN** 授予后该实体的账本条目数为 1、按句柄可解析到条目（`DefTag` / `Level` / `LearnSource` 与授予时一致）；撤销后条目数为 0，且该句柄凭代际失配被拒绝（不 ensure）

#### Scenario: 按来源级联恰摘该来源的条目

- **WHEN** 同一实体先后以**两个不同** `Source` 授予两个技能，再按其中一个来源级联撤销
- **THEN** 恰摘掉该来源的那 1 条，另一来源的条目**原样保留**（读数 = 条目数 2 → 1，且剩余条目的 `DefTag` = 另一个）

#### Scenario: 未登记定义即拒绝授予

- **WHEN** 对一个 `DefTag` **未登记进本世界技能门面**的技能调用 `GrantSkill`
- **THEN** 返回拒绝并留 **Warning**（不 ensure），账本条目数**不变**——MUST NOT 建出取不到定义内容的条目

#### Scenario: 旧句柄不能命中新条目

- **WHEN** 撤销一条后重新授予（槽位被复用），再用**撤销前**拿到的旧句柄查询
- **THEN** 旧句柄被拒绝（代际失配）——`Index` 可以相同，但 `Generation` 必须不同；MUST NOT 解析到新条目

#### Scenario: 在飞句柄随运行态终结而摘除

- **WHEN** 某条目经激活产生 run（`RunHandles` 回填该句柄），随后该 run 被顶替终结
- **THEN** `RunHandles` 中该句柄被摘除（条目仍在册、`RunHandles` 恢复为空）；**MUST NOT** 残留已归还池槽位的句柄

#### Scenario: 冷却轨道状态住条目且随起冷下落

- **WHEN** 某条目配了非空 `Tracks`，起冷后检查该条目的 `CooldownTracks`
- **THEN** 其长度与 Def 侧 `Tracks` **一致且同序**，各项的 `Total` 为**起冷瞬间冻结**的时长（CDR 已作用）、`EndTimeSeconds` 为到期绝对时刻、`ExpiryEntry` 为有效句柄；条目**仍在册**（冷却不改变学习事实）

### Requirement: 账本参数读取面（等级与参数键求值）

技能门面 MUST 提供**账本层参数读取面**——它是参数链轮（`STAT-1` 的 M5 半）的物质基础，且**级别语义 MUST 与设计一致**：

- MUST 提供 `GetLevel(FTcsSkillEntryHandle) -> int32`：返回**生效等级** `EffectiveLevel`，语义为 `clamp(0, LevelBase + Σ参数账本 Level 键修正)`（`LevelBase` 取自该条目 `DefTag` 对应的定义内容；`Σ` 为 `Level` 键上的修正器折叠结果）；
- MUST 提供 `GetNumericParam(FTcsSkillEntryHandle, FGameplayTag Key, double& OutValue) -> bool` 与 `IsSwitchSet(FTcsSkillEntryHandle, FGameplayTag Key, bool& OutValue) -> bool`：分别读数值参数表与布尔开关表，**未命中 MUST 落 miss**（返回 false，MUST NOT 静默返回 0 或 `false` 当命中）；
- **参数键的归属**：MUST 落 `TcsStateParam` 根（该根的 `State` 取**广义**——含技能激活运行态；`gameplay-tag-governance` 明文"技能运行态归状态"）⇒ **MUST NOT 因"技能不住 `TcsState` 模块"另立根**；
- **本轮的分工边界**：`Level` 修正键的**写入面**由参数链轮提供 ⇒ 本能力先落**求值函数**（形参接收"带 `Level` 键的修正器列表"），接入点在参数链轮；**MUST NOT** 依赖条目记录自持修正链字段（该字段在本能力落地时尚不存在，见上一条需求）；
- 数值读取 MUST 走**规范值**（账本内永远规范值，值约定转换发生在写入点）。

#### Scenario: 等级按 Level 键修正求和并钳下界

- **WHEN** 某条目的 `DefTag` 定义 `LevelBase = 1`，且以"带 `Level` 键的修正器列表"（合计 +2）调用求值
- **THEN** `GetLevel` 返回 3（`clamp(0, 1 + 2)`）；当修正合计使和小于 0 时返回 0（钳下界，MUST NOT 返回负数）

#### Scenario: 参数键未命中落 miss 而非静默取 0

- **WHEN** 以一个**不在**该条目参数表里的键调用 `GetNumericParam`
- **THEN** 返回 false（miss），出参内容未定义、调用方不得使用——MUST NOT 把它当作"命中且值为 0"

#### Scenario: 布尔开关与数值参数分属两表

- **WHEN** 某键只配在布尔开关表（`BoolSwitches`）、另一键只配在数值参数表（`Params`）
- **THEN** `IsSwitchSet` 对前者命中、`GetNumericParam` 对前者落 miss；对后者相反——两表 MUST NOT 互相兜底

### Requirement: 技能定义登记口（门面侧）

技能门面 MUST 持**本世界的技能定义登记表**，并以**登记口**接收定义内容——这是账本取得定义内容的**唯一合法通路**（定义缓存住 `UTcsDefinitionSubsystem`，而 `TcsSkill` **MUST NOT** 反向依赖 `TcsIntegration`，反查即成环）：

- MUST 提供 `RegisterSkillDef(FGameplayTag DefTag, const FTcsSkillDefData& Def) -> bool`：**身份由调用方显式传入**（身份归资产、内容归数据 struct——从内容里读身份会迫使策划在同一个资产里把同一个 tag 手填两遍，即双真相）；登记表 MUST **自持副本**（`TUniquePtr` 持有使解析返回的指针地址稳定）；
- 拒绝面 = `DefTag` 无效 / 同 id 重复登记（**不静默覆写**——口径同链与触发行定义登记）；失败 MUST 记 **Error** 并返回 false（调用方是定义库，失败在上游已被拦）；
- MUST 提供 `UnregisterSkillDef` / `GetRegisteredSkillDef(FGameplayTag) -> const FTcsSkillDefData*` / `GetRegisteredSkillDefCount()`；未登记返回 `nullptr`（**正常查询路径，不 ensure**）；解析返回**裸 struct 指针**（UHT 不支持 struct 指针作反射返回——同 `GetRegisteredStateDef` 的取舍）；
- **依赖方向 MUST 单向**：定义库（`TcsIntegration`）写进门面，门面 **MUST NOT 反查**定义库（同 `UTcsStateSubsystem` 的登记口纪律）；**MUST NOT** 为此给 `TcsSkill` 增加对 `TcsIntegration` 的依赖。

#### Scenario: 登记后可按 tag 解析

- **WHEN** 以合法 `DefTag` 与一份定义内容调用 `RegisterSkillDef`
- **THEN** 返回 true，`GetRegisteredSkillDef(DefTag)` 命中且内容与传入副本一致，`GetRegisteredSkillDefCount()` 为 1

#### Scenario: 重复登记被拒且不覆写

- **WHEN** 对同一个 `DefTag` 连续登记两份不同的定义内容
- **THEN** 第二次返回 false 且记 Error，`GetRegisteredSkillDef(DefTag)` 仍解析到**第一份**内容——MUST NOT 静默覆写

#### Scenario: 未登记返回空且不 ensure

- **WHEN** 解析一个从未登记过的 `DefTag`
- **THEN** 返回 nullptr，无 ensure、无日志噪音（正常查询路径）

### Requirement: 实体可操作性门禁（技能侧消费面）

技能侧 MUST 以注入的实体查询契约判定"门禁第一道：实体可操作"，且**注入面与降级语义 MUST 与既有同族口径一致**：

- 门面 MUST 提供 `SetEntityQuery(TScriptInterface<ITcsEntityQuery>)` 注入点，引用 MUST 以 `UPROPERTY` 持有（GC 安全——实现是 `UObject`）；**未注入是配置状态不是错误**（同 `ITcsParamTableReader` 可空的口径）：此时门禁 MUST 降级为"只判句柄有效"，**MUST NOT** ensure、MUST NOT 拦死；
- 门禁判据 MUST 为 `ITcsEntityQuery::IsEntityReady(FTcsCombatEntityHandle)`，**MUST NOT** 用 `IsAlive` 代替——两者是正交轴（`IsAlive` 答"宿主认为它活着吗"、`IsEntityReady` 答"框架能不能在它身上操作"），`IsAlive` 的 PIE 实现是"映射里还有这个句柄"而非"活着"，拿它当门禁会**双向误判**（死亡触发的被动 / 复活技能被误杀；待销毁尸体被误放）；
- **MUST NOT 反查 `TcsIntegration`**（依赖方向单向——`TcsIntegration` 是终端模块，反查会成环）。

#### Scenario: 未注入时降级而非拦死

- **WHEN** 从未注入实体查询实现时走门禁第一道（实体句柄本身有效）
- **THEN** 门禁**通过**（降级为"只判句柄有效"），无 ensure、无红字——MUST NOT 因"能力未注入"把一切施法拦死

#### Scenario: 注入后按宿主判定拒绝

- **WHEN** 注入了实体查询实现，且宿主对某句柄返回 `IsEntityReady == false`
- **THEN** 门禁第一道**拒绝**（返回具名原因），MUST NOT 继续走后续门禁；同一句柄若 `IsAlive == true` 也 MUST NOT 因此放行——两问彼此独立
