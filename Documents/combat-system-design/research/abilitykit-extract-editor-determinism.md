# AbilityKit 对照调研提取：编辑器工具与确定性/回放（C 路）

> 性质：对照调研提取报告——服务于我方战斗系统设计（编辑器工具 + 确定性/回放两条线）。
> 日期：2026-09-01。调研基线：GitHub 快照 `AbilityKit-master`（2026-09-01 静态取证），证据等级沿用 ability-kit-research 约定：【源码】= 报告已核源码；【本次核实】= 本报告在 vendored 源码（`references/AbilityKit-master/`）二次取证；【文档】= 仓库文档转引；【推断】/【未证实】= 同约定。报告编号 R19/R01/R11/R07 指 `~/.agents/skills/ability-kit-research/references/` 下对应调研报告。
> 关联我方文档：裁决 3《2026-08-31-visualization-editing-paradigm.md》、裁决 2 子文档《2026-08-31-tick-pump-and-component-execution.md》、总日志《combat-skill-system-carrier-discussion.md》§6。

## 0 TL;DR

① **actioneditor 不是节点图编辑器**：它是 vendored 第三方 NBC.ActionEditor 2.0.0 的 IMGUI 自绘**时间轴**（Track/Clip 轨道模型，无连线/无 GraphView/无 K2），actioneditor.impl 只提供 authoring 资产，DTO 契约独立成 actionschema 包——与我方"管线步骤图"是两种范式；它是我方 ACT 混合视图**时间轴半边**的先例，exec-pin 图仍走 UEdGraph/SGraphPanel。② **Pipeline Phase 图没有现成图编辑器**（pipeline 包 Editor 仅调试窗口），Phase 编排是代码构建；能对照的是"Phase 壳 + Timeline 内容"的桥接（MOBA demo 侧 AbilityTimelinePhase）。③ 确定性栈最小形态：FrameTime Q32.32 定点帧时钟（float 仅边界视图）+ xoroshiro128+ 可复现随机 + 对外 float/内部 Fixed64 策略；回放 = FrameRecordFile 录"输入帧 + 周期状态哈希 + 可选快照"三轨，FixedStepReplayClock 定步长积累器逐帧重演——正是我方待定"定步长积累器"的现成 9 行参照。④ HFSM 自研确定性内核完备但零生产消费者（迁移半途），一句教训。

## 1 我方关注点回顾

| # | 关注点（我方已定/待定） | 本报告对应章节 |
|---|---|---|
| ① | 技能步骤链可视化编辑：裁决 3 定 UEdGraph/UEdGraphNode/SGraphPanel 自研 exec-pin 图，K2 排除；待定项含"ability-kit-research Pipeline Phase 图对照调研" | §2、§3 |
| ② | tick pump：缩放 dt 单点（ScaledDt）+ 待定"定步长积累器"（自走棋确定性回放需要时再定） | §4 |
| ③ | 自走棋"战斗可回放"待定项的最小形态 | §5 |

**预期校准（重要）**：裁决 3 §7 预设"对照 Pipeline 技能 Phase 图的现成实现"——**该预期落空**：AbilityKit 的 pipeline 包没有任何图编辑器（`com.abilitykit.pipeline/Editor/` 仅 UnityTimeProvider + PipelineRuntimeDebuggerWindow 等调试件，无节点/画布代码【本次核实：目录清单】），Phase 编排靠代码构建（demo 侧 `SkillPipelineBuilder` 工厂模式【本次核实：SkillPipelineBuilder.cs L117-140】）。可对照物实为两件：NBC 时间轴编辑器（§2）与"Phase 壳+Timeline 内容"桥接运行时（§2.5）。

## 2 actioneditor 图编辑器实现

### 2.1 宿主框架：vendored 第三方，IMGUI 自绘时间轴

