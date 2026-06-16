# Tasks: 实现 Skill 核心

## 1. FTcsStateParamInstance 重构

- [x] 1.1 `NumericValue` 拆分为 `BaseValue` + `ModifierScale` + `ModifierOffset`
- [x] 1.2 `GetNumeric()` 改为 `(BaseValue + ModifierOffset) * ModifierScale`
- [x] 1.3 `Evaluate()` 写入 `BaseValue` 而非 `NumericValue`

## 2. UTcsStateInstance 基类变更

- [x] 2.1 `GetLevel()` 加 `virtual`
- [x] 2.2 新增 `virtual GetStateParamInstance(FGameplayTag)`
- [x] 2.3 新增 `virtual PopulateStateParamInstances(Def, Instigator, Target)`
- [x] 2.4 新增 `virtual GetStateParamInstances()` 返回完整表引用

## 3. CreateStateInstance 清理

- [x] 3.1 删除 `EvaluateAndApplyStateParameters` 调用
- [x] 3.2 inline 代码块替换为 `StateInstance->PopulateStateParamInstances()`
- [x] 3.3 删除 `UTcsStateComponent::EvaluateAndApplyStateParameters` 方法（声明 + 实现）

## 4. SkillDefinition 扩展

- [x] 4.1 新增 `CooldownParam` (FTcsStateParameter)

## 5. SkillEntry 扩展

- [x] 5.1 新增 `Level`、`RemainingCooldown`、`ActiveInstance`、`StateParamInstances`
- [x] 5.2 实现 `InitializeFromDef`、`SetLevel`、`StartCooldown`、`TickCooldown`、`IsOnCooldown`、`GetRemainingCooldownRatio`

## 6. SkillInstance 扩展

- [x] 6.1 覆写 `GetLevel()` → `Entry->GetLevel()`
- [x] 6.2 覆写 `GetStateParamInstance()` → `Entry->StateParamInstances.Find()`
- [x] 6.3 覆写 `GetStateParamInstances()` → `Entry->StateParamInstances`
- [x] 6.4 覆写 `PopulateStateParamInstances()` → 空实现

## 7. SkillComponent 填充

- [x] 7.1 新增 `LearnedSkills` + `ETcsSkillActivateResult` 枚举
- [x] 7.2 实现 `LearnSkill` / `ForgetSkill` / `HasSkill` / `GetSkillEntry`
- [x] 7.3 实现 `ActivateSkill` 完整激活管线
- [x] 7.4 `TickComponent` 中驱动 CD 递减
- [x] 7.5 SkillInstance 移除时清理

## 8. AttributeModifierInstance 引用优化

- [x] 8.1 新增 `SourceStateInstance` + `SourceSkillEntry` 字段
- [x] 8.2 `ResolveStateInstanceFromModifier` → `ResolveStateParamInstances`（三层回退）
- [x] 8.3 `RecalculateAttributeCurrentValues` 中调用方适配

## 9. Spec 同步

- [x] 9.1 `attribute-modifier-runtime` spec：ResolveStateInstanceFromModifier → ResolveStateParamInstances；新增引用字段要求
- [x] 9.2 `state-parameter-management` spec：BaseValue/ModifierScale/ModifierOffset；PopulateStateParamInstances；GetStateParamInstance
- [x] 9.3 `state-runtime-access` spec：新增 PopulateStateParamInstances / GetStateParamInstance / GetStateParamInstances / GetLevel virtual

## 10. 验证

- [x] 10.1 `TireflyGameplayUtilsEditor Win64 Development` 编译通过
- [x] 10.2 `openspec validate implement-skill-core --strict` 通过
