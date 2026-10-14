## ADDED Requirements

> **术语订正（2026-10-14）**：本文原用「单位 / `Unit`」，该词已退役，现统一为「**实体 / `Entity`**」（见 `DEC-2026-10-14-entity-vocabulary`）。**正文为当时原话，保留不改。**

### Requirement: 冷却策略的数据形状（轨道与时机）

`TcsSkill` MUST 提供冷却策略的**纯数据**形状（零行为——同 `FTcsBoolSwitchRow` 的处置）：

- `UENUM(BlueprintType) ETcsCooldownTiming`（**四档**）：
  `ECT_OnCastStarted`（**默认**：激活成功后立即起）/ `ECT_OnCastCompleted`（施法走完全部时段、
  **或瞬发当刻完成**时起）/ `ECT_OnCastInterrupted`（被打断**或被顶替**时起）/ `ECT_Manual`
  （**框架不自动起**，由宿主在自己的逻辑处调 `StartCooldown`）；
- `USTRUCT(BlueprintType) FTcsCooldownTrack{ double Duration; FGameplayTag GroupTag; }`；
- `USTRUCT(BlueprintType) FTcsCooldownPolicy{ TArray<FTcsCooldownTrack> Tracks; ETcsCooldownTiming Timing; }`。

**各字段的语义与判据（MUST 守死）**：

- **`GroupTag` 无效 = 自身轨、有效 = 单位级组槽**：这就是**真公共冷却（GCD）的纯数据表达**——
  "自己 8 秒不能再放"与"所有技能共享 1 秒公共冷却"形状完全相同，区别只在 `GroupTag`。
  ⇒ 不需要为两者各立一套类型（`D5-15`："Strategy 三档**溶解进轨道**"）。批次表 `:198` 的
  `GroupTag?` 问号**就此关闭**：字段**恒存在**，**MUST NOT** 另加一个"有没有组"的开关
  （无效 tag 已是"没有组"的天然表达，再加开关就是同一件事两个入口）；
- **空 `Tracks` = 永远可放**：这是 `D5-15` "默认无冷却"的数据表达，也是 Strategy 三档溶解之后的
  唯一"无"档 ⇒ **MUST NOT** 再引入一个 `None` 枚举值；
- **`Timing` 是策略级、不是每轨一个**（`D5-15` 明文）：时机回答"这次施法**什么时候**进冷却"，
  而"进冷却"对全部轨道是**同一个瞬间**；每轨各配一个时机只会造出"自身轨已起、组轨还没起"
  这种没有语义的中间态；
- **`Duration` 本轮只落字面量 `double`**：`D5-15` 原话的 `Duration(字面量|Param)` 的 `Param` 形态
  **未落**——它要求本字段变成参数源载体（`FInstancedStruct`），而参数源族在技能侧**尚无消费者**
  （同族 `STAT-11` 的 `属性换算` 至今零实现者）。该形态已入台账（触发条件型）；
- **MUST NOT 声明 `CustomFragment` 字段、MUST NOT 建 `ITcsCooldownTimingFragment` 接口**
  （2026-10-11 用户裁定）：自定义时机 = "**不自动执行，让宿主在自己的确定逻辑处手动调用**"
  ⇒ 那正是 `ECT_Manual` 档。**判据**：时机是"何时做"的问题，而"何时"只有两种表达——
  **能被事件名说出来**，或**只能由代码在具体位置说出来**；**中间没有第三种形态** ⇒ 两种载体都不需要。
  载体存在的意义是"框架要按它替你决定做什么"，而手动档恰恰是"框架什么都不替你决定"。

#### Scenario: 四档各有时机语义且默认档最保守

- **WHEN** 新建一个 `FTcsCooldownPolicy` 且不显式设置 `Timing`
- **THEN** `Timing == ECT_OnCastStarted`、`Tracks` 为空数组；空 `Tracks` 下该技能**恒可放**

#### Scenario: 组轨与自身轨同形、靠 GroupTag 区分

- **WHEN** 检查 `FTcsCooldownTrack` 的字段列表
- **THEN** 它**恰好**含 `Duration` 与 `GroupTag` 两项，**不存在**"是不是组轨"的布尔字段或独立的组轨类型；`GroupTag` 无效即自身轨

#### Scenario: 自定义时机无任何载体

