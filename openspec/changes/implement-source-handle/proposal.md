# Proposal: 实现 SourceHandle 机制

## 概述

实现 TCS 的 SourceHandle 机制，将 AttributeModifier 的 "SourceName" 从"仅归因字段"升级为"可用于生命周期管理"的统一来源句柄（SourceHandle），并为后续与 State 系统的统一对齐奠定基础。

## 动机

### 当前问题

1. **无法精确撤销**：当前 AttributeModifier 只有 `SourceName` 字段，同一来源的多次应用无法区分，导致只能按 ModifierName/Tag 粗删，无法精确撤销特定的修改器实例。

2. **缺乏统一管理**：技能系统可能同时产生 State 和 AttributeModifier，但二者没有统一的来源标识，无法在技能结束时统一清理。

3. **追踪能力有限**：事件系统中的 `ChangeSourceRecord` 只能按 `FName` 归因，无法提供更详细的来源信息（如施加者、来源对象等）。

4. **生命周期管理困难**：无法实现"按来源撤销"的高级功能，例如：
   - 技能被打断时撤销该技能产生的所有效果
   - Buff 结束时精确移除该 Buff 的属性修改
   - 同一技能多次触发时独立管理每次的效果

### 目标

1. **唯一性**：同一 SourceName 的多次应用可以被区分
2. **可撤销**：支持按来源撤销（Remove by SourceHandle）
3. **可追踪**：事件里能提供详细的来源归因信息
4. **可统一**：同一来源可以同时产生 State + AttributeModifier，二者共享同一 SourceHandle
5. **不强制**：不要求所有项目都必须维护复杂对象引用；句柄可以只包含 Id + DebugName

## 设计原则

1. **向后兼容**：保留旧的 API，通过重载和包装支持旧代码
2. **不依赖资产迁移**：TCS 仍处开发期，可接受 C++/蓝图签名调整
3. **不要求强制 GC 管理**：句柄内部引用使用 `TWeakObjectPtr`，避免延长生命周期
4. **网络/复制友好**：优先满足单机/本地逻辑一致性，为后续网络同步预留扩展空间（使用 `int32` 作为SourceHandle唯一Id）

## 影响范围

### 核心模块

1. **公共结构**：新增 `FTcsSourceHandle` 结构体
2. **属性系统**：
   - `FTcsAttributeModifierInstance` 增加 `SourceHandle` 字段
   - `FTcsAttributeChangeEventPayload` 升级归因记录
   - `UTcsAttributeManagerSubsystem` 增加按句柄操作的 API
3. **状态系统**（后续阶段）：
   - `UTcsStateInstance` 增加 `SourceHandle` 字段
   - 统一移除流程

### 用户影响

- **蓝图用户**：新增 API 可选使用，旧 API 继续工作
- **C++ 用户**：建议迁移到新 API 以获得完整功能
- **数据配置**：无需修改现有数据表

## 实现阶段

### 阶段 1：核心结构（本提案）

1. 新增 `FTcsSourceHandle` 公共结构
2. 属性系统集成 SourceHandle
3. 提供按句柄操作的 API

### 阶段 2：状态系统集成（后续）

1. StateInstance 增加 SourceHandle
2. 统一移除流程
3. StateTree 任务节点支持

### 阶段 3：示例和文档（后续）

1. 添加使用示例
2. 更新文档
3. 迁移指南

## 风险与缓解

### 风险

1. **性能影响**：增加字段可能影响内存占用
   - **缓解**：使用 `TWeakObjectPtr` 和 `int32`，内存增加有限

2. **API 复杂度**：新增 API 可能增加学习成本
   - **缓解**：保留旧 API，提供清晰的迁移文档

3. **网络同步**：`TWeakObjectPtr` 无法复制
   - **缓解**：使用 `int32` 作为主键，指针仅用于 debug

## 成功标准

1. ✅ 可以按 SourceHandle 精确撤销 AttributeModifier
2. ✅ 同一来源的多次应用可以独立管理
3. ✅ 事件系统可以提供详细的来源归因
4. ✅ 旧代码无需修改即可继续工作
5. ✅ 编译通过，无警告
6. ✅ 为后续 State 系统集成预留接口

## 相关文档

- 设计文档：`Documents/文档：SourceHandle机制设计与落地方案.md`
- 架构文档：`CLAUDE.md`