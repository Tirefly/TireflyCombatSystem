# 审查清单：`add-datatable-defasset-sync` 实现问题整理（2026-06-18）

> **状态**：静态审查结果整理，仅列问题，不直接修改代码。
>
> **范围**：本清单仅覆盖 `add-datatable-defasset-sync` 提案在 `TireflyCombatSystem` / `TireflyCombatSystemEditor` 中新增或直接关联的实现代码。

## 1. 审查范围

本轮重点检查以下两类问题：

- 是否仍存在类似 `LogTcs` 那种跨模块导出 / 边界暴露隐患
- 是否存在违反 `openspec/project.md` 与 `CODE_REVIEW_CHECKLIST.md` 的强制编码规范问题

本轮重点审查的文件：

- `Source/TireflyCombatSystem/Public/DataTableSync/TcsDefDataTableRows.h`
- `Source/TireflyCombatSystem/Private/DataTableSync/TcsDefDataTableRows.cpp`
- `Source/TireflyCombatSystemEditor/Public/DataTableSync/TcsDefAssetDataTableSyncSubsystem.h`
- `Source/TireflyCombatSystemEditor/Private/DataTableSync/TcsDefAssetDataTableSyncSubsystem.cpp`
- `Source/TireflyCombatSystem/Public/TcsDeveloperSettings.h`
- `Source/TireflyCombatSystem/Private/TcsDeveloperSettings.cpp`
- `Source/TireflyCombatSystemEditor/TireflyCombatSystemEditor.Build.cs`

---

## 2. 跨模块边界风险

### 2.1 `UTcsDefAssetDataTableSyncSubsystem` 的 Public 头暴露了过多内部实现细节

文件：

- `Source/TireflyCombatSystemEditor/Public/DataTableSync/TcsDefAssetDataTableSyncSubsystem.h`

问题：

- 该头文件位于 `Public/`，但其中暴露了以下明显只服务于子系统内部调度/缓存的类型：
  - `ETcsPendingSyncRequestType`
  - `FTcsPendingSyncRequest`
  - `FTcsCachedDefAssetBinding`
  - `FTcsRemovedDefAssetSnapshot`
- 这些类型不构成稳定对外 API，却被放到了模块公开边界上。
- 这不会像此前 `LogTcs` 那样立刻触发链接错误，但会扩大 Editor 模块 ABI 面，并增加未来其他文件误依赖内部细节的概率。

结论：

- **中风险**
- 建议后续把这类纯内部类型收回到 `.cpp` 或至少收敛到非公开头边界内。

### 2.2 `TcsDefDataTableRows.h` 导出了偏实现化的同步描述符结构

文件：

- `Source/TireflyCombatSystem/Public/DataTableSync/TcsDefDataTableRows.h`

问题：

- `FTcsDefAssetSyncTypeDescriptor` 当前被放在 runtime `Public/` 头中导出。
- 但从职责上看，它更接近“DataTable 同步内部协议描述符”，而不是稳定的 runtime 公共数据模型。
- 当前 editor 侧的确依赖这组 helper，但这不等于必须把完整描述符结构暴露成公共 API。

结论：

- **低到中风险**
- 当前不会直接导致编译错误，但边界设计偏松，后续容易形成不必要耦合。

### 2.3 `TcsLogChannels.h` 的旧有跨模块风险仍在，但不属于本次新增问题

文件：

- `Source/TireflyCombatSystem/Public/TcsLogChannels.h`

问题：

- 该头中多组 `DECLARE_LOG_CATEGORY_EXTERN(...)` 仍沿用当前旧模式。
- 本次实现已经通过在 editor 侧定义 `DEFINE_LOG_CATEGORY_STATIC(LogTcsEditorSync, Log, All)` 绕开了 `LogTcs` 的链接问题。
- 但这只是局部绕开，不是系统级根治。

结论：

- **系统旧债，非本次新增问题**
- 后续如果其他跨模块代码再次直接使用这些日志分类，仍可能重现类似 `LNK2001`。

---

## 3. 强制编码规范偏差

### 3.1 新增的 editor 同步头源文件缺少版权文件头

文件：

- `Source/TireflyCombatSystemEditor/Public/DataTableSync/TcsDefAssetDataTableSyncSubsystem.h`
- `Source/TireflyCombatSystemEditor/Private/DataTableSync/TcsDefAssetDataTableSyncSubsystem.cpp`

问题：

- 两个文件都没有以 `// Copyright Tirefly. All Rights Reserved.` 开头。
- 这直接违反 `openspec/project.md` 与 `CODE_REVIEW_CHECKLIST.md` 的文件头强制规则。

结论：

- **高优先级硬规范问题**

### 3.2 四个新增核心文件都没有按规范使用 `#pragma region / #pragma endregion`

文件：

- `Source/TireflyCombatSystem/Public/DataTableSync/TcsDefDataTableRows.h`
- `Source/TireflyCombatSystem/Private/DataTableSync/TcsDefDataTableRows.cpp`
- `Source/TireflyCombatSystemEditor/Public/DataTableSync/TcsDefAssetDataTableSyncSubsystem.h`
- `Source/TireflyCombatSystemEditor/Private/DataTableSync/TcsDefAssetDataTableSyncSubsystem.cpp`

问题：

- 这四个文件都没有按 TCS 既有风格使用 region 组织结构。
- 与仓内既有头源文件相比，结构组织方式明显不一致。
- 该问题不是“建议优化”，而是项目内明写的强制规范未执行。

