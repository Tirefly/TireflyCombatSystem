# Tasks: 标识体系从 FName 迁移到 GameplayTag

## 1. 项目侧 tag 声明（ini）

- [ ] 1.1 新建 `Config/DefaultGameplayTags.ini`（宿主项目根，当前不存在）
- [ ] 1.2 声明属性名 tag：`Tcs.Attr.Health` / `Tcs.Attr.MaxHealth` / `Tcs.Attr.Attack` / `Tcs.Attr.Armor`
- [ ] 1.3 声明链 id tag：`Tcs.Chain.Slice_Chain` / `Tcs.Chain.Formula_Chain`
- [ ] 1.4 声明流程模板 id tag：`Tcs.Flow.Template.Default_Slice` / `Tcs.Flow.Template.Slice_Flow`（`Default` 属框架词汇，由插件原生声明——见 2.6）
- [ ] 1.5 启动编辑器验证：Project Settings → GameplayTags 能看到上述 tag（**定向人工检查**）

## 2. 插件侧：框架词汇原生声明

- [ ] 2.1 新增 `Source/TcsDamage/Public/Flow/TcsFlowKeys.h`（框架契约键原生声明头）+ `Private/Flow/TcsFlowKeys.cpp`（`UE_DEFINE_GAMEPLAY_TAG`）
- [ ] 2.2 声明 7 个契约键：`Tcs.Flow.Key.BaseDamage` / `Executed` / `Absorbed` / `Kill` / `Hit` / `Crit` / `ExecuteCandidates`（常量名逐点换下划线）
- [ ] 2.3 声明官方默认模板 tag：`Tcs.Flow.Template.Default`
- [ ] 2.4 **导出宏纪律**：声明处 MUST 带 `TCSDAMAGE_API`（`UE_DECLARE_GAMEPLAY_TAG_EXTERN` 展开为裸 extern，跨模块引用会 LNK2001——台账 T-9 已记）
- [ ] 2.5 清理 `TcsFlowStepsCore.cpp` / `TcsFlowStepsRest.cpp` 的裸字面量与死常量（`TcsFlowRestKey_HitRate` / `TcsFlowRestKey_CritRate` 声明后未使用）

## 3. 插件侧：属性名 tag 化（TcsAttribute 为主）

- [ ] 3.1 **删除** `Source/TcsAttribute/Public/Attribute/TcsAttributeName.h`（+ 18 处 `#include`）
- [ ] 3.2 `UTcsAttributeSubsystem` 7 个签名：属性参数 `FTcsAttributeName` → `FGameplayTag`
- [ ] 3.3 `ITcsAttributeProvider` 3 个 `UFUNCTION(BlueprintNativeEvent)` 签名同步
- [ ] 3.4 `FTcsAttributeStore`：3 个容器键 + 5 个方法签名
- [ ] 3.5 `FTcsAttributePipeline` 10 处（含私有签名与 Tarjan 局部容器）
- [ ] 3.6 `UTcsCombatEntityComponent::GetCurrent` 签名
- [ ] 3.7 反射字段 6 处：`FTcsAttrModOperandDef::Attribute` / `FTcsAttributeBound::DynamicAttribute` / `FTcsParamSource_AttributeScaled::Attribute` / `FTcsAttributeChangedEvent::Attribute` / `FTcsStepDamage::TargetAttrKey` / `FTcsAttrModDefTableRow::Target`
- [ ] 3.8 成员访问改写：19 处 `.Name` → `GetTagName()`；11 处 `IsNone()` → `!IsValid()`
- [ ] 3.9 账本 `FTcsAttrModInstance::Tag` / `FTcsAttrModDefTableRow::Tag` **保持 `FName`**（分组标签，非配置引用——MUST NOT 误改）

## 4. 插件侧：定义资产结构拆分（tag 身份）

- [ ] 4.1 新增 `FTcsAttributeDefData`（USTRUCT，抽定义字段：`BaseValue` / `Bounds` / `ValueDomain` / `OverrideTieBreak`）
- [ ] 4.2 `FTcsAttributeDefTableRow` 改为 `DefTag: FGameplayTag` + `Def: FTcsAttributeDefData`
- [ ] 4.3 `UTcsAttributeDef` 字段 `DefId: FName` → `DefTag: FGameplayTag`；`GetPrimaryAssetId()` 用 `DefTag.GetTagName()`
- [ ] 4.4 `UTcsAttributeDef::IsDataValid`：空判定改 `!DefTag.IsValid()`
- [ ] 4.5 `UTcsAttrModDef` 同款：`TemplateId: FName` → `TemplateTag: FGameplayTag` + `GetPrimaryAssetId()` 同步
- [ ] 4.6 `RegisterAttributeDef` 签名：第二参从 `FTcsAttributeDefTableRow` 改为 `FTcsAttributeDefData`（行与数据分离）

## 5. 插件侧：链 id / 模板 id tag 化

