## MODIFIED Requirements

### Requirement: 标准步骤库（十步）

`TcsDamage` MUST 提供十阶段标准步骤库（09 §2.2；D7-5"流程 = 数据模板 + 标准件"），全部为**数据 struct**（无公共基类，D4-16），MUST 经 `UE_DEFINE_FLOW_STEP_EXECUTOR` 自注册。

**执行期行为契约**：正路共用的黑板键名（`BaseDamage` / `Executed` / `Absorbed` / `Kill` / `Hit` / `Crit` / `ExecuteCandidates`）属**标准步骤库的契约**（M8 校验与 Explain 认识）——**2026-09-22 改造：这些契约键从"自由 FName"改为"插件原生声明的 GameplayTag"**（`Tcs.Flow.Key.*`，常量名逐点换下划线，如 `Tcs.Flow.Key.BaseDamage` → `Tag_Tcs_Flow_Key_BaseDamage`）；**项目自定义键仍自由**，但类型同为 `FGameplayTag`、由**项目 ini** 声明。步骤 MUST NOT 自行推导基础伤害（零公式纪律）。

**契约键原生声明的理由**（与事件 tag 同款判据"谁拥有那个词，谁声明"）：契约键是**步骤之间的接口**，属**框架词汇**——若让项目声明，项目漏配即导致上下游步骤对不上（**静默读到 0**），框架契约被项目配置破坏。故 MUST 插件原生声明（`UE_DEFINE_GAMEPLAY_TAG`，随模块加载生效、零项目配置依赖）。

**MUST NOT 再用裸字面量**：现状中 `TcsFlowStepsRest.cpp` 的 `FName(TEXT("Crit"))` / `FName(TEXT("Hit"))` 等内联字面量 MUST 改为引用原生 tag 常量（消除跨文件裸字面量耦合）；`TcsFlowStepsRest.cpp` 中声明后从未使用的死常量（`TcsFlowRestKey_HitRate` / `TcsFlowRestKey_CritRate`）MUST 一并清理或改为 tag 常量并实际使用。

#### Scenario: 契约键跨文件一致

- **WHEN** 生产者步骤与消费者步骤分别在不同 cpp 里读写同一契约键
- **THEN** 二者引用**同一个原生 tag 常量**（编译期保证一致，不存在"两处字面量各写一遍"的失配可能）

#### Scenario: 项目自定义键仍自由

- **WHEN** 项目侧步骤要用一个契约键之外的键
- **THEN** 该项目 tag 由项目 ini 声明后即可用（插件不预设、不校验其存在性——归 M8 校验矩阵）

### Requirement: 通用数据步骤

`TcsDamage` MUST 提供两个**数据化**步骤（把"宿主挂点"变成可配置数据；09 §2.2 的两处）：

- `FTcsFlowModify{ FGameplayTag TargetKey; ETcsAttributeOp Op; FTcsParamValue Operand; TArray<FInstancedStruct> Conditions; }`——数据化黑板写入（"破甲阶段" = 一个数据步骤；**2026-09-22 改造：`TargetKey` 类型 `FName` → `FGameplayTag`**）；Operand 为 `FTcsParamValue`（黑板键引用保留为流程域自身的 Operand 选项，随其来源策略轮落地）。**MUST NOT 携带消耗策略**：`FTcsConsumePolicy` 含 `TFunction OnConsumed` 回调（纯 C++、不可反射、不可作 UPROPERTY）——消耗型提交只能来自 C++ 步骤或事件响应；数据步骤只做纯数值写入；
- `FTcsFlowDelegate{ FGameplayTag TargetKey; TScriptInterface<UTcsDamageFlowDelegate> Delegate; ... }`——数据化委托调用（轻量公式挂法；**2026-09-22 改造：`TargetKey` 类型改 tag**）。

#### Scenario: 数据步骤可配可跑

- **WHEN** 模板里放一个 `FTcsFlowModify`（TargetKey = 某 tag、Op = `TAO_Add`、Operand = Literal 5）
- **THEN** 该步执行后黑板对应键多出 5 的贡献（无需任何 C++ 步骤）
