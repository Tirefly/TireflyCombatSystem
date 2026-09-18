## Context

plan1 Task 4 是 M2（TcsAttribute）的首次落地：把 02-module-attributes.md 的类型词汇与数据宿主变成可编译的 C++ 形状。设计文档已定**语义**（五带/双形状/三态边界/值域模式/脏标记/级联来源），但**实现层形状**有四处在文档与计划之间未收敛，且每处都会渗进后续所有消费者的签名。本文件只记录这四处决策；语义一律以 02 模块文档 + 决策点文档为准，不在此复述。

约束：`TcsAttribute` 依赖 `TcsCore` + `TcsNotation`（D5-18 v2），不得反向依赖 M3/M4/M5；实例数据结构禁 `TSubclassOf`（D2-9）；策略形态一元化 = USTRUCT 反射基类 + C++ 虚分派（D3-7 v3）；"能力探测走虚分派、不做中心 switch"（PV-10 已确立为跨域纪律）。

## Goals / Non-Goals

- Goals：M2 全部类型词汇可编译；单位注册与属性定义可用；`UTcsAttrModDef` 的约定列白名单**在编辑器保存期即可报**（D5-18 v3）；为 Task 5 管线留下零返工的数据形状；为 plan2 Damage 留下稳定的读契约。
- Non-Goals：聚合公式/Override 覆盖/带式折叠（Task 5，D5-5 v3）；脏标记惰性重算与事务 flush（Task 5，D2-5）；读即登记依赖边与 SCC（Task 5，D2-3）；`IValueDomainPolicy` 值域策略接口（R3 未建，**Custom 位只落枚举**）；M8 属性词表注册表与 DataTable 行类型（M8）；网络操作流镜像（NET-1/2）；等级表族与 `Subject`/`EffectiveLevel` 在 Core 上下文的补齐（PV-1：随 TcsState 等级源同批）。

## Decisions

### D1：单位身份 = `FTcsCombatEntityHandle`（Core 词汇），R3 由 M2 门面发号

- **决策**：在 `TcsCore/Public/Handle/TcsCombatEntityHandle.h` 落 `FTcsCombatEntityHandle{uint64 Id, 0=无效}` + `FTcsCombatEntityHandleRegistry`（`std::atomic` 递增、首个为 1、永不复用），与既有 `FTcsSourceHandle` 同形式同目录；`FTcsAttributeStore` 以它键控；R3 的发号方 = `UTcsAttributeSubsystem::RegisterUnit`。
- **依据**：02 §2.2a 明确"`FAttributeStore`（per entity，`FCombatEntityHandle` 键控）"；PV-1 增补已把"`FCombatEntityHandle` 落 TcsCore"记为一处**明示让步**（Core 唯一持有的战斗实体词汇，06 已把它当全插件通用实体键）。设计文档 > 计划文档（冲突裁决顺序），故不用 M2 私有句柄。
- **被否备选 A：M2 私有 `FTcsAttributeUnitHandle`**（`TTcsInstanceHandle<FTcsAttributeUnitTag>`，靠 Task 1 池的代际校验）。优点：零 Core 改动、自带代际防悬空、严守 plan1"具体句柄类型由各域定义"口径。否决理由：**句柄类型出现在每个消费者签名里**（plan2 Damage 的目标引用、plan2 Task 5 组件、M6 适配器）——R3 先发 M2 私有句柄、M6 再引入通用实体句柄，等于把一次类型迁移摊到全部下游；反之实体句柄类型先行，日后只有"发号方"从 M2 门面移到 M6 世界注册表（签名不变）。
- **被否备选 B：复用 `FTcsSourceHandle` 作单位键**。否决理由：语义混载——`Source` 是"归属来源/级联撤销锚点"（D2-2），单位是"被修饰主体"；两者在同一结构体（`FTcsAttrModInstance`）里同时出现，同型会让"传错一个"编译期无法拦截（违反 token 化句柄的初衷）。
- **代价与迁移**：R3 内 `RegisterUnit` 兼发号（M2 成为事实上的实体身份发号方）；M6 世界注册表落地后改为 `RegisterUnit(FTcsCombatEntityHandle)` 由调用方带入（或保留无参重载），**句柄类型与 Store 键不变**。撤销 `UnregisterUnit` 已足够表达"该单位在本世界的属性侧生命周期"。
- **无代际校验的取舍**：Id 单调递增且永不复用 → 悬空句柄表现为"Store 查不到"（`GetStore` 返回 `nullptr` + ensure），而非"误命中回收槽"。这与 `FTcsSourceHandle` 同级（同样无代际），不引入新的判悬空语义。

