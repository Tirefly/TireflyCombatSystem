## ADDED Requirements

### Requirement: 伤害记录浏览命令

`UTcsDamageSubsystem` MUST 提供**入库的**（非测试装置）记录浏览命令 **`Tcs.Damage.DumpRecords [N]`**：

- 数据源 = 环形缓冲（`GetRecentRecords`，**旧 → 新序**）；`N` 默认 10，MUST 钳到 `[1, 缓冲容量]`（非正/超大值不报错、不崩溃）；
- 每行 MUST 至少含：序号 / `FlowId` / 源句柄 / 目标句柄 / **`Base`（链步骤解算输入）** / **`Final`（`BaseDamage` 键折叠后的最终值）** / **差额（`Final − Base`）** / `Executed` / `Absorbed` / 暴击 / 击杀 / 时刻；
- **差额语义**：`Final − Base` = 收集到的全部修正对伤害量的**净影响**（正 = 被增强，负 = 被削弱）——修改器只经收集事件提交（09 §2.3 唯一通道）；
- 输出 MUST 走 UE 原生日志（`LogTcsDamage`，`Display` 级）+ 屏显（`GEngine->AddOnScreenDebugMessage`）；MUST NOT 引入项目级日志设施（TcsCore 零日志设施纪律）；
- 缓冲为空 MUST 明确提示（"尚无记录"）而非静默；
- **MUST NOT 做逐键归因**（"哪几个修改器各改了多少"——09 §5 非目标，归 M8 Explain，台账 R8-6）。

#### Scenario: 打印最近记录

- **WHEN** 环形缓冲有 5 笔记录时执行 `Tcs.Damage.DumpRecords`
- **THEN** 按旧→新序打印 5 行，每行含 Base / Final / 差额 / Executed 等字段（日志 + 屏显各一份）

#### Scenario: 空缓冲不静默

- **WHEN** 环形缓冲为空时执行命令
- **THEN** 输出"尚无记录"提示（不静默、不报错）

#### Scenario: 参数钳制

- **WHEN** 传入 `N = 0` / `N = -3` / `N = 99999`
- **THEN** 分别按 1 / 1 / 缓冲容量处理（不崩溃、不报错）

#### Scenario: 差额反映修改器净影响

- **WHEN** 一笔伤害的输入为 30、收集到一笔 `PercentAdd -0.2` 修正
- **THEN** 该行 `Base = 30`、`Final = 24`、差额 = `-6`（= 削弱 20%）
