# TCS 示例文档：'英雄联盟' 场景（严格对齐数据结构）

说明：本示例用于演示如何仅用 TCS 现有/提案的数据结构与资产形式来表达规则与顶层状态机，不复刻原作数值，仅用于研究方法。中文名称均以引号包裹；标签与枚举使用英文。

## 1. 槽位定义（FTcsStateSlotDefinition）
- 槽位
  - SlotTag: StateSlot.Action.Cast
  - ActivationMode: PriorityOnly
  - StateTreeStateName: '动作_施法'
- 槽位
  - SlotTag: StateSlot.Action.Channel
  - ActivationMode: PriorityOnly
  - StateTreeStateName: '动作_引导'
- 槽位
  - SlotTag: StateSlot.Mobility.Move
  - ActivationMode: PriorityOnly
  - StateTreeStateName: '移动_行走'
- 槽位
  - SlotTag: StateSlot.Mobility.Dash
  - ActivationMode: PriorityOnly
  - StateTreeStateName: '移动_冲刺'
- 槽位
  - SlotTag: StateSlot.CC.Stun
  - ActivationMode: AllActive
  - StateTreeStateName: '控制_眩晕'
- 槽位
  - SlotTag: StateSlot.CC.Root
  - ActivationMode: AllActive
  - StateTreeStateName: '控制_定身'
- 槽位
  - SlotTag: StateSlot.CC.Silence
  - ActivationMode: AllActive
  - StateTreeStateName: '控制_沉默'
- 槽位
  - SlotTag: StateSlot.CC.KnockUp
  - ActivationMode: AllActive
  - StateTreeStateName: '控制_击飞'
- 槽位
  - SlotTag: StateSlot.CC.Suppress
  - ActivationMode: AllActive
  - StateTreeStateName: '控制_压制'
- 槽位
  - SlotTag: StateSlot.SpellShield
  - ActivationMode: AllActive
  - StateTreeStateName: '护盾_法术'
- 槽位
  - SlotTag: StateSlot.Buff.Tenacity
  - ActivationMode: AllActive
  - StateTreeStateName: '增益_韧性'
- 槽位
  - SlotTag: StateSlot.Buff.Unstoppable
  - ActivationMode: AllActive
  - StateTreeStateName: '增益_不可阻挡'
- 槽位
  - SlotTag: StateSlot.Summoner.Cleanse
  - ActivationMode: PriorityOnly
  - StateTreeStateName: '召唤师_净化'

## 2. 状态定义（FTcsStateDefinition）
- 状态
  - StateDefId: '火球术_施法'
  - StateSlotType: StateSlot.Action.Cast
  - Priority: 60
  - DurationType: SDT_Duration
  - Duration: 1.5
  - SameInstigatorMergerType: UTcsStateMerger_UseNewest
  - DiffInstigatorMergerType: UTcsStateMerger_UseNewest
  - StateTreeRef: /Game/TCS/StateTrees/Actions/Fireball
  - Tags: State.Action.Cast;Element.Fire
- 状态
  - StateDefId: '光束_引导'
  - StateSlotType: StateSlot.Action.Channel
  - Priority: 80
  - DurationType: SDT_Duration
  - Duration: 3.0
  - SameInstigatorMergerType: UTcsStateMerger_UseNewest
  - DiffInstigatorMergerType: UTcsStateMerger_UseNewest
  - StateTreeRef: /Game/TCS/StateTrees/Actions/Beam
  - Tags: State.Action.ChannelSpell
- 状态
  - StateDefId: '行走'
  - StateSlotType: StateSlot.Mobility.Move
  - Priority: 10
  - DurationType: SDT_None
  - Duration: 0.0
  - SameInstigatorMergerType: UTcsStateMerger_UseNewest
  - DiffInstigatorMergerType: UTcsStateMerger_UseNewest
  - StateTreeRef: /Game/TCS/StateTrees/Mobility/Walk
  - Tags: State.Mobility.Move
- 状态
  - StateDefId: '冲刺'
  - StateSlotType: StateSlot.Mobility.Dash
  - Priority: 70
  - DurationType: SDT_None
  - Duration: 0.0
  - SameInstigatorMergerType: UTcsStateMerger_UseNewest
  - DiffInstigatorMergerType: UTcsStateMerger_UseNewest
  - StateTreeRef: /Game/TCS/StateTrees/Mobility/Dash
  - Tags: State.Action.Dash
- 状态
  - StateDefId: '眩晕'
  - StateSlotType: StateSlot.CC.Stun
  - Priority: 100
  - DurationType: SDT_Duration
  - Duration: 1.75
  - SameInstigatorMergerType: UTcsStateMerger_UseNewest
  - DiffInstigatorMergerType: UTcsStateMerger_UseNewest
  - StateTreeRef: /Game/TCS/StateTrees/CC/Stun
  - Tags: State.CC.Stun
