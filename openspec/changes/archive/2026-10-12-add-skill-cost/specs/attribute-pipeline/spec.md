## ADDED Requirements

### Requirement: 属性基值的非确保读口

`UTcsAttributeSubsystem` MUST 提供**门面级**的基值读口：

```cpp
bool TryGetBaseValue(FTcsCombatEntityHandle Unit, const FGameplayTag& Attribute, double& OutBaseValue) const;
```

- **它回答两个问题、且两个答案天然自洽**：① 该单位是否**持有**该属性；② 若持有，其**基值**是多少。
  返回 `true` 时 `OutBaseValue` 有效；返回 `false` 时出参内容未定义、调用方不得使用。
- **为什么必须是 `TryGet` 双答案形态**（**MUST NOT** 退回纯 `double` 口）：
  `FTcsAttributeStore::GetBaseValue` 对**未持有**返回 `0.0`，与"确实持有且基值为 0"**不可区分**
  —— 而调用方**必须**区分这两件事。若只给一个 `double` 口，调用方要答一个问题就得**连调两次**
  （先问持有、再取值），而两次调用之间**没有任何东西保证两个答案一致**
  ⇒ 那正是"门禁判的与真扣的不一致"这一类 TOCTOU 缺陷的形状。
- **持有性判据 MUST 走实例查询**（`FindInstance` 命中），
  **MUST NOT** 依赖 `GetBaseValue` 的返回值来判断——理由同上（`0.0` 是二义的）。
- **非确保（MUST）**：走**非确保**解析（`ResolveStore`）而非 `GetStore`。
  读取是**正常查询路径**，"单位未注册 / 属性名为空 / 该单位未持有此属性"都**不是契约违规**
  ⇒ MUST NOT `ensure`、MUST NOT 留红字。调用方拿到 `false` 即可如实报出"该单位没有这个属性"，
  而**不应**为此产生红字噪音（那会让常态路径污染"零意外红字"这条验收信号）。
- **`const`（MUST）**：纯读——MUST NOT 改任何状态、MUST NOT 标脏、MUST NOT 广播。
- **MUST NOT 让跨模块消费者直伸存储**：`TcsSkill` 等消费模块 MUST 经本口读基值，
  **MUST NOT** 自行 `ResolveStore` + `FindInstance`。判据 = **门面是模块的对外唯一入口**：
  跨模块直读会让属性存储的形状（今天 `BaseValue` 是 `FTcsAttributeInstance` 的字段）
  变成**跨模块契约**，M2 想换 `BaseValue` 的载体就得同时改两个模块。
- **无反射面（MUST 记）**：本口是为**框架内部消费面**而加的（技能侧 Cost 的扣减路径），
  **MUST NOT** 加 `UFUNCTION` —— 脚本侧的属性读面归 `SCRIPT` 系列（与 `ResolveStore` 同款处置），
  单独给它加宏会造出"属性面里只有它能脚本调"的孤岛。

#### Scenario: 持有且基值可读

- **WHEN** 某单位持有某属性（基值 100），调用 `TryGetBaseValue`
- **THEN** 返回 `true` 且出参为 `100.0`

#### Scenario: 未持有与"基值为 0"可区分

- **WHEN** 对**未持有**该属性的单位调用 `TryGetBaseValue`
- **THEN** 返回 `false`（**MUST NOT** 返回 `true` 且出参 `0.0`——那会让"属性没加"与
  "属性加了但基值为 0"不可区分，而两者的红字含义完全不同）

#### Scenario: 未注册单位不 ensure

- **WHEN** 传入未注册的单位句柄或无效的属性 tag
- **THEN** 返回 `false`，**无 `ensure`、无红字**（读取是正常查询路径）

#### Scenario: 改基值后读数随之变化

- **WHEN** 先用 `SetBaseValue` 把基值改为 70，再调 `TryGetBaseValue`
- **THEN** 返回 `true` 且出参为 `70.0`——判据 = 本口读的是**基值**，
  与 `EvaluateCurrent`（折叠后当前值，含修正器）**不同层**

#### Scenario: 纯读语义

- **WHEN** 调用 `TryGetBaseValue` 前后比较该属性的脏标记与事件计数
- **THEN** 两者都不变（MUST NOT 标脏、MUST NOT 广播）