### D2：约定列白名单 = 源侧能力位虚函数，不做中央名单

- **决策**：`FTcsParamValueSource` 增 `virtual bool AllowsValueConvention() const { return true; }`；`FTcsParamSource_ParamRef` 与 `FTcsParamSource_AttributeScaled` 覆写为 `false`；`UTcsAttrModDef::IsDataValid` 只做"约定列非 None 且源禁配 → 报错（错误挂配置元素）"，不再枚举源类型。
- **依据**：D5-18 v3 白名单的判据是**源自身**的性质——"这一行书写的数值是否就是结果"：`Literal`/表型源 是（可配）；`ParamRef`（读到的已是规范值，再转 = 二次转换）/ `AttributeScaled`（约定作用在系数还是乘积上有歧义）否。把这条判据放在源上，与 PV-10"索引解析唯一真相在源、`Evaluate` 同源"、D3-7 v3"能力探测 = 虚分派不做中心 switch"、D5-17 v3"宿主自定义一步接入、零公共代码"三条已拍板纪律同构；宿主自定义源也能自行声明而无需改任何公共代码。
- **被否备选 A：TcsAttribute 侧 deny-list**（`IsDataValid` 内 `Source.GetScriptStruct() == FTcsParamSource_ParamRef::StaticStruct()` 比较）。否决理由之一（技术）：**白名单在 TcsAttribute 里根本写不出来**——表型源（`StateLevelArray/Map` 等）住 TcsState，而依赖方向是 TcsState → TcsAttribute，M2 看不到它们，任何真白名单都必然退化为"已知禁配名单 + 其余放行"；之二（纪律）：新增源/宿主自定义源要回头改 M2 校验器 = 中心 switch 的复活。
- **被否备选 B：M8 校验矩阵表**（`源类型 × 可配约定` 由中心注册表承载）。否决理由：M8 是多轴矩阵的**呈现与汇总**方，不是判据的**持有**方；判据散在中心表里会与源实现漂移（PV-10 已经就"高亮 10%、实打 7.5%"论证过这类漂移的代价）。M8 落地时按虚分派查询即可汇总，无需中心表。
- **代价**：`FTcsParamValueSource` 是 TcsCore 的公共基类，加虚函数 = 改 Core 公共面（本提案显式记入 `param-value` 的 MODIFIED 需求）。虚表代价在配置态（Def 资产/物化边界）承担，不在聚合热路径（账本恒为已解析 double，D2-13 不变）。

### D3：Store = 每单位一个容器，门面持外层映射

- **决策**：`FTcsAttributeStore`（纯 C++ struct）是**单个单位**的属性容器（`TMap<FTcsAttributeName, FTcsAttributeInstance> Attributes`）；`UTcsAttributeSubsystem` 持 `TMap<FTcsCombatEntityHandle, FTcsAttributeStore>` + 单位名表；`GetStore(Unit)` 返回该单位 Store 指针。
- **依据**：02 §2.2a"适配器缓存 Store 指针"要求 Store 指针可被消费者长期缓存以跳过外层查找（D2-9 热路径不查询）；UE `TMap` 值节点堆分配且地址稳定，Store 指针与 Store 内 `FTcsAttributeInstance` 指针在后续插入下均不失效——管线（Task 5）与 plan2 组件都能安全缓存。
- **被否备选：单一扁平容器**（`TMap<FTcsCombatEntityHandle, TMap<FTcsAttributeName, FTcsAttributeInstance>>` 直接挂门面，无 Store 类型）。否决理由：与 02 §2.2a 的"per entity Store + 适配器缓存指针"形状不符，且把"单位级批量操作"（`UnregisterUnit` 整块释放、未来按单位快照/镜像）挤成了门面上的散装循环。
- **代价**：一次外层 `TMap` 查找（仅在适应器首次绑定/未缓存指针时发生）；跨单位批量遍历稍繁琐（M8/测试装置用），R3 无此需求。

### D4：反射类型与纯 C++ 类型的分界 = "配置面 vs 账本面"