- 状态
  - StateDefId: '定身'
  - StateSlotType: StateSlot.CC.Root
  - Priority: 90
  - DurationType: SDT_Duration
  - Duration: 1.50
  - SameInstigatorMergerType: UTcsStateMerger_UseNewest
  - DiffInstigatorMergerType: UTcsStateMerger_UseNewest
  - StateTreeRef: /Game/TCS/StateTrees/CC/Root
  - Tags: State.CC.Root
- 状态
  - StateDefId: '沉默'
  - StateSlotType: StateSlot.CC.Silence
  - Priority: 85
  - DurationType: SDT_Duration
  - Duration: 1.50
  - SameInstigatorMergerType: UTcsStateMerger_UseNewest
  - DiffInstigatorMergerType: UTcsStateMerger_UseNewest
  - StateTreeRef: /Game/TCS/StateTrees/CC/Silence
  - Tags: State.CC.Silence
- 状态
  - StateDefId: '击飞'
  - StateSlotType: StateSlot.CC.KnockUp
  - Priority: 110
  - DurationType: SDT_Duration
  - Duration: 1.25
  - SameInstigatorMergerType: UTcsStateMerger_UseNewest
  - DiffInstigatorMergerType: UTcsStateMerger_UseNewest
  - StateTreeRef: /Game/TCS/StateTrees/CC/KnockUp
  - Tags: State.CC.KnockUp
- 状态
  - StateDefId: '压制'
  - StateSlotType: StateSlot.CC.Suppress
  - Priority: 120
  - DurationType: SDT_Duration
  - Duration: 1.50
  - SameInstigatorMergerType: UTcsStateMerger_UseNewest
  - DiffInstigatorMergerType: UTcsStateMerger_UseNewest
  - StateTreeRef: /Game/TCS/StateTrees/CC/Suppress
  - Tags: State.CC.Suppress
- 状态
  - StateDefId: '法术护盾'
  - StateSlotType: StateSlot.SpellShield
  - Priority: 50
  - DurationType: SDT_Duration
  - Duration: 3.0
  - SameInstigatorMergerType: UTcsStateMerger_NoMerge
  - DiffInstigatorMergerType: UTcsStateMerger_NoMerge
  - StateTreeRef: /Game/TCS/StateTrees/Shield/SpellShield
  - Tags: State.Shield.Spell
- 状态
  - StateDefId: '韧性'
  - StateSlotType: StateSlot.Buff.Tenacity
  - Priority: 40
  - DurationType: SDT_Duration
  - Duration: 10.0
  - SameInstigatorMergerType: UTcsStateMerger_UseNewest
  - DiffInstigatorMergerType: UTcsStateMerger_UseNewest
  - StateTreeRef: /Game/TCS/StateTrees/Buff/Tenacity
  - Tags: State.Buff.Tenacity
- 状态
  - StateDefId: '不可阻挡'
  - StateSlotType: StateSlot.Buff.Unstoppable
  - Priority: 95
  - DurationType: SDT_Duration
  - Duration: 1.0
  - SameInstigatorMergerType: UTcsStateMerger_UseNewest
  - DiffInstigatorMergerType: UTcsStateMerger_UseNewest
  - StateTreeRef: /Game/TCS/StateTrees/Buff/Unstoppable
  - Tags: State.Buff.Unstoppable
- 状态
  - StateDefId: '召唤师_净化'
  - StateSlotType: StateSlot.Summoner.Cleanse
  - Priority: 200
  - DurationType: SDT_None
  - Duration: 0.0
  - SameInstigatorMergerType: UTcsStateMerger_UseNewest
  - DiffInstigatorMergerType: UTcsStateMerger_UseNewest
  - StateTreeRef: /Game/TCS/StateTrees/Summoner/Cleanse
  - Tags: State.Summoner.Cleanse

