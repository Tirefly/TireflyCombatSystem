# OpenSpec 使用说明

面向使用 OpenSpec 进行规格驱动开发（spec-driven development）的 AI 编码助手的说明。

## TL;DR 速查清单

- 检索已有工作：`openspec spec list --long`、`openspec list`（仅在需要全文搜索时使用 `rg`）
- 确定范围：新增能力，还是修改已有能力
- 选取唯一的 `change-id`：kebab-case、动词开头（`add-`、`update-`、`remove-`、`refactor-`）
- 搭建脚手架：`proposal.md`、`tasks.md`、`design.md`（仅在需要时），以及每个受影响能力的 delta 规格
- 编写 delta：使用 `## ADDED|MODIFIED|REMOVED|RENAMED Requirements`；每条需求至少包含一个 `#### Scenario:`
- 校验：`openspec validate [change-id] --strict --no-interactive` 并修复问题
- 请求批准：在 proposal 获批之前不要开始实施

## 三阶段工作流

### 阶段 1：创建变更（Creating Changes）
在以下情况需要创建 proposal：
- 新增特性或功能
- 引入破坏性变更（API、schema）
- 调整架构或模式
- 优化性能（会改变行为）
- 更新安全模式

触发语（示例）：
- "帮我创建一个变更提案"
- "帮我规划一个变更"
- "帮我创建一个提案"
- "我想创建一个规格提案"
- "我想创建一个规格"

宽松匹配指引：
- 包含以下之一：`proposal`、`change`、`spec`
- 且包含以下之一：`create`、`plan`、`make`、`start`、`help`

以下情况跳过 proposal：
- Bug 修复（恢复预期行为）
- 拼写错误、格式、注释
- 依赖更新（非破坏性）
- 配置变更
- 针对既有行为的测试

**工作流**
1. 查阅 `openspec/project.md`、`openspec list` 和 `openspec list --specs`，了解当前上下文。
2. 选择唯一的动词开头 `change-id`，在 `openspec/changes/<id>/` 下搭建 `proposal.md`、`tasks.md`、可选的 `design.md` 以及规格 delta。
3. 使用 `## ADDED|MODIFIED|REMOVED Requirements` 起草规格 delta，每条需求至少包含一个 `#### Scenario:`。
4. 运行 `openspec validate <id> --strict --no-interactive`，在分享 proposal 之前解决所有问题。

### 阶段 2：实施变更（Implementing Changes）
把这些步骤作为 TODO 跟踪，逐项完成。
1. **阅读 proposal.md** - 理解要构建什么
2. **阅读 design.md**（如存在）- 审阅技术决策
3. **阅读 tasks.md** - 获取实施清单
4. **按顺序实施任务** - 依次完成
5. **确认完成** - 在更新状态之前，确保 `tasks.md` 中每一项都已完成
6. **更新清单** - 全部工作完成后，把每个任务置为 `- [x]`，使清单如实反映现状
7. **批准关卡** - 在 proposal 经过评审并获批之前不要开始实施

### 阶段 3：归档变更（Archiving Changes）
部署之后，另开 PR 完成：
- 将 `changes/[name]/` 移动到 `changes/archive/YYYY-MM-DD-[name]/`
- 如果能力发生变化，更新 `specs/`
- 对纯工具链变更使用 `openspec archive <change-id> --skip-specs --yes`（始终显式传入 change ID）
- 运行 `openspec validate --strict --no-interactive`，确认归档后的变更能通过校验

## 任何任务之前

**上下文清单：**
- [ ] 阅读 `specs/[capability]/spec.md` 中相关的规格
- [ ] 检查 `changes/` 中待处理的变更是否存在冲突
- [ ] 阅读 `openspec/project.md` 了解约定
- [ ] 运行 `openspec list` 查看活跃变更
- [ ] 运行 `openspec list --specs` 查看已有能力

**创建规格之前：**
- 始终先检查该能力是否已存在
- 优先修改已有规格，而不是创建重复规格
- 使用 `openspec show [spec]` 审阅当前状态
- 如果需求含糊，先提 1–2 个澄清问题再搭建脚手架

