## MODIFIED Requirements

### Requirement: 参数快照的参数表读取适配器

TcsState MUST 提供 `FTcsStateParamTableReader`——把 `FTcsParamSnapshot` 当参数表暴露的读取适配器（把键查询转发到快照查询）。

它是**修正器物化与 `ParamRef` 源从快照取值的唯一入口**（R5 Task 4 的输入口）——消费方 MUST NOT 自行遍历快照条目去拼一套平行的键查找。

**实现形态 = 纯 C++ 类（持快照只读指针），本体 MUST NOT 派生 `UObject`、也 MUST NOT 去实现 `ITcsParamTableReader`**：唯一的直接消费方（物化器）是 C++ 直调，而 `ITcsParamTableReader` **不是 `Blueprintable`**（宿主脚本无法实现它）⇒ 为它造壳属"零消费者预建"，且会把快照指针的寿命绑到 GC 上。

**反射壳（2026-10-04，原定触发时机落地）**：为了让快照能作为"本次求值的参数表"装进反射上下文（`FTcsParamEvaluateContext::ParamTable`——物化求值的参数表就是该实例快照），TcsState MUST 另提供一层**薄壳** `UTcsStateParamTableReader : UObject, ITcsParamTableReader`：

- 壳 MUST 只做**委托**——把键查询原样转发给纯类本体，查找语义 MUST 仍只有一份实现（壳 MUST NOT 复制快照遍历逻辑，也 MUST NOT 引入第二处键查找）；
- 状态门面 MUST 以 `UPROPERTY` 持有壳的**单例**（零 per-apply 分配；宿主脚本因此可在求值上下文中读取该快照）；
- 门面 MUST 提供 **RAII 栈式绑定**（进入作用域时保存上一份快照指针、退出时恢复）：嵌套绑定 MUST 按栈恢复而不是互相覆盖，作用域之外 MUST NOT 留存快照指针。

#### Scenario: 读口命中与 miss

- **WHEN** 用一份含键 K（值 7）的快照构造读取适配器，分别以 K 与不含的键 M 调键查询
- **THEN** 前者返回 true 且输出 7，后者返回 false

#### Scenario: ParamRef 源经适配器从快照取值

- **WHEN** 以该适配器作上下文参数表，用 `FTcsParamSource_ParamRef{K, 兜底}` 求值
- **THEN** 返回快照内 K 的值（不是兜底值）

#### Scenario: 反射壳与纯类本体同源一致

- **WHEN** 用同一份快照分别经纯类本体与反射壳查询同一个键（命中与 miss 各一次）
- **THEN** 两者的命中结果与数值逐项一致（壳不引入第二套查找语义）

#### Scenario: 嵌套绑定按栈恢复

- **WHEN** 在一次绑定作用域内再绑定另一份快照，内层作用域结束后继续经同一个壳查询
- **THEN** 查询回到外层快照的值（内层绑定不残留、不互相覆盖）
