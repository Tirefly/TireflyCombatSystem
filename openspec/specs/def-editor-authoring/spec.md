# def-editor-authoring Specification

## Purpose
为 TCS DefinitionAsset 与受支持的 gameplay runtime 资产提供稳定、插件自有的编辑器 authoring 入口、菜单结构与配置勘误能力。AssetManagerSettings 覆盖勘误按具体非抽象 DefAsset 类型检查（Attribute、AttributeModifier、Buff、Skill、StateSlot、SkillModifier），不得把抽象 `UTcsStateDefinition` 作为独立扫描中心。
## Requirements
### Requirement: 插件自有的 DefinitionAsset Authoring 入口
TCS 编辑器集成 SHALL 为每一个规范、且应直接 authoring 的 TCS DefinitionAsset 类型暴露插件自有的 Content Browser 创建入口。

#### Scenario: 从 TCS 分类中创建 AttributeDef
- **WHEN** 开发者打开 Content Browser 的添加资产菜单
- **THEN** 开发者可以从 TCS 自有分类中创建 `UTcsAttributeDefinition`
- **AND** 整个流程不需要先选择通用 `Data Asset`，再额外选择父类

#### Scenario: 从 TCS 分类中创建 AttributeModifierDef
- **WHEN** 开发者打开 Content Browser 的添加资产菜单
- **THEN** 开发者可以从 TCS 自有分类中创建 `UTcsAttributeModifierDefinition`
- **AND** 整个流程不需要先选择通用 `Data Asset`，再额外选择父类

#### Scenario: 从 TCS 分类中创建 BuffDefinition
- **WHEN** 开发者打开 Content Browser 的添加资产菜单
- **THEN** 开发者可以从 TCS 自有分类中创建 `UTcsBuffDefinition`
- **AND** 整个流程不需要先选择通用 `Data Asset`，再额外选择父类

#### Scenario: 从 TCS 分类中创建 StateSlotDefinition
- **WHEN** 开发者打开 Content Browser 的添加资产菜单
- **THEN** 开发者可以从 TCS 自有分类中创建 `UTcsStateSlotDefinition`
- **AND** 整个流程不需要先选择通用 `Data Asset`，再额外选择父类

#### Scenario: 从 TCS 分类中创建 SkillDefinition
- **WHEN** 开发者打开 Content Browser 的添加资产菜单
- **THEN** 开发者可以从 TCS 自有分类中创建 `UTcsSkillDefinition`
- **AND** 整个流程不需要先选择通用 `Data Asset`，再额外选择父类

#### Scenario: 从 TCS 分类中创建 SkillModifierDefinition
- **WHEN** 开发者打开 Content Browser 的添加资产菜单
- **THEN** 开发者可以从 TCS 自有分类中创建 `UTcsSkillModifierDefinition`
- **AND** 整个流程不需要先选择通用 `Data Asset`，再额外选择父类

### Requirement: 不暴露损坏的抽象 DefinitionAsset 入口
TCS 编辑器集成 SHALL 不把抽象 DefinitionAsset 类暴露成损坏的直接创建目标。

#### Scenario: 抽象 StateDefinition 与编辑器 authoring 保持一致
- **WHEN** `UTcsStateDefinition` is abstract in runtime code
- **THEN** 插件不应暴露一个最终解析到 `UTcsStateDefinition` 的直接创建入口
- **AND** 那些本就应直接 authoring 的具体 state 侧 DefinitionAsset 类型仍应在 TCS 分类下可见

### Requirement: 保持运行时 Def 契约不变
TCS 编辑器 authoring 集成 SHALL 保持现有基于 `UPrimaryDataAsset` 的运行时 Def 资产契约不变。

#### Scenario: 通过编辑器创建的 Def 继续兼容 AssetManager
- **WHEN** 开发者通过插件自有的编辑器入口创建一个 Def 资产
- **THEN** 创建出的资产仍然是基于 `UPrimaryDataAsset` 的 TCS Def 资产
- **AND** 既有的 `PrimaryAssetId` 与 `AssetManager` 加载行为继续有效

