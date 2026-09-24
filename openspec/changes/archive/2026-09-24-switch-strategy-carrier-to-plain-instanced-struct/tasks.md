## 1. 换型（4 处字段定义）

- [x] 1.1 `TcsCore/Public/Parameter/TcsParamValue.h`：`TInstancedStruct<FTcsParamValueSource> Source` → `FInstancedStruct Source` + `meta = (BaseStruct = "/Script/TcsCore.TcsParamValueSource")`；默认构造改用 `Source.InitializeAs<FTcsParamSource_Literal>()`；`Evaluate` 转发改用 `GetPtr` 判空
- [x] 1.2 `TcsTargeting/Public/Chain/TcsStepSelectTargets.h`：`Selector` → `FInstancedStruct` + BaseStruct；`Filters` → `TArray<FInstancedStruct>` + BaseStruct
- [x] 1.3 全库 grep `TInstancedStruct` 复核是否有遗漏字段（`TcsAttrModInstance` 等）
- [x] 1.4 头注释同步：写清"为什么是裸形态"（脚本层可配）+ 代价（丢编译期限定、picker 靠手写 metadata）

## 2. 调用点适配

- [x] 2.1 `TcsTargeting/Private/Chain/TcsStepSelectTargets.cpp`：`GetPtr` 调用点复核（裸版语义一致，但确认判空纪律在位）
- [x] 2.2 `TcsAttribute/Private/Attribute/TcsAttrModDef.cpp`：`Source.IsValid()` / `Source.Get()` / `Source.GetScriptStruct()` 适配
- [x] 2.3 `TcsDamage/Private/Flow/Steps/TcsFlowStepsCore.cpp` / `TcsFlowStepsRest.cpp`：`Operand.Source.GetMutable<FTcsParamSource_Literal>()` 适配（裸版 `GetMutable<T>` 无编译期限定，语法不变）
- [x] 2.4 编译验证：UBT Development 通过

## 3. glue 产物验证（本提案核心判据）

- [x] 3.1 读 `TcsCore/TcsParamValue.generated.cs`——**确认 `Source` 字段生成真实读写代码**（含 `FInstancedStructMarshaller`，非空函数体）
- [x] 3.2 读 `TcsTargeting/TcsStepSelectTargets.generated.cs`——确认 `Selector` / `Filters` 字段可读写
- [x] 3.3 记录实测结果（这是"能导出 ≠ 能往返"的验证点，见 `MEM-20260924-04`）

## 4. 资产兼容性与 picker 实测

- [x] 4.1 编辑器打开 `DA_SliceChain` / `DA_FormulaChain`——确认**内层类型正确反序列化**（步骤类型名与参数源配置保持原值，非空）
- [x] 4.2 逐个字段确认 picker 只列该族类型（`FTcsParamValueSource` / `FTcsTargetSelectorStrategy` / `FTcsTargetFilterStrategy` 派生）
- [x] 4.3 确认基类（`Hidden`）不在 picker 列表中

## 5. 三步骤链实证（补跑）

- [x] 5.1 扩展 `Script/LegendAutoChessCS/TcsProbe/TcsChainProbe.cs`：链内容改为**三步**「选目标（`FTcsStepSelectTargets` + `FTcsSelSelf`）→ 伤害（`FTcsStepDamage`）→ 等待（`FTcsStepWaitDelay`）」
- [x] 5.2 关键验证：**脚本层能构造 `FTcsStepSelectTargets` 并配置 `Selector`**（这正是此前空壳导致做不到的事）
- [x] 5.3 PIE 实测：确认三步链跑通、选目标生效（`Context.Targets` 被正确填充）
- [x] 5.4 记录实测边界（若仍有配不了的字段，如实记录）

## 6. 文档同步与收束

- [x] 6.1 台账 `deferred-inputs-ledger.md`：S-6 标"已消费"（含换型落点与兼容性实证结论）
- [x] 6.2 设计文档：`01-module-m0-core.md`（FTcsParamValue 载体描述）、`04-module-effects.md`（策略载体）、`10-module-targeting.md`（选择器/过滤器载体）——三处 `TInstancedStruct` 口径改裸形态
- [x] 6.3 `openspec/project.md`：`Strategy unification` 条与 `FTcsParamValue` 条的载体描述同步
- [x] 6.4 调研文档 §11.4：S-6 缺口标"已解决"
- [x] 6.5 提案归档 + 规格库全绿（`openspec validate --all --strict`）

---

## 实测结论（2026-09-24 完成，全部判据通过）

| 判据 | 实测结果 |
|---|---|
| UBT 编译 | ✅ `Result: Succeeded` |
| **glue 字段可读写**（核心判据） | ✅ `FTcsParamValue.Source` / `FTcsStepSelectTargets.Selector`·`Filters` 均生成真实读写代码（含 `StructMarshaller<FInstancedStruct>` / `ArrayCopyMarshaller`），**非空壳**——对比换型前零字段 |
| **资产兼容性** | ✅ `链定义 2/2 条，失败 0 条`；`Formula_Chain 步数=7`、`Slice_Chain 步数=3`（两个含 `TInstancedStruct` 数据的资产**原样加载**）——三重论证成立 |
| picker 限定 | ✅ 编辑器确认配置正常（`BaseStruct` metadata 生效） |
| **三步骤链**（补跑） | ✅ `SelectTargets[..]: 候选 1 → 通过 1（Filter 数 0）`——**换型前不可能**（当时构造不出该步骤）；链 `步数=3` → `PC=2 挂起` → `唤醒` → `完成（共 3 步）` |
| **脚本层配数值** | ✅ `DamageBase=25` 生效：`[属性变更] Health：100.000 → 75.000`、`[伤害记录] 输入 25.000 / 执行 25.000`、`第 5.5 步 OK：Health = 75` |
| 零红字 | ✅ `UnregisterChain` 为 Warning（S-1 修复保持有效） |

**结论**：`TInstancedStruct<T>` → 裸 `FInstancedStruct` 换型达成全部目标——脚本层从"配不了任何数值/拼不出选目标步骤"变为**可构造、可配置、值完整往返**，且**不破坏已存资产**。