### 检索指引
- 枚举规格：`openspec spec list --long`（脚本中可用 `--json`）
- 枚举变更：`openspec list`（或 `openspec change list --json` - 已废弃但可用）
- 查看详情：
  - 规格：`openspec show <spec-id> --type spec`（过滤时使用 `--json`）
  - 变更：`openspec show <change-id> --json --deltas-only`
- 全文搜索（使用 ripgrep）：`rg -n "Requirement:|Scenario:" openspec/specs`

## 快速上手

### CLI 命令

```bash
# 核心命令
openspec list                  # 列出活跃变更
openspec list --specs          # 列出规格
openspec show [item]           # 显示变更或规格
openspec validate [item]       # 校验变更或规格
openspec archive <change-id> [--yes|-y]   # 部署后归档（非交互运行时加上 --yes）

# 项目管理
openspec init [path]           # 初始化 OpenSpec
openspec update [path]         # 更新说明文件

# 交互模式
openspec show                  # 提示选择
openspec validate              # 批量校验模式

# 调试
openspec show [change] --json --deltas-only
openspec validate [change] --strict --no-interactive
```

### 命令参数

- `--json` - 机器可读输出
- `--type change|spec` - 区分条目类型
- `--strict` - 全面校验
- `--no-interactive` - 关闭交互提示
- `--skip-specs` - 归档时不更新规格
- `--yes`/`-y` - 跳过确认提示（非交互式归档）

## 目录结构

```
openspec/
├── project.md              # 项目约定
├── specs/                  # 当前事实 - 已经建成的（IS built）
│   └── [capability]/       # 单一聚焦的能力
│       ├── spec.md         # 需求与场景
│       └── design.md       # 技术模式
├── changes/                # 提案 - 应该变更的（SHOULD change）
│   ├── [change-name]/
│   │   ├── proposal.md     # 为什么、做什么、影响
│   │   ├── tasks.md        # 实施清单
│   │   ├── design.md       # 技术决策（可选；见判定标准）
│   │   └── specs/          # delta 变更
│   │       └── [capability]/
│   │           └── spec.md # ADDED/MODIFIED/REMOVED
│   └── archive/            # 已完成的变更
```

## 创建变更提案

### 决策树

```
新需求？
├─ 恢复规格行为的 Bug 修复？ → 直接修
├─ 拼写/格式/注释？ → 直接修
├─ 新特性/新能力？ → 创建 proposal
├─ 破坏性变更？ → 创建 proposal
├─ 架构变更？ → 创建 proposal
└─ 不清楚？ → 创建 proposal（更稳妥）
```

### Proposal 结构

1. **创建目录：** `changes/[change-id]/`（kebab-case、动词开头、唯一）

2. **编写 proposal.md：**
```markdown
# Change: [变更的简要描述]

## Why
[1-2 句说明问题/机会]

## What Changes
- [变更要点列表]
- [用 **BREAKING** 标注破坏性变更]

## Impact
- Affected specs: [列出受影响的能力]
- Affected code: [关键文件/系统]
```

3. **创建规格 delta：** `specs/[capability]/spec.md`
```markdown
## ADDED Requirements
### Requirement: New Feature
The system SHALL provide...

#### Scenario: Success case
- **WHEN** user performs action
- **THEN** expected result

## MODIFIED Requirements
### Requirement: Existing Feature
[完整的修改后需求]

## REMOVED Requirements
### Requirement: Old Feature
**Reason**: [移除原因]
**Migration**: [如何处理迁移]
```
如果涉及多个能力，在 `changes/[change-id]/specs/<capability>/spec.md` 下创建多个 delta 文件——每个能力一个。

4. **创建 tasks.md：**
```markdown
## 1. Implementation
- [ ] 1.1 Create database schema
- [ ] 1.2 Implement API endpoint
- [ ] 1.3 Add frontend component
- [ ] 1.4 Write tests
```

