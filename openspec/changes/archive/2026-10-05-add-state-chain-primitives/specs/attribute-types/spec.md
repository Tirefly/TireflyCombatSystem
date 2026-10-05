## ADDED Requirements

### Requirement: 属性值比较枚举

`TcsAttribute` MUST 提供 `ETcsAttributeComparison`——**属性值域的比较词汇**（供条件 / 规则类消费者取词）：

- 值集 MUST 覆盖四向：`Greater` / `Less` / `GreaterOrEqual` / `LessOrEqual`（自 `0` 起连续编号，`0` 即默认）；等值语义由"大于等于 且 小于等于"组合表达，**MUST NOT** 另立 `Equal` / `NotEqual` 档（零消费者不预建）；
- 枚举 MUST 住 `TcsAttribute`（它与 `ETcsAttributeOp` 同属属性值域词汇，**不是**某个消费者的私有词表）——消费者（触发条件 `AttributeCompare`）从本模块取词，MUST NOT 在消费侧另立同义枚举；
- 枚举只声明**词与语义边界**（比较对象 = 属性的**当前值**，不是基值、不是未提交候选值）；求值行为由消费者实现。

#### Scenario: 四向语义可被消费者区分

- **WHEN** 某属性当前值为 10，分别以 `Greater` / `Less` / `GreaterOrEqual` / `LessOrEqual` 对阈值 10 比较
- **THEN** 结果依次为 假 / 假 / 真 / 真
