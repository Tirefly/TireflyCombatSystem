## 1. 源码改名（TCS）

- [x] 1.1 `TcsAttribute`：`FTcsAttributeInstance::ModifierSlots` → `AttrModInstances`（字段声明 + 全部使用点 + 注释里的字段名）
- [x] 1.2 `TcsSkill`：`FTcsLearnedSkillEntry::ParamSlots` → `NumericParamModInstances`（字段声明 + 全部使用点 + `#pragma region`）
- [x] 1.3 派生命名：`FoldParamChainSlots` → `FoldParamChainInstances`、`StripParamChainSlotsBySource` → `StripParamChainInstancesBySource`、`CountLedgerModifierSlots` → `CountAttrModInstances`、`CountRigLedgerSlots` → `CountRigAttrModInstances`
- [x] 1.4 `R6.5-e` 预留注释：`BoolSlots` → `BoolSwitchModInstances`，并清理同处自认写错的旧名（`FTcsBoolSwitchParamInstance`）
- [x] 1.5 两个字段的类型注释里写明新命名规则（"容器字段名 = 元素类型名复数；`Slot` 保留给可寻址/可复用的空位"）

## 2. 源码改名（LAC 装置）

- [x] 2.1 `TcsDevSliceRig.cpp` / `TcsDevSliceRig_State.cpp`：字段引用跟进；**装置局部名按用户裁决保持不动**（`ModArmorSlots*` / `ModAttackSlots*` / `NullRefSlots` / `SlotsA`–`SlotsH` / 摘要标签 `Slots=%d`）
- [x] 2.2 `TcsDevSkillParamChainProbe.cpp`：字段引用跟进（`SlotsABefore` 等局部名同样保持不动）

## 3. 文档

- [x] 3.1 `SPEC-02-attributes`：字段改名 + **划掉 freelist 假声称**（保留原句 + 就地划改）
- [x] 3.2 `PLN-R6`：字段改名 + 追加本次改名落地记录（三跳谱系 + 命名规则 + 派生命名 + 验证读数 + 环境教训）
- [x] 3.3 未归档草案 `add-skill-param-chain`：草案就地改名（`proposal.md` / `tasks.md` / 四个 delta 规格，共 36 处）
- [x] 3.4 冻结文档 10 篇（`DEC-03` / `PLN-R3-1` / `PLN-R5` / `EVID-*`×2 / `LOG-*`×3 / `LEDGER-deferred` / `RSCH-r6-task4`）：**正文不改**，各追加实施注记（每篇恰 1 处，幂等）

## 4. 验证

- [x] 4.1 双配置编译 —— `UnrealEditor Win64 Development` = `Result: Succeeded`（0 error / 0 warning）+ `LegendAutoChess Win64 Shipping` = `Result: Succeeded`（0 error / 0 warning，产物 `LegendAutoChess-Win64-Shipping.exe` 真重编）
- [x] 4.2 旧名残留反查 —— `Source/` 层 **0 处**（仅剩两处**有意留痕**的类型注释"本条原为 X"）；活文档与草案 **0 处**（仅剩改名记录表里的对照列）；冻结文档 28 处旧名**全部在注记覆盖下**
- [x] 4.3 运行期回归 —— `Tcs.Test.SkillParamChain.Run` **26/0**、`.Reject` **9/0**、`Tcs.Test.SkillCast.Run` **26/0**、`.Reject` **10/0**（与各自基线逐项一致，零红字 / 零 ensure / 零 Fatal）
- [x] 4.4 `openspec validate --all --strict --no-interactive` = **39 passed / 0 failed**
- [x] 4.5 真槽位族未被误伤 —— `FreeSlots` / `PoolSlots` / `StateEventSlots` / `FailedSlots` / `SlotGenerations` / `AllocateSlot` / `ReleaseSlot` / `SlotIndex` 计数与改前一致

## 5. 落地记录（实施期两条 MUST 留痕）

- [x] 5.1 **环境教训**：第一次跑 Shipping 用了引擎内建目标名 **`UnrealGame`** ⇒ `Result: Succeeded` 却**什么都没编**（14.39 秒、产物时间戳不变）。本仓**正确目标名 = `LegendAutoChess`**（`Source/LegendAutoChess.Target.cs`，`ExtraModuleNames` 含 `TcsDev`）。**判据：编译"成功"MUST 与产物时间戳/大小变化对账**。
- [x] 5.2 **一次越界与回退**：首版脚本把装置局部名（`ModArmorSlotsBefore` 等 11 个）也改了——那是用户**明确否掉**的第三档。已全部回退（30 处），并复核 `*=Instances*` 残留为 0。
