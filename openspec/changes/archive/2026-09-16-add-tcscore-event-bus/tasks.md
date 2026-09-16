## 1. Implementation（补录——已于 2026-09-11 实现并编译通过）

- [x] 1.1 `EventBus/UTcsEventHandler.h/.cpp`：共享 Handler 基类（BlueprintNativeEvent；EventTag 随载荷传入——2026-09-11 用户拍板补入、同日 ActualTag 改名 EventTag）
- [x] 1.2 `EventBus/FTcsEventBus.h`：总线内核（ETcsEventDispatch 双通道、订阅池配对清理、TMultiMap 路由、快照派发、惰性摘除、冲洗换出）
- [x] 1.3 `EventBus/UTcsEventBusSubsystem.h/.cpp`：UTickableWorldSubsystem 门面（世界类型过滤、动态多播观察钩子、Deinitialize 确定性清理）
- [x] 1.4 `EventBus/UTcsAsyncAction_ListenForCombatEvent.h/.cpp`：BP/CS 监听节点（Tag 精确/部分 + PayloadType 过滤；Activate/BeginDestroy 配对）

## 2. Verification

- [x] 2.1 Development Editor 编译通过（2026-09-11）
- [x] 2.2 用户 PIE 检查点验证通过：`Tcs.Test.Bus` 全项 PASS（订阅/双通道隔离/队列深度/退订/动态多播绑定与解绑；帧末到达经逐 tick 轮询确认）——BP InstancedStruct 节点面与 CS 载荷读取两条验证项留 CS 接入轮实测（不阻塞）
