## 1. Implementation

### TcsCore（两处公共面改动）

- [x] 1.1 `Public/Handle/TcsCombatEntityHandle.h`（header-only）：`FTcsCombatEntityHandle{uint64 Id, 0=无效}` + `IsValid` + 相等 + `GetTypeHash`；`FTcsCombatEntityHandleRegistry::Allocate()`（std::atomic 递增、首个为 1、进程内永不复用——对齐 `FTcsSourceHandleRegistry` 形式）——**不加导出宏**（见 1.12）
- [x] 1.2 `Public/Parameter/TcsParamValueSource.h`：新增能力位 `virtual bool AllowsValueConvention() const { return true; }`（判据 = "本源书写的数值即结果"，D5-18 v3）
- [x] 1.3 `Public/Parameter/TcsParamSource_ParamRef.h`：覆写 `AllowsValueConvention() → false`（读到的已是规范值，再转 = 二次转换）
- [x] 1.4 **落地期增补**：`FTcsParamEvaluateContext` 新增类型标识虚函数 `GetScriptStruct()`（PV-1 checked cast 的载体——USTRUCT 无内建类型查询；GAS `FGameplayEffectContext` 同款），派生上下文覆写自身、源侧以 `IsChildOf` 判定（支持多层派生）

### TcsAttribute 类型词汇（`Public/Attribute/`）

- [x] 1.5 `TcsAttributeName.h`：USTRUCT `{FName Name}`——默认构造 + `explicit FTcsAttributeName(FName)` + `IsNone()` + 相等 + `GetTypeHash`；不落稠密 id 字段（M8 轮）
- [x] 1.6 `TcsAttrModInstance.h`：`ETcsAttributeOp` 五带（0..4，UMETA 带 DisplayName+ToolTip）+ `GetTcsAttributeBandWeight`（0/10/15/20/30，带序唯一真相在 Op）；`ETcsOperandKind` / `ETcsAttributeBoundMode` / `ETcsAttributeValueDomain`（Custom=1 逃逸位）；`FTcsAttributeBound` / `FTcsAttributeBounds`（反射，带 EditCondition 分流）；`FTcsAttrModOperandDef`（定义侧，Literal=`FTcsParamValue`）/ `FTcsAttrModOperand`（账本侧，Literal=已解析 double）；`FTcsAttrModInstance`（Target/Op/Operand/Source/SortKey/Tag）
- [x] 1.7 `TcsAttributeInstance.h`：账本实例 `{Attr, BaseValue, CachedCurrent, bDirty, Bounds, ValueDomain, ModifierSlots}`
- [x] 1.8 `TcsAttributeProvider.h`：`UINTERFACE(MinimalAPI)` + 三读口（`GetBaseValue` / `GetCurrentValue` / `PeekPending`，`BlueprintNativeEvent`，单位由实现者绑定）
- [x] 1.9 `TcsParamSource_AttributeScaled.h`：`FTcsAttributeEvaluateContext`（持 `Provider`，`GetScriptStruct` 覆写）+ `FTcsParamSource_AttributeScaled`（Evaluate = checked cast → 读口 × 系数，失败/空读口落 Fallback；`AllowsValueConvention → false`）
- [x] 1.10 `TcsAttrModDef.h` + `Private/Attribute/TcsAttrModDef.cpp`：`UDataAsset`（Target/Op/Operand/ValueConvention（Bitmask 元数据）/SortKey/Tag）+ `WITH_EDITOR` `IsDataValid`：约定列白名单（源能力位虚函数判定，不建中心名单）+ 空来源 / 空属性换算 / 空 Target 报错，错误带可操作建议

### TcsAttribute 数据宿主

