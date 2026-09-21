# 竖切内容资产建立指南（TcsDev / plan2 Task 6）

> 落点：`E:\Projects_Dev\LegendAutoChess\Content\TcsDev\`
> 前置：插件与宿主已编译通过（`TcsDev` 模块产出）；编辑器已重启以加载新模块。
> 本指南是**人工步骤**（`.uasset` / `.umap` 是二进制，只能编辑器内建）。

## 0. 准备

1. 编辑器内新建目录 `Content/TcsDev/`（若不存在）。
2. 打开 **Output Log**，确认 `LogTcsDev` 可见（过滤器里加 `LogTcsDev`）。
3. 确认插件已加载：控制台输入 `Tcs.Test.Slice.Run`（应提示"未找到 PIE/Game 世界"——那是正常的，
   说明命令已注册；真正跑要在 PIE 里）。

---

## 1. 属性定义资产 4 条（`UTcsAttributeDef`）

右键 `Content/TcsDev/` → **Miscellaneous → Data Asset** → 选 **TcsAttributeDef**，建 4 个：

| 资产名 | `DefId` | `Def.BaseValue` | `Def.Bounds.Min` | 说明 |
|---|---|---|---|---|
| `DA_Health` | `Health` | `100` | 静态 `0` | 生命（扣血目标） |
| `DA_MaxHealth` | `MaxHealth` | `100` | 静态 `0` | 生命上限（供 Health 动态边界引用） |
| `DA_Attack` | `Attack` | `30` | 无 | 攻击力（公式输入） |
| `DA_Armor` | `Armor` | `5` | 无 | 护甲（公式输入） |

**要点**：
- `DefId` 必须与资产名尾段一致（`DA_Health` → `Health`）——它不是从资产名派生的，是**显式字段**；
  资产文件改名/挪目录不影响解析（身份 = `[PrimaryAssetType, DefId]`）。
- `Def.Bounds.Min.Mode` 选 `静态`（ABM_Static）后才会出现 `StaticValue` 字段（EditCondition）。
- `Health` 想用动态上限（HP ≤ MaxHealth）就把 `Def.Bounds.Max.Mode` 设为 `动态`、
  `DynamicAttribute` 填 `MaxHealth`（**要求同一单位持有 MaxHealth**，否则值被静默钳到 0——已知行为）。
- 4 条建完后：**选中全部 → 右键 → Validate Assets**（应零 Invalid）。

---

## 2. 链资产 1 条（`UTcsEffectChainDef`）——检查点 6 的主体

右键 `Content/TcsDev/` → **Miscellaneous → Data Asset** → 选 **TcsEffectChainDef**，命名 `DA_SliceChain`。

填字段：

| 字段 | 值 |
|---|---|
| `ChainId` | `Slice_Chain` |
| `Chain.ChainId` | `Slice_Chain`（**必须与上一行完全一致**——不一致会被拒绝登记） |
| `Chain.MaxStepsPerFrame` | `64`（默认） |

`Chain.Steps` 数组加 **3 个元素**，每个元素点类型选择器选对应步骤 struct：

### 元素 0 — `TcsStepWaitDelay`
| 字段 | 值 |
|---|---|
| `Seconds` | `0.5` |

### 元素 1 — `TcsStepSelectTargets`
| 字段 | 值 |
|---|---|
| `Selector` | 点类型选择器 → 选 **`TcsSelSelf`**（本任务新增的框架内置选择器） |
| `Filters` | 留空（空数组 = 全过） |

> **这一步是"框架内置选择器"的实证点**：picker 里应该能看到 `TcsSelSelf`。
> 如果看不到，说明插件未重编译或编辑器未重启。

### 元素 2 — `TcsStepDamage`
| 字段 | 值 |
|---|---|
| `DamageBase` → `Source` | 类型选 **`TcsParamSource_Literal`**，其 `Value` = `30` |
| `TargetAttrKey.Name` | `Health` |
| `FlowTemplateId` | `Default_Slice`（**注意不是 `Default`**——切片装配版带公式 delegate，见下） |
| `FormulaParams` | 留空 |

**建完 Validate Assets**（应零 Invalid）。

> **为什么 `FlowTemplateId` 填 `Default_Slice`**：插件在 `UTcsDamageSubsystem::Initialize` 自登记了一份
> `Default`（官方四步，**不带公式 delegate**——插件零公式）。切片装配（`UTcsDevBootstrap`）另登记了
> `Default_Slice`（同样四步 + 竖切公式 `max(1, Attack−Armor)`）。链上填 `Default_Slice` 才会走公式。

---

## 3. 测试地图 + 2 个单位

1. **新建地图**：`File → New Level → Basic`，存为 `Content/TcsDev/L_TcsDev_Slice.umap`。
2. **建 2 个 Actor**：
   - 拖两个 `Empty Actor` 到场景（或任一简单静态网格 Actor），命名 `TcsDev_Caster` 与 `TcsDev_Target`；
   - 各自 **Add Component → Tcs Combat Entity Component**（`UTcsCombatEntityComponent`）。
3. **保存地图**。

> 单位**不需要**预先配属性——`Tcs.Test.Slice.Run` 命令会自己生成两个测试单位并装配属性
> （地图里的单位仅供你手动点选观察，验收以命令为准）。

**可选**：把地图设为 PIE 默认地图（`Project Settings → Maps & Modes`），或直接
`Open Level` 后点 Play。

---

## 4. 验收（PIE 内执行）

在 PIE 运行中，控制台依次执行：

### 4.1 `Tcs.Test.Slice.Run` —— 常规验收（**应当零红字**）

预期看到（屏显 + Output Log）：

```
检查 0：定义库就绪且失败清单为空（IsRuntimeReady=true；失败 0 条）
检查 1a：AssetRegistry 扫到链资产 1 个
检查 1b：定义库自动缓存链 1/1（未调 RegisterChain——发现层实证）
检查 2：链已装配进本世界 1/1（EffectSubsystem::FindChain 可查）
检查 3：链步骤类型全可解析（3/3 步；不可解析 0）
检查 4：属性定义已装载 4/4
检查 5：单位属性装配（施法者 4 条 / 目标 4 条；Attack=30.0 / Armor=5.0 / Health=100.0）
检查 6：宿主自研流程扣血（Health 100.0 → 75.0；模板 Slice_Flow = 2 个宿主自研步骤）
检查 7a：内容链起链成功
—— 检查 7b（+2.0s 战斗时间后的延迟判定）——
检查 7b：WaitDelay 到期后续走，Damage 步扣血生效
```

**判据**：全部 PASS；Output Log 里本命令区间**无 `Error`**（Warning 也不应有）。

### 4.2 `Tcs.Test.Slice.Reject` —— 拒绝面（**故意产生红字**）

预期 3 条预期失败输出（2 条校验失败 + 1 条 Error 日志），命令首行已声明"红字为预期"。

### 4.3 跨世界装配（Task 5 遗留 Scenario）

PIE 运行中切关卡：控制台 `open L_TcsDev_Slice`（或换到别的图再切回来）→ 重新执行
`Tcs.Test.Slice.Run` → **检查 1b/2 仍应通过**（定义库是 GameInstance 级、跨世界存活；
链由新世界的 `OnPostWorldInitialization` 装配路径重新登记）。

### 4.4 检查点 6 端到端（**零 C++ 改链**）

1. 停止 PIE，编辑器内打开 `DA_SliceChain`，把 `DamageBase` 的 `Value` 从 `30` 改成 `45`；
2. **不改任何代码**，直接 PIE → `Tcs.Test.Slice.Run`；
3. 检查 7b 的扣血量应随之变化（走公式时实际扣血 = `max(1, Attack−Armor)`，而 `DamageBase` 是**输入值**——
   见下方说明）。

> **注意公式与输入的关系**：链资产配的 `DamageBase` 是**输入值**；切片装配的公式 delegate
> `CalculateBaseDamage` 会把它**替换**为 `max(1, Attack − Armor)` = `max(1, 30 − 5)` = `25`。
> 所以在带公式的 `Default_Slice` 模板下，改 `DamageBase` **不会**改变扣血量——这会让检查点 6 失效。
>
> **要验"改数值零 C++ 生效"，二选一**：
> - **做法 A（推荐）**：把链的 `FlowTemplateId` 改成 `Default`（插件的无公式模板）——此时扣血量 = `DamageBase`
>   原样（`Execute` 直接扣黑板值），改 `30 → 45` 扣血量随之变化，**这才是"纯数据驱动"的实证**；
> - **做法 B**：改 `DA_Attack` 的 `BaseValue`（公式输入变了）——但这是改**属性资产**不是链资产，
>   验的是"属性数据驱动"而非"链数据驱动"。
>
> 两者都合法，**建议两种各跑一次**：A 验链数据驱动、B 验属性数据驱动。

---

## 5. 记录与回报

> **本节已于 2026-09-22 执行完毕，全部通过**（清单保留供将来回归时对照；实测结果见下）。
>
> | 回报项 | 实测结果 |
> |---|---|
> | 4.1 `Tcs.Test.Slice.Run` 汇总行 | **通过 9 / 失败 0**（即时）+ **7b PASS**（延迟）＝ 10/10；本命令区间**零 Error、零 Warning** |
> | 4.2 `.Reject` 三条预期输出 | **3/3 命中**（双真相 1 条错误 / 空 ChainId 被拒 / 未登记链 Error + 无效句柄） |
> | 4.3 切关卡后检查 1b/2 | **通过**——`open L_TbnsShowcase` 后重跑 10/10 全绿；日志无"发现链资产"行，证定义库未重扫、直接装配缓存 |
> | 4.4 改数值前后对照 | **25.0 → 45.0**（`DamageBase` 30→45 且模板改指无公式的 `Default`）；带公式的 `Default_Slice` 下改 `DamageBase` 确实不影响扣血（§4.4 预告的坑实测复现） |
> | 非预期 FAIL / 日志 | **无** |
>
> **额外获得（本轮意外收益）**：地图里摆的 2 个单位实测走通**关卡加载期注册**路径（`StaticMeshActor_1/2` 句柄 1/2），这是装置"运行时 spawn"永远覆盖不到的一条路。
>
> **修复记录**：本轮照出一处装置缺陷——检查 7b 初稿读**目标**的 Health，但链上 `SelectTargets(Self)` 会把目标集替换为**施法者**（`TcsStepSelectTargets.cpp` 是"清空后写回"），故 7b 会恒 FAIL。已修（改读施法者）并实证。详见 plan2 Task 6 注记⑨-A。

<!-- 以下为原始回报清单，保留供回归对照

- 4.1 的完整屏显汇总行（通过/失败数）+ 是否有红字；
- 4.2 的三条预期输出是否命中；
- 4.3 切关卡后检查 1b/2 的结果；
- 4.4 改数值前后的扣血量对照；
- 任何 FAIL 或非预期日志的原文。

-->