### Requirement: 以组合优先作为 Authoring 方向
TCS 编辑器 authoring 集成 SHALL 不把 Def subclassing 作为主要扩展模型。

#### Scenario: 基础 Def 类型是默认 authoring 路径
- **WHEN** 插件注册其内建的 Def 资产创建入口时
- **THEN** 这些入口应创建 TCS 期望开发者直接 authoring 的规范内建 DefinitionAsset 类型
- **AND** 下游团队在正常 TCS authoring 中不应被迫使用 Def subclassing

### Requirement: 结构化的 TCS Authoring 菜单
TCS 编辑器 authoring 集成 SHALL 将插件自有创建入口组织到 `Tirefly Combat System` 下的稳定子菜单中。

#### Scenario: Definition 资产归入 Definition Asset 子菜单
- **WHEN** 开发者打开 Content Browser 的添加资产菜单
- **THEN** 每个规范 TCS DefinitionAsset 创建入口都出现在 `Tirefly Combat System -> Definition Asset` 下

#### Scenario: Gameplay runtime 资产归入 Gameplay Runtime 子菜单
- **WHEN** 开发者打开 Content Browser 的添加资产菜单
- **THEN** 受支持的 runtime authoring 入口都出现在 `Tirefly Combat System -> Gameplay Runtime` 下

### Requirement: TCS Gameplay Runtime Authoring 入口
TCS 编辑器集成 SHALL 为 TCS 开发者需要直接 authoring 的那一小部分 gameplay runtime 资产暴露插件自有创建入口。

#### Scenario: 为 TcsStateComponent 创建 StateTree
- **WHEN** 开发者从 `Tirefly Combat System -> Gameplay Runtime` 创建一个 StateTree
- **THEN** 插件应暴露一个面向组件的 StateTree 入口
- **AND** 创建出的资产应使用 `UTcsSTSchema_StateComponent`
- **AND** 整个流程不需要 schema picker

#### Scenario: 为 Buff 创建 StateTree
- **WHEN** 开发者从 `Tirefly Combat System -> Gameplay Runtime` 创建一个 StateTree
- **THEN** 插件应暴露一个面向 `UTcsBuffInstance` 的 Buff StateTree 入口
- **AND** 创建出的资产应使用 `UTcsSTSchema_Buff`
- **AND** 整个流程不需要 schema picker

#### Scenario: 不再暴露 generic StateInstance StateTree
- **WHEN** `UTcsStateInstance` 被确认为抽象共享执行态基类，且 generic `StateInstance` schema 已删除
- **THEN** 插件不应继续把 generic `StateInstance StateTree` 当作 gameplay runtime authoring 入口
- **AND** 当前运行时树入口应收敛到 concrete runtime owner
- **AND** 受支持的 concrete runtime 入口仍不应要求 schema picker

#### Scenario: 创建 learned-skill data Blueprint
- **WHEN** 开发者从 `Tirefly Combat System -> Gameplay Runtime` 创建一个 Blueprint
- **THEN** 插件应暴露一个 learned-skill data Blueprint 入口
- **AND** 创建出的资产应为 `UTcsSkillEntry` 的 Blueprint 子类
- **AND** 整个流程不需要 parent-class picker

### Requirement: 为未来 TCS Authoring 保留稳定扩展路径
TCS 编辑器 authoring capability SHALL 为后续的 StateComponent schema 和 Skill authoring 扩展保留稳定升级点，而不是把它们分裂成互不相关的菜单路径。

#### Scenario: State Component StateTree 入口沿用专用 schema 升级
- **WHEN** 当前阶段已经提供专用的 `UTcsSTSchema_StateComponent`
- **THEN** `Tirefly Combat System -> Gameplay Runtime -> State Component StateTree` 入口应创建基于该 schema 的资产
- **AND** 如果未来继续扩展组件树 authoring，这个同一入口仍应是预期的升级目标

#### Scenario: 过渡性的 generic StateTree 入口需要迁移
- **WHEN** `refactor-state-runtime-access-contract` 删除 generic `StateInstance` schema
- **THEN** 现有 `Gameplay Runtime` 中的过渡性 generic StateTree 入口应被移除或改造成 concrete runtime owner 入口
- **AND** editor authoring 面不应继续把抽象共享运行时类型暴露成稳定创建目标

