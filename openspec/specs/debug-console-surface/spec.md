# debug-console-surface Specification

## Purpose
定义 TCS 如何通过 `TcsConsoleCommands` / `TcsConsoleCommandRuntime` 提供统一的控制台命令命名面，并要求像 `FTcsStateSlotDebugEvaluator` 这类常驻调试路径只能在显式控制台开关开启后才允许构造高成本状态快照字符串。
## Requirements
### Requirement: Centralized TCS console command definitions
TCS SHALL provide a centralized module-level console command definition surface for TCS-owned console commands.

#### Scenario: Shared command metadata is consumed by concrete call sites
- **WHEN** a TCS runtime file registers, documents, or references a TCS-owned console command
- **THEN** it SHALL consume shared command definitions instead of hardcoding raw command literals in that call site
- **AND** the shared definition surface SHALL include command strings, parameter conventions, and help text metadata

### Requirement: Minimal first-phase command scope
TCS SHALL keep the first-phase console surface limited to explicit control switches instead of target-scoped snapshot query commands.

#### Scenario: On-demand snapshot browsing stays out of this change
- **WHEN** a developer needs to inspect a target's state slot snapshot or state instance snapshot on demand
- **THEN** this change SHALL NOT introduce dedicated console `dump / list` commands for that query
- **AND** `GetSlotDebugSnapshot()` / `GetStateDebugSnapshot()` MAY remain available as future debug UI support APIs

### Requirement: Console-gated recurring snapshot generation
TCS SHALL require an explicit console-controlled switch before recurring runtime debug paths are allowed to build large state snapshot strings.

#### Scenario: Recurring debug evaluator is disabled
- **WHEN** the recurring state snapshot debug switch is disabled
- **THEN** `FTcsStateSlotDebugEvaluator` SHALL NOT call `GetSlotDebugSnapshot()` during its recurring evaluation path

#### Scenario: Recurring debug evaluator is enabled
- **WHEN** the recurring state snapshot debug switch is enabled
- **THEN** `FTcsStateSlotDebugEvaluator` MAY call `GetSlotDebugSnapshot()` during its recurring evaluation path
- **AND** that behavior SHALL remain explicitly opt-in instead of default-on

### Requirement: Stable key-value argument convention
TCS SHALL use a stable key-value argument convention for TCS-owned console commands introduced by this change.

#### Scenario: Command argument parsing remains extension-friendly
- **WHEN** a first-phase TCS debug command accepts command-line arguments
- **THEN** it SHALL parse arguments in `key=value` form
- **AND** the first-phase argument keys SHALL support explicit switch control without relying on positional ordering