- 双包结构：`com.abilitykit.thirdparty.actioneditor`（NBC.ActionEditor 2.0.0 完整 vendored，82 cs 主体在 Editor）承载编辑器本体；`com.abilitykit.actioneditor.impl`（18 cs）仅提供 AbilityKit 侧 authoring 资产；DTO 契约在 `com.abilitykit.actionschema`【R19 §1/§2】。
- 宿主是 `ActionEditorWindow : EditorWindow`（菜单"Window/AbilityKit/时间轴编辑器"），IMGUI 自绘 View 体系：WelcomeView/TimelineView 双视图；Timeline 视图族分区 Header/Middle/Bottom/Pointer/TrackItem/Dragger/ClipDraw + Inspectors + Languages(CHS/EN) + Preview（AssetPlayer/PreviewBase/TimePointers）【R19 §3.3；【本次核实】ActionEditorWindow.cs L1-40】。
- **全包递归搜索 GraphView/NodeGraph/GraphEditorView 零命中**【本次核实】——既不用 Unity GraphView，更不涉 K2；菜单/窗口/轨道拖拽全部手写 IMGUI。

### 2.2 节点模型：时间轴四层，非"动作节点图"

任务书预设"节点=ActionSchema 动作的包装？"——**不成立**。真实模型是轨道时间轴四层【R19 §3.3】：

| 层 | 类型 | 说明 |
|---|---|---|
| 资产 | `SkillAsset : Asset(:IDirector)` | 10 行，仅名称/元数据，无执行逻辑 |
| 轨道 | `Track(:IDirectable)` ×5 | Action/Animation/Audio/Effect/Signal（impl 侧 Directables）；`ActionTrack` 仍残留 Test1/2/3 实验字段 |
| 片段 | `Clip(:IClip)` ×11 | PlayAnimation/PlayAudio/PlayParticle/MoveBy/MoveTo/RotateTo/ScaleTo/VisibleTo/TriggerEvent(:ClipSignal)/TriggerLog/TriggerShake |
| 组 | `Group` | 含 ActorId（按角色分组轨道） |

"动作"在这里是**带起止时间的轨道片段（Clip）**，不是节点；ActionSchema/SkillAssetDto 是导出侧 DTO 契约，不是编辑器节点。

### 2.3 连线/端口语义：不存在

无 exec-pin、无节点连线。语义全部由"时间排布 + 事件点"承担：Clip 按 start/length 排在 Track 上；`TriggerEvent : ClipSignal` 在时间点上声明事件（`IsValid => eventName > 0`，不自行发布事件，运行时消费见 §2.5）【R19 §3.3】。

### 2.4 序列化格式：FullSerializer 资产 + logic JSON 导出（CLR FullName 协议）

- 编辑资产落盘：上游内嵌 FullSerializer（`ActionEditor\Runtime\ThirdParty\FullSerializer.dll`）；design 08 文档称"Newtonsoft.Json"与实测不符【R19 §8.1-1，文档滞后】。
- 显式导出：`LogicJsonExporter.ExportLogicJson` → `<name>.logic.json`；`type` 字段写 **CLR FullName**（重命名=破坏性数据迁移）；`FillClipArgs` 仅对 `ILogicJsonExportable` 开放——**11 个 Clip 仅 2 个（PlayAnimation/TriggerLog）可导出参数**；fired key = `group.name|track.name|clip.type|start|length` 字符串拼接，**无稳定 GUID，可碰撞且未用 InvariantCulture**【R19 §3.3/§4.2/§8】。
- 分层契约：编辑资产（impl）→ 导出 JSON → `SkillAssetDto`（actionschema 包 `ActionTimelineDtos.cs L7`，经 `ActionTimelineJson` 加载）【R19 §4.2；【本次核实】类型位置】。

### 2.5 与 Pipeline/Trigger 数据的往返关系（跨包链）

【R19 §4.2；【本次核实】AbilityTimelinePhase.cs 全文 + SkillPipelineBuilder.cs L115-140】

```
SkillAsset (impl, NBC 编辑器资产)
  → LogicJsonExporter → <name>.logic.json (thirdparty)
  → SkillAssetDto (actionschema, ActionTimelineJson)
  → Pipeline: TimelinePhaseConfig → AbilityTimelinePhase (demo.moba.runtime 自建)
  → MobaTimelinePlayer.Update(dt) 逐帧播轨道
  → MobaClipHandlerRegistry (副作用映射) + TimelineEventSink/EventBuffer (事件回 Pipeline 上下文)
```