- **决策**：
  - **USTRUCT/UENUM（反射面，能被 Def 资产 / DataTable / BP 承载）**：`FTcsAttributeName`、`ETcsAttributeOp`、`ETcsOperandKind`、`ETcsAttributeBoundMode`、`ETcsAttributeValueDomain`、`FTcsAttributeBound`、`FTcsAttributeBounds`、`FTcsAttrModOperandDef`、`FTcsAttributeEvaluateContext`、`FTcsParamSource_AttributeScaled`；`UTcsAttrModDef` 为 `UCLASS(BlueprintType) UDataAsset`。
  - **纯 C++ struct（账本面，不进反射）**：`FTcsAttrModOperand`、`FTcsAttrModInstance`、`FTcsAttributeInstance`、`FTcsAttributeStore`、`FTcsCombatEntityHandle`。
- **依据**：D2-13"定义侧丰富、账本侧恒为已解析规范值"——账本形状要被聚合热路径零开销遍历，且不该出现在任何编辑器 picker 里；反之 `FTcsAttributeBounds` 类型要能被属性定义 DataTable 行（plan2 Task 6 的 `R3_Attributes`）承载，故必须反射。`FTcsSourceHandle` 是纯 C++ 类型，这决定了**含它的 `FTcsAttrModInstance` 不可能是反射 USTRUCT**（UPROPERTY 无法承载非反射成员）——分界线因此是强制的，不是口味。
- **被否备选：把账本类型也反射化**（把 `FTcsSourceHandle` 一并升级为 USTRUCT）。否决理由：为一处便利改动 Core 既有词汇的反射性，且账本数据的"编辑器可见"没有消费者（D2-8 实例是运行时产物）。

## 实施定案（2026-09-16 落地期回写）

落地过程中出现四处"文档未覆盖、必须在动代码前定掉"的事实，逐条登记（原提案正文保留为审批时的原貌）：

1. **扩展上下文的 checked cast 需要一个虚函数锚点（Core 公共面 +1 虚函数）**：USTRUCT 没有内建类型查询（`GetScriptStruct()` 不是 USTRUCT 的成员——已核源码），PV-1 的"结构体继承 + 源内 checked cast"因此缺载体。定案：在 `FTcsParamEvaluateContext` 加 `virtual const UScriptStruct* GetScriptStruct() const { return StaticStruct(); }`（GAS `FGameplayEffectContext::GetScriptStruct` 同款机制），域侧覆写为自身类型、源侧以 `IsChildOf` 判定（支持多层派生）。属 D2 同类的"能力探测走虚分派"形态，已同步 `param-value` 规格的"反射可见求值上下文"需求。
2. **`FTcsAttributeEvaluateContext` 不持 `Subject`（PV-1 既定时序）**：PV-1 已把 `Subject`/`EffectiveLevel` 的落地时机定为"随 TcsState 等级源同批"，且 `FTcsCombatEntityHandle` 是本设计 D4 认定的纯 C++ 值类型——**不能作反射 USTRUCT 的 UPROPERTY**，预建 Subject 会直接违反"上下文反射可见"约束。定案：以 `Provider`（其实现者自带单位绑定，02 §2.3）代表"对谁求值"，Subject 按 PV-1 时序随等级源同批进 Core；已同步 `attribute-types` 规格。
3. **导出宏不能加在全内联值类型上（Task 1 潜伏缺陷，本次链接期炸开）**：`TCSCORE_API`/`TCSATTRIBUTE_API` 会让消费方去 TcsCore.dll **导入**符号，而 MSVC 只为"本模块自己用到的类型"生成导出符号——本模块未使用的类型（`FTcsSourceHandle` 的隐式构造、`FTcsCombatEntityHandleRegistry::Allocate`）在消费方以 LNK2019 炸开。定案：导出宏只给"有 out-of-line 成员（`FTcsCorePoolStats`）或反射符号（USTRUCT/子系统类）"的类型；全内联值类型去除宏（含新类型 `FTcsCombatEntityHandle` 家族与 M2 账本四类型）。同时修掉同类的潜伏项 `FTcsEventSubscriptionHandle`。
4. **UHT 把枚举值上方的 `//` 注释当作该值的 ToolTip 元数据**：与同一 `UMETA(... ToolTip = "...")` 并存时报 `Metadata key 'ToolTip' first seen with value <注释文本> then <显式 Tooltip>`（本文件四个枚举共 5 处命中，UHT 直接失败）。定案：枚举值的说明只写在 `UMETA` 的 `ToolTip` 里，前缀注记移入枚举 doc 块（skill 侧记住这条引擎事实）。