- [x] 1.11 `TcsAttributeStore.h`：每单位容器 `{TMap<FTcsAttributeName, FTcsAttributeInstance> Attributes}` + `FindInstance`（mutable/const）+ `GetBaseValue`；**不提供**"看似会重算"的当前值读取口（惰性重算随 Task 5 管线）
- [x] 1.12 `Public/TcsAttributeSubsystem.h` + `Private/TcsAttributeSubsystem.cpp`：`UWorldSubsystem`（非 Tickable）+ 世界类型过滤 + `RegisterUnit`/`UnregisterUnit`/`GetUnitName`/`AddAttribute(Unit, Name, Def)`（重复添加与动态边界自引用 ensure 拒绝）/`GetStore`（mutable/const）/`Deinitialize` 清空；日志走 `LogTcsAttribute`
- [x] 1.13 **落地期修复（Task 1 潜伏缺陷）**：全内联值类型去除模块导出宏——`FTcsSourceHandle` + `FTcsSourceHandleRegistry`（本次链接期 LNK2019 直接命中）、`FTcsEventSubscriptionHandle`（同类潜伏项）；新类型（`FTcsCombatEntityHandle` 家族、M2 账本四类型）一律不加
- [x] 1.14 **落地期修复**：`TcsAttrModInstance.h` 四个枚举的"前缀注记"从枚举体内 `//` 注释移入枚举 doc 块——UHT 把枚举值上方 `//` 注释当作该值 ToolTip 元数据，与 `UMETA(... ToolTip=...)` 并存直接报错
- [x] 1.15 **复评补正（2026-09-17）**：新增 `Public/Attribute/TcsAttributeDef.h`（`FTcsAttributeDef : FTableRowBase`，`BaseValue`/`Bounds`/`ValueDomain`，行名即属性名）；门面 `DefineAttribute(Unit, Name, BaseValue, Bounds, ValueDomain)` → `AddAttribute(Unit, Name, Def)`（定义数据回到词表行单一载体）
- [x] 1.16 **复评补正（2026-09-17）**：`UTcsAttrModDef` 基类 `UDataAsset` → `UPrimaryDataAsset`（Def 资产族统一约定，含注释说明判据）；族级约定回写设计 02/03/08 + project.md + plan1（未来 StateDefAsset 家族与 SkillModDef 同此）
- [x] 1.17 **二次复评补正（2026-09-17）：文件名去类型前缀**（UE 规范：文件名 = 类型名去前缀）——全插件 **35 个文件**改名（`UTcsAttrModDef.h` → `TcsAttrModDef.h`、`FTcsAttributeName.h` → `TcsAttributeName.h`、`TTcsInstancePool.h` → `TcsInstancePool.h`、`ITcsTimeSource.h` → `TcsTimeSource.h` 等；含 Task 0-3 既有文件的**存量违规**），全库 include + `.generated.h` + 活动文档路径引用同步，UHT 产物按新名重生；规范写入 `unreal-cpp-style`（structure.md 新增「文件命名」节 + 检查清单项）
- [x] 1.18 **计划文档同步**：plan1/plan2 的 File Structure 与各 Task 文件清单（含**尚未创建**的 plan2 全部文件与 Task 5 的 `TcsAttributePipeline.h`）一律改写为去前缀名——否则后续任务会照旧名再犯（归档提案 `changes/archive/` 保持原貌，为冻结历史记录）
- [x] 1.19 **三次复评补正（2026-09-17）：属性定义改双轨制**（用户口径：表 = 编辑期策划载体、资产 = 运行期载体，运行期零 DataTable 加载）——属性定义拆出运行期资产（`PrimaryDataAsset`）与编辑期表行（`FTableRowBase`）；族级双轨语义回写 02 §2.1 / 03 §2 / 08 §5
- [x] 1.20 **四次复评补正（2026-09-17，用户五条口径）**：
  - **形态收口**：删除载荷层 `FTcsAttributeDef`（含 `TcsAttributeDef.h`）——`FTcsAttributeDefTableRow`（`DefId` + Base/Bounds/ValueDomain）成为**字段形状唯一声明处**，`UTcsAttributeDef` 组合持有 `Def`（不复制字段集），两类型同住 `TcsAttributeDef.h`；资产 `IsDataValid` 增"资产 DefId ≠ 行内 DefId → Invalid"
  - **修正器模板同款**：新增 `FTcsAttrModDefTableRow : FTableRowBase`（`Target/Op/Operand/ValueConvention/SortKey/Tag`——身份 = RowName），`UTcsAttrModDef` 改为 `TemplateId` + `Def` 行；`IsDataValid` 改读 `Def.*` 并增身份一致性校验；CSV 局限写入表行注释
  - **调用面收口**：`AddAttribute(Unit, AttrName)` / `RemoveAttribute(Unit, AttrName)`（单位侧只认属性名）+ 内部**属性定义表**（`RegisterAttributeDef(属性名, 行)` / `FindAttributeDef`）；定义未登记、重复登记、移除未持有均入拒绝面
  - **候选能力 AttributeSet 入档（不实现）**：02 §2.1 候选能力块 + plan1/README + design.md 决策 13
  - **文件职责整理**：新增 `TcsAttributeBounds.h`（`ETcsAttributeBoundMode` + `FTcsAttributeBound` + `FTcsAttributeBounds` + `ETcsAttributeValueDomain`——值域一对概念同文件）；修正器文件只留运算带/运算数双形状/账本条目