5. **在需要时创建 design.md：**
若符合以下任一情况则创建 `design.md`；否则省略：
- 横切性变更（涉及多个服务/模块）或引入新的架构模式
- 新增外部依赖或显著的数据模型变更
- 安全、性能或迁移方面的复杂度
- 存在含糊之处，先做技术决策再编码更有利

`design.md` 最小骨架：
```markdown
## Context
[背景、约束、干系人]

## Goals / Non-Goals
- Goals: [...]
- Non-Goals: [...]

## Decisions
- Decision: [做什么以及为什么]
- Alternatives considered: [备选方案 + 理由]

## Risks / Trade-offs
- [风险] → 缓解措施

## Migration Plan
[步骤、回滚]

## Open Questions
- [...]
```

## 规格文件格式

### 关键：Scenario 的格式

**正确**（使用 #### 标题）：
```markdown
#### Scenario: User login success
- **WHEN** valid credentials provided
- **THEN** return JWT token
```

**错误**（不要用项目符号或粗体）：
```markdown
- **Scenario: User login**  ❌
**Scenario**: User login     ❌
### Scenario: User login      ❌
```

每条需求 MUST 至少包含一个 scenario。

### Requirement 的措辞
- 规范性需求使用 SHALL/MUST（除非有意写成非规范性，否则避免 should/may）

### Delta 操作

- `## ADDED Requirements` - 新增能力
- `## MODIFIED Requirements` - 行为变更
- `## REMOVED Requirements` - 废弃的特性
- `## RENAMED Requirements` - 名称变更

标题按 `trim(header)` 匹配 - 忽略空白字符。

#### 何时用 ADDED、何时用 MODIFIED
- ADDED：引入可作为独立需求存在的新能力或子能力。当变更是正交的（例如新增 "Slash Command Configuration"）而不是改变既有需求的语义时，优先使用 ADDED。
- MODIFIED：改变既有需求的行为、范围或验收标准。必须粘贴完整、更新后的需求内容（标题 + 全部 scenario）。归档器会用你在这里提供的内容整体替换原需求；只写部分 delta 会丢掉此前的细节。
- RENAMED：仅在名称变化时使用。如果同时改变了行为，则用 RENAMED（名称）加 MODIFIED（内容）并引用新名称。

常见陷阱：用 MODIFIED 增加新内容却没有包含原有文本。这会在归档时造成细节丢失。如果你并不是在明确修改既有需求，就在 ADDED 下新增一条需求。

正确撰写一条 MODIFIED 需求：
1) 在 `openspec/specs/<capability>/spec.md` 中定位既有需求。
2) 复制整个需求块（从 `### Requirement: ...` 到它的所有 scenario）。
3) 粘贴到 `## MODIFIED Requirements` 下，并编辑以反映新行为。
4) 确保标题文本完全一致（不区分空白字符），并保留至少一个 `#### Scenario:`。

RENAMED 示例：
```markdown
## RENAMED Requirements
- FROM: `### Requirement: Login`
- TO: `### Requirement: User Authentication`
```

## 故障排查

### 常见错误

**"Change must have at least one delta"（变更必须至少包含一个 delta）**
- 检查 `changes/[name]/specs/` 是否存在且包含 .md 文件
- 确认文件带有操作前缀（## ADDED Requirements）

**"Requirement must have at least one scenario"（需求必须至少包含一个 scenario）**
- 检查 scenario 是否使用 `#### Scenario:` 格式（4 个井号）
- 不要用项目符号或粗体作为 scenario 标题

**scenario 解析静默失败**
- 必须使用精确格式：`#### Scenario: Name`
- 调试命令：`openspec show [change] --json --deltas-only`

### 校验技巧

```bash
# 始终使用 strict 模式做全面检查
openspec validate [change] --strict --no-interactive

# 调试 delta 解析
openspec show [change] --json | jq '.deltas'

# 检查指定需求
openspec show [spec] --json -r 1
```

## Happy Path 脚本

