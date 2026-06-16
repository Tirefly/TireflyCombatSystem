# Tasks: 实现 SkillModifier 机制

## 1. 基础类型定义

- [x] 1.1 创建 `ETcsSkillModifierMergePolicy` 枚举（Stack / Exclusive）
- [x] 1.2 创建三个独立 ModifierInstance USTRUCT（Numeric/Bool/Vector）
- [x] 1.3 创建 `UTcsStateParamNumericModifierExecution` 基类
- [x] 1.4 创建 `UTcsStateParamBoolModifierExecution` 基类 + `UTcsSkillModExec_SetBool`
- [x] 1.5 创建 `UTcsStateParamVectorModifierExecution` 基类 + `UTcsSkillModExec_SetVector`
- [x] 1.6 创建 `UTcsSkillEntrySelector` 基类

## 2. FTcsNumericStateParamInstance 扩展（Bool/Vector 同步扩展）

- [x] 2.1 新增 `ModifierInstances` (TArray<对应类型 ModifierInstance>)
- [x] 2.2 实现 `AssignModifier()`（含 MergePolicy 判定 + bActive 顶替 + Sort）
- [x] 2.3 实现 `RemoveModifiersBySourceHandle()`（含被顶替实例恢复）
- [x] 2.4 实现 `GetModifiedValue()`（沿链调用 Evaluator，仅 bActive==true）

## 3. 内建 Evaluator 子类

- [x] 3.1 `UTcsSkillModExec_Addition` — `Value + Config.Operand`
- [x] 3.2 `UTcsSkillModExec_MultiplyAdditive` — `Value * (1 + Config.Operand)`
- [x] 3.3 `UTcsSkillModExec_MultiplyContinued` — `Value * Config.Operand`
- [x] 3.4 `UTcsSkillModExec_Override` — `Config.Operand`
- [x] 3.5 `UTcsSkillModExec_SetBool` — `Config.Value`
- [x] 3.6 `UTcsSkillModExec_SetVector` — `Config.Value`

## 4. 内建 EntrySelector 子类

- [x] 4.1 `UTcsSkillEntrySelector_ById` — 精确匹配 SkillDefId
- [x] 4.2 `UTcsSkillEntrySelector_ByGameplayTag` — 按 GameplayTag 筛选
- [x] 4.3 `UTcsSkillEntrySelector_All` — 全部已学技能

## 5. UTcsSkillModifierDefinition (DefAsset)

- [x] 5.1 创建 DefAsset 类，含 ModifierId / EntrySelector / EvaluatorClass / Priority / MergePolicy 字段
- [x] 5.2 PrimaryAssetType 注册

## 6. Level 迁移到 NumericParamInstances

- [x] 6.1 `UTcsDeveloperSettings` 新增 `DefaultLevelParamTag`
- [x] 6.2 `UTcsStateDefinition` 基类新增 `LevelParamTag`（构造函数读取 DefaultLevelParamTag）
- [x] 6.3 LearnSkill 时注入 Level 常量到 `NumericParamInstances[LevelParamTag]`
- [x] 6.4 `UTcsSkillEntry::GetLevel()` 改为读取 `NumericParamInstances[LevelTag].GetModifiedValue()`
- [x] 6.5 `UTcsSkillEntry::SetLevel()` 改为写入 `NumericParamInstances[LevelTag].NumericValue`
- [x] 6.6 删除 `UTcsSkillEntry::Level` (int32) 成员字段

## 7. StartCooldown 适配

- [x] 7.1 `StartCooldown()` 改用 `GetModifiedValue()` 获取含修正的 CD 时长

## 8. Spec 同步

- [x] 8.1 `skill-runtime` spec
- [x] 8.2 `state-parameter-management` spec
- [x] 8.3 `state-runtime-access` spec

## 9. 验证

- [x] 9.1 `TireflyGameplayUtilsEditor Win64 Development` 编译通过
- [x] 9.2 `openspec validate implement-skill-modifier --strict` 通过
