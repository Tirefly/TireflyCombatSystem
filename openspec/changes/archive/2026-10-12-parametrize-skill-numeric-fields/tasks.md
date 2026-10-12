# Tasks：技能数值字段参数化

## 1. 形状层

- [x] 1.1 `TcsCooldownTypes.h`：`FTcsCooldownTrack::Duration` → `FTcsParamValue` + include
- [x] 1.2 `TcsCostTypes.h`：`FTcsCostConfig::Amount` → `FTcsParamValue` + include
- [x] 1.3 `TcsSkillParamKeys.h` / `.cpp`：新增契约键 `Tag_TcsSkillParam_CostAmount`
      （`TcsStateParam.CostAmount`，带 `TCSSKILL_API`）

## 2. 求值内核

- [x] 2.1 `TcsParamChain.h` / `TcsParamChain_Evaluate.cpp`：新增 `FoldParamKeyWithBase`
      （以显式初值折叠；与 `EvaluateParamKey` 并存的归属判据写在头注释）
- [x] 2.2 `TcsCostOps.h` / `.cpp`：新增 `ResolveEffectiveCost`
      （基数求值 ⊕ 减免折叠；**`OutBaseAmount` 出参 MUST 有**——负基数与减免过度分开）
- [x] 2.3 `TcsCooldownOps.cpp`：起冷处装配上下文 + 逐轨载体求值（循环外配一次）

## 3. 调用点接线

- [x] 3.1 `CanAffordCost` / `CommitCost` 增可选 `Entry` 形参 + 接生效值
- [x] 3.2 `CanAffordCost` / `CommitCost` 的**负基数**守卫改判"未钳的基数"
- [x] 3.3 `PayCostForEntry` 传 `Entry`（TOCTOU 重查与提交各一次）
- [x] 3.4 `BroadcastCostApplied` 增 `AppliedAmount` 形参；载荷取实际扣减量
- [x] 3.5 激活路径：门禁⑤ 与 `OnCastStarted` 提交传 `Entry`
- [x] 3.6 `TcsSkillSubsystem::CanAfford` 公开口传 `Learned`（宿主预判与提交点一致）

## 4. 装置

- [x] 4.1 `R65LiteralValue` 辅助（载体字面量构造）+ 5 处赋值改用
- [x] 4.2 `J0` 夹具自证（载体求值 = 配置基数；否则后续检查全恒真）
- [x] 4.3 `J1` 百分比减免 / `J2` 固定值减免及**次序**（19 而非 20）
- [x] 4.4 `J3` 减免过度 ⇒ 钳 0 且恒可付 / `J4` 按来源摘除后回基数
- [x] 4.5 `J5` 冷却侧 `Track.Duration` 载体已接上（Total 非 0）

## 5. 验证

- [x] 5.1 双配置编译 `Result: Succeeded` 且 0 warning / 0 error
- [x] 5.2 无头：`Run` 立即面 + 尾段、（预期 24/27）、`Reject` 7/0
- [x] 5.3 **回归**：`SkillLayer.Run` 24/0 且 `[复现性摘要]` 逐字不变
- [x] 5.4 **★ 拒绝面检查 `d`（负 Amount 恒不可付）MUST 仍 PASS**
      ——它是本次设计缺陷的现场，MUST NOT 为让它变绿而放宽判据
- [x] 5.5 编辑器 PIE 路线：三条命令同读数（第二路线）
- [x] 5.6 `openspec validate parametrize-skill-numeric-fields --strict --no-interactive` 通过

## 6. 台账与文档

- [x] 6.1 台账：`COST-1` / `COOLDOWN-1` **闭合**；`STAT-11` 复核（同根但不同触发面）
- [x] 6.2 台账：**新增"成本组"条目**（多资源 = 新原语；触发条件型）
- [x] 6.3 `SPEC-04-skill` §3.5 / §3.6 就地实施注记（原文保留）
- [x] 6.4 `INDEX` / `GLOSSARY` / 决策日志 / `PLN-R6.5-1` 同步
- [x] 6.5 证据档：新读数 + 那条设计缺陷的全过程

## 7. 归档

- [x] 7.1 `openspec archive parametrize-skill-numeric-fields --yes`
- [x] 7.2 归档后复算：`validate --all --strict`、能力数、**场景数逐能力零丢失**
