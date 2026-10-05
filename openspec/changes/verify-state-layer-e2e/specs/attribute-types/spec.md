## ADDED Requirements
### Requirement: 修正器模板身份词（TemplateTag）

`UTcsAttrModDef::TemplateTag` 是模板的内容身份，MUST 取 `AttrModDef` 根下的词（`AttrModDef.<名>`，2 段），
且 MUST 由**宿主** `Config/DefaultGameplayTags.ini` 声明（属宿主内容词，插件 MUST NOT 声明它）。

**身份三用（MUST 一致）**：该词同时是 ①资产主身份名（`GetPrimaryAssetId()` = `[PrimaryAssetType, TemplateTag.GetTagName()]`）、
②发现期去重键（`UTcsDefinitionSubsystem::DiscoverAttrModDefs`）、③作者侧校验对象（`IsDataValid` 判其有效）——三者 MUST 用同一个词，MUST NOT 各用一套。

**根归属（2026-10-05 定，台账 `ATTR-1` 的闭合）**：`TemplateTag` 此前**零解析消费者**，故 `ATTR-1` 记"不为它开根、归属 = 触发条件"；
本变更落地了那条解析路径（`DiscoverAttrModDefs` / `ResolveAttrModDef`），触发条件成立 ⇒ 新立 `AttrModDef` 根（见 `gameplay-tag-governance` 能力）。
**MUST NOT 借用 `StateDef` 根**（那是状态定义资产的身份角色；不同代码路径 = 不同角色 = 不同根）。

**运行期取用现状（MUST 如实记录，防过度声称）**：`ModifierRows` 走的是**资产直引用**（`TSoftObjectPtr<UTcsAttrModDef>`），
物化器只 `Get()` / `LoadSynchronous()` 取对象、读其 `->Def`，**从不读 `TemplateTag`**。故本轮交付的是**身份解析与索引**
（可寻址、可去重、可在编辑器里被校验），**不是**"按 tag 取模板供运行期物化"——后者的调用者为零，如实入边界清单。

#### Scenario: 模板身份词住 AttrModDef 根

- **WHEN** 为一个 `UTcsAttrModDef` 资产填写 `TemplateTag`
- **THEN** 该词 MUST 形如 `AttrModDef.<名>`（2 段）并由宿主 ini 声明；MUST NOT 落在 `StateDef` / `Attribute` 或其它根下

#### Scenario: 身份无效则两道校验各自报错

- **WHEN** `TemplateTag` 无效（空 tag）
- **THEN** `IsDataValid` 报错（既有判据，见本能力"修正器模板资产与约定列白名单"），且发现期同样把该资产计入失败清单——
  两道校验 MUST NOT 互相替代：`IsDataValid` 管作者侧可操作性，发现期管"能否被按 tag 寻址"

#### Scenario: 模板身份词不参与运行期物化

- **WHEN** 状态施加时物化 `ModifierRows` 引用的模板资产
- **THEN** 取用的是资产对象与其 `Def` 内容（`Get()` / `LoadSynchronous()`），从不按 `TemplateTag` 反查；
  内容侧 MUST NOT 假设框架会按该词解析模板
