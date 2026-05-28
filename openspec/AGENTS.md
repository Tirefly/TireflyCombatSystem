# OpenSpec 使用说明

面向在 TireflyCombatSystem 插件中使用 OpenSpec 进行规格驱动开发的 AI 编码助手说明。

## 作用范围与路径

- 这套 OpenSpec 工作区位于 `Plugins/TireflyCombatSystem/openspec`。
- 所有 OpenSpec CLI 命令都应在 `Plugins/TireflyCombatSystem` 目录下执行。
- 除非另有说明，`openspec/`、`Source/`、`Documents/`、`Resources/`、`CODE_REVIEW_CHECKLIST.md` 等路径都相对于插件根目录。
- 插件外的仓库级路径仍保持仓库相对路径写法，例如 `.github/skills/...` 和 `Script/...`。

## 当前开发阶段

- TireflyCombatSystem 当前处于架构设计与开发实践阶段。
- 默认认为当前还没有 Blueprint 资产引用 TCS 的 API、委托、定义资产或编辑器 authoring 流程。
- 在评估 TCS 内部的破坏性与迁移风险时，除非用户明确说明现已有 Blueprint 资产引用，否则不要凭空引入 Blueprint 兼容性约束。
- 如果未来真的开始出现 Blueprint 资产引用，只有在用户明确确认之后，才将其视为新的仓库事实。

## 快速检查清单

- 先进入插件根目录：`cd Plugins/TireflyCombatSystem`
- 搜索已有工作：`openspec spec list --long`、`openspec list`（全文检索优先用 `rg`）
- 判断范围：是新增 capability，还是修改已有 capability
- 选择唯一的 `change-id`：使用 kebab-case，并以动词开头（如 `add-`、`update-`、`remove-`、`refactor-`）
- 搭建骨架：`proposal.md`、`tasks.md`、`design.md`（仅在需要时），以及按 capability 划分的 delta spec
- 编写 delta：使用 `## ADDED|MODIFIED|REMOVED|RENAMED Requirements`，每个 requirement 至少包含一个 `#### Scenario:`
- 校验：执行 `openspec validate [change-id] --strict --no-interactive` 并修复问题
- 等待批准：提案批准前不要开始实现

## 三阶段工作流

### 阶段 1：创建变更提案
当你需要做下列事情时，应创建 proposal：
- 新增功能或能力
- 引入破坏性变更（API、Schema）
- 调整架构或模式
- 做会改变行为的性能优化
- 更新安全模式

触发示例：
- “帮我创建一个 change proposal”
- “帮我规划一个 change”
- “帮我创建一个 proposal”
- “我想建一个 spec proposal”
- “我想建一个 spec”

宽松匹配指导：
- 语句中包含 `proposal`、`change`、`spec` 之一
- 同时包含 `create`、`plan`、`make`、`start`、`help` 之一

以下情况通常可以跳过 proposal：
- 恢复既有预期行为的 Bug 修复
- 错别字、格式或注释修正
- 非破坏性的依赖更新
- 配置变更
- 针对既有行为补测试

**标准流程**
1. 在插件根目录下查看 `openspec/project.md`、`openspec list` 和 `openspec list --specs`，建立当前上下文。
2. 选择一个唯一且以动词开头的 `change-id`，并在 `openspec/changes/<id>/` 下创建 `proposal.md`、`tasks.md`、可选的 `design.md`，以及 spec delta。
3. 使用 `## ADDED|MODIFIED|REMOVED Requirements` 编写 spec delta，并确保每个 requirement 至少带一个 `#### Scenario:`。
4. 执行 `openspec validate <id> --strict --no-interactive`，修复问题后再对外分享提案。

### 阶段 2：实现变更
将以下步骤作为 TODO，按顺序完成。
1. **阅读 proposal.md**：理解要构建什么
2. **阅读 design.md**（如果存在）：理解技术决策
3. **阅读 tasks.md**：获取实现清单
4. **按顺序实现任务**：逐项完成，不跳步
5. **确认完成状态**：更新状态前，确保 `tasks.md` 中每项都已完成
6. **更新勾选项**：所有工作完成后，把任务状态更新为 `- [x]`
7. **批准门槛**：proposal 未评审并批准前，不要开始实现