- **WHEN** 检查 `Source/TcsSkill/` 的类型清单与 `FTcsCooldownPolicy` 的字段列表
- **THEN** **不存在** `ITcsCooldownTimingFragment` 类型，也不存在名为 `CustomFragment` 的字段；`ECT_Manual` 档**零载体**（宿主直接调门面入口）

#### Scenario: 两个 Timing 枚举各自独立

- **WHEN** 比较 `ETcsCooldownTiming` 与 `ETcsCostTiming` 的档位集
- **THEN** 两者**不是同一个枚举**、也**不互相"覆盖"**：冷却侧**无** `OnEvent` 档（冷却起因永远在本模块手里）、消耗侧**无** `OnCastInterrupted` 档（判据见 `skill-cost` 的同名需求）

### Requirement: `CooldownPct` 冷却缩减契约键的声明归属

`TcsSkill` MUST 提供原生 GameplayTag `TcsStateParam.CooldownPct`（常量名 `Tag_TcsSkillParam_CooldownPct`，
`Public/Param/TcsSkillParamKeys.h`，**带模块导出宏 `TCSSKILL_API`**），作为**冷却缩减（CDR）的框架契约键**：
挂在该键上的修正**不进普通参数折叠**，而是作为系数作用于该次冷却**各轨的 `Duration`**。

**为什么声明方是 `TcsSkill` 而不是 `TcsState`**（判据三条，与 `TcsStateParam.Level` 住 `TcsState` 是
**同一判据的相反结论**——该对照 MUST 被记录，防后人误以为两处不一致）：

1. **语义归属**：CDR 的公式（"作用于各轨 `Duration`"）写在 `D5-15`、由**技能冷却域**解释；
   状态侧**无此概念**——状态实例没有"冷却"，它的时长语义是"持续多久"（另一种东西）；
2. **依赖方向（决定性，与 `Level` 恰好相反）**：`TcsSkill` 的 `Build.cs` 含 `TcsState`（**已依赖**），
   而 `TcsState` **不依赖** `TcsSkill` ⇒ 声明在 `TcsSkill` **零成环风险**。`Level` 反过来
   （M5 没有自己的等级定义层，只是复用状态侧定的等级语义）故住 `TcsState`。
   **同一个词被多域消费是常规情形**（M2/M5/状态三域共用同一套五带即先例），
   声明方 MUST 取**语义所有者 + 依赖上游**的那一侧；
3. **声明方 = "谁解释那个词"**：宿主只是"往本键上挂修正器"（`ModifierRows` 声明式 /
   `ApplyParamModifiers` 命令式都在宿主手上），它**不解释**这个键。

**为什么 tag 文本仍在 `TcsStateParam` 根下**（与常量名住 `TcsSkill` 不矛盾）：本键是**技能参数表的键**，
而技能 Def 的数值参数表**继承**自 `FTcsStateDefBase::Params` ⇒ 键的词根由**参数表本身**决定
（宿主在同一根下声明自己的内容参数），而**声明方**由上述判据 ①②③ 决定。
两者是**两个独立问题**——`gameplay-tag-governance` 的「同一根共享」正是为"契约词 + 内容词并存"准备的。

**导出宏（跨模块实证）**：`UE_DECLARE_GAMEPLAY_TAG_EXTERN` 展开为**裸 `extern`**（无 `__declspec(dllexport)`）
⇒ 其它模块引用本变量会 `LNK2001`。本键的设计意图正是"供宿主与装置引用" ⇒ MUST 带模块导出宏。

#### Scenario: 契约键由插件原生声明且不需宿主配置

- **WHEN** 检查 `TcsStateParam.CooldownPct` 的声明方与宿主 `Config/DefaultGameplayTags.ini`
- **THEN** 它由**插件模块**原生声明（`TCSSKILL_API` 导出宏）、**不需要**宿主 ini 配置即生效；声明处住 `TcsSkill`（拥有冷却语义，且 `TcsState` 不依赖它 ⇒ 零成环）

#### Scenario: 契约键不进普通参数折叠

- **WHEN** 某条目的参数账本上挂一条 `ParamKey = TcsStateParam.CooldownPct` 的修正
- **THEN** 该修正**不改变**普通参数的折叠结果（它是冷却触发时刻读取的**系数**，不是参数表的值）；`TcsStateParam` 根下的宿主内容参数键仍由宿主 ini 声明
