# Change: 删除修正器族无消费者的 `Tag` 字段

## Why

`FTcsAttrModInstance` 与 `FTcsAttrModDefTableRow` 各有一个 `FName Tag` 字段（注释写"同来源内分组标签（可选）"），从旧 TCS 搬来（plan1 sketch `plan1-core-attributes.md:422`）。用户 2026-09-23 确认：**当初想给 Attribute 加标签方便归类，现在确实不需要**。

两个事实使删除成为正确处置：

1. **零消费者**——全库对 `Source/TcsAttribute/` 的 `.Tag` / `->Tag` 读写搜索**一处命中都没有**（命中的全是 `DefTag` / `TemplateTag` / `GetTagName()`）。折叠器不接收它、物化器不读它、级联撤销不读它。
2. **留着有害**——模板侧那份带 `UPROPERTY(EditAnywhere, BlueprintReadOnly)`，配置者在编辑器里看到"同来源内分组标签"会以为填了有用；实际是死配置。**死字段比缺字段更难查**（配置者会以为是自己的用法不对）。

**为什么不改成 `FGameplayTag`**：2026-09-22 tag 化改造的口径是"**内容引用**改 tag"，而这个字段不承担任何引用语义（它不指向 Def / 参数键 / 黑板键）。给一个零消费者的字段换类型只是把问题包起来。若将来真需要"同来源内分组"，那时它应是 `FGameplayTag`（与全系统一致），而非 FName。

## What Changes

- `attribute-types` 的「账本修正器与属性实例」需求：`FTcsAttrModInstance` 形状去掉 `Tag: FName`。
- `attribute-types` 的「修正器模板资产与约定列白名单」需求：`FTcsAttrModDefTableRow` 字段清单去掉 `Tag`。

## 影响面

- **Affected specs**: `attribute-types`（MODIFIED × 2）
- **Affected code**:
  - `Source/TcsAttribute/Public/Attribute/TcsAttrModInstance.h`（删字段 + 留删除说明注释）
  - `Source/TcsAttribute/Public/Attribute/TcsAttrModDef.h`（删字段 + 留删除说明注释）
- **Affected docs**: `02-module-attributes.md` §2.2 权威形状行、§2.2a 实例形状行。
- **不影响**：`SortKey` 保留（它在 `damage-flow` 侧有真实语义——消耗裁决选一）；任何行为（零消费者 ⇒ 删除是纯形状收敛）。

## 非目标

- **不做**"同来源内分组"的替代机制：需求不存在就不预建（与 `Custom 逃逸位`/`DevSettings` 同款"零消费者不预建设施"纪律）。
- **不改** `SortKey`（`attribute-pipeline` / `attribute-types` 有专门场景钉住它"零语义但保留作审计位"）。
