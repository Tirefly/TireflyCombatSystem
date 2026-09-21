# Change: 伤害记录浏览命令（damage-record-inspection）

## Why
2026-09-21 用户质询："策划希望通过调试工具查看 InputDmg 和 FinalDmg 的差别，该如何做"。

现状核查结论：**能看见，但没有工具**——

- `FTcsDamageRecord` 已有 `Base`（= 链步骤解算输入）与 `Final`（= `BaseDamage` 键折叠后的最终值），二者之差即"修改器一共改了多少钱"；`AppendRecord` 每次结算也打一条 `Log` 级日志；
- 但**记录只进环形缓冲**（`GetRecentRecords` 是 C++ API），**没有控制台命令**——策划敲不出来；现有两条 `Tcs.Damage.*` 命令都住在**临时装置**里（不入库，plan2 Task 6 后退役）→ **入库的调试工具目前是零**；
- 归因（"谁改的、各改多少"）**刻意不做**（09 §5 非目标：生产期过程级溯源是量灾难），正规出路是 M8 的 Explain（`08 §4`）——**已登记台账 R8-6**（设计有定义、无 Task 认领）。

本提案补上"**入库的最小浏览口**"：让策划/开发在 PIE 里一条命令看到最近若干笔伤害的输入值、最终值、差额与执行量——它是 R8-6 Explain 落地前的**临时替代**（非替代品：Explain 要做的是逐键归因与面板，本命令只做"结果对账"）。

## What Changes
- 新增能力规格 **`damage-record-inspection`**（1 条需求）：`UTcsDamageSubsystem` MUST 提供**入库的**记录浏览命令 `Tcs.Damage.DumpRecords [N]`（默认打印最近 10 笔）：
  - 数据源 = 环形缓冲（`GetRecentRecords`，**旧 → 新序**）；
  - 每行 MUST 含：序号 / FlowId / 源 / 目标 / **Base（输入值）** / **Final（最终值）** / **差额（Final − Base，即修改器净影响）** / Executed / Absorbed / 暴击 / 击杀 / 时刻；
  - 输出通道 = **UE 原生日志**（`LogTcsDamage`，`Display` 级）+ 屏显（`GEngine->AddOnScreenDebugMessage`）；
  - 空缓冲 MUST 给出提示而非静默（"尚无记录"）；
  - 参数 `N` 非正 / 超缓冲容量 MUST 钳到合法区间（不报错、不崩溃）。
- **不做**：不做逐键归因（"哪几个修改器各改了多少"——归 R8-6 Explain，09 §5 非目标）；不做 UI 面板；不做过滤/排序/导出；不做跨会话持久化。

## 落点与口径
- 命令实现住**正式模块**（`Private/TcsDamageSubsystem.cpp`，随门面——不是临时装置），故**入库**；
- 命名 `Tcs.Damage.DumpRecords`（与既有命令族 `Tcs.Test.*` 区分：本命令是**入库的正式调试口**，不是测试装置）；
- 差额语义：`Final − Base` = 收集到的全部修正对伤害量的净影响（正数 = 被增强，负数 = 被削弱）——与 09 §2.3 的"修改器唯一通道"一致（修改器只经收集事件提交）。

## Impact
- Affected specs：`damage-record-inspection`（新建能力）。
- Affected code：`Source/TcsDamage/Private/TcsDamageSubsystem.cpp`（+ 命令注册）+ `Public/TcsDamageSubsystem.h`（如需暴露格式化助手则一并；否则命令全住 `.cpp`）。
- 决策依据：09 §2.4（记录字段）/§5（非目标：不做过程级溯源）；`08 §4`（Explain 归 M8——已登记台账 **R8-6**）；`project.md`（日志走模块原生通道；TcsCore 零日志设施）。
- 验证：UBT 编译零警告 + 用户 PIE 敲命令（有记录时打印表格；无记录时提示）。

## 检查点
落点验收 = 编译零警告 + PIE 实测（先跑 `Tcs.Test.Damage.Primitive` 产生记录，再敲 `Tcs.Damage.DumpRecords` 看表格与差额）。Explain 本体不在本提案（台账 R8-6）。
