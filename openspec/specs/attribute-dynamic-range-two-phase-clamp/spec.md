# attribute-dynamic-range-two-phase-clamp Specification

## Purpose
TBD - created by archiving change fix-attribute-state-semantic-integrity. Update Purpose after archive.
## Requirements
### Requirement: FAttributeValueResolver结构

 MUST 提供属性值解析器，用于指定范围解析时使用的值类型。

#### Scenario: Resolver定义

**Given**: 需要解析动态范围

**When**: 创建Resolver

**Then**:
- Resolver MUST 指定值类型（Base或Current）
- Resolver MUST 提供属性值查询接口
- Resolver MUST 处理属性不存在的情况

#### Scenario: WorkingBase Resolver

**Given**: 解析Base值的动态范围

**When**: 创建WorkingBase Resolver

**Then**:
- Resolver返回属性的WorkingBase值
- WorkingBase = BaseValue + Base修改器效果
- 不包含Current修改器效果

#### Scenario: WorkingCurrent Resolver

**Given**: 解析Current值的动态范围

**When**: 创建WorkingCurrent Resolver

**Then**:
- Resolver返回属性的WorkingCurrent值
- WorkingCurrent = BaseValue + Base修改器 + Current修改器
- 包含所有修改器效果

### Requirement: Clamp函数签名更新

`ClampAttributeValueInRange`  MUST 接受Resolver参数。

#### Scenario: Clamp函数调用

**Given**: 需要clamp属性值

**When**: 调用 `ClampAttributeValueInRange`

**Then**:
-  MUST 传入Resolver参数
- Resolver用于解析动态范围
- Clamp结果基于Resolver提供的值