#### Scenario: Skill authoring 沿用同一 capability 扩展
- **WHEN** 开发者从 `Tirefly Combat System -> Definition Asset` 使用 TCS 自有的 skill 相关创建入口
- **THEN** `UTcsSkillDefinition` 与 `UTcsSkillModifierDefinition` 应沿用同一 capability 下的菜单结构与 authoring 流程
- **AND** 后续 skill 相关扩展仍不应被拆成另一条无关的编辑器创作路径

### Requirement: 编辑器阶段 AssetManagerSettings 覆盖勘误
TCS 编辑器 authoring 集成 SHALL 在编辑器阶段检测 `AssetManagerSettings` 对 TCS DefinitionAsset 的覆盖完整性，并在漏配时通过错误日志与编辑器通知提供明确勘误提示。

#### Scenario: 检测到 Definition 类型或扫描路径漏配
- **WHEN** `PrimaryAssetTypesToScan` 未正确覆盖 `UTcsAttributeDefinition`、`UTcsAttributeModifierDefinition`、`UTcsBuffDefinition`、`UTcsSkillDefinition`、`UTcsStateSlotDefinition`、`UTcsSkillModifierDefinition` 对应的类型或扫描目录
- **THEN** 编辑器应输出可读勘误信息，明确缺失的 PrimaryAssetType 与扫描路径
- **AND** 该勘误信息至少应同时出现在错误日志与编辑器通知中
- **AND** 勘误信息应可用于直接指导开发者修正工程配置
- **AND** 系统 MUST NOT 把抽象 `UTcsStateDefinition` 作为独立 PrimaryAssetType、扫描目录或覆盖勘误对象

#### Scenario: 类型与路径漏配必须分别可见
- **WHEN** 某个 TCS DefAsset 类型已存在于 `PrimaryAssetTypesToScan`，但其扫描目录配置错误或缺失
- **THEN** 编辑器仍应报错，且该错误不得被“类型已存在”判定掩盖
- **AND** 报错信息应明确这是路径覆盖问题

#### Scenario: DevSettings 忽略列表可以抑制指定类型报错
- **WHEN** 某个 TCS DefAsset 类型被加入 `UTcsDeveloperSettings` 的勘误忽略列表
- **THEN** 该类型的类型漏配或路径漏配不应再产生勘误报错
- **AND** 其他未被忽略类型的漏配检测继续生效

#### Scenario: 勘误校验不改写工程配置
- **WHEN** 编辑器执行 `AssetManagerSettings` 覆盖勘误检查
- **THEN** 该检查应只报告配置问题，不自动改写项目 `AssetManagerSettings`

#### Scenario: 修复漏配后勘误消失
- **WHEN** 开发者按提示补齐 `AssetManagerSettings` 的缺失类型与扫描目录
- **THEN** 后续勘误检查不应再报告同一漏配项
- **AND** 既有 Definition 资产同步与加载行为保持有效

#### Scenario: 未修复漏配在常用 Save 入口重复提示
- **WHEN** 编辑器中仍存在未忽略且未修复的 DefAsset 漏配项
- **AND** 开发者执行普通单资产/单包保存、主窗口/快捷键 `Save All` 或 Content Browser 顶部工具栏 `Save All`
- **THEN** 编辑器应再次输出对应勘误提示
- **AND** 该重复提示应继续同时使用错误日志与编辑器通知两条通道
- **AND** 该重复提示行为持续到漏配被修复或该类型被加入忽略列表

### Requirement: DataTable ↔ DefAsset 双向自动同步

TCS 编辑器集成 SHALL 支持通过 `UTcsDeveloperSettings` 启用的 DataTable ↔ DefAsset 自动同步机制。保存 DataTable 时自动创建/更新/删除其绑定目录下的 DefAsset；保存 DefAsset 时自动回写其绑定 DataTable 行。每条同步配置都显式绑定一个受管 DefAsset 文件夹和一张固定 DataTable，并形成严格镜像同步关系。