结论：

- **高优先级硬规范问题**

### 3.3 `UTcsDefAssetDataTableSyncSubsystem` 头文件中，成员函数注释覆盖明显不足

文件：

- `Source/TireflyCombatSystemEditor/Public/DataTableSync/TcsDefAssetDataTableSyncSubsystem.h`

问题：

- 该类 private 区中的绝大多数成员函数只有声明，没有对应的成员函数注释。
- 按项目约束，“所有成员变量与成员函数需要注释”，这里没有达到要求。

典型缺失区域包括：

- 回调注册/注销
- 请求队列调度
- DataTable / DefAsset 双向同步入口
- 删除快照与抑制删除相关工具函数
- cache / config 查找工具函数

结论：

- **高优先级硬规范问题**

### 3.4 多参数函数虽然有总说明，但参数逐项注释不完整

文件：

- `Source/TireflyCombatSystem/Public/DataTableSync/TcsDefDataTableRows.h`
- `Source/TireflyCombatSystem/Public/TcsDeveloperSettings.h`

问题：

- 若按你当前要求执行，参数数目大于等于 2 的函数，需要为每个参数明确写注释。
- 当前若干函数只有整体描述，没有把参数逐个解释完整。

典型对象：

- `TryBuildDefAssetDataTableRow(...)`
- `TryApplyDefAssetDataTableRow(...)`
- `ValidateAllConfigs(...)`

结论：

- **中优先级规范问题**

### 3.5 新增 `.cpp` 文件没有按仓内习惯做实现层级分区

文件：

- `Source/TireflyCombatSystem/Private/DataTableSync/TcsDefDataTableRows.cpp`
- `Source/TireflyCombatSystemEditor/Private/DataTableSync/TcsDefAssetDataTableSyncSubsystem.cpp`

问题：

- 两个实现文件当前基本是顺排函数，没有按职责做区域分组。
- 与 TCS 现有大量 `.cpp` 文件的组织风格不一致。

结论：

- **中优先级规范问题**

---

## 4. 命名规范问题与补充约束

### 4.1 本次新增的多种 RowStruct 名称未携带 `Tcs` 模块前缀

文件：

- `Source/TireflyCombatSystem/Public/DataTableSync/TcsDefDataTableRows.h`

问题：

- 当前新增的多个结构体命名为：
  - `FAttributeDefRow`
  - `FAttributeModifierDefRow`
  - `FBuffDefRow`
  - `FSkillDefRow`
  - `FSkillModifierDefRow`
  - `FStateSlotDefRow`
- 这些名字都没有携带 `Tcs` 模块前缀。
- 按 TCS 既有命名体系，自定义类型应以模块前缀作为“命名空间声明”存在，避免类型名脱离模块语境后变得过于通用。

结论：

- **高优先级命名规范问题**

### 4.2 新增强制命名约束：class / struct / enum / interface / delegate 等自定义类型，必须带 `Tcs` 模块前缀

新增约束说明：

- 后续在 TCS 模组中新增自定义类型时，必须像现有 TCS 代码体系一样，把 `Tcs` 作为模块级命名空间声明体现在类型名中。
- 换句话说，不应再出现缺少模块语义前缀的裸类型名。

推荐形式：

- `UTcs...`
- `FTcs...`
- `ETcs...`
- `ITcs...`
- 委托名也应保持 `Tcs` 模块归属语义

禁止形式：

- `FAttributeDefRow`
- `FBuffDefRow`
- `ESyncRequestType`
- `FRemovedSnapshot`

说明：

- 这里的“像 TCS 模组一样，有 `Tcs` 的字段作为 namespace 声明”，在执行层面就落实为：**所有自定义类型名必须显式携带 `Tcs` 模块前缀，而不是依赖文件路径隐式表达归属**。

---

## 5. 这次实现里不应误报的问题

以下内容本轮**不应继续作为本次提案新增问题**反复追责：

- `FInstancedStruct` / `StructUtils` 的显式模块依赖问题

原因：

- TCS 插件在本提案之前就已经存在多处 `FInstancedStruct` 使用。
- 本轮完整 UBT 编译已经通过。
- 因此这次不应再把“必须显式新增 `StructUtils` 依赖”当成本提案整改结论。

---

## 6. 建议整改顺序

建议按以下顺序整改，而不是混在一起同时改：

1. **先修硬规范问题**
   - 补版权文件头
   - 给新增头源文件补 `#pragma region / #pragma endregion`
   - 补齐类成员函数注释

2. **再修命名问题**
   - 统一补 `Tcs` 模块前缀
   - 特别是本次新增 RowStruct 类型名

3. **最后再收边界**
   - 把 `UTcsDefAssetDataTableSyncSubsystem` public 头中不必要暴露的内部结构体收回实现侧
   - 评估 `FTcsDefAssetSyncTypeDescriptor` 是否需要继续作为 runtime public API 暴露

4. **系统旧债单独排期**
   - `TcsLogChannels.h` 的跨模块导出风险不要混进这次小修里顺手乱改
   - 如果要改，应该单独作为一次明确边界治理任务处理

---

## 7. 本文档用途

本文档仅用于：

- 记录 `add-datatable-defasset-sync` 当前实现中已确认的问题
- 为后续精确整改提供顺序化输入

本文档不表示：

- 已经修改这些问题
- 已经重新验证整改结果
- 已经完成最终代码规范闭环