### 阶段 3：归档变更
部署完成后，单独发起归档 PR，并执行：
- 将 `changes/[name]/` 移动到 `changes/archive/YYYY-MM-DD-[name]/`
- 如果 capability 发生变化，同步更新 `specs/`
- 对于只涉及工具链的变更，可使用 `openspec archive <change-id> --skip-specs --yes`（务必显式传入 change ID）
- 执行 `openspec validate --strict --no-interactive`，确认归档后的变更仍然通过校验

### Archive 使用规则

- `changes/archive/` 下的内容是历史快照，不是当前事实声明。
- 如果归档文本与 `openspec/specs/` 或活动 change 冲突，优先级始终是：当前 `specs/` > 活动 `changes/` > `changes/archive/`。
- 归档 change 的价值主要是解释“当时为什么这样做”，而不是替代当前 capability spec 或当前设计决策。
- 对近期仍在快速收敛的主题，尤其是 `State Core / Buff / Skill` 分层、`StateTree schema` 命名、`UTcsSkillInstance` / `UTcsSkillEntry` 语义边界，不要从 archive 单独推断当前契约；必须先检查当前 spec 和活动 change。

## 开始任何任务前

**上下文检查清单：**
- [ ] 确认当前工作目录为 `Plugins/TireflyCombatSystem`
- [ ] 阅读相关 spec：`specs/[capability]/spec.md`
- [ ] 检查 `changes/` 中是否有潜在冲突的进行中变更
- [ ] 如果需要查看 archive，先把它视为历史背景而不是当前契约，并用当前 `specs/` / 活动 `changes/` 交叉验证
- [ ] 阅读 `openspec/project.md` 了解仓库约定
- [ ] 执行 `openspec list` 查看活动变更
- [ ] 执行 `openspec list --specs` 查看已有 capability

## 仓库内 UnrealSharp 路由

本仓库 vendored 了 `Plugins/UnrealSharp`，并提供本地工作流技能 `.github/skills/unrealsharp-agent-skill/SKILL.md`。

- 在处理 UnrealSharp 集成、C# 脚本、`Script/*.csproj`、`*.Glue`、`generated.cs`、`BuildEmitLoadOrder`、热重载、编辑器启动/构建问题之前，先读本地 UnrealSharp skill。
- 当前用户脚本工程位于 `Script/ManagedTireflyGameplayUtils/ManagedTireflyGameplayUtils.csproj`，生成的 Glue 工程位于 `Script/TireflyGameplayUtils.RuntimeGlue/TireflyGameplayUtils.RuntimeGlue.csproj`，并且当前仓库使用的是单数 `Script/` 根目录。
- 当本地插件源码与通用 UnrealSharp 经验冲突时，应优先参考 `Plugins/UnrealSharp` 下的本地源码与配置。
- `Script/**/*.Glue`、`obj/UHT/**/*.generated.cs` 以及托管构建输出都视为生成物；应修改反射源声明或生成流程，而不是直接改生成物。
- 如果 C# 侧缺少某个 API，应先检查 Unreal 反射暴露是否完整，再考虑生成器或 Glue 链路问题。

**创建 spec 前的额外检查：**
- 始终先检查 capability 是否已存在
- 优先修改已有 spec，而不是重复创建新 spec
- 使用 `openspec show [spec]` 查看当前状态
- 如果请求语义不清，先问 1 到 2 个澄清问题，再搭建骨架

### 搜索建议
- 列出现有 spec：`openspec spec list --long`（脚本场景可用 `--json`）
- 列出现有 change：`openspec list`（或 `openspec change list --json`，但后者已弃用）
- 查看详情：
  - Spec：`openspec show <spec-id> --type spec`（筛选时可配合 `--json`）
  - Change：`openspec show <change-id> --json --deltas-only`
- 全文检索时优先使用 ripgrep：`rg -n "Requirement:|Scenario:" openspec/specs`

## 快速上手

### CLI 命令