#### Scenario: 保存 DataTable 后自动创建 DefAsset
- **WHEN** 策划在 DataTable 中新增一行（RowName = "ATTR_NewAttack"），保存 DataTable
- **THEN** 对应子目录下自动创建 `UTcsAttributeDefinition` 资产，属性值从 DataTable 行映射
- **AND** 创建的资产立即与 AssetManager 兼容

#### Scenario: 保存 DataTable 后自动更新已有 DefAsset
- **WHEN** 策划修改 DataTable 中已有行的属性值，保存 DataTable
- **THEN** 对应的已存在 DefAsset 被更新并标记为脏

#### Scenario: 保存 DefAsset 后自动回写 DataTable
- **WHEN** 策划双击 DefAsset 修改属性并保存
- **THEN** 对应 DataTable 中的行被更新并标记为脏

#### Scenario: 手动删除 DefAsset 后自动删除 DataTable 行
- **WHEN** 策划在受管相对子目录中手动删除某个 DefAsset
- **THEN** 同步系统 SHALL 通过资产删除事件识别该删除操作，而不是依赖保存回调
- **AND** 对应 DataTable 中 `RowName == DefId` 的行 SHALL 被删除并标记为脏

#### Scenario: 保存 DefAsset 后自动创建缺失的 DataTable
- **WHEN** 策划在某个受管目录下新建并保存一个 DefAsset，且该配置绑定的 DataTable 尚不存在
- **THEN** 系统自动创建目标 DataTable
- **AND** 以该 DefAsset 的 ID 字段值作为 `RowName` 新增对应行
- **AND** 新建 DataTable 的 `RowStruct` SHALL 来自该 `DefAssetClass` 的静态类型描述符，而不是运行时猜测

#### Scenario: 双向同步防止无限循环
- **WHEN** DataTable 保存触发 DefAsset 更新，或 DefAsset 保存触发 DataTable 更新
- **THEN** 同步系统 SHALL 将已保存对象入队并在下一 tick batch 执行真实同步
- **AND** batch 同步过程 SHALL 通过防循环守卫与去重集合阻止递归

#### Scenario: 每条配置显式绑定一个目录和一张表
- **WHEN** DeveloperSettings 配置 `ManagedDefAssetDirectory="/Game/TCS/AttributeDefs/Common"` 且 `TargetDataTable="/Game/TCS/AttributeTables/DT_CommonAttributeDefs"`
- **THEN** 该目录中的 DefAsset 只与这张固定 DataTable 建立同步关系
- **AND** 同步系统 SHALL NOT 再从根路径或相对子目录自动派生其他 DataTable

#### Scenario: 孤立 DefAsset 被清理
- **WHEN** DataTable 中已删除某行，但子目录中仍存在对应的 DefAsset
- **THEN** 保存该 DataTable 后，同步操作 SHALL 删除该孤立 DefAsset

#### Scenario: 空表与空目录严格对应
- **WHEN** 某个受管 DataTable 被保存且表中没有任何行
- **THEN** 对应 DefAsset 相对子目录 SHALL 收敛为零个 DefAsset

#### Scenario: 同类型多目录配置不发生错配
- **WHEN** 同一个 `DefAssetClass` 在 DeveloperSettings 中配置了多条目录-表绑定
- **THEN** 同步系统 SHALL 基于 DataTable 路径或 DefAsset 所在目录匹配唯一配置
- **AND** SHALL NOT 仅按 `DefAssetClass` 选择目标 DataTable

#### Scenario: 目标 DataTable 行结构不匹配时拒绝同步
- **WHEN** 某条配置绑定的 `TargetDataTable` 的 `RowStruct` 与该 `DefAssetClass` 的期望 RowStruct 不一致
- **THEN** 同步系统 SHALL 拒绝执行该请求并输出错误
- **AND** SHALL NOT 尝试写入不匹配的 DataTable

#### Scenario: DefAsset 迁移到另一受管目录时重绑定
- **WHEN** 某个受管 DefAsset 被移动到另一条配置绑定的受管目录
- **THEN** 同步系统 SHALL 将该变化视为“旧 DataTable 删除对应行 + 新 DataTable 新增或更新对应行”

