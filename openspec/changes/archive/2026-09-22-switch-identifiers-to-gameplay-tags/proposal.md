# Change: 标识体系从 FName 迁移到 GameplayTag（框架原生 + 项目 ini）

## Why

**问题（用户 2026-09-22 指出）**：R3 全量交付后，**所有配置引用都是裸 `FName` 手填**——属性名、参数键、黑板键、链 id、模板 id、DefId 七类引用点，**零个有编辑器期存在性校验**（全库仅 3 个 `IsDataValid`，都只查自身字段非空/双真相）。填错的后果是**静默降级**：

| 引用 | 填错时 |
|---|---|
| `FTcsStepDamage::FlowTemplateId` | 空值**静默兜底 `Default`**（零日志）；填错 → Error + 本步按完成处理 |
| `FTcsParamSource_ParamRef::Key` | **静默落 `Fallback`** |
| 属性名 | `EvaluateCurrent` 属性无实例 → **静默返回 0.0** |
| 黑板键 | `Read` 未知键 → **静默返回 0.0** |

**设计已承诺但未落地**：`08-module-editor-tooling.md:37` 写明"未登记的 DefId = **保存期报错**"，归 R8 轮，至今未做。

**GameplayTag 提供的收益**（用户拍板采纳）：
1. **编辑器 tag picker**——从"手填文本框"变"下拉选择"，源头防拼错
2. **重命名自动修引用**——引擎 `GameplayTagRedirects`（`GameplayTagsSettings.h:44`），FName 无此能力
3. **存在性校验前移**——`FGameplayTag(const FName&)` 是 **protected**（`GameplayTagContainer.h:219`），只能走 `RequestGameplayTag`（默认 `ErrorIfNotFound=true` → ensure）或原生 tag 常量 → **拼错的 tag 在构造那一刻就炸**，而非运行期静默降级
4. **零功能损失**——`FGameplayTag` 的 `IsValid()` / `operator==` / `GetTypeHash` 与现 `FTcsAttributeName` 的三个手写成员**逐字等价**（`GameplayTagContainer.h:138/69/168`）

**D2-1 重开的正当性（显式交代）**：`2026-09-02-m0min-m2-decision-points.md:48` 当年否决"FGameplayTag 当属性 ID"，三条理由是"无编译期检查、字符串比较开销、易拼错"。经核实：
- **"无编译期检查"**——FName 同样没有，**平手**（且 tag 的 protected 构造 + ensure 反而更强）
- **"字符串比较开销"**——`FGameplayTag` 内部就是 `FName`，`GetTypeHash` 实现是 `GetTypeHash(Tag.TagName)`，**逐字相同，平手**
- **"易拼错"**——**tag 反超**（picker + 重定向 + ensure）

故本次是**有据重开**，非无理翻案。

## What Changes

### 落点口径（用户 2026-09-22 拍板：丙方案）

| 词汇类别 | 实例 | 归属 | 声明处 |
|---|---|---|---|
| **事件 tag** | `Tcs.Event.Attribute.ValueChanged` 等 10 个 | 框架 | **插件原生**（不变） |
| **黑板契约键** | `BaseDamage` / `Executed` / `Absorbed` / `Kill` / `Hit` / `Crit` / `ExecuteCandidates` | 步骤库 | **插件原生**（`Tcs.Flow.Key.*`） |
| **项目词汇** | 属性名 / 参数键 / 链 id / 模板 id / DefId | 项目 | **项目 `Config/DefaultGameplayTags.ini`** + 代码侧缓存解析 |

**判据（"谁拥有那个词，谁声明"）**：
- 框架词汇放项目表 → 宿主漏配 → **框架行为被宿主配置破坏**（事件静默丢失）→ 不可接受 → **必须插件原生**
- 项目词汇放项目表 → 项目漏配 → **项目自己的内容配不出来** → 项目自己的事，且可校验拦住 → **必须项目声明**

**关键澄清**：`openspec/project.md:18` 的"never in the project's tag table"**只管事件 tag**（标题即 `Event tag naming standard`，理由原文是 `a missing project config entry would silently drop events`，且 `Rejected alternatives` 明写"project-side **tag declaration**"指事件 tag）。**它不禁止项目给自己的词汇配 tag 表**——本次改造不推翻该禁令，而是**在它之外**为项目词汇开通道。

### 范围（用户拍板"全部替换"）

1. **删除 `FTcsAttributeName`**——属性名直接用 `FGameplayTag`
2. **属性名 → tag**：门面 7 签名 + `ITcsAttributeProvider` 3 反射签名 + `FTcsAttributeStore` 3 容器/5 方法 + `FTcsAttributePipeline` 10 处 + 6 个反射字段
3. **黑板键 → tag**：`FTcsFlowAttributes` 键类型 `FName` → `FGameplayTag`；7 个契约键原生声明
4. **链 id / 模板 id / DefId → tag**：`FTcsEffectChain::ChainId` / `FTcsFlowTemplate::TemplateId` / `UTcsEffectChainDef::ChainId` / `UTcsAttributeDef::DefId` / `FTcsStepDamage::FlowTemplateId`
5. **参数键 → tag**：`FTcsParamSource_ParamRef::Key`
6. **DataTable 行结构拆分**（用户方案）：`RowName` = DA 资产名（FName 引擎硬约束）；行内 `DefTag + DefStruct`
7. **新增项目 ini**：`Config/DefaultGameplayTags.ini`（当前不存在）
8. **新增项目 tag 解析 helper**：首次 `RequestGameplayTag` + 缓存（`RequestGameplayTag` 带 `TScopeLock` + 重定向查询，**不可进热路径**）

