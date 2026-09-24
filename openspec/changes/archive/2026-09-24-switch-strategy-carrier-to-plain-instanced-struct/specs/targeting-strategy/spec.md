## MODIFIED Requirements

### Requirement: 选择器策略契约

`TcsTargeting` MUST 提供选择器策略基类 `FTcsTargetSelectorStrategy`（USTRUCT 反射基类，D3-7 v3）——解析目标集写入 `Context.Targets`：

- **抽象由约定达成，MUST NOT 使用纯虚（`= 0` / `PURE_VIRTUAL`）**：UHT 对每个 USTRUCT 生成 `TCppStructOps<T>`（需可默认构造），抽象类报 C2259；`PURE_VIRTUAL` 在 `CHECK_PUREVIRTUALS` 下展开为 `= 0`（`CoreMiscDefines.h:100-102`）。故基类 MUST 提供**中性默认实现** + `meta = (Hidden)`（编辑器 picker 不可选基类；先例 `FTcsParamValueSource`）；
- **填充纪律**：`Resolve` 只**填充** `OutTargets`（调用方先清空），MUST NOT 假设其为空；
- **注入可空**：`EntityQuery` MAY 为 `nullptr`——策略 MUST 容忍（降级 + Warning），MUST NOT 解引用空指针；
- **宿主扩展 = C++ 新 struct 子类**（零框架改动）：配置面经**裸 `FInstancedStruct`** 内嵌编辑（**2026-09-24 换型**，提案 `switch-strategy-carrier-to-plain-instanced-struct`），`BaseStruct` 限定由**手写 metadata** 提供（`meta = (BaseStruct = "/Script/TcsTargeting.TcsTargetSelectorStrategy")`）——**MUST NOT** 用 `TInstancedStruct<T>`（其字段在宿主脚本层导出为空壳，脚本层配不了，台账 S-6）；BP 策略扩展通道放弃（R0 §9）。
- **换型代价（明示接受）**：丢编译期类型限定 ⇒ 改由运行期校验兜底（`GetPtr<T>()` 的 `IsChildOf` 检查 + 调用点判空）；且 `BaseStruct` metadata **写错则 picker 静默不限**（`TryFindTypeSlow` 找不到返回 nullptr 不报错）——故该 metadata 的存在性与正确性 MUST 进 M8 校验矩阵（台账 R8-1 范围）。

#### Scenario: 基类不可被编辑器选中

- **WHEN** 在步骤的 `FInstancedStruct`（带 `BaseStruct` metadata）成员上打开类型 picker
- **THEN** 基类不在列表中（`Hidden` 元数据被过滤），只列 C++ 子类

#### Scenario: Resolve 只填充不清空

- **WHEN** 调用方预留了非空 `OutTargets` 后调用任一选择器的 `Resolve`
- **THEN** 策略只追加/改写自己的产出，清空责任在调用方

#### Scenario: 选择器字段对脚本层可读写

- **WHEN** 检查 `FTcsStepSelectTargets` 的宿主脚本绑定产物（UnrealSharp glue）
- **THEN** `Selector` / `Filters` 字段生成真实的读写代码（非空函数体）——脚本层可构造并配置选目标步骤

### Requirement: SelectTargets 步骤与执行器

`TcsTargeting` MUST 提供链步骤 `FTcsStepSelectTargets`（`Public/Chain/TcsStepSelectTargets.h`）：字段 `FInstancedStruct Selector`（`meta = (BaseStruct = "/Script/TcsTargeting.TcsTargetSelectorStrategy")`）+ `TArray<FInstancedStruct> Filters`（`meta = (BaseStruct = "/Script/TcsTargeting.TcsTargetFilterStrategy")`）——**2026-09-24 换型**（原为 `TInstancedStruct<T>`，在脚本层导出为空壳导致脚本拼不出本步骤）。

- 执行器 MUST 经 `UE_DEFINE_EFFECT_STEP_EXECUTOR` **跨模块自注册**（TcsTargeting → TcsEffect 注册表，机制层零改动）；
- 执行序：**清空** `Context.Targets` → `Selector->Resolve(Context, EntityQuery, Candidates)`（`EntityQuery` 取自 `UTcsEffectSubsystem::GetEntityQuery()`，经运行态 `Run.Owner`）→ 逐候选过 `Filters`（AND + 短路）→ 通过者**保序**写回 `Context.Targets`；
- **MUST NOT 做存活过滤**：候选的有效性由宿主 Filter 表达（框架不认识"存活"）；选择器产出的失效句柄由 Filter 淘汰，无 Filter 时原样传递；
- **即时步骤**：恒返回 `TSR_Completed`；
- **未配置 Selector**：保持 `Context.Targets` 原样 + Warning（不静默清空、不 ensure）；
- **取用纪律（换型连带）**：`Selector` / `Filters` 元素的取用 MUST 走 `GetPtr<T>()` + **判空**（裸载体的编译期限定已移除，运行期 `IsChildOf` 校验失败时返回 nullptr）。

#### Scenario: 未配置选择器时目标集原样

- **WHEN** 步骤的 `Selector` 为空载体（未配置）
- **THEN** `Context.Targets` 保持原样，留 Warning（不静默清空）

#### Scenario: 过滤按 AND 且保序

- **WHEN** 配两个 Filter（都通过 / 一过一不过）与多个候选
- **THEN** 只有全通过的候选留下，且保持选择器产出的顺序