#### Scenario: DefId 修改时重建主键行
- **WHEN** 策划修改了受管 DefAsset 内部的 DefId 字段并保存
- **THEN** 同步系统 SHALL 删除旧 `RowName` 对应的数据行，并以新 DefId 作为 `RowName` 新增或更新数据行
- **AND** 若新旧主键与现有行冲突，系统 SHALL 拒绝同步并输出错误

### Requirement: Row Struct 直接赋值映射

除标识字段外，Row Struct 与 DefAsset 的 UPROPERTY SHALL 保持类型和名称 1:1 一致。映射通过每种类型各一对编译期静态 `SyncRowToAsset` / `SyncAssetToRow` 函数直接赋值，不使用运行时反射。

#### Scenario: `RowName` 承载 DefId
- **WHEN** 同步系统处理任意一行 DataTable 与其对应 DefAsset
- **THEN** `RowName` SHALL 作为唯一主键并承载 DefId
- **AND** Row Struct 内 SHALL NOT 再重复声明 `DefId` / `AttributeDefId` / `ModifierId` / `StateSlotDefId` 等同义标识字段

#### Scenario: 直接赋值编译期校验完整性
- **WHEN** DefAsset 新增或删除一个 UPROPERTY 字段
- **THEN** 对应的 Sync 函数编译报错，强制同步更新 Row Struct

#### Scenario: 嵌套结构体在 DataTable 中自动展开
- **WHEN** Row 包含 `FTcsStateParameter CooldownParam`
- **THEN** DataTable 编辑器自动展开为子列 `CooldownParam.ParameterType`、`CooldownParam.NumericParamEvaluator` 等

#### Scenario: FInstancedStruct 和 FGameplayTagContainer 完整映射
- **WHEN** Row Struct 包含 `FInstancedStruct` 或 `FGameplayTagContainer` 类型的 UPROPERTY
- **THEN** 直接赋值 MUST 完整深拷贝到 DefAsset 属性
- **AND** DataTable 编辑器 MUST 能直接编辑这些类型的值

#### Scenario: AttributeModifier Row 与新 Definition 非标识字段保持 1:1
- **WHEN** 同步 `UTcsAttributeModifierDefinition` 与 `FTcsAttributeModifierDefRow`
- **THEN** Priority、Merger、Operation Map 以及每条 Operation 的 TargetAttributeId、Operator、Evaluator、OperandPayload、Custom Operator 等非标识字段 MUST 名称与类型 1:1
- **AND** Row / Asset MUST NOT 再包含旧 `AttributeId`、`ModifierMode`、`Operands` 或 `ModifierType`

#### Scenario: Operation Map 中嵌套 OperandPayload 可编辑并完整深拷贝
- **WHEN** AttributeModifier Operation 使用 `FInstancedStruct` 承载 OperandPayload
- **THEN** DataTable 与 DefAsset 编辑器 MUST 能编辑该 Payload
- **AND** 双向同步 MUST 完整深拷贝 Payload 内容，不得丢失嵌套字段

### Requirement: DefAsset 策略字段默认值与镜像同步
TCS 编辑器 authoring 集成 SHALL 为受支持 DefAsset 中声明为策略模式的 `UClass` 字段提供稳定的 concrete 默认类；并在 DataTable ↔ DefAsset 双向同步中把这些默认类显式镜像到对应 RowStruct。DataTableRow 自身 SHALL NOT 复制 DefAsset 级别的有效性勘误职责。

#### Scenario: AttributeDef 使用线性 Clamp 作为默认策略
- **WHEN** 开发者新建或同步得到一个 `UTcsAttributeDefinition`
- **AND** `ClampStrategyClass` 为空
- **THEN** authoring 或同步流程 SHALL 将其归一化为 `UTcsAttrClampStrategy_Linear`
- **AND** 后续回写的 `FTcsAttributeDefRow.ClampStrategyClass` SHALL 显式写入同一 concrete 类