- `AbilityTimelinePhase : AbilityInterruptiblePhaseBase<TCtx>` 是 Pipeline 的一个 Phase：OnExecute 从 Pipeline 上下文取 `SkillAssetDto`（无则用配置注入），OnTick 里 `MobaTimelinePlayer.Update(deltaTime)`，`player.Time >= asset.length` 即 `Complete`——**Phase 是壳（阶段编排），Timeline 是内容（轨道编排）**，两层解耦。
- TriggerEvent Clip 本身不执行：事件经 Sink/Buffer 进 Pipeline 上下文由下游消费；基础 TimelinePlayer 只识别 TriggerLog【R19 §4.2】。
- 框架 `pipeline` 包内建的 TimelinePhase 已停用（R03↔R05 交叉校验，转引 SKILL.md §0.9），现役桥接全在 demo.moba.runtime——demo 侧同名类在用【本次核实：SkillPipelineBuilder.cs L128 调用中】。

### 2.6 编辑期能力（校验/预览/调试）

| 能力 | 现状 | 证据 |
|---|---|---|
| 校验 | 仅 `Clip.IsValid` authoring 期口径（如 eventName>0）；无结构校验器 | 【R19 §3.3】 |
| 预览 | `[CustomPreview(typeof(XxxClip))] : PreviewBase<T>` 按类型注册，7 个 Clip 预览；Sampler 中 Audio 120 行最完整，Animation/Spine 是空类 | 【R19 §3.2/§3.3】 |
| 元数据联动 | `[Name]/[Color]/[Attachable]/[MenuName]` + `[OptionParam(typeof(枚举))]`（int 字段渲染为枚举下拉）+ `[OptionRelateParam]`（字段级联显隐）——特性驱动 Inspector，免手写面板 | 【R19 §3.3】 |
| 调试 | 编辑器无运行时调试联动；Pipeline 侧有独立 PipelineRuntimeDebuggerWindow（非图） | 【R19 §3.2；【本次核实】】 |
| 测试 | impl/thirdparty 双包 0 自动测试，E3-E5 未闭合 | 【R19 §7】 |
| 坑 | Initializer 打开即强切 SampleScene；NBC.Editor asmdef 以 GUID+名称双形式硬编码引用 ActionSchema；BasePlayer 依赖 `AbilityKit.ActionEditorImpl.TriggerLog` 全名匹配（namespace 调整=协议破坏） | 【R19 §8】 |

## 3 与 UEdGraph 路线逐点对照表（裁决 3）

我方路线：验收期自研管线步骤图 = `UEdGraph/UEdGraphNode/SGraphPanel` 自绘 exec-pin 节点，K2 排除；数据即 `FEffectStep` 引用。