## 复评定案（2026-09-17，用户三条疑问驱动）

用户在 Task 4 交付复评时指出三点，逐条登记：

5. **属性定义类型（AttrDef）缺失——补 `FTcsAttributeDef`**：设计口径本就存在（02 §2.1"词表本体 = 项目 DataTable（DT_AttributeDefinitions）+ DevSettings 注册"、竖切剧本"属性表 4 行 = FName 词表 + 显式包装结构 + DataTable 行"、plan2 Task 6"FName 键 + Base + Bounds"），但 **plan1 Task 4 的类型清单没有它**——本提案照清单实现，把定义数据内联成了 5 参 API（`DefineAttribute(Unit, Name, BaseValue, Bounds, ValueDomain)`），于是"每个调用点手写本该由词表行承载的数据"，且 plan2 Task 6 要建的那张 DataTable 无行类型可挂。定案：`FTcsAttributeDef : FTableRowBase`（`BaseValue` / `Bounds` / `ValueDomain`；**行名即属性名**，名称不进结构体以免行名与字段双真相），门面改 `AddAttribute(Unit, Name, Def)`。**词表装载/注册（`FAttributeRegistry`、DevSettings、`Resolve(FName) → 稠密 id`、行名↔常量映射校验）仍归 M8**——零消费者不预建（PV-10 同纪律），R3 由调用方显式传定义行。
6. **Def 资产族基类统一 `UPrimaryDataAsset`**：原 `UTcsAttrModDef : UDataAsset` 只是 plan1 写下的，**全部 11 篇设计文档 + 决策点文档对 `PrimaryDataAsset` 零命中**——即该选择从未被论证。判据重述：Def 的引用语义已是"FName Id + 注册表/DefLibrary 解析"（03 §2 命名批：`FStateDefBase.ModifierRows` 行仅存 `TemplateId` FName；06：`ResolveDef(FName)`/`LoadAll/LoadSelected/EnsureLoaded(DefId)`），而主资产身份正是让"FName ↔ 资产"解析、按类型发现/加载、打包分块归属由引擎提供的机制。定案：**族级统一**（`UTcsAttrModDef` 起，未来 `UTcsStateDef` 家族与 `UTcsSkillModDef` 同此基类）——族内混用两套基类会让 DefLibrary 的发现逻辑分叉。落地注记：`PrimaryAssetTypes` 的注册（插件自带 Config 或宿主项目 Config）属 DefLibrary 轮（M6）——`GetPrimaryAssetId()` 本身无需配置，仅"按类型扫描/加载"需要；插件目录 `DefaultGame.ini` 是否被引擎自动加载未实证，实施时按需实测。
7. **命名：`AddAttribute`（非 `DefineAttribute`）**：动词纪律下 "Define" 是泛化动词、且极易读成"定义词表"（那件事属 M8）；本 API 的动作是"往单位加一条属性实例"，定义数据由 Def 行承载——`Add` 与容器语义一致。
8. **文件名去类型前缀（二次复评，2026-09-17）**：原 `UTcsAttrModDef.h` / `FTcsAttributeName.h` / `TTcsInstancePool.h` / `ITcsTimeSource.h` 一类命名违反 UE 规范（文件名 = 类型名去前缀字母），且**是 plan1/plan2 的 File Structure 系统性写下的**——存量违规覆盖 Task 0-3 与本次新增共 **35 个文件**。定案：全插件改名 + 全库 include / `.generated.h` / 文档路径引用清零 + 规范写入 `unreal-cpp-style`（structure.md 新增「文件命名」节 + 检查清单项）+ **尚未创建文件的计划路径一并改写**（否则后续任务照旧名再犯）；归档提案目录保持原貌（冻结历史）。
9. **Def 双轨制的语义澄清（用户口径，2026-09-17；**全 Def 族适用**）**：用户指出其既定策略是——**DataTable 供策划编辑（编辑器阶段），运行期一律用 DataAsset**（资产制扩展性更好：未来给定义加 Fragment 之类只动资产与载荷）。据此属性定义改为**定义形状与资产同文件、资产组合持有定义行**；**两轨同步器是编辑器侧工具（资产为权威），运行期零 DataTable 加载路径**——这条推翻本提案前一轮"只落 DataTable 行"的形态（当时把"行 + 资产双形态"判为超出设计，实为误判：08 §5 的"双轨同步"本就是该策略的落点）。族级回写：02 §2.1、03 §2、08 §5。
10. **形态收口：定义行 = 唯一字段形状，资产组合持有（三次复评，2026-09-17）**：载荷层 `FTcsAttributeDef` 删除（定义字段就是行的字段）；`FTcsAttributeDefTableRow`（`FTableRowBase`：`DefId` + `BaseValue` + `Bounds` + `ValueDomain`）与 `UTcsAttributeDef`（`DefId` + `Def: FTcsAttributeDefTableRow`）**同住 `TcsAttributeDef.h`**——字段集单份，`DefId` 兼作 PrimaryAssetId 的名；资产 `IsDataValid` 报空 `DefId`、报"资产 DefId ≠ 行内 DefId"（身份分叉）、提示资产名不一致。**同款组织同步给修正器模板**：`FTcsAttrModDefTableRow`（`TemplateId` + 模板字段）+ `UTcsAttrModDef`（`TemplateId` + 行）——记录在案的局限：模板行含 `FTcsParamValue`（`TInstancedStruct`）列，CSV/Excel 往返丢该列，只支持编辑器内表格编辑。
11. **调用面收口：单位侧只认属性名（用户口径，2026-09-17）**：`AddAttribute(Unit, Name)` / `RemoveAttribute(Unit, Name)` + 内部**属性定义表**（`RegisterAttributeDef(行)` / `FindAttributeDef`）——"定义解析是 AttrSubsystem 内部的执行流程"，调用方 MUST NOT 传定义数据。`RemoveAttribute` 语义 = 整条属性下线（实例连同槽位内容丢弃；来源级联撤销仍走 `RemoveBySource`，二者不互相替代）。资产 → 行的解析归 DefLibrary（M6）/ 词表（M8）。
12. **文件职责整理：`TcsAttrModInstance.h` 只留修正器自己的词汇（用户指出，2026-09-17）**：原文件混装了"属性定义与实例所需的值域词汇"与"修正器词汇"。拆出 `TcsAttributeBounds.h`（`ETcsAttributeBoundMode` + `FTcsAttributeBound` + `FTcsAttributeBounds` + `ETcsAttributeValueDomain`——值域模式一度单独立文件，经用户质疑后按第 14 条合并回来）；`TcsAttrModInstance.h` 保留运算带（`ETcsAttributeOp` + 带权助手）、运算数（`ETcsOperandKind` + 定义侧/运行侧双形状）与账本修正器 `FTcsAttrModInstance`。
13. **候选能力：AttributeSet（用户提出，2026-09-17，**记录不实现**）**：不同宿主/不同游戏模式下，同一个 CombatEntity 类可能需要不同属性集合（如"竞技场模式"与"生存模式"各一套），故 `TcsAttribute` 需要一个 **AttributeSet** 概念：由宿主项目定义"某情景下该用哪些属性"。候选形态（未裁决）：`UTcsAttributeSetAsset : UPrimaryDataAsset`，内容 = `TArray<FName> DefIds`（+ 可能的按属性覆写）；施加 = 遍历集合调用 `AddAttribute(Unit, Name)`（与第 11 条的门面天然衔接）；**归属与时机**未定——每实体类型选集 / 每游戏模式选集 / 由谁在何时施加（M6 战斗实体组件的注册流程？），列入后续计划（R3 竖切用不到）。**与 Store 的联动（agent 建议，待用户确认）**：施加/撤离 = roster diff 替换（旧 Set 独有 → `RemoveAttribute`、新 Set 独有 → `AddAttribute`、共有保留）；`FTcsAttributeStore` 记"当前生效的 Set"；**每条实例的溯源推迟**（要动实例结构与 `AddAttribute` 签名，零消费者不预建）。**建议边界**：属性 = 结构（注册期/模式切换确定的 roster）、修正器 = 动态——守住它则实例无需来源追踪。
14. **值域词汇同文件（用户质疑后合并，2026-09-17）**：`ETcsAttributeValueDomain` 曾单独立文件；用户问"为什么单独一个文件"——理由有二（属定义/实例侧词汇而非修正器词汇；D2-6 独立裁决、未来 `IValueDomainPolicy` 的自然落点），但弱点明显：14 行单枚举文件违背本仓库"一文件多类型、取主导类型"惯例（我拆得偏细）。定案：**并入 `TcsAttributeBounds.h`**（值域 = 边界三态 + 越界模式，一对概念同文件），策略接口落地时按需再拆。
15. **修正器族命名归位（用户提议，2026-09-17）**：`TcsAttributeModifier.h` → **`TcsAttrModInstance.h`**，随之族内类型统一到短前缀：`FTcsAttributeModifier` → **`FTcsAttrModInstance`**、`FTcsAttributeModOperand(Def)` → **`FTcsAttrModOperand(Def)`**。理由：模板侧早已是 `UTcsAttrModDef`（2026-09-14 命名批钉的短前缀），实例侧却写全 `TcsAttribute*`，同族两个前缀；改名后 `Def（模板）↔ Instance（按模板物化出的账本条目）` 的成对关系一眼可读，也与全插件"定义资产 ↔ 实例"的分野一致。**两族前缀有意不同**：修正器族 `TcsAttrMod*`、属性族 `TcsAttribute*`（Name / Instance / Bounds / Store / Provider / Def / Subsystem）——已记入 `openspec/project.md` 命名约定。`ETcsAttributeOp` 与 `ETcsOperandKind` 不动：五带是**属性级**词汇（M5 参数链同用、聚合公式的家），不属修正器族。
16. **Def 命名标准：`<族>Def`，不带 `Asset` 后缀（用户定案，2026-09-17）**：用户指出资产侧命名不齐（`UTcsAttributeDefAsset` 带 Asset vs `UTcsAttrModDef` 不带）并要求定标准。定案：**`<族>Def` = 定义资产**（设计文档的概念词本就是"Def 资产"，`…DefAsset` 属语义重复），**`<族>DefTableRow` = 表行**。执行：`UTcsAttributeDefAsset` → **`UTcsAttributeDef`**（文件随之 `TcsAttributeDef.h` / `.cpp`；改名时漏改 `.generated.h` include 触发 UHT 报错，已修）+ 文档侧未实现的 `UTcsStateDefAsset` / `UTcsBuffDefAsset` / `UTcsSkillDefAsset` → **`UTcsStateDef` / `UTcsBuffDef` / `UTcsSkillDef`**（2026-09-14 命名批只钉了 `UTcsAttrModDef` / `UTcsSkillModDef` 不带 Asset，此三个按标准对齐）。标准落 `openspec/project.md`。
17. **身份只声明一次：行身份 = RowName，资产身份 = `[PrimaryAssetType, DefId]`（用户口径，2026-09-17）**：用户指出两张表行内不该再声明 `DefId` / `TemplateId`——DataTable 的键本就是 RowName。定案：**行内删除 id 字段**（消除"行名与字段谁为准"的双真相；上一轮我为冗余字段配的"资产 DefId ≠ 行内 DefId → Invalid"校验随之删除），**登记 API 改为 `RegisterAttributeDef(Attribute, DefRow)`**（属性名由调用方显式给出，行不再自带名）。资产侧同轮补齐（用户要求）：**显式声明 `static const FPrimaryAssetType PrimaryAssetType`（不靠类名派生）+ 覆写 `GetPrimaryAssetId()` 使名取 `DefId`/`TemplateId`**——资产文件可自由改名/挪目录而不失联（引擎 `UPrimaryDataAsset` 头文件明确"要改行为就在原生类里覆写本函数"）。连带：`IsDataValid` 的"资产名 ≠ DefId"警告删除（身份的名不取资产名，二者无需同名）；`PrimaryAssetTypesToScan` 注册仍属 M6 DefLibrary 轮（不注册也可解析 id）。



