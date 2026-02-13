# state-param-eval-strict-check Specification

## Purpose
TBD - created by archiving change fix-attribute-state-semantic-integrity. Update Purpose after archive.
## Requirements
### Requirement: 状态实例创建流程

状态实例创建流程 MUST 包含参数评估验证。

#### Scenario: 创建流程顺序

**Given**: 调用CreateStateInstance

**When**: 执行创建流程

**Then**:
1. 验证输入参数（Actor、StateDefId等）
2. **新增：执行参数评估验证阶段**
3. 创建状态实例对象
4. 初始化状态实例
5. 设置参数值
6. 返回成功

#### Scenario: 参数评估失败提前返回

**Given**: 参数评估验证失败

**When**: 创建流程执行到验证阶段

**Then**:
- 立即返回失败
- 不执行后续步骤
- 不创建任何对象
- 不消耗资源