| 对照点 | AbilityKit actioneditor 做法 | 结论（可借鉴/不适用） |
|---|---|---|
| 编辑范式 | 轨道时间轴（线性时间动作），非节点图 | **不适用**作 exec-pin 图先例——但印证"编辑范式跟随数据形状"：线性异步命令序列配图（我方技能链）、线性时间动作配时间轴（双方一致） |
| 宿主技术 | 不用 GraphView 不用 K2，纯 IMGUI 自绘（82 cs 编辑器主体） | **参照系**：印证 K2 排除正确（不碰蓝图 VM 也能做编辑器）；也印证全自绘成本高——我方选 UEdGraph/SGraphPanel = 复用引擎布局/拖拽/连线，比 NBC 省一个编辑器团队的工作量 |
| 数据模型 | Track/Clip 四层时间轴 | 不适用（形状不同）；但"authoring 资产包 / 编辑器包 / DTO 契约包"三层分离可借鉴——编辑器可整体替换而数据契约稳定，正好支撑我方"DefAsset 列表编辑器先行、图编辑器后置"的节奏 |
| 连线语义 | 无；时间排布 + ClipSignal 事件点 | 不适用——exec 线/继续点/等待挂起语义无现成先例，我方需自定义（参照 GAS exec-pin 概念而非本仓库） |
| 序列化 | FullSerializer 资产 + 显式导出 logic.json；type=CLR FullName；fired key 无 GUID | **部分否决**：CLR FullName 做持久化协议、字符串拼接 key 可碰撞均不可学；"编辑资产→显式导出可执行 DTO"单向门思想可借鉴（我方 DefAsset→FEffectStep 序列化同构） |
| 运行时往返 | DTO → Handler 注册表（MobaClipHandlerRegistry）白名单副作用映射；基础 Player 只认 TriggerLog | **可借鉴**：图/轨道数据与运行时 Handler 的注册表映射模式；但"11 Clip 仅 2 个可执行"的半成品状态不可学——我方步骤原语应一次定全导出契约 |
| 校验 | Clip.IsValid 弱校验 | 不借鉴——我方图上应做结构校验（悬空分支、等待无出口、步进预算） |
| 预览 | [CustomPreview] 按类型注册 Clip 预览 | 可借鉴（UE 对应 DetailCustomization/自定义预览；预览器按步骤类型注册） |
| 时间轴 lane | Header/Middle/Bottom/Pointer/TrackItem/Dragger/ClipDraw 分区 + Clip 拖拽 | **可借鉴**——我方 §5 ACT 混合视图"管线图+时间轴"的时间轴半边现成先例（分区结构、轨道条拖拽、播放指针） |
| 测试纪律 | 0 测试 + GUID 硬编码 + 实验字段残留 | 反例清单，不借鉴 |

## 4 确定性设计

### 4.1 帧时间如何定义（FrameTime / Q32.32）

- `FrameIndex`：裸 `readonly struct { int Value; }`（包设计文档的富 API 是未实现的目标设计【R11 §8.1-a】）。
- `FrameTime`：累计时刻 `_time` 与步长 `_fixedDelta` 均为 **Fixed64（Q32.32 raw long 整数累加，无精度漂移）**；`Time/DeltaTime` float 只是接口边界单次换算视图；`AlignTo(targetFrame)` 用"帧号×固定步长"整数乘重建时间，注释明确"与逐帧 StepTo 累加位一致——客户端预测对齐必须用它，不要经 float 中转"；`TimeMilliseconds => (raw*1000) >> 32` 纯整数域【R11 §3.2；【本次核实】FrameTime.cs L11-24、L52-66】。
- 回滚侧 `FrameTimeRollbackStateProvider` payload **v2** 存 `TimeRaw/FixedDeltaRaw`，版本不匹配严格拒绝【R11 §3.2】。
- 帧驱动：`WorldManagerFrameDriver.Step(dt)` → `worlds.Tick(dt)` → `frame++`；host 侧 `FrameSyncDriverModule` PreTick **先 flush 本帧输入（无输入帧也提交空数组，"缺帧≠跳帧"）** 再 Tick——帧号不因无输入分叉【R11 §3.1/§4.1】。

### 4.2 定时器如何确定性化

- `PeriodicTask`：对外 float 无感、**内部 `_elapsedRaw/_nextFireRaw` 均 Fixed64**，`Update` 用 while 补执行多周期，触发时刻恒为 period 整数倍【R01 §4.4】。
- 但接入层诚实标注：`TimerSchedulerModule` 当前由宿主**墙钟 dt**驱动，源码注释自认"确定性世界接入时应改用逻辑时间"【R01 §4.4/§8.2】——即框架的定时器链路本身尚未接确定性时钟，确定性靠调用方换时间源。

### 4.3 随机数如何确定性化

`DeterministicRandom`：**xoroshiro128+ 可复现随机流**（SplitMix64 做种子扩展，`SetSeed` 重置 + `Sequence` 计数，可捕获完整生成器状态供回滚/快照恢复）【R01 §3.5；【本次核实】DeterministicRandom.cs L6-30】。

### 4.4 数值边界与逻辑/表现分离