## Risks / Trade-offs

- **`SortKey` 与 Op 双真相**：设计里带权表（Override 0 / Add 10 / PercentAdd 15 / Mul 20 / FlatAdd 30）与 `SortKey` 字段语义重叠。缓解：本任务以 `GetTcsAttributeBandWeight(Op)` 钉住"带序唯一真相在 Op"，并在 `FTcsAttrModInstance.SortKey` 注释里写明"仅作同带内展示/审计位，折叠 MUST NOT 依赖它"。Task 5 的折叠器按 Op 分桶。
- **R3 内 `RegisterUnit` 兼发号**：M6 世界注册表落地需改发号方。缓解：句柄类型与 Store 键不变，迁移面限门面一个函数（已在 D1 记录）。
- **`ITcsAttributeProvider` 零实现者**：本任务只声明契约，首个实现随 plan2 Task 5 的战斗实体组件。缓解：契约形状取自 02 §2.3 原文（三方法、单位隐含——由实现者绑定单位），不发明新签名；编译期即被 TcsAttribute 内部 `TScriptInterface` 使用（`FTcsAttributeEvaluateContext`），不是死代码。
- **`AVD_Custom` 只有枚举没有策略**：R3 用 `AVD_Custom` 会静默退化。缓解：Task 5 的收口点必须对该值 ensure 提示（记入 Task 5 输入，本提案 Non-Goals 已声明）；R3 竖切不配置 Custom。
- **`AllowsValueConvention` 是 Core 公共面新增**：宿主若已实现自定义源，需要（可选）覆写。缓解：默认 true = 现有源行为不变（`Literal` 无需改动），是纯增量；已在 `param-value` 规格 MODIFIED 需求中显式记载。