## 3. 顶层 StateTree（树形结构与转换）
```yaml
StateTree:
  Name: '顶层_英雄联盟_示例'
  Root:
    Children:
      - Name: '战斗_主流程'
        Children:
          - Name: '移动_行走'
            OnEnter: ['打开Gate: StateSlot.Mobility.Move']
            OnExit:  ['关闭Gate: StateSlot.Mobility.Move']
            Transitions:
              - When: '收到输入: 冲刺'   To: '移动_冲刺'
          - Name: '移动_冲刺'
            OnEnter: ['打开Gate: StateSlot.Mobility.Dash']
            OnExit:  ['关闭Gate: StateSlot.Mobility.Dash']
            Transitions:
              - When: '冲刺结束'       To: '移动_行走'
          - Name: '动作_施法'
            OnEnter: ['打开Gate: StateSlot.Action.Cast']
            OnExit:  ['关闭Gate: StateSlot.Action.Cast']
          - Name: '动作_引导'
            OnEnter: ['打开Gate: StateSlot.Action.Channel']
            OnExit:  ['关闭Gate: StateSlot.Action.Channel']
            Transitions:
              - When: '引导被中断'     To: '移动_行走'

      - Name: '控制_域'
        Children:
          - Name: '控制_眩晕'
            OnEnter: ['关闭Gate: StateSlot.Action.*','关闭Gate: StateSlot.Mobility.*']
            OnExit:  ['打开Gate: StateSlot.Action.Cast','打开Gate: StateSlot.Mobility.Move']
          - Name: '控制_定身'
            OnEnter: ['关闭Gate: StateSlot.Mobility.Dash']
          - Name: '控制_沉默'
            OnEnter: ['关闭Gate: StateSlot.Action.Channel']
          - Name: '控制_击飞'
            OnEnter: ['关闭Gate: StateSlot.Action.*','关闭Gate: StateSlot.Mobility.*']
            OnExit:  ['打开Gate: StateSlot.Action.Cast','打开Gate: StateSlot.Mobility.Move']
          - Name: '控制_压制'
            OnEnter: ['关闭Gate: StateSlot.Action.*','关闭Gate: StateSlot.Mobility.*']
            OnExit:  ['打开Gate: StateSlot.Action.Cast','打开Gate: StateSlot.Mobility.Move']

      - Name: '护盾_与_增益'
        Children:
          - Name: '护盾_法术'
            OnEnter: ['注册一次性格挡: 命中[State.CC.*,State.Debuff.*]→阻断并清空自身']
          - Name: '增益_韧性'
            OnEnter: ['标记韧性: 由属性/数值系统处理“抗性/缩短时长”等效果（不在策略层返回缩放）']
          - Name: '增益_不可阻挡'
            OnEnter: ['规则层对State.CC.(Stun|Root|Silence)返回免疫（阻断）']
```

## 5. 策略资产（Policies）

```yaml
SlotRelations:
  - SlotTag: StateSlot.CC.*
    CloseGatesOnEnter:   [StateSlot.Action.Cast, StateSlot.Action.Channel, StateSlot.Mobility.Dash, StateSlot.Mobility.Move]
    OpenGatesOnExit:     [StateSlot.Action.Cast, StateSlot.Mobility.Move]
    Notes: '进入任意控制时关闭动作/移动；退出后恢复基础动作/移动'

  - SlotTag: StateSlot.SpellShield
    OneShotBlockSelectors: [State.CC.*, State.Debuff.*]
    Notes: '法术护盾命中一次后清空自身'

  - SlotTag: StateSlot.Action.Channel
    CloseGatesOnEnter:   [StateSlot.Mobility.Dash]
    Interruptors:        [StateSlot.CC.Stun, StateSlot.CC.KnockUp, StateSlot.CC.Suppress]
    ClearSlotsOnInterrupt: [StateSlot.Action.Channel]
    Notes: '引导被强控中断并清槽'
```

<!-- 旧版规则资产（RuleSet/Dependencies/Exclusions）已废弃，示例段落移除。规则请在 StateTree 编排，免疫/净化通过策略层实现。 -->


## 附：任务/条件调用清单
- 节点 '控制_眩晕'（OnEnter）
  - 关闭 Gate: , 
  - 备注: 退出节点时在 OnExit 打开 , 
- 节点 '控制_定身'（OnEnter）
  - 关闭 Gate: 
- 节点 '控制_沉默'（OnEnter）
  - 关闭 Gate: 
- 节点 '控制_击飞'（OnEnter/OnExit）
  - OnEnter 关闭 Gate: , 
  - OnExit 打开 Gate: , 
- 节点 '控制_压制'（OnEnter/OnExit）
  - 同 '控制_击飞'
- 节点 '护盾_法术'（OnEnter）
  - 注册一次性格挡: 命中选择器 [State.CC.*, State.Debuff.*] → 阻断并清空自身
- 节点 '动作_引导'（OnEnter/OnExit/中断）
  - OnEnter 关闭 Gate: 
  - 中断来源: , ,  命中时→清槽 
  - OnExit 打开 Gate: 
- 节点 '召唤师_净化'（OnEnter）
  - Cleanse 任务参数（YAML，仅示意）

```yaml
Task: Cleanse
  Params:
  Selector:
    Class: UTcsSelector_ByTags
    Params:
      IncludeTags: [State.Debuff.*, State.CC.Slow, State.CC.Root]
      MatchLogic: Any
      ExcludeSubtrees: []
      Exceptions: []
      MaxPriority: 120
  MaxRemoveCount: -1
```
- 轻量前置（可选，StateManager.CheckImmunity）
  - 策略仅做免疫判定（允许/阻断），不返回“缩放/抗性”。
