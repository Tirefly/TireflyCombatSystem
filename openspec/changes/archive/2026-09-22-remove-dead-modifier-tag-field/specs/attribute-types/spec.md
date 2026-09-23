## MODIFIED Requirements

### Requirement: 账本修正器与属性实例

`TcsAttribute` MUST 提供账本侧两个**纯 C++ struct**（不进反射面——D2-13 账本形状只为聚合热路径服务）：

- `FTcsAttrModInstance{ Target: FGameplayTag, Op: ETcsAttributeOp, Operand: FTcsAttrModOperand, Source: FTcsSourceHandle, OverridePriority: int32, SortKey: int32 }`——`Source` 是级联撤销锚点（D2-2：来源注销 → 按 Source 全量移除）；`OverridePriority` **仅 `TAO_Override` 读**（其余带忽略），`SortKey` 始终不参与折叠（**2026-09-23 删除：原 `Tag: FName` 字段**——零消费者，见「修正器模板资产与约定列白名单」的同款说明）；
- `FTcsAttributeInstance{ Attr: FGameplayTag, BaseValue: double, CachedCurrent: double, bDirty: bool, Bounds: FTcsAttributeBounds, ValueDomain: ETcsAttributeValueDomain, OverrideTieBreak: ETcsAttrOverrideTieBreak, ModifierSlots: TArray<FTcsAttrModInstance> }`——`CachedCurrent` 是**派生缓存非权威**（聚合管线唯一生产者、惰性重算，D2-8）；`OverrideTieBreak` 由定义侧展开（热路径不回查定义）。

**无分组标签字段（2026-09-23 用户拍板）**：账本修正器**不携带**任何"同来源内分组标签"字段——原 `Tag: FName` 从旧 TCS 搬来且**全库零消费者**（折叠/物化/级联撤销均不读），已删除。若将来出现"同来源内分组"的真实需求，该能力 MUST 以 `FGameplayTag` 形态重新引入（与全系统标识体系一致），**MUST NOT** 复活 `FName` 版本。

#### Scenario: 修正器携带级联锚点

- **WHEN** 同一来源挂两条修正器到不同属性
- **THEN** 两条修正器的 `Source` 相等，可按该句柄一次性全量摘除（D2-2）

#### Scenario: 覆盖优先级随实例进账本

- **WHEN** 一条 `TAO_Override` 修正器以 `OverridePriority = 9` 挂到某属性
- **THEN** 账本条目保留该值，折叠按它参与强弱裁决（非覆盖带上该字段不参与任何计算）

#### Scenario: 账本修正器无分组标签字段

- **WHEN** 检查 `FTcsAttrModInstance` 的字段集
- **THEN** 不含任何分组标签字段（`Target` / `Op` / `Operand` / `Source` / `OverridePriority` / `SortKey` 六项之外无字段）

#### Scenario: 实例具备脏标记位

- **WHEN** 属性实例被新建
- **THEN** 实例带 `bDirty` 标记位且**新建即脏**——初值由聚合管线在"批外读取或提交"时结算（值域收口与既有修正器在该次结算生效）并清零脏标记（见 `attribute-store` 与 `attribute-pipeline` 能力）

### Requirement: 修正器模板资产与约定列白名单

`TcsAttribute` MUST 以**同款双轨组织**提供修正器模板（D3-19 纯模板=默认值，引用处零字段覆写）：两个类型同住 `Attribute/TcsAttrModDef.h`——

- **`FTcsAttrModDefTableRow : FTableRowBase`**（**定义字段的唯一声明处** + **编辑期载体**）：`TemplateTag: FGameplayTag`（身份，2026-09-22 改造——原 `FName TemplateId`）+ `Target: FGameplayTag` / `Op` / `Operand: FTcsAttrModOperandDef` / `ValueConvention: ETcsValueConventionFlag`（D5-18 约定列，物化边界经 `FTcsValueConvention::ConvertToCanonical` 转规范值）/ `OverridePriority`（`TAO_Override` 用，2026-09-18 增补）/ `SortKey`。**行身份 = RowName（= DA 资产名）**，`TemplateTag` 是内容身份；
- **`UTcsAttrModDef : UPrimaryDataAsset`**（Def 资产族统一基类）：持自身 `TemplateTag` 与**组合持有的一行** `Def`——MUST NOT 复制模板字段集；MUST 同款**显式声明 `PrimaryAssetType` + 覆写 `GetPrimaryAssetId()` 返回 `[PrimaryAssetType, TemplateTag.GetTagName()]`**。

**无分组标签字段（2026-09-23 用户拍板）**：模板行**不携带**"同来源内分组标签"字段——原 `Tag: FName` 与账本侧同名同物、同样零消费者，已删除。**删除的额外理由**：该字段在模板行上是 `UPROPERTY(EditAnywhere)`，配置者在编辑器里看得见它、填了却无任何效果——**死字段比缺字段更难查**。若将来出现真实需求，MUST 以 `FGameplayTag` 形态重新引入。

**表格编辑局限（记录在案）**：`Operand.Literal` 是 `FTcsParamValue`（`TInstancedStruct` 载荷），CSV/Excel 往返不保留该列（引擎 CSV 导入无法表达多态实例结构）——本表行只支持**编辑器内表格编辑**，标量列仍可表格批量编辑。

基类取 `UPrimaryDataAsset` 是 **Def 资产族的统一约定**（2026-09-17 拍板）——Def 引用语义本是"id + 注册表/DefLibrary 解析"，主资产身份让"id ↔ 资产"解析、按类型发现/加载与打包分块归属由引擎提供（未来 `UTcsStateDef` 家族与 `UTcsSkillModDef` 同此基类）。

MUST 在 `IsDataValid`（`WITH_EDITOR`）实现 **D5-18 v3 约定列白名单**：约定列非 `VCF_None` 时，其数值来源 MUST 允许约定（判据由源自身声明——见 `param-value` 能力的约定能力位）；`ParamRef` 与 `AttributeScaled` 禁配（前者二次转换、后者约定作用对象有歧义）——违者报错且**错误挂该配置元素**（可操作建议，非仅日志）。同时 MUST 校验：`TemplateTag` 无效、数值来源为空、`OPK_AttributeScaled` 而属性 tag 无效、`Target` 无效。
**覆盖优先级的适用范围 MUST 给警告（非错误）**：`Op != TAO_Override` 而行内 `OverridePriority != 0` 时，该值不参与折叠——配置本身无害，但**不得静默**（2026-09-18 增补）。

#### Scenario: 模板双轨同款组织

- **WHEN** 检查修正器模板的资产与表行
- **THEN** 资产持 `TemplateTag` + 一行 `Def`，字段集只在表行声明一次（资产不复制字段）

#### Scenario: 模板行无分组标签字段

- **WHEN** 在编辑器里检视 `FTcsAttrModDefTableRow` 的字段
- **THEN** 不含任何"同来源内分组标签"字段（不给配置者一个填了无效果的控件）

#### Scenario: 字面量源可配约定

- **WHEN** 模板配 `Literal` 源 + `VCF_Percent`
- **THEN** 物化边界按 `ConvertToCanonical` 转规范值（策划写 85 → 账本 0.85）