- [ ] 5.1 `FTcsEffectChain::ChainId` / `UTcsEffectChainDef::ChainId` → `FGameplayTag`
- [ ] 5.2 `UTcsEffectSubsystem`：`RegisterChain` / `UnregisterChain` / `FindChain` / `ExecuteChain` 签名 + 登记表键类型
- [ ] 5.3 `FTcsFlowTemplate::TemplateId` / `UTcsDamageSubsystem` 同款
- [ ] 5.4 `FTcsChainRun::ChainId` 同步
- [ ] 5.5 `FTcsParamSource_ParamRef::Key` + `ITcsParamTableReader::TryGetNumericParam` 键类型
- [ ] 5.6 `FTcsEffectContext::Variables` 键类型
- [ ] 5.7 `FTcsFlowAttributes` 键类型 + `FTcsFlowModify::TargetKey` / `FTcsFlowDelegate::TargetKey`

## 6. 宿主侧适配（`Source/TcsDev/`）

- [ ] 6.1 `UTcsDevBootstrap::Get*Name()` 6 个返回类型 `FName` → `FGameplayTag`（改为缓存解析项目 tag）
- [ ] 6.2 **项目 tag 缓存解析 helper**：首次 `RequestGameplayTag` + 缓存成静态 `FGameplayTag`（`RequestGameplayTag` 带锁，MUST NOT 进热路径）
- [ ] 6.3 **时序纪律**：解析时机 MUST 晚于 ini 加载（定在 GameInstance 级——`UTcsDevBootstrap::Initialize`），MUST NOT 在模块静态初始化期或 `StartupModule` 期
- [ ] 6.4 `TcsDevDamageFormula` / `TcsDevFlowSteps` 字段类型同步
- [ ] 6.5 `TcsDevSliceRig` 16 处构造点改写（删 `FTcsAttributeName(...)` 包装）
- [ ] 6.6 `TcsDevScreenObserver` 的 `.Name.ToString()` → `GetTagName().ToString()`

## 7. 内容资产迁移

- [ ] 7.1 **重建** `DA_SliceChain`（含 `FTcsAttributeName` 字段，无自动升级路径）
- [ ] 7.2 **重建** `DA_FormulaChain`（同上）
- [ ] 7.3 4 个 `UTcsAttributeDef`：`DefId` 是裸 `FName` → 引擎 `SerializeFromMismatchedTag` 可自动升级，但**新增的 `DefTag` 字段需手工补填**（升级只填旧字段）
- [ ] 7.4 验证 `Content/TcsDev/` 全部资产 `IsDataValid` 无错

## 8. 验证（禁 TDD：编译 + 定向人工检查）

- [ ] 8.1 UBT **Development** 编译通过
- [ ] 8.2 UBT **Shipping** 编译通过（唯一能照出 `WITH_EDITOR` 类缺陷的检查）
- [ ] 8.3 `Tcs.Test.Slice.Run` **10/10 全绿、零红字**
- [ ] 8.4 `Tcs.Test.Slice.Reject` **3/3**
- [ ] 8.5 **检查点 1 重跑**：`Tcs.Test.Slice.Run Formula_Chain` → 公式 25 + clamp 到 0
- [ ] 8.6 **检查点 5 重跑**：依赖铁律 grep（`^#include` 精确匹配）
- [ ] 8.7 **检查点 6 重跑**：改链资产数值零 C++ 生效
- [ ] 8.8 **检查点 7 重跑**：`slomo 0.1` 实时:战斗 = 10:1 + `pause` 冻结
- [ ] 8.9 **定向人工检查（本次核心收益）**：编辑器内打开 `DA_SliceChain`，确认 `FlowTemplateId` / `TargetAttrKey` 字段显示为 **tag picker（下拉）** 而非文本框
- [ ] 8.10 `openspec validate --specs --strict` 全绿

## 9. 文档回写

- [ ] 9.1 `openspec/project.md`：Def 身份标准 / 事件 tag 标准 → 更新为 tag 口径；新增项目 tag 命名约定（`Tcs.Attr.*` / `Tcs.Chain.*` / `Tcs.Flow.Template.*` / `Tcs.Param.*`）
- [ ] 9.2 `Documents/combat-system-design/2026-09-02-r3-plan2-damage-chain.md`：Task 7 注记追加"tag 化改造"节
- [ ] 9.3 `Documents/combat-system-design/README.md`：决策日志追加 D2-1 重开条目 + 进度行
- [ ] 9.4 `Documents/combat-system-design/deferred-inputs-ledger.md`：R8-3 的 `FAttributeRegistry` 承诺更新（"行名 ↔ 常量映射校验"作废）；新增"跨资产 id 引用校验"条目（tag 化后由 `IsValid()` 部分覆盖，剩余归 R8）
- [ ] 9.5 `Documents/combat-system-design/02-module-attributes.md`：D2-1 段落更新为 tag 口径
- [ ] 9.6 本提案归档（`openspec archive`，delta 自动合入主 spec）