- [x] 1.21 **命名归位（用户提议，2026-09-17）**：`TcsAttributeModifier.h` → `TcsAttrModInstance.h`，族内类型统一短前缀——`FTcsAttributeModifier` → `FTcsAttrModInstance`、`FTcsAttributeModOperand(Def)` → `FTcsAttrModOperand(Def)`（与模板侧 `UTcsAttrModDef` 同族；`Def` 模板 ↔ `Instance` 账本条目的成对关系一眼可读）；两族前缀差异（`TcsAttrMod*` vs `TcsAttribute*`）记入 `openspec/project.md` 命名约定；`ETcsAttributeOp`/`ETcsOperandKind` 不动（属性级词汇，M5 参数链同用）
- [x] 1.22 **Def 命名标准（用户定案，2026-09-17）**：定义资产 = `<族>Def`（**不带 `Asset` 后缀**——概念词"Def 资产"已含 Asset 义）、表行 = `<族>DefTableRow`。执行：`UTcsAttributeDefAsset` → `UTcsAttributeDef`（文件 `TcsAttributeDef.h` / `.cpp`；改名时漏改 `.generated.h` include 触发 UHT 报错，已修）；文档侧未实现的 `UTcsStateDefAsset`/`UTcsBuffDefAsset`/`UTcsSkillDefAsset` → `UTcsStateDef`/`UTcsBuffDef`/`UTcsSkillDef`（2026-09-14 命名批按新标准更新，该决策文档加修订注记）；标准条文落 `openspec/project.md`

- [x] 1.23 **身份只声明一次（用户口径，2026-09-17）**：表行删除 id 字段（`FTcsAttributeDefTableRow.DefId`、`FTcsAttrModDefTableRow.TemplateId`）——**身份 = RowName**；登记 API 改 `RegisterAttributeDef(Attribute, DefRow)`（属性名由调用方给出）；上一轮为冗余字段配的"资产 DefId ≠ 行内 DefId"校验删除
- [x] 1.24 **主资产身份显式化（用户要求，2026-09-17）**：`UTcsAttributeDef` / `UTcsAttrModDef` 各加 `static const FPrimaryAssetType PrimaryAssetType`（显式声明，不靠类名派生）+ 覆写 `GetPrimaryAssetId()` 返回 `[PrimaryAssetType, DefId/TemplateId]`（名不取资产名——资产文件可自由改名/挪目录）；`IsDataValid` 的"资产名 ≠ DefId"警告删除；装置增"主资产身份 = [PrimaryAssetType, DefId]"检查
- [x] 1.25 **首次 PIE 实测三项 FAIL 的归因与修复（2026-09-17，用户实跑 28 PASS / 3 FAIL）**：
  - **两条装置断言缺陷（非实现缺陷）**：`UObject::IsDataValid` 基类默认返回 **`NotValidated`**（`Obj.cpp:6096`，已核源码），故"干净配置"的返回值从来不是 `Valid`——两个 `模板校验 → Valid` 断言直接写错。修复分两侧：①实现侧在两个 Def 资产的 `IsDataValid` 末尾把 `NotValidated` 提升为 **`Valid`**（本资产确带规则且通过；否则编辑器把已校验通过的资产显示为"未验证"，且 `unreal-cpp-style` 的"警告降级 Valid→NotValidated"范式才成立）；②装置侧断言同时要求 `GetNumErrors() == 0`
  - **一条真缺陷（规格论断错误）**：`容器稳定性` 断言暴露我写进规格/设计/注释的"TMap 节点地址稳定、可长期缓存实例指针"是**错的**——引擎 `TSet`/`TMap` 元素存在 `TArray<FElementOrFreeListLink>` 连续缓冲里（`SparseArray.h:499`），**扩容即搬移**。修复：①**外层注册表改 `TMap<Handle, TUniquePtr<FTcsAttributeStore>>`**（间接层使"适配器缓存 Store 指针"这一 02 §2.2a 设计前提真正成立，并新增正面验证：新增 32 个单位后既有容器指针不变）；②**撤回实例指针可缓存的论断**（改为"实例按属性名查询"，并以注释/规格写明边界）；③"热路径不重查定义"（D2-9）的真实依据是**实例自持定义字段**，与指针缓存无关

