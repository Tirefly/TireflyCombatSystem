# Change: 落地目标选择策略层——选择器/过滤器契约、默认选择器与 SelectTargets 步骤（targeting-strategy）

## Why
plan2 Task 2（TcsTargeting）动工前的规格先行提案。Task 1 已把 TcsEffect 的机制层落成规格与代码（执行器注册表 + 自注册宏 + 解释器 + 挂起协议），但**往表里喂执行器的第一块领域拼图还没有规格**：目标选择层（04 §2.3 / 10 文档 v3"策略接口 + 默认实现 + SelectTargets 执行器"）此前只有设计文档措辞，没有可执行规格。

本任务同时是两个机制的**首个实证**：①**跨模块自注册**（TcsTargeting 的步骤执行器注册进 TcsEffect 的表——若此处需要改机制层，说明 Task 1 的注册契约没设计好）；②**策略载体 D3-7 v3**（`TInstancedStruct<USTRUCT 抽象基类>` 的内嵌编辑与虚分派，R3 竖切的第一个策略族）。

## What Changes
- 新增能力规格 **`targeting-strategy`**（4 条需求）：
  - **选择器策略契约** `FTcsTargetSelectorStrategy`：`Resolve(Context, ITcsEntityQuery*, OutTargets)`；**抽象由约定达成（中性默认实现 + `meta=(Hidden)`），禁 `= 0`/`PURE_VIRTUAL`**（USTRUCT 生成 `TCppStructOps` 需可默认构造；`PURE_VIRTUAL` 在 `CHECK_PUREVIRTUALS` 下展开为 `= 0`——同一份代码 Development 编不过、Shipping 能过）；`Resolve` 只填充不清空；产出弱引用；宿主扩展 = C++ 子类，`BaseStruct` 限定由 UHT 自动写入。
  - **过滤器策略契约** `FTcsTargetFilterStrategy`：`Pass(Candidate, Context)`；**框架零默认 Filter**（存活/敌对 = 宿主本体论）；组合语义 = AND + 短路。
  - **R3 无框架默认选择器**（2026-09-20 用户拍板）：`FTcsSelSelf` 与 `FTcsSelEventTarget` **都不进 R3**——与"框架零默认 Filter"同一条纪律（选谁是宿主/内容语义）。EventTarget 依赖的"事件载荷 → 目标"通路随触发行轮（台账 R5-1）；Self 本身零依赖、可随时补，但不先钉一个可能被竖切实际用法否掉的默认。R3 竖切的选择器与过滤器**均由测试装置提供**（宿主侧、不入库）。
  - **SelectTargets 步骤与执行器**：清空 → Resolve → Filters AND（保序）→ 写回 `Context.Targets`；即时步骤恒 `TSR_Completed`；悬空候选跳过；未配 Selector → 目标集原样 + Warning（与"选了空集"可区分）；确定性保序。
- **不做的**（非目标，保持 R3 边界）：不做 RadiusArea / `FTargetingShape`（后置——竖切单体选择无消费者，用户拍板）；不做 TagQuery / Instigator 选择器；不做指示器/瞄准渲染（宿主/LAC）；不做目标评分/权重；不做空间加速结构；不做框架默认 Filter；不做脚本/蓝图策略通道。

## 顺延与落点裁决（提案显式交代）

1. **两个默认选择器都不进 R3**（计划 Task 2 写"Self/EventTarget 两默认实现"——此处按实施视角收窄，**2026-09-20 用户拍板**）：`FTcsSelEventTarget` 依赖"事件载荷 → 目标"的解析通路，而载荷形状要等触发行轮的真实样本（届时载荷多半携带实体的**身份句柄**而非 `AActor*`）——现在发明的契约会被推翻，该通路与"Context 默认目标初始化"同源，**已登记台账 R5-1**（触发轮次 = 触发行轮 M4a）；`FTcsSelSelf` 零依赖、随时可补，但**不先钉一个可能被竖切实际用法否掉的默认**——与"框架零默认 Filter"同一条纪律（选谁是宿主/内容语义）。R3 竖切的选择器与过滤器均由测试装置提供（宿主侧、不入库）。
2. **R3 的 Filter 实现住测试装置**（装置不入库）：设计明文"框架零默认 Filter、R3 竖切由测试装置/宿主实现"。框架侧只立契约；内容资产版装置随 plan2 Task 6 落地时，装置 Filter 一并迁移。
3. **`IRelationResolver`（10 §2.3 阵营判定注入契约）无落点**：plan2 任何 Task 都未点名它，R3 零消费者（宿主 Filter 自带判定即可）。本提案不改动它——**记入遗留台账**（触发条件 = 第一个需要阵营判定的宿主实现出现）。
4. **文件落点**（照 `cpp-module-structure`：对外头按领域子目录分层）：策略契约与默认实现住 `Public/Targeting/`，链步骤住 `Public/Chain/`（与 TcsDamage 计划的 `Chain/TcsStepDamage.h` 同级同款）；门面/日志通道在 `Public/` 根。
5. **本任务 MUST NOT 改动机制层**：执行器注册表、解释器、`FTcsEffectContext` 一律不动——需要改动即为 Task 1 契约缺陷的信号（写入验收判据）。

