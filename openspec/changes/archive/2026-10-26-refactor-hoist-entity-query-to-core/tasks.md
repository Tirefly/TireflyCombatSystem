## 1. 规格 delta

- [x] 1.1 `entity-query-contract`：主语从 `TcsEffect` 改为 `TcsCore` 定义契约 + 路径改 `TcsCore/Public/Host/TcsEntityQuery.h`
- [x] 1.2 `entity-query-contract`：新增「注入面唯一」需求（宿主只接 `UTcsEffectSubsystem` 一处）+ 2 Scenario
- [x] 1.3 `skill-registry`：门禁需求整条重写（不再自持注入点；降级条件改为「宿主整个未注入」）+ 重述 2 Scenario + 新增 1 Scenario
- [x] 1.4 `integration-entity`：注入方式加强为「MUST 只注入这一处」
- [x] 1.5 `openspec validate refactor-hoist-entity-query-to-core --strict --no-interactive` 通过

## 2. 代码：契约搬家

- [x] 2.1 新增 `TcsCore/Public/Host/TcsEntityQuery.h`（内容移植；类注释主语从 `TcsEffect` 改为 `TcsCore`）
- [x] 2.2 删除 `TcsEffect/Public/Host/TcsEntityQuery.h`
- [x] 2.3 五个 `#include "Host/TcsEntityQuery.h"` 复核（路径不变，应自动解析到 `TcsCore`）

## 3. 代码：删技能侧槽位

- [x] 3.1 `TcsSkillSubsystem.h`：删 `SetEntityQuery` 声明、`GetEntityQuery()` 内联、`EntityQuery` 成员（`#include "Host/TcsEntityQuery.h"` **保留**——`IsEntityReady` 的形参类型仍需它）
- [x] 3.2 `TcsSkillSubsystem_EntityReady.cpp`：删 `SetEntityQuery` 实现；`IsEntityReady` 改经 `UTcsEffectSubsystem::GetEntityQuery()` 取查询
- [x] 3.3 降级语义随实现改动（判据从"技能侧槽位空"变成"Effect 槽位空"）

## 4. 装置同步（`TcsDev`）

- [x] 4.1 `TcsDevSkillCastProbe.cpp`：C12 的发散探针改注入 Effect 槽位 + 断言 + 还原
- [x] 4.2 `TcsDevSkillRegistryProbe_Gate.cpp`：G1 改清 Effect 槽位；G2/G3 改换 Effect 槽位
- [x] 4.3 删除 **8 处**重复注入（`SkillSubsystem->SetEntityQuery`）——保留 `Effect->SetEntityQuery`（计划写 7，实测 8）
- [x] 4.4 复核：发散探针期间无 Target 选择在跑（否则 Targeting 会读到发散查询）

## 5. 验证

- [x] 5.1 `LegendAutoChessEditor Win64 Development` 编译 0 error 0 warning
- [x] 5.2 `LegendAutoChess Win64 Shipping` 编译 0 error 0 warning
- [x] 5.3 `openspec validate --all --strict` 全绿
- [x] 5.4 PIE：`Tcs.Test.SkillRegistry.Gate` 全 PASS（G1/G2/G3/G4 → **4/4**）
- [x] 5.5 PIE：`Tcs.Test.SkillCast` 全 PASS（含 C12 → **26/26**）
- [x] 5.6 反查：全仓 `SetEntityQuery` 只剩 1 处声明 + 1 处实现（`UTcsEffectSubsystem`）
- [x] 5.7 反查：`TcsEntityQuery.h` 只剩 1 个实体文件（`TcsCore` 那份）

## 6. 收束（2026-10-26）

- [x] 6.1 手工把三份 delta 应用到 `openspec/specs/` 本体（**未跑 `openspec archive` 命令**，见下）
- [x] 6.2 `git mv` 归档到 `changes/archive/2026-10-26-refactor-hoist-entity-query-to-core/`
- [x] 6.3 `INDEX.md` 补 R-19 两处（§7 修订记录 + §8 修复表）+ `ARCH-11` U-1 状态行

---

## 归档方式说明（MUST 读）

**本 change 用「手工应用 delta + 手工 `git mv`」归档，MUST NOT 误读为「跑过 `openspec archive`」。**

**为什么不用工具**：`entity-query-contract` 的 delta 正文含 **4 行表格**，而归档器的需求解析器无法处理含表格的能力
（与本仓 `refactor-split-state-specs-by-module` 同一限制——那次 6 份里 2 份失败）。
⇒ 走手工路径，delta 内容本身无误，卡的只是工具解析能力。

**⚠ 一条本轮踩到的教训（MUST 记）**：我第一轮**只写了 delta、没有更新 `specs/` 本体**，而
`openspec validate --all --strict` 当时仍报 **48/0 全绿**——因为 **`validate` 只查结构（需求有无 Scenario、标题格式），
查不出「规格与代码说的是反的」**。⇒ **`validate` 全绿不构成"规格内容正确"的证据**；
判断规格是否落地 MUST 靠**逐条比对本体与代码**（如本轮的：`specs/entity-query-contract:12` 的路径是否已是 `TcsCore/`）。

**应用前后的差异**（本体的实际变化）：

| 能力 | 需求数 | 变化 |
|---|---|---|
| `entity-query-contract` | 1 → **2** | 新增「注入面唯一」；原需求主语改 `TcsCore` + 路径 + 归属判据段 |
| `skill-registry` | 4 → **4** | 门禁需求整条重写（不再自持注入点 + 降级语义改判 + 新增 1 Scenario） |
| `integration-entity` | 3 → **3** | 注入方式加强为「全仓唯一」+ 新增 Scenario「宿主只需接线一处」 |
