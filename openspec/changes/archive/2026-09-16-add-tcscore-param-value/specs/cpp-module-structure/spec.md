## REMOVED Requirements

### Requirement: Task 0 载荷类型与设施

**Reason**: 能力归属错位补正（2026-09-16 用户质询）——参数载体体系与值约定是正交能力，不应寄居"模块结构"能力名下；且该 requirement 的归属地无法承接后续增量（Task 4 AttributeScaled、PV-10 可枚举源、PV-1 上下文补齐）。
**Migration**: TcsCore 载体部分迁入新能力 `param-value`（本提案 ADDED）；TcsNotation 值约定部分迁入新能力 `value-convention`（姊妹提案 `add-tcsnotation-value-convention`）；`UTcsDeveloperSettings`（空壳）不再保留规格——待其长出真实配置项时随对应能力规格化（2026-09-16 用户拍板口径）。