- 策略："**对外 float 无感、内部 Fixed64 定点**"——普通 float 四则/比较本身 IEEE 位一致，仅漂移敏感运算（开方/归一化/三角）经 `DeterministicMathBridge` 切到 `DeterministicMath` 整数实现（CORDIC 三角、逐位开方，跨 .NET/Mono/IL2CPP 位一致）；float 边界收敛为三条：**配置入口 / 表现出口 / 公开 API 兼容层**；NaN/Inf 在桥接层归零守卫；Fixed64 表示范围约 ±2.1e9【R01 §4.5；R11 §4.5】。
- 逻辑/表现分离的落点：float 换算只发生在"表现/接口边界"（FrameTime 的 float 视图、MobaTimelinePlayer.Update(float) 是表现侧播放器）；逻辑侧消费 raw/定点。**诚实边界**：属性系统仍 float 存储（未排期迁定点）、跨运行时 JSON float 解析有理论末位差【R11 §4.5】。
- 事件/遍历确定性配套：EventDispatcher 订阅时二分插入（priority 降序、同优先级按订阅序），发布路径零排序；字符串事件名经 FNV-1a 稳定压缩，冲突即抛【R01 §3.1/§4.10】。
- 总边界声明："**帧时钟确定 ≠ 全战斗确定**"——玩法状态、物理、随机源、容器遍历和业务 codec 是否确定需分别审计【R11 §4.5】。

### 4.5 对照我方 tick pump（缩放 dt 单点 + 待定定步长积累器）

- 我方"缩放 dt 单点" ≈ AbilityKit 的"帧时间单点注入"（ServerFrameTimeModule 注册 IFrameTime / 帧驱动 Step）：两家共识是**时间只允许从一个口进**；差异在 AbilityKit 进一步把逻辑时刻做成 Q32.32 定点，我方当前是 float ScaledDt。
- 我方待定"定步长积累器"的现成最小参照 = `FixedStepReplayClock`：`_acc += deltaTime * Speed; if (_acc < _fixedDelta) return false; _acc -= _fixedDelta; frame++`（约 9 行）【R11 §3.7；【本次核实】FixedStepReplayClock.cs L29-42】。注意定位：它只是**回放分发节奏器**（float 累加，本身非逐位确定），逐位确定性由逻辑侧定点 FrameTime + 输入重演承担；变速=改 Speed，暂停=不喂 dt。对我方的启示：积累器可以很薄，**先回答"逻辑帧与渲染帧是否解耦"再决定引入**——200 单位同帧顺序 tick 时无需它；需要慢机追赶/加速观看/逐位回放时，在 pump 外包一层积累器即可，组件仍收逻辑 dt。
- 我方四条护栏与之对应的增量项：若引入定步长，护栏 1（dt 单点）应升级为"逻辑 dt=定点 raw，ScaledDt 仅表现"。

## 5 Record 回放最小形态

### 5.1 录制什么

按帧 `FrameRecordFile { Meta; Inputs; StateHashes; Snapshots; Index }`【R11 §3.7】：

- **Meta**：WorldId/WorldType/**TickRate**/**RandomSeed**/PlayerId/StartedAtUnixMs——随机种子是录制头一等公民。
- **Inputs**（必录）：`PlayerInputCommand{Frame, Player, OpCode, byte[] Payload}`，输入语义全交协议层；等价比较只看 OpCode+Player+Payload，**payload 必须含决定模拟结果的全部参数**【R11 §4.2】。
- **StateHashes**（周期）：默认 `RecordProfile` StateHashIntervalFrames=10——用于对账与分歧定位，不是状态本体。
- **Snapshots**（可选）：同帧可多条；seek/校正用，体积大头。
- 另有通用轨 `RecordContainer/Track/Event`（多轨事件+JSON，RecordEventType 经 FNV-1a 稳定派生），Seek 接口 E0 占位无实现【R11 §3.7】。

### 5.2 codec v1-v4 差异

