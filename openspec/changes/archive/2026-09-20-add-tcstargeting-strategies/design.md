## Context

TcsTargeting 是 R3 竖切里"把执行器喂进 TcsEffect 注册表"的第一块领域拼图（D4-14 注册制分派的首个跨模块实证），也是策略载体 D3-7 v3（`TInstancedStruct<USTRUCT 抽象基类>` + C++ 虚分派）的第一个落地族。约束：TcsTargeting 只依赖 `{Core, Attribute, Effect}`（不依赖 State/Attribute 语义——具名审计 MEM-20260902-13）；单游戏线程（D0-4）；确定性（D0-1）；禁 TDD。

## Goals / Non-Goals

- Goals：选择器/过滤器两份策略契约；`SelectTargets` 步骤与执行器（跨模块自注册）；两条边界行为（空 Filter、未配 Selector）钉死；确定性保序。**框架零默认选择器**（R3 范围，2026-09-20 用户拍板——与零默认 Filter 同一条纪律）。
- Non-Goals：框架默认选择器（`FTcsSelSelf`/`FTcsSelEventTarget` 均后置）、RadiusArea/`FTargetingShape`、TagQuery/Instigator、指示器渲染、目标评分、空间加速、框架默认 Filter、脚本/蓝图策略通道。

## Decisions

- **抽象用"中性默认实现 + `meta=(Hidden)`"，不用 `= 0` / `PURE_VIRTUAL`**。UHT 为每个 USTRUCT 无条件生成 `TCppStructOps<T>`（默认构造路径），抽象类会在 UHT 生成码处报 C2259；而 `PURE_VIRTUAL` 只在 `CHECK_PUREVIRTUALS` 关闭时才展开成"致命错误函数体"，开启时是 `= 0`（`CoreMiscDefines.h:100-102`）——**同一份代码的编译结果随配置不同**，是最坏的一类失败。*被否*：`PURE_VIRTUAL`（配置依赖的编译结果 + 计划 sketch 的原始写法）；纯虚 `= 0`（直接编不过）。
- **`OutTargets` 取弱引用（`TWeakObjectPtr<AActor>`）**：与 `FTcsEffectContext::Targets` 同型，免一次转换；也把"目标 Actor 可能在链执行期间销毁"这件事编码进类型（Pin 失败即跳过）。
- **执行器清空、策略只填充**：清空责任唯一（执行器），策略不必假设传入为空。
- **未配 Selector ≠ 选了空集**：前者保持目标集原样 + Warning（配置缺失），后者是合法结果（清空后为空）。二者行为不同、可区分——这是"静默错值"防线的一条。
- **Filter 框架零默认**：存活/敌对是宿主本体论；R3 由测试装置提供两个 Filter 作实证（装置不入库）。

## Risks / Trade-offs

- **R3 竖切没有真实 Filter**（装置实现）→ 内容资产版装置（Task 6）落地时把装置 Filter 一并迁移；框架侧契约不变。
- **`FTcsSelEventTarget` 缺位**使"单步链零 SelectTargets 直接消费"的完整形态要等触发行轮 → 已入台账 R5-1，届时与"Context 默认目标初始化"同批（两者同源）。
- **策略基类在编辑器 picker 里不可选**靠 `Hidden` 元数据（引擎行为，已核 `SInstancedStructPicker.cpp:104`）→ 若将来有引擎版本改动该过滤，症状是"能选到基类"，属可发现的软失败（不会静默错值）。
- **跨模块注册依赖模块加载**（TcsTargeting 必须先加载）→ 与 GameplayTag 同款既有前提，R3 无按需卸载模块的场景。

## Open Questions

- 阵营判定契约 `IRelationResolver`（10 §2.3）落点：本提案不动，记台账（触发条件 = 第一个需要阵营判定的宿主实现）。
- 目标集的"评分/排序"是否需要：现无需求，YAGNI 挂起。
