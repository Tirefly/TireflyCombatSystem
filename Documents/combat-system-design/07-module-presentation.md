# 07-module-presentation.md — M7 表现层设计

- 日期：2026-09-02
- 状态：设计 v1（**TcsCue** = 插件默认演绎者模块，R0 §9；依据 D4-1 Cues 引用制、D4-2 帧末通道、裁决 3；~~契约住 TcsEffect~~ 为 D4-14 翻转前残留——Cue 契约（FCueEvent/ICuePresenter）与 PlayCue 步骤+执行器均住 TcsCue）
- 职责一句话：**Cue 的契约与派发——核心只说"发生了什么"，表现层只答"我来演"。插件不含任何 VFX/音频/相机配置。**

## 1. 模块边界

- 消费者：M4（PlayCue 步骤/触发行 Cues）、宿主表现层（实现适配接口）。
- 依赖：TcsCore（总线/帧末队列/词表）、TcsEffect（FEffectStep 基类型 + 执行器自注册宏——PlayCue 步骤类型+执行器住本模块，D4-14）。**零 include**：Niagara/物理/音频/动画一概不碰。
- **TcsCue 模块**（R0 §9）：本契约的插件默认实现 + Cue 定义资产骨架 + 可复用 VFX/SFX **经验封装（=代码助手/默认参数映射模板，非内容资产——内容配置仍归宿主）**；宿主可整表替换。

## 2. 类型词汇（对外）

- `FCueEvent`：`CueId(FGameplayTag) + 上下文{Source/Target/位置/变量快照(FInstancedStruct)}`——**CueId 是引用不是配置**（D4-1 定案）：Niagara UserParam、Decal、MaterialParamCollection、程序化控制的参数映射全部住宿主适配层。**身份 2026-09-22 tag 化**：CueId 由 `FName` 改 `FGameplayTag`（与全插件标识体系统一；Cue 词汇归项目，走项目 tag 表）。
- `ICuePresenter`（UINTerface，宿主实现）：`OnCue(FCueEvent)`——宿主把 CueId 映射到具体表现资产与参数；**插件提供的是词汇，宿主提供的是演绎**。
- 可合并标记：CueId 声明 `bMergeable`——帧末派发时同帧同类合并（一帧 5 次掉血 = 一个汇总飘字）。

## 3. 关键机制

- **派发通道**：全部走帧末队列（D4-2 默认表）；派发时按 CueId 合并策略去重/汇总。
- **预览协同**：表现层预览读 `PeekPending`（D2-5）而非等待事件——"伤害数字预演"与正式事件不双发。
- **复制姿态**：CueId + 上下文快照随操作流/触发事件复制到客户端播放（接口位，本期不实现）。
- **死事件防线**（TCS 教训，ZZ:61 死 API）：每个 CueId 必须有注册的消费者——校验器（M8）扫描"发而无收"的 CueId。

## 4. 非目标

不做 VFX 配置系统；不做音频总线/混音；不做相机/震屏策略（宿主）；不做 UI 框架；不做 Timeline 演出（表现层职责，LevelSequence/Niagara）。

## 5. 依据

D4-1（Cues 引用制，用户反馈"战斗系统无法在插件层提供足够通用的表现配置"→ 引用而非配置）；D4-2（帧末）；裁决 3；TCS 死事件教训。

## 6. 验收钩子

（**R3 之后路径**——TcsCue 移出 R3、验收信号走屏显，M9 收尾轮拍板。）竖切剧本 Cue logger stub：`Cue_Burn_Tick`/`Cue_Whirlwind_Spin` 帧末到达、合并行为、宿主映射可替换。