#### Scenario: AttributeModifierDef、BuffDef 与 StateSlotDef 使用 concrete 默认策略
- **WHEN** 开发者新建或同步得到 `UTcsAttributeModifierDefinition`、`UTcsBuffDefinition` 或 `UTcsStateSlotDefinition`
- **AND** 其可默认化策略字段为空
- **THEN** `MergerType` / `SamePriorityPolicy` SHALL 分别归一化为 `UTcsAttrModMerger_NoMerge`、`UTcsBuffMerger_NoMerge`、`UTcsStateSamePriorityPolicy_UseNewest`
- **AND** 对应 RowStruct 在回写时 SHALL 显式写入这些 concrete 默认类
- **AND** AttributeModifierDef MUST NOT 再因已删除的 `ModifierType` 或其他非默认项被自动补值

#### Scenario: StateParameter 与 Skill cooldown 使用共享 constant evaluator 默认值
- **WHEN** 开发者在 `UTcsStateDefinition::Parameters` 或 `UTcsSkillDefinition::CooldownParam` 中 authoring 一个 `FTcsStateParameter`
- **AND** 当前 `ParameterType` 对应的 evaluator 类为空
- **THEN** 系统 SHALL 为 Numeric / Bool / Vector 分别归一化到各自 shared constant evaluator
- **AND** 对应 RowStruct 的嵌套 `FTcsStateParameter` 数据 SHALL 镜像同一 concrete evaluator 类

#### Scenario: SkillModifierDef 使用类型匹配的 concrete 默认执行器
- **WHEN** 开发者新建或同步得到一个 `UTcsSkillModifierDefinition`
- **AND** `TargetParamType` 已确定，但对应类型的 evaluator 类为空
- **THEN** Numeric / Bool / Vector SHALL 分别归一化到 `UTcsSkillModExec_Addition`、`UTcsSkillModExec_SetBool`、`UTcsSkillModExec_SetVector`
- **AND** 对应 `FTcsSkillModifierDefRow` SHALL 显式镜像相同的 concrete 执行器类

#### Scenario: DataTableRow 不承担 DefAsset 级勘误
- **WHEN** 某个受支持 RowStruct 中的默认化策略字段为空或缺失
- **THEN** Row → Asset 同步流程 MAY 归一化补值
- **BUT** RowStruct 本身 SHALL NOT 提供 `IsDataValid`、错误日志或编辑器通知级别的勘误入口

### Requirement: DefAsset 策略字段有效性勘误
TCS 编辑器 authoring 集成 SHALL 在 DefAsset 级别校验受支持策略字段的有效性，并在字段缺失、落到抽象类、与当前参数类型不匹配，或违反 Operator / Merger 兼容规则时提供明确错误提示。

#### Scenario: 缺失或抽象策略类触发勘误
- **WHEN** 某个 DefAsset 的必需策略字段为空，或被设置为抽象类
- **THEN** 编辑器校验 SHALL 报告错误
- **AND** 报错信息 SHALL 明确指出字段名、资产类型与期望的 concrete authoring 方向

#### Scenario: SkillModifier evaluator 与目标参数类型不匹配
- **WHEN** `UTcsSkillModifierDefinition` 的 `TargetParamType` 为 Bool 或 Vector
- **AND** 开发者填入了非对应类型的 evaluator 字段，或遗漏了对应字段
- **THEN** 编辑器校验 SHALL 报告错误
- **AND** 只有与 `TargetParamType` 匹配的 evaluator 字段才应被视为有效 authoring 输入

#### Scenario: Operation 默认 Operand 保持可编辑
- **WHEN** 开发者新建 AttributeModifier Operation
- **THEN** 系统 SHALL 默认配置 Constant Operand Evaluator 与 Constant OperandPayload
- **AND** 系统 MUST NOT 默认配置 Operator 或 Custom Operator
- **AND** 编辑器 MUST NOT 在后续编辑、同步或校验期间重置开发者选择的 Evaluator 或 OperandPayload
- **AND** Buff/State 的 `ActiveConditions` 为空时，编辑器 SHALL NOT 自动注入默认值