## Impact
- Affected specs: `targeting-strategy`（新建能力）——**无 MODIFIED/REMOVED**（TcsEffect 侧契约按需求零改动）。
- Affected code（`Source/TcsTargeting/`）：
  - 新增 `Public/Targeting/TcsTargetSelectorStrategy.h`、`Public/Targeting/TcsSelSelf.h`、`Public/Targeting/TcsTargetFilterStrategy.h`、`Public/Chain/TcsStepSelectTargets.h`、`Private/Chain/TcsStepSelectTargets.cpp`（执行器 + 跨模块自注册宏）；
  - 临时测试装置 `Private/Testing/TcsTargetingTestRig.h/.cpp`（**不入库**，plan1 Task 6 同款约定）：两个装置 Filter（存活类/敌对类语义以装置自己的判定为准）+ 一个**观察步**（打印 `Context.Targets` 数量与名字，用于屏显验收）+ 命令 `Tcs.Test.Targeting`（正路：Self 选择 → 过滤 AND → 观察步读回；含"未配 Selector 保持原样"与"空 Filter 全过"两条边界）+ `Tcs.Test.Targeting.Reject`（opt-in：无施法者 → 空集 + Warning）。
  - **零改动**：TcsEffect 全部文件（跨模块自注册的验收判据）、TcsTargeting 的 `Build.cs`（依赖边 `{Core, Attribute, Effect}` 已就位）。
- 决策依据：`10-module-targeting.md` v3（策略化三拍板 + D3-7 v3 载体）、D4-4 v2（战斗步骤不内嵌 selector、消费 `Context.Targets`）、D4-14（注册制分派）、D4-15 v2（模块成立）、D4-16（Spawn 移除）、D0-1（确定性）、R0 §9（蓝图不承诺）；引擎实证：`CoreMiscDefines.h:100-102`（`PURE_VIRTUAL` 与 `CHECK_PUREVIRTUALS`）、`UhtStructProperty.cs:634`（`TInstancedStruct` 自动 `BaseStruct`）、`SInstancedStructPicker.cpp:104`（`Hidden` 不进 picker）。
- 验证：UBT Development Editor 编译（零警告）+ 装置定向检查（自注册可查 / 选择过滤写回 / 边界两例）+ **零改动自检**（`git diff --stat` 不含 `Source/TcsEffect/`）。

## 提案内钉名（plan / 设计文档未钉或需收窄，供审阅否决）

| 项 | 钉法 | 依据 |
|---|---|---|
| 抽象手法 | **中性默认实现 + `meta=(Hidden)`**，禁 `= 0` 与 `PURE_VIRTUAL` | 计划 sketch 写的是"PURE_VIRTUAL 纯虚"——**照抄会在 Development 编不过**（`PURE_VIRTUAL` → `= 0` → USTRUCT 抽象类 → UHT 的 `TCppStructOps` 报 C2259；`CHECK_PUREVIRTUALS` 下才展开为 `=0`，字体取决于配置）。本仓先例 `FTcsParamValueSource` 即是"默认实现 + Hidden"，本提案与之对齐 |
| `Resolve` 签名 | `void Resolve(const FTcsEffectContext& Context, ITcsEntityQuery* EntityQuery, TArray<TWeakObjectPtr<AActor>>& OutTargets) const` | 设计写 `Resolve(Context, 注入查询, OutTargets)`；注入查询按 D4-14 走注入接口而不进 Context（Task 1 已把注入点放在门面，经 `Run.Owner` 可达）。`OutTargets` 取 `TWeakObjectPtr` 与 `Context.Targets` **同型**——免一次转换、弱引用纪律贯通 |
| 填充 vs 清空 | **执行器清空、策略只填充** | 否则"策略自己清空"与"执行器清空"会变成两份隐含约定；钉死一处 |
| `Pass` 签名 | `virtual bool Pass(const AActor* Candidate, const FTcsEffectContext& Context) const`；默认实现返回 `true` | 设计写 `Pass(Candidate, Context)`；`Candidate` 取 `const AActor*`（过滤器不应改写候选对象），`const` 方法（过滤是纯判定） |
| 未配 Selector 的行为 | 保持 `Context.Targets` 原样 + Warning | "没配选择器"与"选择器选中空集"是两种状态，必须可区分（后者是合法结果，前者是配置缺失）；**不 ensure**——内容/配置缺失由作者侧校验与 M8 校验矩阵兜底 |
| `EntityQuery` 可空 | 契约声明"MAY 为 nullptr，策略 MUST 容忍（降级 + Warning，禁解引用）" | 注入点是可选的宿主能力（Task 1 的 `GetEntityQuery()` 未注入返回 nullptr）——把"没注入"编码进契约，宿主策略才不会写出崩溃路径 |
| 观察步的落点 | 测试装置内的 `FTcsTestStepObserveTargets`（打印 `Context.Targets`） | R3 没有下游消费者（Damage 步骤在 Task 4）——用装置造一个"下游"来读回目标集，才能实证"写回成功"；该步骤**不入库**，随内容资产版装置（Task 6）退役 |

## 检查点
落点验收 = UBT 编译（Development Editor，零警告）+ 装置定向检查（跨模块自注册可查 / Self 选择 → 过滤 AND → 观察步读回 / 空 Filter 全过 / Selector 未配置目标集原样 / 无施法者空集 + Warning）+ **零改动自检**（机械层文件未被触碰）。`FTcsSelEventTarget`、RadiusArea、`IRelationResolver` 不在本提案。
