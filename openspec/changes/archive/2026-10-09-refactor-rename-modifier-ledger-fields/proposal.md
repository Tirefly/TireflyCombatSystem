# Change: 修正器账本字段改名（`ModifierSlots` → `AttrModInstances`、`ParamSlots` → `NumericParamModInstances`）

## Why

两个账本容器的字段名用了 `Slots`，但它们的**形状与语义都不是槽位**：

1. **沿袭自一个已不存在的包装层**：`Slots` 来自 AbilityKit 的"修改器槽位自由链表"（`ModifierSlot{Handle, ModifierData, NextFree, Active}` + handle→槽位索引）。TCS 拆掉了槽壳、元素直接就是修正器条目（`FTcsAttrModInstance` / `FTcsNumericParamModInstance`），容器名却留在原地。
2. **今天没有任何使用点支撑"槽位"这个词**：全仓 `ModifierSlots[` / `ParamSlots[` / `.Insert` / `.RemoveAt` **零命中**——只有 `.Add` / `.RemoveAll` / `.Num()` / range-for。既不可按下标寻址，也不复用，也不带代际。
3. **同一文件内撞词**：`FTcsSkillRegistry.h` 里真有一个槽位族（`AllocateSlot` / `ReleaseSlot` / `SlotGenerations` / `FreeSlots`），而 `ParamSlots` 就住在同一个头、隔二十行——同一个词在同一个文件里有两个互不兼容的意思。M2 侧当时无害（属性容器是 `TMap`，整个文件没有槽位词汇），M5 侧把条目放进"本身就是槽位桶"的类型隔壁，歧义才成立。
4. **同族命名法不一致**：`FTcsLearnedSkillEntry` 的另一个集合字段是 `RunHandles`（按内容命名），设计字段表（`SPEC-04-skill` §3.1）本就把两者写作对称的"运行句柄集 / 参数修正链集"——一个按内容、一个按机制，对称被拆散。

不改的代价会随 `R6.5-e` 扩大：布尔侧预留的第三个"内容数组叫 Slots"（`BoolSlots`）一落地，歧义从两处变三处。

## What Changes

- **BREAKING（编译期，无序列化面）**：
  - `FTcsAttributeInstance::ModifierSlots` → **`AttrModInstances`**（`TArray<FTcsAttrModInstance>`）
  - `FTcsLearnedSkillEntry::ParamSlots` → **`NumericParamModInstances`**（`TArray<FTcsNumericParamModInstance>`）
- **新名判据**：**容器字段名 = 元素类型名的复数形式**。`FTcsAttrModInstance` → `AttrModInstances`、`FTcsNumericParamModInstance` → `NumericParamModInstances`。可自检——写出来的字段名必须能从元素类型名推出来（`ModifierSlots` 推不出来，元素叫 `Instance`）。
- **同批立规则**：**`Slot` 一词保留给"可按下标寻址、可复用、带代际的空位"**（`FreeSlots` / `PoolSlots` / `StateEventSlots` / `FailedSlots` 一类的真槽位）。非槽位的容器 MUST 按内容命名。
- **同批的派生命名**（不跟着改会在同一文件里制造新的名实不符）：`FoldParamChainSlots` → `FoldParamChainInstances`、`StripParamChainSlotsBySource` → `StripParamChainInstancesBySource`、`CountLedgerModifierSlots` → `CountAttrModInstances`、`CountRigLedgerSlots` → `CountRigAttrModInstances`、`#pragma region ParamSlots` → `#pragma region NumericParamModInstances`、`R6.5-e` 预留注释里的 `BoolSlots` → **`BoolSwitchModInstances`**。
- **同批划掉一处假声称**：`SPEC-02-attributes` 原写 `ModifierSlots` 是"槽位自由链表/数组 + freelist，M0 池机制复用"——实测 `TcsAttribute` 模块既无 `FreeList` 也无 `FreeSlots` / `AllocateSlot`（全仓 `FreeList` 只存在于 `TcsInstancePool.h` 与 `TcsCoreStats.h`）。该声称**与命名无关地假**，按"保留原句 + 就地划改"体例处置。
- **MUST NOT 改的**：真槽位族（`FreeSlots` / `PoolSlots` / `GTcsCorePoolSlots*` / `StateEventSlots` / `CountFailedSlots` / `FailedSlots`）一律保留——它们确实是槽位，本次改名正是为了让这个词只指它们。

## Impact

- **Affected specs**：`attribute-types`（"账本修正器与属性实例" MODIFIED）、`attribute-store`（"属性定义表与单位侧添加移除" MODIFIED）
- **Affected code**：
  - TCS `TcsAttribute`：`Attribute/TcsAttributeInstance.h`、`Attribute/TcsAttrModInstance.h`、`Attribute/TcsAttributePipeline.cpp`、`Attribute/TcsAttributePipeline_Cascade.cpp`、`TcsAttributeSubsystem.cpp`
  - TCS `TcsSkill`：`Skill/TcsSkillRegistry.h`、`Skill/TcsNumericParamModifier.h`、`Skill/TcsParamChain.h`、`Skill/TcsParamChain.cpp`、`Skill/TcsParamChain_Evaluate.cpp`、`Skill/TcsParamChain_Materialize.cpp`、`Skill/TcsSkillOps_ParamRead.cpp`、`TcsSkillSubsystem.h`
  - LAC `TcsDev`：`Private/Dev/TcsDevSliceRig.cpp`、`Private/Dev/TcsDevSliceRig_State.cpp`、`Private/Dev/TcsDevSkillParamChainProbe.cpp`
  - 未归档草案 `add-skill-param-chain`：M5 字段随草案**就地改名**（该字段尚未进生效规格，故本提案不为其立 delta）
- **零迁移风险（已实测）**：两个宿主结构体都是**裸 `struct`（非反射）**——按既有 `NewProp_*` 判据复验，`Intermediate/` 与 `Binaries/` 里两个旧名命中 **0**，UHT 生成的 `*.generated.h` / `*.generated.cs` 命中 **0**。⇒ 无 `UPROPERTY`、无资产序列化、无存档、无 C# 面、无蓝图面，改完即生效。
- **文档分轨**（按 `CONVENTION` §7）：生效规格（`SPEC`）与未归档草案、当前轮活计划（`PLN-R6`）改正文；**冻结文档（`DEC` / 已冻结 `PLN` / `EVID` / `RSCH` / `LOG`）与 `openspec/changes/archive/` 正文不改**，只追加实施注记。