#### Scenario: StateParameter 默认值共享不替代 DefAsset 本地校验
- **WHEN** `UTcsStateDefinition::Parameters` 或 `UTcsSkillDefinition::CooldownParam` 依赖 shared `FTcsStateParameter` 默认 evaluator 规则
- **THEN** 默认值归一化 MAY 复用 shared `FTcsStateParameter` 的公共逻辑
- **AND** 参数是否合法 SHALL 继续由拥有该字段的 DefAsset 在自身 `IsDataValid()` 中判定
- **AND** 系统 SHALL NOT 通过 `Instance` 层导出通用 DefAsset 校验入口

#### Scenario: AttributeModifier Operator 与 Merger 不兼容时报错
- **WHEN** AttributeModifierDef 的 Operator / Merger 组合在 `TcsDeveloperSettings` 中为 Forbidden
- **THEN** `IsDataValid` MUST 报告 Error
- **AND** 系统 MUST NOT 自动清空用户已有 Operator 或 Merger 选择

#### Scenario: 多 Operation 使用内建选择或聚合 Merger 时报错
- **WHEN** AttributeModifierDef 的 Operation Map 含多个 Operation
- **AND** Merger 为 `UseMaximum`、`UseMinimum`、`UseAdditiveSum`、`UseNewest` 或 `UseOldest`
- **THEN** Data Validation MUST 默认报告 Error
- **AND** 若项目设置将该组合降级，则 MAY 报告强 Warning
- **AND** `NoMerge` MUST 保持合法

### Requirement: AttributeModifier Operation Map 编辑器创作

TCS 编辑器 authoring SHALL 支持在 AttributeModifier DefinitionAsset 与对应 DataTable 中编辑 Operation Map。每条 Operation MUST 能独立配置 `OperationId`、`TargetAttributeId`、Operator / Custom Operator、Evaluator 与 OperandPayload。旧单 Operation 字段 MUST NOT 继续作为创作入口。

#### Scenario: 在 DefAsset 中编辑多 Operation
- **WHEN** 开发者打开 `UTcsAttributeModifierDefinition`
- **THEN** 其可编辑字段 MUST 包含 Operation Map
- **AND** MUST NOT 再暴露旧 `AttributeId`、`ModifierMode`、`Operands` 或 `ModifierType` 作为有效创作路径

### Requirement: Operator 与 Merger 兼容规则驱动创作与验证

AttributeModifier 的 Operator / Merger 兼容规则 SHALL 以 `TcsDeveloperSettings` 为权威来源，采用二元 `Allowed` / `Forbidden`。DefinitionAsset 校验、必要的编辑器过滤与运行时防御 MUST 读取同一规则集。Custom Merger 类的默认兼容 Operator 列表可由类声明，项目设置只能收紧。

#### Scenario: 先设 Operator 后过滤 Merger 候选
- **WHEN** 开发者先为 Operation 选择 Operator
- **THEN** 编辑器 SHOULD 仅显示或仅允许选择 Allowed 的 Merger
- **AND** 即使没有下拉过滤，Data Validation 与运行时检查 MUST 仍阻止 Forbidden 组合

#### Scenario: 先设 Merger 后改 Operator 不自动清空
- **WHEN** 开发者先设置 Merger，再将 Operator 改为与其 Forbidden 的组合
- **THEN** 系统 MUST NOT 自动清空已有 Merger 或 Operator
- **AND** MUST 通过 `PostEditChangeProperty` / `IsDataValid` 报告 Error

### Requirement: 旧 AttributeModifier 创作数据不迁移

系统 MUST NOT 自动迁移旧 AttributeModifier DefinitionAsset、DataTable 行或 OperandBindings 资产。开发阶段相关验证资产 MUST 按新 Operation Map schema 重建。同步系统遇到无法直接赋值到新 Row / Def 字段的旧结构时 MUST 拒绝静默兼容。

#### Scenario: 旧 schema 不自动升级
- **WHEN** 仓库中仍存在旧 `AttributeId + ModifierMode + Operands + ModifierType` 形态的创作数据
- **THEN** 系统 MUST NOT 提供自动迁移工具或兼容读取路径
- **AND** 开发者 MUST 按新 Operation Map 重建该资产

