## 1. Implementation（补录——已于 2026-09-11 实现并编译通过）

- [x] 1.1 `Public/Handle/TTcsInstanceHandle.h`：Index+Generation 强类型句柄模板（幻影 Tag 隔离、IsValid、相等比较）
- [x] 1.2 `Public/Handle/FTcsSourceHandle.h`：来源标识（uint64，0=无效）+ Registry 原子发号（首个为 1、永不复用）
- [x] 1.3 `Public/Pool/TTcsInstancePool.h`：稠密数组 + FreeList 复用 + 代际防悬空 + 奇偶簿记（奇=已分配/偶=空闲）+ 游戏线程断言
- [x] 1.4 `Public/TcsCoreStats.h` + `Private/TcsCoreStats.cpp`：`Tcs.Core.PoolSlots` CVar 与维护口（2026-09-15 拆分为 Add/Remove 无参对）+ 模板显式实例化锚点（无消费者期的全成员编译检查）

## 2. Verification

- [x] 2.1 Development Editor 编译通过（2026-09-11）
- [x] 2.2 用户 PIE 检查点验证通过：`Tcs.Test.Pool`（8 项：分配/解析/释放/复用/遍历/统计）与 `Tcs.Test.Pool.Dangling`（悬空 Resolve → ensure 命中 + nullptr）全项 PASS
