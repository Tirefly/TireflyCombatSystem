## MODIFIED Requirements

### Requirement: 模块物化声明

插件描述文件 MUST 声明当前实际物化的**十一个** Runtime 模块——`TcsCore`、`TcsNotation`、`TcsAssetButler`、`TcsAttribute`、`TcsEffect`、`TcsState`、`TcsTargeting`、`TcsDamage`、`TcsIntegration`、`TcsSkill`、`TcsBuff`——并按依赖序排列，LoadingPhase 均为 `Default`。

模块清单是**随轮次推进的现况声明**（R3 七模块 → R5 加入 `TcsState`，2026-10-04 → R6 加入 `TcsSkill`，2026-10-06 → 拆出 `TcsBuff`，2026-10-14 → **新增 `TcsAssetButler`**，2026-10-15）：它 MUST 如实反映"今天物化了哪些模块"，MUST NOT 被读成"目标架构的固定清单"，也 MUST NOT 为了凑目标数把未物化的模块提前写进去。

R0 的 `TcsCue`、`TcsEditor` 仍属目标架构模块，不得被描述成当前已经物化的模块；不得引用已删除的旧模块。

**`TcsSkill` 的落位口径（2026-10-06 R6 Task 1）**：它 MUST 追加在 `Modules` **数组末尾**（`TcsIntegration` 之后），MUST NOT 插在 `TcsState` 与 `TcsTargeting` 之间。理由：`TcsSkill` 是依赖链**第五层终点**，而既有八条已按依赖序排列——**按依赖序排在前/后都可加载**（无模块在加载期读它），取末尾可保持既有八条顺序**一字不动**，把改动面压到最小。**这是"可加载性"判据下的取舍，不是对依赖序的否认**：`TcsSkill` 依赖 `TcsState`，该依赖由 `TcsSkill.Build.cs` 的 `PublicDependencyModuleNames` 声明，不靠 `Modules` 数组顺序表达。

**`TcsAssetButler` 的落位与来源（2026-10-15 PLN-REFACTOR-1 Task 8）**：它同样**追加在数组末尾**（`TcsBuff` 之后），判据同上（无模块在加载期读它）。**它是新建模块（也是新能力面）**：承载 **PrimaryAsset 加载/注册层**——类型注册表（**类型擦除**：只收 `FPrimaryAssetType` + `UClass*` + 扫描路径 + 策略）、运行期加载三策略（`ETcsAssetLoadPolicy`）、异步默认 + 同步逃生口、加载防重、编辑器期规则。**它住得比所有注册者都低**：`TcsCore` / `TcsNotation` 旁，**零 Tcs 业务依赖**（只看引擎 `Core` / `CoreUObject` / `Engine` / `GameplayTags` / `AssetRegistry`）——因为各域（含层级最低的 `TcsAttribute`）要经 `UE_DEFINE_PRIMARY_ASSET_TYPE` **向下注册**，它一旦认识任一业务 Def 类型就必须住到那些类型之上，各域即无法再向下注册（会成环）。**两条必须守死的口径**：① **注册键 MUST 类型擦除**，注册表 MUST NOT 认识任何业务 Def 类型；② **类型清单 MUST 走代码注册**（类型名由 `StaticClass()` 反射派生，零字符串面），**MUST NOT 用 ini 提供**——否则改名会漏改 ini 而静默失联。

**`TcsBuff` 的落位与来源（2026-10-14 PLN-REFACTOR-1 Task 4）**：它同样**追加在数组末尾**（`TcsSkill` 之后），沿用上条同一判据（依赖链第五层，与 `TcsSkill` 平级；无模块在加载期读它）。**它是自 `TcsState` 拆出的新模块**（不是新能力）：Buff 专属语义与机制（定义资产 / 五轴堆叠 / 时值周期 / 行为片段 / 施加与移除门面 / 等级读口与其四个源）整体搬入；而 `TcsState` **保留原名**、定位改述为**共享层**（两域共用的 Def 族 + 通用运行时机制 + 参数表读口）。**依赖方向单向**：`TcsBuff` → `TcsState`，由 `TcsBuff.Build.cs` 声明。**★ 一处判据必须守死（MUST 读）**：`TcsSkill` **MUST NOT** 依赖 `TcsBuff`——技能侧对状态侧的唯一**代码级**依赖是参数表读口 `UTcsStateSubsystem::GetParamTableReader()`，该口留在共享层正是为了保住这条边。

#### Scenario: 模块列表与当前 R3 实现一致

> 场景标题里的 "R3" 是建立时的轮次标签（开源规格的 `#### Scenario:` 标题在 MODIFIED 语义下不可改名）——本场景的判据随模块清单一起现况化。

- **WHEN** 读取 `TireflyCombatSystem.uplugin` 的 `Modules` 数组
- **THEN** 恰好包含上列十一个 Runtime 模块，LoadingPhase 均为 `Default`；既有九条（`TcsCore` … `TcsBuff`）的顺序与 2026-10-14 时**逐字相同**，`TcsAssetButler` 位于数组**末尾**

#### Scenario: 目标模块不被误报为当前实现

- **WHEN** 对照 `openspec/project.md` 的目标架构与 `Source/` 目录
- **THEN** `TcsCue`、`TcsEditor` 被标为目标模块或后续轮次，不出现在当前物化模块清单中；`TcsState` 自 2026-10-04（R5 Task 1）起、`TcsSkill` 自 2026-10-06（R6 Task 1）起、`TcsBuff` 自 2026-10-14（PLN-REFACTOR-1 Task 4）起、`TcsAssetButler` 自 2026-10-15（PLN-REFACTOR-1 Task 8）起**已物化**，不再属于该清单

#### Scenario: 旧模块不复活

- **WHEN** 检查插件描述文件全文
- **THEN** 不存在 `TireflyCombatSystem`、`TireflyCombatSystemEditor` 等已删除旧模块条目

#### Scenario: 技能模块不反向依赖领域模块

- **WHEN** 检查 `Source/TcsSkill/TcsSkill.Build.cs` 的 `PublicDependencyModuleNames` / `PrivateDependencyModuleNames`
- **THEN** 其中**不含** `TcsDamage`、`TcsTargeting` 与 **`TcsBuff`**（`SPEC-04-skill` §1 的依赖铁律 + 2026-10-14 模块拆分新增的第三条）；含 `TcsCore` / `TcsNotation` / `TcsAttribute` / `TcsEffect` / `TcsState`
