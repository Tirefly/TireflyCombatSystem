## MODIFIED Requirements

### Requirement: 已学技能账本（per-unit 条目与代际句柄）

`TcsSkill` MUST 提供**已学技能账本**——把"某个单位会哪些技能"变成**可查询、可增删**的 per-unit 登记表，且它是技能层唯一的"学习事实"来源（`SPEC-04-skill` §3.1 的 `FLearnedSkillEntry`）：

- 账本 MUST 以 `UTcsSkillSubsystem`（`UWorldSubsystem`）为门面，持 **per-unit 桶**（一个单位一个句柄空间）；桶的存储与全局身份分工 MUST 照 `TcsState` 的既有体例（`FTcsStateRegistry` / `FStateBucket`：**每单位一个句柄空间**，槽位索引在单位内定位、代际由本模块统一的进程内发号器分配**不复用的奇数**，释放 +1 置偶数），**MUST NOT 复用** `TTcsInstancePool`（该池是"一池 = 一个句柄空间"，与 per-unit 桶的形状不符）；
- 条目句柄类型 MUST 为 `FTcsSkillEntryHandle{int32 Index, int32 Generation}` 并 **MUST 反射**（`USTRUCT(BlueprintType)`）——展平两字段（**不内嵌模板句柄**：模板类型无法作 `UPROPERTY`，脚本层往返会得到空壳 ⇒ 读不到值、传回全零 ⇒ 代际失配），`Index` 取 `int32`（UHT 不支持 `uint32` 作属性类型；`-1` 与池的 `0xFFFFFFFF` 位模式相同）；**有效性判据 = `Generation` 非 0**，MUST NOT 判"下标非 0"（首个槽位的 `Index` 就是 0）；
- 条目记录类型 MUST 为 `FTcsLearnedSkillEntry`，字段**恰好**为六项 `{FGameplayTag DefTag, FTcsSkillEntryHandle Handle, FTcsCombatEntityHandle Unit, FTcsSourceHandle LearnSource, int32 Level, TArray<FTcsCastRunHandle> RunHandles}`：
  - **非反射结构体**（无 `USTRUCT`）——它是桶内池化纯数据、本轮零过网与零脚本消费面（同 `FTcsStateInstance` 的处置与理由）；脚本/蓝图侧的读写面另属 `SCRIPT-10`（触发条件型），**MUST NOT** 在本轮为此反射化；
  - `DefTag` = **权威内容身份**（`EffectiveDefId` 概念已整体移除——D5-9）；`Handle` = **桶内句柄**（条目自身身份；`Index` / `Generation` 的语义由所属桶定义）；`Unit` = **条目所属单位**（门面按句柄取条目时句柄里没有单位段 ⇒ 自持后是 O(1) 定位，否则只能遍历所有桶找句柄；且"这个技能在谁身上"本就是条目身份的一部分，非重复真相——同 `FTcsStateInstance::Unit` 的理由）；`LearnSource` = 学习来源（级联撤销锚点）；`Level` = **持久等级**（运行中升级不追溯，快照时点在激活期）；`RunHandles` = 在飞施法运行句柄集（**激活期回填、run 终结时摘除**——回填与摘除 MUST 成对，否则账本会永久持有已归还的句柄，表现为"技能显示在飞但实际放完了"；写入者 = 施法运行态能力）；
  - **`Unit` 缺失时的后果（MUST 记录，防后人误删）**：施法终结路径（打断 / 顶替）只拿得到**运行句柄**，需经 `Unit` 才能定位条目并摘除 `RunHandles`、按来源回收；缺它会让终结路径要么无法定位，要么退化为全桶扫描；
  - **MUST NOT** 声明任何冷却轨道状态字段（归 `R6.5-a`）或参数修正链字段（归参数链轮）——两者的元素类型在本能力落地时**尚不存在**，而 `UPROPERTY TArray<T>` 与 `TArray<T>` 成员均要求元素类型完整/可见 ⇒ 提前声明即**未声明标识符**（硬编译错误），且与"零消费者不预建"纪律一致；
- 门面 MUST 提供 `GrantSkill(FTcsCombatEntityHandle Unit, FGameplayTag DefTag, FTcsSourceHandle Source)` / `RevokeSkill(...)`，并 MUST 支持**按来源级联撤销**（摘该来源在该单位的全部条目）；`LearnSource` 语义同状态侧的施加方来源句柄（**不是**状态侧的 `CascadeAnchor`——账本条目不挂修正器与触发行，无独立级联锚点需求，MUST NOT 照抄状态侧的两个句柄）；
- **授予前置门禁**：`DefTag` 在本世界**未登记**时 MUST **拒绝授予**并为 `Warning`（**不 ensure**——内容缺口不是契约违规）；MUST NOT 建出一个取不到定义内容的条目（那会让参数读取面全部落空，而失败面表现为"读数为 0"而非"没学到"，属静默错误）；
- 门面 MUST 提供 `DoesSupportWorldType`（仅游戏世界实例化）、`Initialize` / `Deinitialize`，且在 `Deinitialize` MUST 清空**登记表 / 账本注册表 / 注入的接口**三者（跨世界确定性清理——旧句柄一律凭代际失配失效，同状态门面的清理口径）；
- **GC 纪律**：本类持**非 `UPROPERTY` 容器**且元素含对象引用时 MUST 覆写 `AddReferencedObjects` 补引用；注入的 `TScriptInterface` MUST 以 `UPROPERTY` 持有；判据是"**容器是否 GC 可见**"，与值语义/指针语义无关。

#### Scenario: 授予后可查询、撤销后归零

- **WHEN** 对某单位 `GrantSkill` 一个已登记定义的技能，随后 `RevokeSkill` 该条目
- **THEN** 授予后该单位的账本条目数为 1、按句柄可解析到条目（`DefTag` / `Level` / `LearnSource` 与授予时一致）；撤销后条目数为 0，且该句柄凭代际失配被拒绝（不 ensure）

#### Scenario: 按来源级联恰摘该来源的条目

- **WHEN** 同一单位先后以**两个不同** `Source` 授予两个技能，再按其中一个来源级联撤销
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