### 必须交代的代价（不藏）

1. **资产迁移**：`DA_SliceChain` / `DA_FormulaChain` 含 `FTcsAttributeName` 字段——`SerializeFromMismatchedTag`（`GameplayTagContainer.cpp:1284`）**只支持裸 `FName` 属性 → tag**，**不覆盖结构体字段** → 这 2 个资产**必须重建**。4 个 `UTcsAttributeDef` 的 `DefId` 是裸 `FName` → **可自动升级**
2. **规格改动面**：22 份规格中 **11 份**含 `FName` 引用需 MODIFIED（归档 delta 不改——冻结历史）
3. **`FAttributeRegistry` 的"行名 ↔ 常量映射校验"承诺作废**（tag 无"行名"概念）——`02-module-attributes.md:17` 四项承诺之一失效
4. **热路径时序风险**：项目 tag 首次解析必须晚于 ini 加载（`RequestGameplayTag` 在未加载时返回空 tag + ensure）——本提案定为 **GameInstance 级时机**（`UTcsDevBootstrap` / DefLibrary 同款），MUST NOT 在模块静态初始化期或 `StartupModule` 期解析项目 tag
5. **`PrimaryAssetId` 仍是 `{FPrimaryAssetType, FName}`**——`GetPrimaryAssetId()` 内需 `DefTag.GetTagName()`（用户已确认可接受："代码里直接写成 GetTagName() 我觉得没什么问题"）

### 非目标

- 不做 tag 层级匹配（原生订阅仍是精确匹配，台账 M0-1 未变）
- 不改事件 tag 的既有声明方式（继续插件原生、不进项目表）
- 不做"跨资产 id 引用校验器"（tag 化后 `IsValid()` 天然覆盖大部分；剩余的存在性校验仍归 R8）
- 不做 `PrimaryAssetTypesToScan` 注册（R7-3）
- 不改 DataTable 的 `RowName` 类型（引擎硬约束，FName）

## 与既有规格的关系

**MODIFIED**（11 份）：`attribute-types` / `attribute-store` / `attribute-pipeline` / `attribute-transaction` / `param-value` / `effect-chain` / `effect-chain-asset` / `effect-interpreter` / `effect-step-dispatch` / `damage-flow` / `damage-primitive` / `damage-step-library` / `integration-entity`

**关键口径变化**：
- `attribute-types`：删"属性名包装"需求（`FTcsAttributeName` 整体移除），改"属性身份 = `FGameplayTag`"
- `attribute-store`：全部 `FTcsAttributeName` → `FGameplayTag`
- `damage-step-library`：黑板契约键从"项目词表 FName"改为"框架原生 tag"（`Tcs.Flow.Key.*`）；项目自定义键仍自由，但类型改 tag
- `effect-chain-asset` / `attribute-types`：双真相禁令保留，字段类型改 tag

## 落点与口径

1. **`FGameplayTag` 构造是 protected**（`GameplayTagContainer.h:219`）——代码侧只有两条路：①原生 tag 常量（`FNativeGameplayTag` 有 `operator FGameplayTag()`，零成本隐式转换，`NativeGameplayTags.h:74`）；②`RequestGameplayTag`（带锁，须缓存）。
2. **项目 tag 命名约定**（本提案定，写入 `project.md`）：`Tcs.Attr.<Name>` / `Tcs.Chain.<Id>` / `Tcs.Flow.Template.<Id>` / `Tcs.Param.<Key>`。**与事件 tag 的 `Tcs.Event.*` 并列**，共用 `Tcs` 命名空间但域段隔离。
3. **原生 tag 常量名约定**（沿用 2026-09-21 已定规则）：`常量名 = tag 文本逐点换下划线` → `Tcs.Flow.Key.BaseDamage` → `Tag_Tcs_Flow_Key_BaseDamage`。
4. **`FTcsAttributeName` 删除的正当性**：其全部能力（explicit 保护 / `IsNone` / `operator==` / `GetTypeHash`）tag 都以等价或更强形式提供；保留它是"tag 外套一层空壳"，且要多写三个与 tag 重复的成员。
5. **热路径零成本**：项目 tag 首次解析后缓存为静态 `FGameplayTag`；`FGameplayTag` 内部是 `FName`，比较与哈希与现实现等价。

## 验收

- UBT **Development + Shipping** 双配置编译通过（Shipping 是唯一能照出 `WITH_EDITOR` 类缺陷的检查）
- `Tcs.Test.Slice.Run` 10/10 全绿 + `.Reject` 3/3（回归 Task 6/7 全部验收项）
- **检查点 1/5/6/7 重跑**（属性名 tag 化后须重新实证）
- `openspec validate --specs --strict` 全绿
- **定向人工检查（本次核心收益，必须亲眼验）**：编辑器内属性字段出现 **tag picker**（下拉），而非文本框