| Codec | 识别 | 版本 | 特点 | 默认 |
|---|---|---|---|---|
| JSON | 扩展名非 .bin | 无独立根版本 | 可读、payload Base64 | 是 |
| 基础 Binary | AKFR | 1 | 逐字段、保留 StateHash Version | 否 |
| Optimized Binary | AKFR | **writer 4 / reader 1-4** | Deflate 包三轨 + ZigZag signed VarInt 帧差 + PlayerId 字符串表 + hash delta（v3 引入 delta/字符串表；v4 起保留真实 StateHash schema version） | 是（.bin） |
| MemoryPack | PMLR | 1 | 整文件 MemoryPack；可选包 Installer 全局接管 | 否 |

【R11 §3.7；【本次核实】FrameRecordOptimizedBinaryDataCodec.cs L452-453 常量】坑：codec 按**扩展名**分派不探测 magic（AKFR 双形态共用）、writer 内存累积 Dispose 才落盘（非原子）、JSON DTO 无 SchemaVersion【R11 §8.2】。

### 5.3 回放如何驱动

`FrameRecordReplaySource` 按帧查询（TryGetInputs/Snapshots/StateHash）→ `BasicReplayController.Tick`：`while (clock.TryConsume(dt, out frame))` 逐帧分发给 Handler，首帧消费后 dt=0 防同帧重复累计 → `FixedStepReplayClock` 定步长积累（§4.5）；`TypedReplayEventHandler` 四类强类型回调【R11 §3.7/§4.4】。配套 `FrameRecordDiffAnalyzer`：比较两份记录 `(Frame, Ordinal)` 哈希（SchemaVersion=2，分歧输出 SHA-256 上下文），Record.Tools CLI 退出码 0/1/2 进门禁【R11 §4.4/§7.1】。

### 5.4 对自走棋"战斗可回放"的最小可行建议

1. **最小录制集 = 战斗头（TickRate + RandomSeed + 内容/代码版本 hash）+ 每帧指令轨 + 周期状态哈希轨**。自走棋战斗输入极稀疏（开局布阵+少量技能指令），指令轨近零成本；快照轨留作调试 seek，不进最小集。
2. **回放 = 换输入源的重演**：录制端不新增执行器；回放端把"玩家/决策输入源"换成文件读取 + 定步长积累器逐帧喂 pump。前提是 §4 逻辑侧确定性清单先闭合（定点时间、种子化随机、稳定遍历序、dt 单点），否则回放只能"表演"不能"对账"。
3. codec 起步用 JSON（可读可 diff），量大后学 Optimized 的 delta+VarInt；**别学扩展名选 codec**（存 magic）。
4. 对账工具学 `FrameRecordDiffAnalyzer` 的保守构造（同帧+非零哈希才可比，避免误判分歧）+ CLI 退出码进门禁。
5. 哈希对账必须用"含业务数据"的稳定哈希：通用 `StateHashComputer` 含 Timestamp 不含业务数据，跨端对账不可直接用【R11 §8.2】。

## 6 HFSM 确定性内核一句话对照

AbilityKit HFSM 处于 Legacy（UnityHFSM v2.2.0 vendored）→ 自研确定性内核（Next：`HfsmDefinition` 编译期 transition 按 priority 降序+id ordinal 排序、`HfsmRuntime.Tick(frame, Fixed64)` 拒倒退时钟、快照校验含 definition hash、双运行时差分测试钉语义）迁移半途——内核完备有测试但**零生产消费者**（MOBA/Shooter/Flow 全在 Legacy）【R07 §3.5/§4.7/§6.2/§7.2】。对照我方：其内核管的是"**状态机流转自身可重演**"，与我方"约束进 Tag 关系表（数据查询）、流转进 StateTree 状态槽"分工不同岗，无直接借鉴；唯一教训——确定性内核应作为既有宿主的替换件按需立项，平行新建完整内核而不迁移 = 白做。

## 7 可借鉴清单与否决清单

