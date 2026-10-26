## 1. 规格 delta

- [ ] 1.1 `entity-query-contract`：主语从 `TcsEffect` 改为 `TcsCore` 定义契约 + 路径改 `TcsCore/Public/Host/TcsEntityQuery.h`
- [ ] 1.2 `entity-query-contract`：新增「注入面唯一」需求（宿主只接 `UTcsEffectSubsystem` 一处）+ 2 Scenario
- [ ] 1.3 `skill-registry`：门禁需求整条重写（不再自持注入点；降级条件改为「宿主整个未注入」）+ 重述 2 Scenario + 新增 1 Scenario
- [ ] 1.4 `integration-entity`：注入方式加强为「MUST 只注入这一处」
- [ ] 1.5 `openspec validate refactor-hoist-entity-query-to-core --strict --no-interactive` 通过

## 2. 代码：契约搬家

- [ ] 2.1 新增 `TcsCore/Public/Host/TcsEntityQuery.h`（内容移植；类注释主语从 `TcsEffect` 改为 `TcsCore`）
- [ ] 2.2 删除 `TcsEffect/Public/Host/TcsEntityQuery.h`
- [ ] 2.3 五个 `#include "Host/TcsEntityQuery.h"` 复核（路径不变，应自动解析到 `TcsCore`）

## 3. 代码：删技能侧槽位

- [ ] 3.1 `TcsSkillSubsystem.h`：删 `SetEntityQuery` 声明、`GetEntityQuery()` 内联、`EntityQuery` 成员、`#include "Host/TcsEntityQuery.h"`
- [ ] 3.2 `TcsSkillSubsystem_EntityReady.cpp`：删 `SetEntityQuery` 实现；`IsEntityReady` 改经 `UTcsEffectSubsystem::GetEntityQuery()` 取查询
- [ ] 3.3 降级语义随实现改动（判据从"技能侧槽位空"变成"Effect 槽位空"）

## 4. 装置同步（`TcsDev`）

- [ ] 4.1 `TcsDevSkillCastProbe.cpp`：C12 的发散探针改注入 Effect 槽位 + 断言 + 还原
- [ ] 4.2 `TcsDevSkillRegistryProbe_Gate.cpp`：G1 改清 Effect 槽位；G2/G3 改换 Effect 槽位
- [ ] 4.3 删除 7 处重复注入（`SkillSubsystem->SetEntityQuery`）——保留 `Effect->SetEntityQuery`
- [ ] 4.4 复核：发散探针期间无 Target 选择在跑（否则 Targeting 会读到发散查询）

## 5. 验证

- [ ] 5.1 `LegendAutoChessEditor Win64 Development` 编译 0 error 0 warning
- [ ] 5.2 `LegendAutoChess Win64 Shipping` 编译 0 error 0 warning
- [ ] 5.3 `openspec validate --all --strict` 全绿
- [ ] 5.4 PIE：`Tcs.Test.SkillRegistry.Gate` 全 PASS（G1/G2/G3/G4）
- [ ] 5.5 PIE：`Tcs.Test.SkillCast` 全 PASS（含 C12）
- [ ] 5.6 反查：全仓 `SetEntityQuery` 只剩 1 处声明 + 1 处实现（`UTcsEffectSubsystem`）
- [ ] 5.7 反查：`TcsEntityQuery.h` 只剩 1 个实体文件（`TcsCore` 那份）