```bash
# 基础命令
openspec list                  # 列出活动 change
openspec list --specs          # 列出现有规范
openspec show [item]           # 展示 change 或 spec
openspec validate [item]       # 校验 change 或 spec
openspec archive <change-id> [--yes|-y]   # 部署后归档（非交互场景加 --yes）

# 项目管理
openspec init [path]           # 初始化 OpenSpec
openspec update [path]         # 更新说明文件

# 交互模式
openspec show                  # 交互选择展示对象
openspec validate              # 批量校验模式

# 调试
openspec show [change] --json --deltas-only
openspec validate [change] --strict --no-interactive
```

### 常用参数

- `--json`：机器可读输出
- `--type change|spec`：显式指定对象类型
- `--strict`：执行严格校验
- `--no-interactive`：禁用交互提示
- `--skip-specs`：归档时跳过 spec 更新
- `--yes` / `-y`：跳过确认提示（适用于非交互归档）

## 目录结构

```
Plugins/TireflyCombatSystem/
├── openspec/
│   ├── project.md              # 项目约定
│   ├── specs/                  # 当前事实：系统已经具备什么
│   │   └── [capability]/       # 单一聚焦的 capability
│   │       ├── spec.md         # Requirement 与 Scenario
│   │       └── design.md       # 技术模式与设计说明
│   ├── changes/                # 提案：系统接下来应该怎么变
│   │   ├── [change-name]/
│   │   │   ├── proposal.md     # 为什么做、改什么、影响什么
│   │   │   ├── tasks.md        # 实现清单
│   │   │   ├── design.md       # 技术决策（按需）
│   │   │   └── specs/          # 各 capability 的 delta
│   │   │       └── [capability]/
│   │   │           └── spec.md # ADDED/MODIFIED/REMOVED
│   │   └── archive/            # 已完成并归档的 change
├── Source/
├── Documents/
└── CODE_REVIEW_CHECKLIST.md
```

## 创建变更提案

### 决策树

```
收到新请求？
├─ 是恢复既有规范行为的 Bug 修复？ → 直接修
├─ 是错别字/格式/注释？ → 直接修
├─ 是新增能力或功能？ → 创建 proposal
├─ 是破坏性变更？ → 创建 proposal
├─ 是架构调整？ → 创建 proposal
└─ 不确定？ → 优先创建 proposal（更安全）
```

### Proposal 结构

1. **创建目录：** `changes/[change-id]/`（kebab-case、动词开头、唯一）

2. **编写 proposal.md：**
```markdown
# 变更：[简要描述本次变更]

## 背景
[1-2 句话说明问题或机会点]

## 变更内容
- [改动点列表]
- [若有破坏性变更，使用 **BREAKING** 标记]

## 影响范围
- 受影响规范：[capability 列表]
- 受影响代码：[关键文件/系统]
```

3. **创建 spec delta：** `specs/[capability]/spec.md`
```markdown
## ADDED Requirements
### Requirement: 新能力
系统 SHALL 提供……

#### Scenario: 成功场景
- **WHEN** 用户执行某个动作
- **THEN** 得到预期结果

## MODIFIED Requirements
### Requirement: 既有能力
[贴出完整修改后的 requirement]

## REMOVED Requirements
### Requirement: 旧能力
**Reason**: [为什么删除]
**Migration**: [如何迁移]
```
如果同一个 change 会影响多个 capability，则应在 `changes/[change-id]/specs/<capability>/spec.md` 下分别创建多个 delta 文件，每个 capability 一个。

4. **创建 tasks.md：**
```markdown
## 1. 实现
- [ ] 1.1 创建数据库结构
- [ ] 1.2 实现 API 接口
- [ ] 1.3 添加前端组件
- [ ] 1.4 编写测试
```

5. **何时需要 design.md：**
如果满足以下任一条件，就创建 `design.md`；否则可以省略：
- 跨多个服务/模块，或引入新的架构模式
- 引入新的外部依赖，或涉及显著数据模型调整
- 涉及安全、性能或迁移复杂度
- 在编码前需要先固定关键技术决策的模糊问题