## Open Questions

- `RegisterUnit(FName UnitName)` 的 `UnitName` 在 M6 落地后是否保留（调试名 vs 权威身份）？R3 按 plan1 保留为调试/屏显名；M6 轮再定。
- **"对被修饰主体之外的实体求值"如何表达**：R3 的属性值源只认读口自带的单位绑定（一个源 = 一个主体）。当出现"取*施法者*的攻击力"这类跨主体读取时，要么由调用方换一个绑定到该主体的读口实例，要么随 PV-1 的 `Subject` 一起进 Core 上下文（届时 `FTcsCombatEntityHandle` 需升格为反射 USTRUCT 才能作 UPROPERTY）。R3 无此需求（竖切用字面量源），留给 M5/TcsSkill 轮。
- 属性名稠密 id 缓存（D2-1）与 M8 词表注册表的落点：本任务不落字段，M8 轮需决定 id 缓存是"注册表世代号校验"还是"实例内 int32 冗余"（会影响 `FTcsAttributeInstance` 与 `FTcsAttributeName` 形状）。
18. **首次 PIE 实测的三项 FAIL（2026-09-17，用户实跑 28 PASS / 3 FAIL）**：两条是装置断言缺陷、一条是我写进规格的错误论断，均非实现缺陷：
    - **验证器返回值语义**：`UObject::IsDataValid` 基类默认返回 `NotValidated`（`Obj.cpp:6096` 已核源码）——"干净配置"从来不返回 `Valid`，故两个 `模板校验 → Valid` 断言写错。两侧修复：实现侧在两个 Def 资产末尾把 `NotValidated` 提升为 **`Valid`**（本资产确带规则且通过；否则编辑器把已校验通过显示为"未验证"，且 `unreal-cpp-style` 的"警告把 `Valid` 降级为 `NotValidated`"范式不成立）；装置侧断言加 `GetNumErrors() == 0`。
    - **`TMap`/`TSet` 元素地址不稳定（真缺陷）**：`TSet`（`TMap` 底层）元素存在 `TArray<FElementOrFreeListLink<...>>` 连续缓冲（`SparseArray.h:499`），**扩容即搬移**——"可长期缓存实例/Store 指针"是我在规格/设计/注释里写下的错误论断。修复：①**外层注册表改 `TMap<Handle, TUniquePtr<FTcsAttributeStore>>`**，使 02 §2.2a"适配器缓存 Store 指针"的设计前提真正成立（装置新增正面验证：新增 32 个单位后既有容器指针不变）；②**撤回实例指针可缓存的论断**（实例按属性名查询；边界写进规格/02 §2.2a/头注释）；③明确"D2-9 热路径不重查定义"的真实依据是**实例自持定义字段**，与指针缓存无关。
    - 装置同时把"实例指针是否搬移"降级为**观察输出**（不判 PASS/FAIL——搬移取决于分配器与容量，属非确定性期望，违反"断言只判结构不变量"纪律 MEM-20260916-01）。
