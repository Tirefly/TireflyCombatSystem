# TireflyCombatSystem (TCS)

UE5 战斗系统插件，当前按 `State Core / Buff / Skill` 三层边界持续收敛。

## 当前定位

TCS 当前版本不再把“技能、Buff、普通状态”简单当成同一种作者语义对象处理。

当前已经收敛下来的边界是：

1. `State Core` 负责共享运行态宿主、Apply / Remove / Query / Slot / StateTree 生命周期主链。
2. `Buff` 负责 Duration / Stack / Merge / Period 等 Buff 专属语义，但仍挂接在共享 `UTcsStateComponent` 主链上执行。
3. `Skill` 当前仍保持轻量骨架；`UTcsSkillEntry` 表示 learned skill 拥有态，`UTcsSkillInstance` 表示一次技能激活执行态。
4. 技能执行态仍通过 `UTcsStateComponent` 进入共享运行主链，而不是让 Skill 自己复制一套平行宿主框架。

## 当前可直接 Author 的 DefinitionAsset

当前编辑器中的 TCS 创建入口已收敛为两组二级子分类：

1. `Tirefly Combat System -> Definition Asset`
2. `Tirefly Combat System -> Gameplay Runtime`

当前编辑器 authoring 面向的 canonical DefinitionAsset 集合是：

1. `UTcsAttributeDefinition`
2. `UTcsAttributeModifierDefinition`
3. `UTcsStateSlotDefinition`
4. `UTcsBuffDefinition`

补充说明：

1. `UTcsStateDefinition` 现在是抽象共享基类，不再是直接可创建的最终资产类型。
2. 运行时仍以 `StateDefId` 和 `PrimaryAssetId` 为统一加载入口；`UTcsBuffDefinition` 只是把 Buff 专属配置从抽象基类中拿回 Buff 侧。

## 当前可直接 Author 的 Gameplay Runtime 资产

`Tirefly Combat System -> Gameplay Runtime` 当前提供三类快捷创建入口：

1. `State Component StateTree`
   - 创建后直接预设为 `UTcsStateSchema_StateComponent`，用于 `UTcsStateComponent` 挂载的 StateTree 资产。
2. `Buff StateTree`
   - 创建后直接预设为 `UTcsStateSchema_Buff`，用于 `UTcsBuffInstance` 执行的 StateTree 资产。
3. `Skill Entry Blueprint`
   - 创建后直接以 `UTcsSkillEntry` 为父类，不再弹出通用父类选择器。

## 文档入口

如果你要继续在当前代码基线上开发，建议按下面顺序阅读：

1. `Documents/原始架构设计/设计：战斗系统架构（UE5-Version）.md`
   - 当前架构方向与边界定义。
2. `Documents/文档：State模块与Buff模块核心函数流程详解（当前实现）.md`
   - 当前实现真相，适合看调用链和真实行为。
3. `Documents/文档：StateCore-Buff-Skill迁移清单与资产迁移策略.md`
   - 当前 `State` 层残留债务、Skill 接入点清单、旧资产迁移策略。
4. `Documents/文档：TCS手工验证指南（StateCore-Buff-Skill）.md`
   - 当前阶段手工验证建议。
5. `Documents/文档：BuffMerger流程优化方案对比（最小增量方案与完整方案）.md`
   - Buff merge 外层调度优化方案。
6. `Documents/文档：Buff增量反应语义扩展方案（叠层、时长、Period）.md`
   - Buff 增量语义下一阶段扩展方案。

## 构建与验证

项目当前常用验证命令：

```powershell
"E:\UnrealEngine\UE_5.7\Engine\Binaries\DotNET\UnrealBuildTool\UnrealBuildTool.exe" TireflyGameplayUtilsEditor Win64 Development -Project="E:\Projects_Unreal\TireflyGameplayUtils\TireflyGameplayUtils.uproject" -rocket -progress
```

如果工作内容落在 `Plugins/TireflyCombatSystem/openspec/` 下，请从插件根目录运行 OpenSpec：

```powershell
Set-Location "E:\Projects_Unreal\TireflyGameplayUtils\Plugins\TireflyCombatSystem"
openspec list
openspec validate <change-id> --strict --no-interactive
```

## 当前约束

1. 不要再把 Buff 的 Duration / Stack / Merge 语义塞回 `State Core`。
2. 不要把 Skill 的重复激活冲突策略直接复用为 Buff merger。
3. 不要再新增直接 author `UTcsStateDefinition` 的流程。
4. 处理运行时边界时，优先区分“共享宿主职责”和“Buff / Skill 专属语义”，而不是继续沿用旧的“一切都挂在 State 基类上”习惯。