**可借鉴（按优先级）**
1. 三层工具链分离：authoring 资产包（impl）/ 编辑器包（thirdparty）/ DTO 契约包（actionschema）——编辑器可替换、数据契约稳定，支撑我方"列表编辑器→overlay→图编辑器"三步节奏（§2.1）。
2. `FixedStepReplayClock` 形态的定步长积累器（≈9 行）作为我方待定项的最小实现参照，含变速/暂停语义（§4.5）。
3. 回放最小录制集：Meta(TickRate+RandomSeed) + 指令轨 + 周期哈希轨 + 保守 diff 工具 + CLI 退出码门禁（§5.4）。
4. FrameTime 的"逻辑定点 raw / 表现 float 视图"分离与 AlignTo 整数重建——我方定步长落地时"逻辑 dt 与表现 dt 分离"的样板（§4.1）。
5. "对外 float 无感、内部 Fixed64"漂移敏感运算白名单策略（只切 sqrt/normalize/trig，其余留 IEEE）（§4.4）。
6. 特性驱动 Inspector 联动（[OptionParam]/[OptionRelateParam] 式声明，UE 侧对应 DetailCustomization 的声明化前置）（§2.6）。
7. [CustomPreview] 按类型注册预览器；Timeline 视图分区结构（我方 ACT 时间轴 lane 先例）（§2.6/§3）。
8. "Phase 壳 + Timeline 内容"桥接：阶段编排与轨道编排分层，Phase 以时长为完成条件（§2.5）。

**否决（不学）**
1. CLR FullName 做持久化 type 协议（重命名=破坏性迁移）；fired key 字符串拼接无稳定 GUID（§2.4）。
2. 11 个 Clip 仅 2 个可导出可执行——"编辑器能画 ≠ 运行时能跑"的半成品态；我方步骤原语一次定全导出契约（§2.4）。
3. codec 按扩展名分派不探测 magic；writer 内存累积一次性落盘非原子（§5.2）。
4. 编辑器强切场景（Initializer）、asmdef GUID 硬编码、实验字段残留、0 测试（§2.6）。
5. 全自绘 IMGUI 编辑器路线对我方 exec-pin 图不适用（成本高、无连线语义先例）——保留 UEdGraph/SGraphPanel 裁决（§3）。
6. HFSM 平行新建确定性内核而不迁移存量消费者的做法（§6）。

## 8 未证实与假设清单

1. 本报告全部结论为静态取证转引+二次源码核对，**未运行**任何构建/测试；各报告的 E3/E4/E5 等级（如 Record.Tests 18 用例、HFSM 差分测试 26 Fact）均为转引【R07/R11 §7】。
2. 任务书预设"actioneditor 节点=ActionSchema 动作包装"不成立（轨道时间轴模型）——已按源码实况修正（§2.2）。
3. "框架 pipeline 包内建 AbilityTimelinePhase 已停用"转引 SKILL.md §0.9（R03↔R05 交叉校验），本报告未读 R03/R05 全文；demo 侧同名类在用已【本次核实】（§2.5）。
4. NBC.ActionEditor 上游版本号 2.0.0 取自 vendored package.json，上游仓库演进未核对【R19 §1】。
5. codec v3/v4 布局细节、DeterministicRandom 算法名以报告【源码】行号为准，本报告仅复核版本常量与类头部（§4.3/§5.2）。
6. 假设：我方 FEffectStep 协程泵与"Phase 内 Update 轨道播放器"可在同一 pump 下并存（AbilityTimelinePhase.OnTick 调 MobaTimelinePlayer.Update(dt) 佐证该形态存在，但 AbilityKit 未与协程泵混用过）【推断】。
7. 我方文档与 AbilityKit 事实无直接冲突；唯一概念校准：我方"等待语义"（步骤游标+等待句柄）对应的是 AbilityKit **Flow**（WAKE/PUMP 事件驱动）而非 Timeline（时间驱动），本报告未展开 Flow 细节（见 R07 §4.1/§4.2）。
8. ActionEditorWindow 中 `App.OnInitialize`/`OnOpenAsset` 的注册（base.editor Initializer）行为链未逐行核对，坑项转引 R19 §8.1-7【本次核实仅窗口头部】。
