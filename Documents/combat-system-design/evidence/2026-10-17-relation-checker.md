# EVID-2026-10-17-relation-checker — `R5.5-e1` 关系表检查器取证

- **文档 ID**：`EVID-2026-10-17-relation-checker`
- **类型**：EVID / 取证记录
- **状态**：ACTIVE（2026-10-17 落盘；对应变更 `add-relation-table-checker`）
- **权威范围**：`R5.5-e1` 的**实施与验收读数**——代码落点、双配置编译、资产迁移、装置读数、PIE 读数，以及**如实登记的偏差与缺陷**。
- **不在本文**：检查器的设计理由（见变更的 `design.md`）、关系表族的语义裁决（见 `DEC-2026-10-13-relation-*`）。

---

## 1. 交付物与落点

| 件 | 路径 | 说明 |
|---|---|---|
| 关系行 + 检查器 | `Source/TcsState/Public/State/TcsStateRelation.h` / `Private/State/TcsStateRelation.cpp` | 共享层纯值；判定只走 `MatchesTag` 层级匹配 |
| 在跑实例视图 | `Source/TcsState/Public/State/TcsStateLiveView.h` | **事实半**（`CategoryTags`）+ **声明半**（`RelationRow`） |
| 跨域契约 | `Source/TcsState/Public/State/TcsStateLiveQuery.h` | 共享层定义、状态域注册 |
| 状态侧挂点 | `Source/TcsBuff/Private/State/TcsBuffOps_Relation.cpp` | apply 门禁（含**反向查找**）+ 取消 + 阻断 |
| 技能侧挂点 | `Source/TcsSkill/Private/Skill/TcsSkillOps_Relation.cpp` | 激活门禁（三方向一次判完） |
| 重定向配置 | `Config/DefaultTireflyCombatSystem.ini` | **新增**：同型改名走 `PropertyRedirects` |
| 装置检查 | `Source/TcsDev/Private/Dev/TcsDevRelationProbe.cpp` | `Tcs.Test.State.Relation` |

## 2. 双配置编译

| 配置 | 结果 | warning |
|---|---|---|
| `LegendAutoChessEditor Win64 Development` | **Succeeded** | **0** |
| `LegendAutoChess Win64 Shipping` | **Succeeded** | **0** |

## 3. 资产迁移（两个口径完全不同的改名）

### 3.1 `e0` = 类型变更 ⇒ 编辑器内搬迁

`StatusTag`（单 tag）→ `CategoryTags`（容器）：`PropertyRedirects` **不做类型转换** ⇒ 只能"读旧值 → 写新字段 → 保存"。

### 3.2 `e1` = 同型改名 ⇒ `PropertyRedirects`

**实测证据（决定性）**：2 个含 `bInterruptible` 的资产，**文件一个字节没动**，只加 ini + 重启编辑器：

| 字段 | 加重定向前 | 加重定向后 |
|---|---|---|
| `bCancellableDefault` | `False` | **`True`** |
| `Phases[0].bCancellable` | `False` | **`True`** |

⇒ **同型改名会让资产里的值静默丢失**（读出来只是新字段的构造默认值）。

**落盘后磁盘级复核**（`grep -a`）：

| 资产 | `bInterruptible`（旧） | `bCancellable`（新） |
|---|---|---|
| `DA_SkillDef_E2E_Replaceable` | **0** | **2** |
| `DA_SkillDef_E2E_SinglePhase` | **0** | **2** |

**全库反查**：`bInterruptible` / `Blocks` / `Requires` / `Cancels` 命中 **均为 0**。

> **本次采值的额外发现**：全库资产的四个关系数组**一个都没序列化**（全空 = 默认值）
> ⇒ 这既是"阴性基线"的来源，也是**装置检查必需**的原因（见 §5）。

## 4. 装置检查：`Tcs.Test.State.Relation`

**首轮 17 PASS / 2 FAIL → 修复后 19 PASS / 0 FAIL**。

覆盖：`ForbiddenTags` 拒绝 / `RequiredTags` 未满足 / `TagsToCancel` 移除 / `TagsToBlock` 两处时机 / 两种抢占策略 / 两条作者期 Error，**每条带阴性对照**。

## 5. ★ 装置抓到的真实缺陷（PIE 永远发现不了）

**症状**：装置 `R5 ★ 时机①（目标后到）` FAIL，PIE 四轮**全绿**。

**根因**（`CheckApplyGate` 的快速路径）：

```cpp
// 错：只看"我自己"，把反向查找一起跳过
if (Def.RequiredTags.Num() == 0 && Def.ForbiddenTags.Num() == 0
    && Def.TagsToBlock.Num() == 0 && Def.TagsToCancel.Num() == 0)
    return true;
```

**为什么错**：反向查找问的是「**别人**是否阻断我」，与「我自己有没有字段」**无关**——
B 自己四数组全空，却仍应被在跑的 Blocker 拦住。

**为什么 PIE 发现不了**：既有资产四数组全空 ⇒ 每次都从这行走掉 ⇒ 整段扫描**从未执行**。

## 6. PIE 四轮（零回归）

| 轮次 | 命令 | 本值 | `e0` 基线 | FAIL |
|---|---|---|---|---|
| R1 | `Tcs.Test.State.Run` | **15** | 15 | 0 |
| R2 | `Tcs.Test.State.Reject` | **7** | 7 | 0 |
| R3 | `Tcs.Test.Slice.Run` | **73** | 73 | 0 |
| R4 | `Tcs.Test.State.Run`（重跑） | **15** | 15 | 0 |

`Error=0 / ensure=0 / Warning=0`；检查 18 读数仍为 `CategoryTags 含定义词=是`。

## 7. 如实登记的偏差与未落面

| # | 项 | 状态 |
|---|---|---|
| 1 | **技能↔技能的阻断判定** | ❌ 未落（需"按实体枚举在跑 run"，技能侧今天**没有这个词面读口**） |
| 2 | **状态→技能方向** | ❌ 未落（无对称契约、`TcsBuff` 与 `TcsSkill` 平级） |
| 3 | **`ITcsStateLiveQuery` 单槽位** | ⚠️ `SetLiveQuery` 静默覆写 + `Deinitialize` 无条件清（今天只有一个注册者，未暴露） |
| 4 | 跨资产双向强夺校验 | ❌ 未落（`IsDataValid` 只看一份资产）⇒ 归定义库发现期 |

**上述 1~3 已由用户 2026-10-17 的设计讨论确认为下批要解决的缺口**（见 §8）。

## 8. ★★ 契约部分将被推翻（用户 2026-10-17 明确要求标记）

**用户 2026-10-17 的设计裁定**：`Buff` 与 `Skill` **在关系表眼里都只是"状态"**
（两者同继承 `FTcsStateInstance`），故：

1. **TagRelationship 的管理归 `TcsState`**，两域都受其影响；
2. **统一读口不暴露域**——判定对象 = 该实体上在跑的全部运行态；
3. **`DefTag` 与 `CategoryTags` 都进匹配面**（**取代** `e0` 的 D3「两通道独立、MUST NOT 并集」）；
4. **"彼此打断"是硬性要求**。

⇒ **本文档所记的 `ITcsStateLiveQuery`（单向、只读状态域、单槽位）将被统一读口取代**。
检查器、关系字段、挂点判定逻辑**继续有效**；被取代的**只是契约形状**。
