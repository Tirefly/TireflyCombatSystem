## MODIFIED Requirements

### Requirement: 变更广播

重算产生**实质变化**时 MUST 经总线立即通道广播属性变更事件：

- 比较阈值 **epsilon = 1e-5**：`|NewValue - OldValue| <= 1e-5` 视为未变，**不广播**；
- 事件形状 `FTcsAttributeChangedEvent{ Unit, Attribute, OldValue, NewValue }`（核心词汇 FStruct，走 TcsCore 总线；`Reason` 字段待有消费者再加）——语义是**当前值（对外可读值）变了**，不是"某次写操作发生了"：改基值 / 挂摘修正器 / 依赖连带变化都只在**当前值确实动了**时产生这一条事件，因此它 MUST NOT 被当作"基础值变更日志"（基础值改了但被值域收口吃掉 = 对外没变 = 不广播，与"未变不广播"一致）；
- **事件 Tag 为原生 Tag `TcsEvent.Attribute.ValueChanged`**（常量 `Tag_TcsEvent_Attribute_ValueChanged`，TcsAttribute 内声明——订阅方按它过滤）：命名公约 **`TcsEvent.<域>.<事件名>`**（**2026-10-01 换根**：原 `Tcs.Event.<域>.<事件名>`；域段是真层级节点，同域后续事件如属性上线/下线挂 `TcsEvent.Attribute` 下；根 = `TcsEvent`，见 `gameplay-tag-governance` 能力的根段注册表）；**原生**而非宿主 Tag 表（插件自足、宿主漏配不会静默丢事件）；由事件所属模块声明（TcsCore 不持战斗域词汇）。注意**原生订阅路径当前为精确匹配**——父标签订阅需等原生层级匹配落地（已列总线输入），期间原生消费者按叶子逐条订阅；BP/CS 动态层已支持部分匹配；
- 广播时机由事务控制（提交尾、行内 flush）——见 `attribute-transaction` 能力；同一提交内同一属性**最多广播一次**。

#### Scenario: 变化才广播

- **WHEN** 挂一个把属性值从 100 改成 100.000001（差 < 1e-5）的修正器，然后挂一个改成 110 的修正器（各自提交）
- **THEN** 第一次提交不广播、第二次提交广播一次（`OldValue=100`、`NewValue=110`）

#### Scenario: 事件 tag 路径不超过 3 段

- **WHEN** 检查 `TcsEvent.Attribute.ValueChanged` 的段数
- **THEN** 恰为 3 段（根 + 域 + 词），该族仍留 1 段深度余量（换根前 `Tcs.Event.Attribute.ValueChanged` 为 4 段零余量）