## 2. Verification

- [x] 2.1 `TcsAttribute.Build.cs` 依赖现状确认（Task 0 已就位，本任务未改）
- [x] 2.2 Development Editor 编译通过（UBT，LAC 项目）：2026-09-16 首次失败于导出宏 LNK2019 + UHT 枚举注释元数据冲突，两处修复后通过；其后复评补正、改名、指针纪律修复各轮均再次通过——**全部零警告**（最终一次：源码已全部进二进制，DLL 时间戳晚于全部源文件）
- [x] 2.3 临时测试装置（`Source/TcsAttribute/Private/Testing/`，**不入库、不进规格**）`Tcs.Test.Attribute`：**33 项**正向检查（属性名语义与哈希 / 带权五带 / 约定能力位 / 属性值源四条路径 / 模板校验五例 / 定义双轨两轨承载 / 定义资产校验与主资产身份 / 单位注册 / 属性添加与初值 / 空查询 / 值域回读 / 增长期容器指针纪律 / 属性移除 / 注销清记录）+ 一条**观察输出**（实例指针是否搬移，不判 PASS/FAIL——非确定性期望不作断言）；故意 ensure 的拒绝面拆为 `Tcs.Test.Attribute.Dangling`（opt-in，六组八项）
- [x] 2.4 **用户 PIE 实测通过**：首跑 `Tcs.Test.Attribute` = 28 PASS / 3 FAIL → 三项归因（两条装置断言错、一条规格论断错）并修复 → 编译后再测**确认无问题**（用户回报"测试没什么问题"）
- [x] 2.5 plan1 Task 4 步骤勾选 + 实施注记（含 2026-09-16/17 六轮复评注记）
- [x] 2.6 停点待用户检查——用户已多轮复评并确认本任务收束；**代码提交仍单独经用户授权**（工作区未提交）

## 3. 后续任务输入（不在本任务范围）

- Task 5：折叠器（D5-5 v3 带式聚合 + Override 组取最大）按 Op 分桶、忽略 SortKey；`AVD_Custom` 收口点须 ensure 提示（策略接口未建）；`GetCurrentValue` 惰性重算口与 `PeekPending` 语义随管线补入 Store/门面。
- **M8 词表注册表**：`FAttributeRegistry`（`Resolve(FName) → 稠密 id`、重名/非法引用加载期报错、行名 ↔ 项目侧常量映射校验）+ DevSettings 指路 DataTable + 世界/游戏实例启动装载；届时门面可增 `AddAttribute(Unit, Name)` 重载（Def 从词表取）。R3 的属性 DataTable 只验证"能被反射承载"。
- **M6 DefLibrary**：`PrimaryAssetTypes` 注册（插件自带 Config 或宿主项目 Config；插件目录 `DefaultGame.ini` 是否被引擎自动加载未实证，实施时实测）+ Def 资产族的按类型发现/加载。
- plan2 Task 5：`UTcsCombatEntityComponent` 实现 `ITcsAttributeProvider`（单位绑定的首个实现者）；M6 世界注册表接管 `FTcsCombatEntityHandle` 发号。
- 跨主体求值（"取施法者攻击力"）的表达方式留待 M5/TcsSkill 轮（design.md Open Questions）。
