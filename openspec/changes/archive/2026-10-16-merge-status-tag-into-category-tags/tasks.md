## 1. 字段合并（共享层）

- [ ] 1.1 `FTcsStateDefBase`：删 `FGameplayTag StatusTag`，增 `FGameplayTagContainer CategoryTags`（**非必填**；注释写明"声明我是什么"vs 消费者侧"我要匹配谁"的分工，以及 `DefTag`（唯一键）与它（语义词）的两通道判据）
- [ ] 1.2 `Source/TcsBuff/Private/State/TcsBuffOps_Snapshot.cpp`：日志读数由 `Def.StatusTag` 改 `Def.CategoryTags`

## 2. 校验面收窄

- [ ] 2.1 `TcsDefinitionSubsystem_State.cpp`：发现期校验**四条 → 三条**（删 `Def.StatusTag` 非空那条及其注释）
- [ ] 2.2 核对：状态侧定义库校验注释的"四条身份级"表述同步

## 3. 选择器侧消歧改名

- [ ] 3.1 `FTcsSkillEntrySelector::CategoryTags` → `ByCategoryTags`（`TcsSkillEntrySelector.h`：字段 + `EditCondition` meta + 头注释）
- [ ] 3.2 `TcsParamChain.cpp`：具名报出文案与注释同步（`TEXT("ByCategoryTags")` 保持，字段读取点改名）
- [ ] 3.3 核对 `skill-param-chain` 规格的字段名引用

## 4. 资产迁移（宿主仓 LAC）

- [ ] 4.1 6 个资产：`StatusTag` → `CategoryTags`（`DA_Check_BuffDef` / `DA_State_E2E` / 4 个 `DA_SkillDef_E2E*`）——**类型由单 tag 变容器**，MUST 走编辑器重存（`PropertyRedirects` 不覆盖类型变更，见 design.md §Decisions）
- [ ] 4.2 `DA_SkillDef_E2E_Replaceable` / `DA_SkillDef_E2E_SinglePhase`：三处 `bInterruptible*` 保持原值迁移（**e0 不改名**，改名归 e1）

## 5. 装置读数同步（宿主仓 LAC）

- [ ] 5.1 `TcsDevSliceRig.cpp` 检查 18：`bStatusTagMatches` 判据由"`StatusTag ==` 定义词"改为**容器包含该词**；读数文案同步
- [ ] 5.2 全仓 grep 复核：`StatusTag` 在 `Source/` 与 `Script/` **零残留**（历史文档留痕除外）

## 6. 规格与台账收口

- [ ] 6.1 台账 `STAT-4` 与 `STAT-10` **合并为一条**（状态词根归属 + 类别容器载体是同一字段的两面）；条目总数 75 → 74，前缀分布同步
- [ ] 6.2 取代登记：`DEC-2026-10-13-relation-row-shape` **文末追加日期注记**（§4.5 的"`StatusTag` 单义"判据已随 `PLN-REFACTOR-1` 瓦解；§4.6 甲案的匹配面读法在 e1 重写）；**正文一字不改**
- [ ] 6.3 `SPEC-02-states` §3.4 / `SPEC-04-skill` §2 的关系字段节：就地注明字段换名与匹配面
- [ ] 6.4 `integration-entity` 规格正文：`Def.StatusTag` 非空校验删除后同步（归档时由 delta 落地）

## 7. 验证（硬门禁）

- [ ] 7.1 UBT **双配置**编译：`LegendAutoChessEditor Win64 Development` + `LegendAutoChess Win64 Shipping`，**零 warning**
- [ ] 7.2 无头装置读数（pwsh 启动）：`Tcs.Test.Slice.Run` 与 `Tcs.Test.State.Run` 的基线不得退化
- [ ] 7.3 **PIE 复跑**（硬门禁：无头通过不构成验收；`Tcs.Test.State.Run` 在无头下不跑）
- [ ] 7.4 资产面人工确认：6 个资产打开零红字、`CategoryTags` 值正确
- [ ] 7.5 提案归档 `openspec archive merge-status-tag-into-category-tags --yes` + `validate --strict` 全绿

## 8. 边界（MUST NOT 越界）

- [ ] 8.1 **不开新根**（`CategoryTags` 本轮仍零解析消费者）；根表 12 与 `## Purpose` 数字**不变**
- [ ] 8.2 **不改事件 tag** `TcsEvent.Cast.Interrupted`（裁定 F：宿主项目该 tag 之后大概率退役）
- [ ] 8.3 **不做** `bInterruptible*` / `ECT_OnCastInterrupted` / `ECFR_Interrupted` / `IsInterruptibleNow` 改名（**归 e1**，与关系表检查器同批）
- [ ] 8.4 **不引入** `FGameplayTagQuery`（`A7` 仍有效）
