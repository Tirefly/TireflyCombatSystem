## 1. 实现

- [x] 1.1 在 TCS runtime 模块中新增 `UTcsDefinitionRegistrySubsystem`，作为权威的实时 Def 快照持有者。
- [x] 1.2 将编辑器期刷新编排从 `UTcsDeveloperSettings` 中移出，并迁入 registry subsystem。
- [x] 1.3 实现刷新请求合并，使一波资产事件最终收敛成一次权威快照重建。
- [x] 1.4 让快照发现覆盖 AttributeDef、AttributeModifierDef、state-side DefinitionAsset（通过 `UTcsStateDefinition` 基类，包含 `UTcsBuffDefinition`）和 StateSlotDef 资产。
- [x] 1.5 让快照发现接受基于对应 TCS 基类派生的资产，其中 State 侧兼容所有派生自 `UTcsStateDefinition` 的具体定义资产。
- [x] 1.6 将最新 registry 快照镜像回 `UTcsDeveloperSettings` 的兼容缓存。
- [x] 1.7 让 `UTcsAttributeManagerSubsystem` 从 registry 快照重建，并订阅 registry 刷新通知。
- [x] 1.8 让 `UTcsStateManagerSubsystem` 从 registry 快照重建，并订阅 registry 刷新通知。
- [x] 1.9 在把编辑器期权威来源从 `UTcsDeveloperSettings` 切换到 registry 的同时，保留当前 loading-strategy 语义（`PreloadAll`、`OnDemand`、`Hybrid`）。

## 2. 验证

- [x] 2.1 验证在编辑器中新建 AttributeDef 时，registry 快照、`UTcsDeveloperSettings` 兼容视图以及 Attribute manager subsystem 都会在不重启编辑器的前提下更新。
- [x] 2.2 验证在编辑器中新建具体 state-side DefinitionAsset（例如 `UTcsBuffDefinition`）或 StateSlotDef 时，registry 快照、`UTcsDeveloperSettings` 兼容视图以及 State manager subsystem 都会在不重启编辑器的前提下更新。
- [x] 2.3 验证在同一编辑器会话中，修改、重命名和删除现有 Def 资产也会被正确传播。
- [x] 2.4 验证基于 TCS 基类派生出来的 Def 资产仍然能被 registry 发现并建立索引，尤其是派生自 `UTcsStateDefinition` 的具体 state-side 定义资产。
- [x] 2.5 运行 `openspec validate add-tcs-live-definition-registry --strict --no-interactive`。