最小 `design.md` 骨架：
```markdown
## 背景
[背景、约束、参与方]

## 目标 / 非目标
- 目标：[…]
- 非目标：[…]

## 决策
- 决策：[做什么、为什么]
- 备选方案：[方案与理由]

## 风险 / 取舍
- [风险] → [缓解方式]

## 迁移计划
[步骤、回滚策略]

## 开放问题
- [...]
```

## Spec 文件格式

### 关键：Scenario 格式

**正确写法**（使用 `####` 标题）：
```markdown
#### Scenario: 用户登录成功
- **WHEN** 提供有效凭证
- **THEN** 返回 JWT Token
```

**错误写法**（不要用列表或粗体代替标题）：
```markdown
- **Scenario: User login**  ❌
**Scenario**: User login     ❌
### Scenario: User login      ❌
```

每个 requirement 都 **必须** 至少有一个 scenario。

### Requirement 表述规范
- 规范性要求必须使用 `SHALL` / `MUST`；除非你明确希望表达非规范性建议，否则不要使用 `should` / `may`

### Delta 操作类型

- `## ADDED Requirements`：新增能力
- `## MODIFIED Requirements`：修改既有行为
- `## REMOVED Requirements`：废弃能力
- `## RENAMED Requirements`：仅名称变更

Header 会用 `trim(header)` 做匹配，也就是会忽略前后空白。

#### 何时使用 ADDED，何时使用 MODIFIED
- ADDED：新增一个可以独立成立的新 requirement 或子 capability。如果本次改动与现有 requirement 平行，而不是修改其语义，优先使用 ADDED。
- MODIFIED：改变现有 requirement 的行为、范围或验收标准。此时必须贴出**完整的、更新后的 requirement 内容**（标题 + 全部 scenario）。归档器会用你提供的整段内容替换旧 requirement；如果只贴部分内容，旧细节会在归档时丢失。
- RENAMED：只有名称变化时使用。如果同时改了行为，应使用 RENAMED（改名）+ MODIFIED（改内容），且 MODIFIED 应引用新名称。

常见陷阱：
为了补充一个新关注点而错误使用 MODIFIED，但没有把旧 requirement 的完整内容一起带上。这样在 archive 时会丢失细节。如果你并没有显式修改既有 requirement，应该在 ADDED 下新增 requirement。

正确编写 MODIFIED requirement 的步骤：
1. 在 `openspec/specs/<capability>/spec.md` 中找到现有 requirement。
2. 复制完整 requirement 区块（从 `### Requirement: ...` 到它的所有 scenario）。
3. 将其粘贴到 `## MODIFIED Requirements` 下，并编辑成新的行为描述。
4. 确保标题文本完全匹配（忽略空白差异），并保留至少一个 `#### Scenario:`。

RENAMED 示例：
```markdown
## RENAMED Requirements
- FROM: `### Requirement: 登录`
- TO: `### Requirement: 用户认证`
```

## 故障排查

### 常见错误

**"Change must have at least one delta"**
- 检查 `changes/[name]/specs/` 是否存在且包含 `.md` 文件
- 确认文件带有操作前缀（如 `## ADDED Requirements`）

**"Requirement must have at least one scenario"**
- 检查 scenario 是否使用 `#### Scenario:` 格式（4 个 `#`）
- 不要用项目符号或粗体来写 scenario 标题

**Scenario 静默解析失败**
- 必须严格使用：`#### Scenario: 名称`
- 可用以下命令调试：`openspec show [change] --json --deltas-only`

### 验证提示

```bash
# 始终使用 strict 模式做完整检查
openspec validate [change] --strict --no-interactive

# 调试 delta 解析
openspec show [change] --json | jq '.deltas'

# 查看指定 requirement
openspec show [spec] --json -r 1
```

## 标准流程脚本

