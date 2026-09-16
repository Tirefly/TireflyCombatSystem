# Change: 补录 TcsCore 句柄与池能力规格（instance-handle-pool）

## Why
plan1 Task 1 的句柄与池实现（2026-09-11 落地、编译通过、用户 PIE 检查点验证）当时未走 OpenSpec 提案流程；按用户要求补录——本提案描述**已构建**的既有行为，使 `openspec/specs/` 与仓库实现同步（specs 记录当前真相）。无任何代码改动。

## What Changes
- 新增能力规格 `instance-handle-pool`，覆盖四组既有设施：
  - `TTcsInstanceHandle<TTagType>` 强类型句柄（Index + Generation，幻影 Tag 编译期隔离）；
  - `FTcsSourceHandle` 归属来源标识 + `FTcsSourceHandleRegistry` 原子发号（D2-2 级联撤销锚点）；
  - `TTcsInstancePool` 实例池（稠密数组 + FreeList 复用 + 代际防悬空 + 奇偶簿记稳定遍历 + 游戏线程断言 D0-4）；
  - `FTcsCorePoolStats` 统计口（CVar `Tcs.Core.PoolSlots`；Add/Remove 无参对——2026-09-15 用户拍板拆分）。
- **BREAKING**：无（纯规格补录，行为即现状）。

## Impact
- Affected specs: `instance-handle-pool`（新建能力）。
- Affected code: 无——实现已在 `Source/TcsCore/Public/Handle/`、`Source/TcsCore/Public/Pool/`、`Source/TcsCore/Public/TcsCoreStats.h`（提交 ec08c1b）。验证载体（`Private/Testing/` 临时装置）按用户决定不入库、不进规格。
