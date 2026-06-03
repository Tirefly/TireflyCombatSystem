## 1. 提案与设计

- [x] 1.1 将 Buff 增量反应语义整理为独立 change，并明确它与 `buff-runtime` / `buff-merge-runtime` 的职责边界。
- [x] 1.2 写清最终设计，明确只对 `Stack / Duration` 做通用配置，而 `Period` 回到 BuffStateTree。

## 2. 定义层与可见性建模

- [x] 2.1 在 `UTcsBuffDefinition` 上新增 `ETcsBuffDurationRefreshPolicy`、`ETcsBuffStackExpirationPolicy`、`FTcsBuffOnStackIncreasePolicy`、`FTcsBuffOnDurationExpiredPolicy`。
- [x] 2.2 补齐与 `MaxStackCount` 绑定的编辑器可见性和运行时忽略规则，确保单层 Buff 不暴露也不消费这批配置。
- [x] 2.3 补齐与 `DurationType` 绑定的编辑器可见性和运行时忽略规则，确保无限时长 Buff 不暴露也不消费 `OnDurationExpired` 配置。
- [x] 2.4 保持 `UTcsBuffDefinition::Period` 为默认周期间隔输入，不新增通用 `PeriodPolicy` 配置面。

## 3. Buff 运行时增量反应链路

- [x] 3.1 在保持 `UTcsStateComponent` 为共享宿主的前提下，在 `UTcsBuffInstance::SetStackCount()` 写回后接入统一的叠层上涨反应入口。
- [x] 3.2 在持续时间耗尽路径中引入统一的 `HandleBuffDurationExpired()` 入口，而不是直接 `ExpireBuffInstance()`，并继续通过共享移除链收敛最终离场。
- [x] 3.3 明确 `ClearEntireBuff`、`RemoveSingleStack`、`RemoveSingleStackAndRefreshDuration` 三种处理规则，并保证最后一层离场仍以 `Expired` 等真实理由收敛。
- [x] 3.4 确保掉层路径不会错误复用叠层上涨反应。

## 4. BuffStateTree 周期驱动能力

- [x] 4.1 新增可复用的 `FTcsSTTask_BuffPeriodDriver` 与对应实例数据，至少覆盖 `PeriodOverride` 和 `ElapsedTime`。
- [x] 4.2 约定统一的 `Event.Buff.PeriodTick` 事件契约，并让 `PeriodDriverTask` 只产出中性节拍事件。
- [x] 4.3 明确 `PeriodDriverTask` 只负责“到点了”，具体伤害 / 治疗 / 掉层 / 特效逻辑继续由 BuffStateTree 其他节点执行。
- [x] 4.4 明确“叠层时重置周期 / 叠层时立刻补一次”这类语义由具体 BuffStateTree 的专用节点实现，而不是回填到通用 BuffDef 配置。

## 5. 验证

- [x] 5.1 编译 `TireflyGameplayUtilsEditor Win64 Development`。
- [ ] 5.2 等待开发者手动执行编辑器测试，覆盖单层 Buff 忽略新增配置并在持续时间耗尽时直接整 Buff 移除、叠层刷新时长、持续时间耗尽掉层、最后一层因持续时间耗尽离场时不误报为 `StackDepleted`、`PeriodDriverTask` 的自然周期触发与停止推进语义。
- [x] 5.3 运行 `openspec validate add-buff-reactive-lifecycle --strict --no-interactive`。