```bash
# 1) 了解当前状态
openspec spec list --long
openspec list
# 可选的全文搜索：
# rg -n "Requirement:|Scenario:" openspec/specs
# rg -n "^#|Requirement:" openspec/changes

# 2) 选定 change id 并搭建脚手架
CHANGE=add-two-factor-auth
mkdir -p openspec/changes/$CHANGE/{specs/auth}
printf "## Why\n...\n\n## What Changes\n- ...\n\n## Impact\n- ...\n" > openspec/changes/$CHANGE/proposal.md
printf "## 1. Implementation\n- [ ] 1.1 ...\n" > openspec/changes/$CHANGE/tasks.md

# 3) 添加 delta（示例）
cat > openspec/changes/$CHANGE/specs/auth/spec.md << 'EOF'
## ADDED Requirements
### Requirement: Two-Factor Authentication
Users MUST provide a second factor during login.

#### Scenario: OTP required
- **WHEN** valid credentials are provided
- **THEN** an OTP challenge is required
EOF

# 4) 校验
openspec validate $CHANGE --strict --no-interactive
```

## 多能力示例

```
openspec/changes/add-2fa-notify/
├── proposal.md
├── tasks.md
└── specs/
    ├── auth/
    │   └── spec.md   # ADDED: Two-Factor Authentication
    └── notifications/
        └── spec.md   # ADDED: OTP email notification
```

auth/spec.md
```markdown
## ADDED Requirements
### Requirement: Two-Factor Authentication
...
```

notifications/spec.md
```markdown
## ADDED Requirements
### Requirement: OTP Email Notification
...
```

## 最佳实践

### 简单优先
- 默认新增代码少于 100 行
- 在证明单文件实现不够之前，先用单文件实现
- 没有明确理由就不要引入框架
- 选择朴素、经过验证的模式

### 复杂度触发条件
只有在满足以下条件时才增加复杂度：
- 性能数据表明当前方案过慢
- 具体的规模需求（超过 1000 用户、超过 100MB 数据）
- 有多个经过验证、确实需要抽象的使用场景

### 清晰的引用
- 代码位置使用 `file.ts:42` 格式
- 引用规格时写作 `specs/auth/spec.md`
- 关联相关的变更与 PR

### 能力命名
- 使用动词-名词：`user-auth`、`payment-capture`
- 每个能力只承担单一用途
- 10 分钟可理解原则
- 如果描述里需要用 "AND" 连接，就拆分

### Change ID 命名
- 使用 kebab-case，简短且具描述性：`add-two-factor-auth`
- 优先使用动词开头的词缀：`add-`、`update-`、`remove-`、`refactor-`
- 确保唯一；若已被占用，追加 `-2`、`-3` 等

## 工具选择指南

| 任务 | 工具 | 原因 |
|------|------|------|
| 按模式查找文件 | Glob | 快速的模式匹配 |
| 搜索代码内容 | Grep | 优化的正则搜索 |
| 读取指定文件 | Read | 直接访问文件 |
| 探查未知范围 | Task | 多步调查 |

## 错误恢复

### 变更冲突
1. 运行 `openspec list` 查看活跃变更
2. 检查规格之间是否重叠
3. 与变更负责人协调
4. 考虑合并 proposal

### 校验失败
1. 加上 `--strict` 参数运行
2. 查看 JSON 输出中的详细信息
3. 核对规格文件格式
4. 确认 scenario 格式正确

### 上下文缺失
1. 先阅读 project.md
2. 查看相关规格
3. 回顾近期的归档
4. 请求澄清

## 快速参考

### 阶段标识
- `changes/` - 已提案，尚未构建
- `specs/` - 已构建并部署
- `archive/` - 已完成的变更

### 文件用途
- `proposal.md` - 为什么与做什么
- `tasks.md` - 实施步骤
- `design.md` - 技术决策
- `spec.md` - 需求与行为

### CLI 要点
```bash
openspec list              # 有哪些正在进行？
openspec show [item]       # 查看详情
openspec validate --strict --no-interactive  # 是否正确？
openspec archive <change-id> [--yes|-y]  # 标记完成（自动化时加上 --yes）
```

记住：规格是事实，变更是提案。保持两者同步。
