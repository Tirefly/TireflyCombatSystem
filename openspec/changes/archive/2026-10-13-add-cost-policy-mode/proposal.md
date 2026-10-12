# Change: 消耗改点策略模式（`PLN-R6.5-CC` 落地回写）

> 对应计划 `PLN-R6.5-CC`（`Documents/combat-system-design/plans/plan-r65-cost-policy.md`）
> **本提案是"已实施并取证后的规格回写"**（代码与读数先于提案——本仓既有节奏，
> 见 `R6.5-1` 的三份同款）。证据 = `EVID-2026-10-12-cost-policy`。

## Why

用户 2026-10-12 改点：消耗的"检查 / 提交"**不应该由框架写死**，应允许扩展——且进一步
指定**策略模式**（"感觉也不需要 `ETcsCostPolicy`，直接用策略模式就好"、
"TCS 自带一个 `ResourceAttr`，但也遵循这套方案，而不是直接把 `ResourceAttr` 写死在代码里"）。

**`openspec/specs/` 是与代码同等级的**权威事实**，而它今天仍描述已被删除的类型**
（`ETcsCostPolicy` / `FTcsCostConfig` / `ECP_*`）⇒ 规格与代码**已经分叉**。
本提案把规格补到代码的实际形态。

**★ 一处元教训**：`openspec validate` **只校验格式与场景完整性，不校验类型是否还存在**
⇒ 代码里删掉的类型可以在规格里静默留存。**只有实现批次的逐处核对能抓到**
（本批即由"回写时 grep 旧类型名"抓到 41 处）。

## What Changes

- **`skill-cost`**：
  - 数据形状要求由"`ETcsCostPolicy` 三档 + `FTcsCostConfig`"改为
    **`ETcsSkillCostTiming`（四档含 `ECT_None`）+ `FTcsSkillCostConfig{Timing, CostMode}`**；
  - 运行态要求由"按 `Policy` 分派的七类处置 + 禁止用批"改为
    **分派给策略 + `Timing == None` 短路 + 空策略 fail-closed + 框架用批包住提交**；
  - "自定义片段档具名降级"场景**整体删除**（该降级已不存在，改为"空策略 fail-closed"）；
  - 求值实现名 `ResolveEffectiveCost` → **策略成员 `ResolveEffectiveAmount`**。
- **`skill-cooldown`**：`ETcsCooldownTiming` → **`ETcsSkillCooldownTiming`**（**只改类型名**，
  四档语义一律不动；两侧同批加域前缀以保持姐妹枚举对称）。
- **`skill-def-asset`**：`FTcsCostConfig Cost` → **`FTcsSkillCostConfig Cost`**；
  `Cost.Policy == ECP_None` → **`Cost.Timing == ECT_None`**；`ECP_CustomFragment` 的
  "具名未实现"注记改为**策略化说明**。

## Impact

- **Affected specs**：`skill-cost`（4 条需求中 3 条 MODIFIED + 1 条场景删除）、
  `skill-cooldown`（1 条 MODIFIED）、`skill-def-asset`（1 条 MODIFIED）。
- **Affected code**：`TcsSkill` 的 `Def/TcsCostTypes.h` / `Skill/TcsCostOps.{h,cpp}` /
  `Skill/TcsCostPolicyHost.h`（新）/ `Def/TcsCooldownTypes.h` / `Def/TcsSkillDefData.h`；
  LAC 装置 `TcsDevSliceRig_CooldownCost.{h,cpp}`。
- **破坏性**：**是**——`ETcsCostPolicy` / `FTcsCostConfig` / `ECP_*` 三档整体退役；
  `ETcsCooldownTiming` 改名。**资产迁移成本 = 0**（全仓 `.uasset` 扫 `Cost` / `ResourceAttr` /
  `Cooldown` **零命中**，已实测）。
- **行为收紧一处**：换型前 `ECP_CustomFragment` 是"免费放"（恒可付 + 零扣减），
  现改为 **fail-closed**（`Timing != None` 而 `CostMode` 空 ⇒ 拦 + `Warning`）。
