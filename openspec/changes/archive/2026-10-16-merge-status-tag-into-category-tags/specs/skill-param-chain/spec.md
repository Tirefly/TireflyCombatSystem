## MODIFIED Requirements

### Requirement: 条目选择器（外部施加的作用域）

`TcsSkill` MUST 提供 `FTcsSkillEntrySelector`（**名字 MUST 带 `Skill` 段**——它只服务技能账本条目；"看起来通用"的名字会在第二个域真出现时逼出改名，那时改动面是整个公开面），用于表达"外部施加的修正器作用到**哪些已学条目**"：

- 形状 MUST 为 `{EMode: ETcsSkillEntrySelectorMode, TArray<FGameplayTag> DefTagFilter, FGameplayTagQuery ByCategoryTags, FInstancedStruct CustomFragment}`——**本字段名 MUST 为 `ByCategoryTags`，MUST NOT 复用它要查询的那个容器名 `CategoryTags`**（2026-10-15 改名）：本字段是**查询**（表达"我要匹配谁"），而 Def 侧的 `CategoryTags` 是**容器**（声明"我是什么"）——同名不同型且语义方向相反，属 `DEC-2026-10-13-relation-row-shape` §4.5 记过的同类静默陷阱（词形相近而方向相反）——**每一档都有字段载体**，MUST NOT 出现"枚举有档、字段缺失"；
- 档位 MUST 恰好四档：`ESS_All`（无需筛选）/ **`ESS_ByDefTag`**（读 `DefTagFilter`，**层级匹配**——填父 tag 命中其全部子技能）/ **`ESS_ByCategoryTags`**（读 `ByCategoryTags`，**预留空壳**）/ `ESS_Custom`（读 `CustomFragment`）；
- **`ByTag` MUST NOT 作为档名**（2026-10-09 用户裁定改名）：它读的是条目的 `DefTag`（内容身份），而"Tag"一词在 TCS 里同时指属性词/状态词/事件词/参数键等**七八种**东西 ⇒ 档名 MUST 与字段名 `DefTag` 对齐，读代码即可知"按哪个 tag"；
- **原 `ById` 档 MUST NOT 存在**（2026-10-09 用户裁定删除，**判据三条**）：① **它不可被作者配置**——`FTcsSkillEntryHandle` 是**运行时产物**（桶内槽位下标 + 进程内发号代际），编辑器里无法预先填出合法值 ⇒ 对"以配置为入口的外部施加"而言**等于没有这一档**；② **它的"Id"是历史遗留**——所有 Def 资产身份原为 `FName DefId`，2026-09-22 起**已全量迁移到 `GameplayTag`（`DefTag`）** ⇒ "按 Id 筛"的正当形态就是**按 tag 筛**，已被 `ByDefTag` 覆盖（同一份内容身份不需要两档）；③ 留一个**带代际的句柄过滤位**会诱导后人用它做长期引用的精确命中，而代际会让跨帧引用自然失效（释放即 +1 置偶）⇒ 形状**看起来能用、实际易碎**；
- **`ESS_ByCategoryTags` 是本轮唯一新增的"预留空壳"档**（承接上述删除的位置）：其目标形态 = 按定义上的**类别标识容器**（`FGameplayTagContainer`，如一个 Buff 同时具"火属性伤害 + 异常状态"，一个技能同时具"左手/右手/双手释放（三选一）+ 投掷类 + 引导类"）筛选，规则**倾向复用引擎 `FGameplayTagQuery`**（`HasAll` / `HasAny` 等表达式，自带编辑器定制）。**本轮 MUST 只留空壳、MUST NOT 落机制**——判据三条：① ~~**载体不存在**（`StateDef` 侧的类别标识容器字段尚未新增）~~ → **★ 2026-10-15 订正：载体已落地**（变更 `merge-status-tag-into-category-tags` 把 `StatusTag` 并入 `FTcsStateDefBase.CategoryTags`）⇒ 该阻塞**已消失**，本档只剩"规则未定"与"落地时机"两条；② **规则与档名都未拍板**（用户明示"后续改名为 `ByCategoryTag` 或 `ByCategoryTags`，**还要讨论**"）；③ **可能另立模块**（GameplayTag 筛选机制在 TCS 里将是重要角色、**甚至可能单独开 `TcsGameplayTag` 模块**）⇒ 现在写进 `TcsSkill` 内部将来极可能要搬家。**台账登记 = `STAT-10`**；
- **两个未实现档（`ByCategoryTags` / `Custom`）的行为 MUST 同款**：被命中时 MUST **具名报出"该档未实现"**且**不施加任何修正**（`MUST NOT` 静默当 `All`、`MUST NOT` 静默无操作——静默会让宿主以为已施加/已筛选）；
- 选择器**只服务外部施加**：定义侧 `ParamChainRows` MUST NOT 接受它。

#### Scenario: 四档各有字段载体

- **WHEN** 检查 `EMode` 的四个枚举值与 `FTcsSkillEntrySelector` 的字段表
- **THEN** `All` 无需筛选、`ByDefTag` 读 `DefTagFilter`、`ByCategoryTags` 读 `ByCategoryTags`、`Custom` 读 `CustomFragment`——四档都有对应字段；`EMode` 类型是**本能力自有的** `ETcsSkillEntrySelectorMode`，且**不存在** `ById` 档与任何句柄过滤字段（`TArray<FTcsSkillEntryHandle>` 一类）

#### Scenario: ByDefTag 支持完整与层级两种粒度

- **WHEN** 分别以**完整 `DefTag`**（如 `SkillDef.Check.ParamChainCompete`）与**父 tag**（如 `SkillDef.Check`）作 `ByDefTag` 筛选
- **THEN** 前者只命中那一条（其余条目的槽位不变），后者命中其全部子技能条目——**两次筛选的命中条数不同**（读数可分辨）

#### Scenario: ByCategoryTags 档具名报出且零修正被施加（预留空壳）

- **WHEN** 以 `EMode = ByCategoryTags` 并给出一个"若被误实现就会命中"的查询（如 `HasAny [SkillDef.Check]`）调用外部施加入口
- **THEN** 留**具名**未实现提示（提示里带档名），且**零修正被施加**——读数 MUST 断言"**没有条目的 `NumericParamModInstances` 被改动**"，**MUST NOT** 只查"有 Warning"（warning 照样可能发；只查它无法区分"报了未实现且没施加"与"报了未实现却仍然施加了"）

#### Scenario: Custom 档具名报出而未实现

- **WHEN** 以 `EMode = Custom` 调用外部施加入口
- **THEN** 留下**具名**未实现提示，且**零修正被施加**（判据同 `ByCategoryTags`：断言槽位未被改动）