```bash
# 0) 进入插件根目录
cd Plugins/TireflyCombatSystem

# 1) 查看当前状态
openspec spec list --long
openspec list
# 可选：做全文搜索
# rg -n "Requirement:|Scenario:" openspec/specs
# rg -n "^#|Requirement:" openspec/changes

# 2) 选择 change id 并搭建骨架
CHANGE=add-two-factor-auth
mkdir -p openspec/changes/$CHANGE/{specs/auth}
printf "## 背景\n...\n\n## 变更内容\n- ...\n\n## 影响范围\n- ...\n" > openspec/changes/$CHANGE/proposal.md
printf "## 1. 实现\n- [ ] 1.1 ...\n" > openspec/changes/$CHANGE/tasks.md

# 3) 添加 delta（示例）
cat > openspec/changes/$CHANGE/specs/auth/spec.md << 'EOF'
## ADDED Requirements
### Requirement: 双因素认证
用户 MUST 在登录过程中提供第二认证因子。

#### Scenario: 必须提供 OTP
- **WHEN** 用户提供了有效凭据
- **THEN** 系统要求额外的 OTP 挑战
EOF

# 4) 验证
openspec validate $CHANGE --strict --no-interactive
```

## 多 Capability 示例

```
openspec/changes/add-2fa-notify/
├── proposal.md
├── tasks.md
└── specs/
    ├── auth/
  │   └── spec.md   # ADDED: 双因素认证
    └── notifications/
    └── spec.md   # ADDED: OTP 邮件通知
```

auth/spec.md
```markdown
## ADDED Requirements
### Requirement: 双因素认证
...
```

notifications/spec.md
```markdown
## ADDED Requirements
### Requirement: OTP 邮件通知
...
```

## 最佳实践

### 先求简单
- 默认把新增代码控制在 100 行以内
- 在有明确证据之前，优先单文件实现
- 没有清晰理由时避免引入框架
- 优先选择朴素、经过验证的模式

### 复杂度触发条件
只有在以下情况下才增加复杂度：
- 有性能数据证明当前方案过慢
- 有明确的规模要求（例如超过 1000 用户、超过 100MB 数据）
- 已有多个被验证的用例确实需要抽象

### 清晰引用
- 优先使用插件根目录相对引用，例如 `Source/...:42` 或 `Documents/...`
- 引用 spec 时使用 `specs/auth/spec.md`
- 关联相关 changes 和 PR

### Capability 命名
- 使用动词-名词形式，例如 `user-auth`、`payment-capture`
- 每个 capability 只做一件事
- 遵守“10 分钟可理解”规则
- 如果描述里需要出现 “AND”，就考虑拆分

### Change ID 命名
- 使用 kebab-case，保持简短且可描述，例如 `add-two-factor-auth`
- 优先使用动词前缀：`add-`、`update-`、`remove-`、`refactor-`
- 确保唯一性；如果已被占用，就追加 `-2`、`-3` 等后缀

## 工具选择指南

| 任务 | 工具 | 原因 |
|------|------|------|
| 按模式找文件 | Glob | 模式匹配速度快 |
| 搜索代码内容 | Grep | 正则搜索效率高 |
| 读取具体文件 | Read | 直接读取文件 |
| 探索未知范围 | Task | 适合多步骤排查 |

## 错误恢复

### Change 冲突
1. 运行 `openspec list` 查看活跃 changes
2. 检查是否有重叠的 specs
3. 与相关 change 负责人协调
4. 评估是否应该合并 proposal

### 验证失败
1. 使用 `--strict` 重新运行
2. 查看 JSON 输出里的细节
3. 核对 spec 文件格式
4. 确认 scenarios 的格式正确

### 上下文缺失
1. 先读 `project.md`
2. 检查相关 specs
3. 回看最近的 archives
4. 再决定是否需要补充询问

## 快速参考

### 阶段标识
- `changes/` - 已提出，但尚未落地
- `specs/` - 已落地并部署
- `archive/` - 已完成的 change

### 文件用途
- `proposal.md` - 说明背景与变更内容
- `tasks.md` - 实现步骤
- `design.md` - 技术决策
- `spec.md` - 需求与行为契约

### 常用 CLI
```bash
cd Plugins/TireflyCombatSystem
openspec list              # 当前有哪些在进行中的变更？
openspec show [item]       # 查看详情
openspec validate --strict --no-interactive  # 校验是否正确
openspec archive <change-id> [--yes|-y]  # 标记完成（自动化时加 --yes）
```

请记住：Specs 是事实契约，Changes 是提案。两者必须始终保持同步。
