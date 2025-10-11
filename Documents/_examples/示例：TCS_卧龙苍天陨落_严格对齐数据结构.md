# TCS 示例文档：'卧龙：苍天陨落' 场景（严格对齐数据结构）

说明：本示例用于演示复杂动作/化劲/受击与五行的表达方式，严格使用 TCS 的数据结构。中文名称用引号包裹；标签与枚举使用英文。

## 1. 槽位定义（FTcsStateSlotDefinition）
- 槽位
  - SlotTag: StateSlot.Mobility.Walk
  - ActivationMode: PriorityOnly
  - StateTreeStateName: '移动_行走'
- 槽位
  - SlotTag: StateSlot.Mobility.Sprint
  - ActivationMode: PriorityOnly
  - StateTreeStateName: '移动_疾跑'
- 槽位
  - SlotTag: StateSlot.Mobility.Jump
  - ActivationMode: PriorityOnly
  - StateTreeStateName: '移动_跳跃'
- 槽位
  - SlotTag: StateSlot.Mobility.Dodge
  - ActivationMode: PriorityOnly
  - StateTreeStateName: '移动_闪避'
- 槽位
  - SlotTag: StateSlot.Action.Attack.Light
  - ActivationMode: PriorityOnly
  - StateTreeStateName: '动作_轻攻击'
- 槽位
  - SlotTag: StateSlot.Action.Attack.Heavy
  - ActivationMode: PriorityOnly
  - StateTreeStateName: '动作_重攻击'
- 槽位
  - SlotTag: StateSlot.Action.Combo
  - ActivationMode: PriorityOnly
  - StateTreeStateName: '动作_连击'
- 槽位
  - SlotTag: StateSlot.Action.Spell
  - ActivationMode: PriorityOnly
  - StateTreeStateName: '动作_施法'
- 槽位
  - SlotTag: StateSlot.Action.Deflect
  - ActivationMode: PriorityOnly
  - StateTreeStateName: '动作_化劲'
- 槽位
  - SlotTag: StateSlot.Action.Counter
  - ActivationMode: PriorityOnly
  - StateTreeStateName: '动作_反击'
- 槽位
  - SlotTag: StateSlot.Action.Guard
  - ActivationMode: PriorityOnly
  - StateTreeStateName: '动作_防御'
- 槽位
  - SlotTag: StateSlot.Channel.SpellCast
  - ActivationMode: PriorityOnly
  - StateTreeStateName: '引导_施法'
- 槽位
  - SlotTag: StateSlot.CC.Stagger
  - ActivationMode: AllActive
  - StateTreeStateName: '受击_硬直'
- 槽位
  - SlotTag: StateSlot.CC.Break
  - ActivationMode: AllActive
  - StateTreeStateName: '受击_破防'
- 槽位
  - SlotTag: StateSlot.CC.KnockDown
  - ActivationMode: AllActive
  - StateTreeStateName: '受击_击倒'
- 槽位
  - SlotTag: StateSlot.Buff.HyperArmor
  - ActivationMode: AllActive
  - StateTreeStateName: '增益_霸体'
- 槽位
  - SlotTag: StateSlot.Buff.Element.Wood
  - ActivationMode: AllActive
  - StateTreeStateName: '元素_木_增益'
- 槽位
  - SlotTag: StateSlot.Buff.Element.Fire
  - ActivationMode: AllActive
  - StateTreeStateName: '元素_火_增益'
- 槽位
  - SlotTag: StateSlot.Buff.Element.Earth
  - ActivationMode: AllActive
  - StateTreeStateName: '元素_土_增益'
- 槽位
  - SlotTag: StateSlot.Buff.Element.Metal
  - ActivationMode: AllActive
  - StateTreeStateName: '元素_金_增益'
- 槽位
  - SlotTag: StateSlot.Buff.Element.Water
  - ActivationMode: AllActive
  - StateTreeStateName: '元素_水_增益'
- 槽位
  - SlotTag: StateSlot.Buff.Element.Wood.Resonance
  - ActivationMode: PriorityOnly
  - StateTreeStateName: '元素_木_共鸣'
- 槽位
  - SlotTag: StateSlot.Buff.Element.Fire.Resonance
  - ActivationMode: PriorityOnly
  - StateTreeStateName: '元素_火_共鸣'
- 槽位
  - SlotTag: StateSlot.Buff.Element.Earth.Resonance
  - ActivationMode: PriorityOnly
  - StateTreeStateName: '元素_土_共鸣'
- 槽位
  - SlotTag: StateSlot.Buff.Element.Metal.Resonance
  - ActivationMode: PriorityOnly
  - StateTreeStateName: '元素_金_共鸣'
- 槽位
  - SlotTag: StateSlot.Buff.Element.Water.Resonance
  - ActivationMode: PriorityOnly
  - StateTreeStateName: '元素_水_共鸣'
- 槽位
  - SlotTag: StateSlot.Debuff.Element.Wood
  - ActivationMode: AllActive
  - StateTreeStateName: '元素_木_减益'
- 槽位
  - SlotTag: StateSlot.Debuff.Element.Fire
  - ActivationMode: AllActive
  - StateTreeStateName: '元素_火_减益'
- 槽位
  - SlotTag: StateSlot.Debuff.Element.Earth
  - ActivationMode: AllActive
  - StateTreeStateName: '元素_土_减益'
- 槽位
  - SlotTag: StateSlot.Debuff.Element.Metal
  - ActivationMode: AllActive
  - StateTreeStateName: '元素_金_减益'
- 槽位
  - SlotTag: StateSlot.Debuff.Element.Water
  - ActivationMode: AllActive
  - StateTreeStateName: '元素_水_减益'

## 2. 状态定义（FTcsStateDefinition）
- 状态
  - StateDefId: '轻攻击'
  - StateSlotType: StateSlot.Action.Attack.Light
  - Priority: 50
  - DurationType: SDT_None
  - Duration: 0.0
  - SameInstigatorMergerType: UTcsStateMerger_UseNewest
  - DiffInstigatorMergerType: UTcsStateMerger_UseNewest
  - StateTreeRef: /Game/TCS/StateTrees/Action/LightAttack
  - Tags: State.Action.Attack.Light
- 状态
  - StateDefId: '重攻击'
  - StateSlotType: StateSlot.Action.Attack.Heavy
  - Priority: 80
  - DurationType: SDT_None
  - Duration: 0.0
  - SameInstigatorMergerType: UTcsStateMerger_UseNewest
  - DiffInstigatorMergerType: UTcsStateMerger_UseNewest
  - StateTreeRef: /Game/TCS/StateTrees/Action/HeavyAttack
  - Tags: State.Action.Attack.Heavy
- 状态
  - StateDefId: '施法_瞬发'
  - StateSlotType: StateSlot.Action.Spell
  - Priority: 70
  - DurationType: SDT_None
  - Duration: 0.0
  - SameInstigatorMergerType: UTcsStateMerger_UseNewest
  - DiffInstigatorMergerType: UTcsStateMerger_UseNewest
  - StateTreeRef: /Game/TCS/StateTrees/Action/SpellInstant
  - Tags: State.Action.Spell
- 状态
  - StateDefId: '施法_引导'
  - StateSlotType: StateSlot.Channel.SpellCast
  - Priority: 85
  - DurationType: SDT_Duration
  - Duration: 2.5
  - SameInstigatorMergerType: UTcsStateMerger_UseNewest
  - DiffInstigatorMergerType: UTcsStateMerger_UseNewest
  - StateTreeRef: /Game/TCS/StateTrees/Channel/SpellCast
  - Tags: State.Channel.SpellCast
- 状态
  - StateDefId: '化劲_窗口'
  - StateSlotType: StateSlot.Action.Deflect
  - Priority: 90
  - DurationType: SDT_Duration
  - Duration: 0.25
  - SameInstigatorMergerType: UTcsStateMerger_UseNewest
  - DiffInstigatorMergerType: UTcsStateMerger_UseNewest
  - StateTreeRef: /Game/TCS/StateTrees/Action/Deflect
  - Tags: State.Action.Deflect
- 状态
  - StateDefId: '反击_窗口'
  - StateSlotType: StateSlot.Action.Counter
  - Priority: 95
  - DurationType: SDT_Duration
  - Duration: 0.8
  - SameInstigatorMergerType: UTcsStateMerger_UseNewest
  - DiffInstigatorMergerType: UTcsStateMerger_UseNewest
  - StateTreeRef: /Game/TCS/StateTrees/Action/Counter
  - Tags: State.Action.Counter
- 状态
  - StateDefId: '防御_举盾'
  - StateSlotType: StateSlot.Action.Guard
  - Priority: 60
  - DurationType: SDT_None
  - Duration: 0.0
  - SameInstigatorMergerType: UTcsStateMerger_UseNewest
  - DiffInstigatorMergerType: UTcsStateMerger_UseNewest
  - StateTreeRef: /Game/TCS/StateTrees/Action/Guard
  - Tags: State.Action.Guard
- 状态
  - StateDefId: '受击_硬直'
  - StateSlotType: StateSlot.CC.Stagger
  - Priority: 100
  - DurationType: SDT_Duration
  - Duration: 0.6
  - SameInstigatorMergerType: UTcsStateMerger_UseNewest
  - DiffInstigatorMergerType: UTcsStateMerger_UseNewest
  - StateTreeRef: /Game/TCS/StateTrees/CC/Stagger
  - Tags: State.CC.Stagger
- 状态
  - StateDefId: '受击_破防'
  - StateSlotType: StateSlot.CC.Break
  - Priority: 120
  - DurationType: SDT_Duration
  - Duration: 2.0
  - SameInstigatorMergerType: UTcsStateMerger_UseNewest
  - DiffInstigatorMergerType: UTcsStateMerger_UseNewest
  - StateTreeRef: /Game/TCS/StateTrees/CC/Break
  - Tags: State.CC.Break
- 状态
  - StateDefId: '增益_霸体'
  - StateSlotType: StateSlot.Buff.HyperArmor
  - Priority: 110
  - DurationType: SDT_Duration
  - Duration: 1.2
  - SameInstigatorMergerType: UTcsStateMerger_UseNewest
  - DiffInstigatorMergerType: UTcsStateMerger_UseNewest
  - StateTreeRef: /Game/TCS/StateTrees/Buff/HyperArmor
  - Tags: State.Buff.HyperArmor
- 状态
  - StateDefId: '元素_木_增益'
  - StateSlotType: StateSlot.Buff.Element.Wood
  - Priority: 30
  - DurationType: SDT_Duration
  - Duration: 10.0
  - SameInstigatorMergerType: UTcsStateMerger_UseNewest
  - DiffInstigatorMergerType: UTcsStateMerger_UseNewest
  - StateTreeRef: /Game/TCS/StateTrees/Element/Buff_Wood
  - Tags: Element.Buff.Wood
- 状态
  - StateDefId: '元素_火_增益'
  - StateSlotType: StateSlot.Buff.Element.Fire
  - Priority: 30
  - DurationType: SDT_Duration
  - Duration: 10.0
  - SameInstigatorMergerType: UTcsStateMerger_UseNewest
  - DiffInstigatorMergerType: UTcsStateMerger_UseNewest
  - StateTreeRef: /Game/TCS/StateTrees/Element/Buff_Fire
  - Tags: Element.Buff.Fire
- 状态
  - StateDefId: '元素_土_增益'
  - StateSlotType: StateSlot.Buff.Element.Earth
  - Priority: 30
  - DurationType: SDT_Duration
  - Duration: 10.0
  - SameInstigatorMergerType: UTcsStateMerger_UseNewest
  - DiffInstigatorMergerType: UTcsStateMerger_UseNewest
  - StateTreeRef: /Game/TCS/StateTrees/Element/Buff_Earth
  - Tags: Element.Buff.Earth
- 状态
  - StateDefId: '元素_金_增益'
  - StateSlotType: StateSlot.Buff.Element.Metal
  - Priority: 30
  - DurationType: SDT_Duration
  - Duration: 10.0
  - SameInstigatorMergerType: UTcsStateMerger_UseNewest
  - DiffInstigatorMergerType: UTcsStateMerger_UseNewest
  - StateTreeRef: /Game/TCS/StateTrees/Element/Buff_Metal
  - Tags: Element.Buff.Metal
- 状态
  - StateDefId: '元素_水_增益'
  - StateSlotType: StateSlot.Buff.Element.Water
  - Priority: 30
  - DurationType: SDT_Duration
  - Duration: 10.0
  - SameInstigatorMergerType: UTcsStateMerger_UseNewest
  - DiffInstigatorMergerType: UTcsStateMerger_UseNewest
  - StateTreeRef: /Game/TCS/StateTrees/Element/Buff_Water
  - Tags: Element.Buff.Water
- 状态
  - StateDefId: '元素_木_共鸣'
  - StateSlotType: StateSlot.Buff.Element.Wood.Resonance
  - Priority: 90
  - DurationType: SDT_Duration
  - Duration: 3.0
  - SameInstigatorMergerType: UTcsStateMerger_UseNewest
  - DiffInstigatorMergerType: UTcsStateMerger_UseNewest
  - StateTreeRef: /Game/TCS/StateTrees/Element/Resonance_Wood
  - Tags: Element.Buff.Wood.Resonance
- 状态
  - StateDefId: '元素_火_共鸣'
  - StateSlotType: StateSlot.Buff.Element.Fire.Resonance
  - Priority: 90
  - DurationType: SDT_Duration
  - Duration: 3.0
  - SameInstigatorMergerType: UTcsStateMerger_UseNewest
  - DiffInstigatorMergerType: UTcsStateMerger_UseNewest
  - StateTreeRef: /Game/TCS/StateTrees/Element/Resonance_Fire
  - Tags: Element.Buff.Fire.Resonance
- 状态
  - StateDefId: '元素_土_共鸣'
  - StateSlotType: StateSlot.Buff.Element.Earth.Resonance
  - Priority: 90
  - DurationType: SDT_Duration
  - Duration: 3.0
  - SameInstigatorMergerType: UTcsStateMerger_UseNewest
  - DiffInstigatorMergerType: UTcsStateMerger_UseNewest
  - StateTreeRef: /Game/TCS/StateTrees/Element/Resonance_Earth
  - Tags: Element.Buff.Earth.Resonance
- 状态
  - StateDefId: '元素_金_共鸣'
  - StateSlotType: StateSlot.Buff.Element.Metal.Resonance
  - Priority: 90
  - DurationType: SDT_Duration
  - Duration: 3.0
  - SameInstigatorMergerType: UTcsStateMerger_UseNewest
  - DiffInstigatorMergerType: UTcsStateMerger_UseNewest
  - StateTreeRef: /Game/TCS/StateTrees/Element/Resonance_Metal
  - Tags: Element.Buff.Metal.Resonance
- 状态
  - StateDefId: '元素_水_共鸣'
  - StateSlotType: StateSlot.Buff.Element.Water.Resonance
  - Priority: 90
  - DurationType: SDT_Duration
  - Duration: 3.0
  - SameInstigatorMergerType: UTcsStateMerger_UseNewest
  - DiffInstigatorMergerType: UTcsStateMerger_UseNewest
  - StateTreeRef: /Game/TCS/StateTrees/Element/Resonance_Water
  - Tags: Element.Buff.Water.Resonance
- 状态
  - StateDefId: '元素_木_减益'
  - StateSlotType: StateSlot.Debuff.Element.Wood
  - Priority: 30
  - DurationType: SDT_Duration
  - Duration: 8.0
  - SameInstigatorMergerType: UTcsStateMerger_Stack
  - DiffInstigatorMergerType: UTcsStateMerger_Stack
  - StateTreeRef: /Game/TCS/StateTrees/Element/Debuff_Wood
  - Tags: Element.Debuff.Wood
- 状态
  - StateDefId: '元素_火_减益'
  - StateSlotType: StateSlot.Debuff.Element.Fire
  - Priority: 30
  - DurationType: SDT_Duration
  - Duration: 8.0
  - SameInstigatorMergerType: UTcsStateMerger_Stack
  - DiffInstigatorMergerType: UTcsStateMerger_Stack
  - StateTreeRef: /Game/TCS/StateTrees/Element/Debuff_Fire
  - Tags: Element.Debuff.Fire
- 状态
  - StateDefId: '元素_土_减益'
  - StateSlotType: StateSlot.Debuff.Element.Earth
  - Priority: 30
  - DurationType: SDT_Duration
  - Duration: 8.0
  - SameInstigatorMergerType: UTcsStateMerger_Stack
  - DiffInstigatorMergerType: UTcsStateMerger_Stack
  - StateTreeRef: /Game/TCS/StateTrees/Element/Debuff_Earth
  - Tags: Element.Debuff.Earth
- 状态
  - StateDefId: '元素_金_减益'
  - StateSlotType: StateSlot.Debuff.Element.Metal
  - Priority: 30
  - DurationType: SDT_Duration
  - Duration: 8.0
  - SameInstigatorMergerType: UTcsStateMerger_Stack
  - DiffInstigatorMergerType: UTcsStateMerger_Stack
  - StateTreeRef: /Game/TCS/StateTrees/Element/Debuff_Metal
  - Tags: Element.Debuff.Metal
- 状态
  - StateDefId: '元素_水_减益'
  - StateSlotType: StateSlot.Debuff.Element.Water
  - Priority: 30
  - DurationType: SDT_Duration
  - Duration: 8.0
  - SameInstigatorMergerType: UTcsStateMerger_Stack
  - DiffInstigatorMergerType: UTcsStateMerger_Stack
  - StateTreeRef: /Game/TCS/StateTrees/Element/Debuff_Water
  - Tags: Element.Debuff.Water

## 3. 顶层 StateTree（树形结构与转换）
```yaml
StateTree:
  Name: '顶层_卧龙_示例'
  Root:
    Children:
      - Name: '战斗_姿态'
        Children:
          - Name: '移动_行走'
            OnEnter: ['打开Gate: StateSlot.Mobility.Walk']
            OnExit:  ['关闭Gate: StateSlot.Mobility.Walk']
          - Name: '移动_疾跑'
            OnEnter: ['打开Gate: StateSlot.Mobility.Sprint']
            OnExit:  ['关闭Gate: StateSlot.Mobility.Sprint']
          - Name: '移动_跳跃'
            OnEnter: ['打开Gate: StateSlot.Mobility.Jump']
            OnExit:  ['关闭Gate: StateSlot.Mobility.Jump']
          - Name: '移动_闪避'
            OnEnter: ['打开Gate: StateSlot.Mobility.Dodge']
            OnExit:  ['关闭Gate: StateSlot.Mobility.Dodge']

          - Name: '动作_轻攻击'
            OnEnter: ['打开Gate: StateSlot.Action.Attack.Light']
            OnExit:  ['关闭Gate: StateSlot.Action.Attack.Light']
          - Name: '动作_重攻击'
            OnEnter: ['打开Gate: StateSlot.Action.Attack.Heavy','打开Gate: StateSlot.Buff.HyperArmor']
            OnExit:  ['关闭Gate: StateSlot.Action.Attack.Heavy','关闭Gate: StateSlot.Buff.HyperArmor']

          - Name: '动作_施法'
            OnEnter: ['打开Gate: StateSlot.Action.Spell']
            OnExit:  ['关闭Gate: StateSlot.Action.Spell']
          - Name: '引导_施法'
            OnEnter: ['打开Gate: StateSlot.Channel.SpellCast','关闭Gate: StateSlot.Mobility.Dodge']
            OnExit:  ['关闭Gate: StateSlot.Channel.SpellCast','打开Gate: StateSlot.Mobility.Dodge']

      - Name: '受击_域'
        Children:
          - Name: '受击_硬直'
            OnEnter: ['关闭Gate: StateSlot.Action.*','关闭Gate: StateSlot.Channel.*']
            OnExit:  ['打开Gate: StateSlot.Action.Attack.Light','打开Gate: StateSlot.Mobility.Walk']
          - Name: '受击_破防'
            OnEnter: ['关闭Gate: StateSlot.Action.*','关闭Gate: StateSlot.Mobility.*','关闭Gate: StateSlot.Channel.*','清空槽: StateSlot.Action.Combo']
            OnExit:  ['打开Gate: StateSlot.Mobility.Walk']

      - Name: '化劲_与_反击'
        Children:
          - Name: '动作_化劲'
            OnEnter: ['打开Gate: StateSlot.Action.Deflect']
            OnExit:  ['关闭Gate: StateSlot.Action.Deflect']
            Transitions:
              - When: '化劲成功'  To: '动作_反击'
          - Name: '动作_反击'
            OnEnter: ['打开Gate: StateSlot.Action.Counter']
            OnExit:  ['关闭Gate: StateSlot.Action.Counter']

      - Name: '元素_域'
        Children:
          - Name: '元素_木_增益'
            OnEnter: ['打开Gate: StateSlot.Buff.Element.Wood']
          - Name: '元素_火_增益'
            OnEnter: ['打开Gate: StateSlot.Buff.Element.Fire']
          - Name: '元素_土_增益'
            OnEnter: ['打开Gate: StateSlot.Buff.Element.Earth']
          - Name: '元素_金_增益'
            OnEnter: ['打开Gate: StateSlot.Buff.Element.Metal']
          - Name: '元素_水_增益'
            OnEnter: ['打开Gate: StateSlot.Buff.Element.Water']
          - Name: '元素_木_共鸣'
            OnEnter: ['打开Gate: StateSlot.Buff.Element.Wood.Resonance']
            OnExit:  ['关闭Gate: StateSlot.Buff.Element.Wood.Resonance']
          - Name: '元素_火_共鸣'
            OnEnter: ['打开Gate: StateSlot.Buff.Element.Fire.Resonance']
            OnExit:  ['关闭Gate: StateSlot.Buff.Element.Fire.Resonance']
          - Name: '元素_土_共鸣'
            OnEnter: ['打开Gate: StateSlot.Buff.Element.Earth.Resonance']
            OnExit:  ['关闭Gate: StateSlot.Buff.Element.Earth.Resonance']
          - Name: '元素_金_共鸣'
            OnEnter: ['打开Gate: StateSlot.Buff.Element.Metal.Resonance']
            OnExit:  ['关闭Gate: StateSlot.Buff.Element.Metal.Resonance']
          - Name: '元素_水_共鸣'
            OnEnter: ['打开Gate: StateSlot.Buff.Element.Water.Resonance']
            OnExit:  ['关闭Gate: StateSlot.Buff.Element.Water.Resonance']
          - Name: '元素_木_减益'
            OnEnter: ['打开Gate: StateSlot.Debuff.Element.Wood']
          - Name: '元素_火_减益'
            OnEnter: ['打开Gate: StateSlot.Debuff.Element.Fire']
          - Name: '元素_土_减益'
            OnEnter: ['打开Gate: StateSlot.Debuff.Element.Earth']
          - Name: '元素_金_减益'
            OnEnter: ['打开Gate: StateSlot.Debuff.Element.Metal']
          - Name: '元素_水_减益'
            OnEnter: ['打开Gate: StateSlot.Debuff.Element.Water']
```

## 5. 策略资产（Policies）

```yaml
SlotRelations:
  - SlotTag: StateSlot.CC.Stagger
    CloseGatesOnEnter:   [StateSlot.Action.*, StateSlot.Channel.*]
    OpenGatesOnExit:     [StateSlot.Action.Attack.Light, StateSlot.Mobility.Walk]
    Notes: '受击硬直期间禁止动作与引导；结束后恢复基础移动与轻攻'

  - SlotTag: StateSlot.CC.Break
    CloseGatesOnEnter:   [StateSlot.Action.*, StateSlot.Mobility.*, StateSlot.Channel.*]
    ClearSlotsOnEnter:   [StateSlot.Action.Combo]
    Notes: '破防关闭所有域并清连击槽'

  - SlotTag: StateSlot.Action.Attack.Heavy
    CloseGatesOnEnter:   [StateSlot.Mobility.Dodge]
    Notes: '重击期间禁用闪避'

  - SlotTag: StateSlot.Channel.SpellCast
    CloseGatesOnEnter:   [StateSlot.Mobility.Dodge, StateSlot.Action.Attack.*]
    Interruptors:        [StateSlot.CC.Stagger, StateSlot.CC.Break]
    ClearSlotsOnInterrupt: [StateSlot.Channel.SpellCast]
    Notes: '引导被硬直/破防中断并清槽'
```

<!-- 旧版规则资产（RuleSet/Dependencies/Exclusions）已废弃，示例段落移除。规则请在 StateTree 编排，免疫/净化通过策略层实现。 -->

## 6. 五行元素关系表（FTcsElementRelationRow，多行，克制与相生全量）
```yaml
ElementRelationRows:
  # 相克（木克土，土克水，水克火，火克金，金克木）
  - Type: Counter
    BuffElement: Element.Buff.Wood
    DebuffElement: Element.Debuff.Earth
    CloseGates: [StateSlot.Debuff.Element.Earth]
    OpenGates:  [StateSlot.Buff.Element.Wood.Resonance]
    ClearSlots: []
    Notes: '木克土：关闭土减益Gate，开启木共鸣'

  - Type: Counter
    BuffElement: Element.Buff.Earth
    DebuffElement: Element.Debuff.Water
    CloseGates: [StateSlot.Debuff.Element.Water]
    OpenGates:  [StateSlot.Buff.Element.Earth.Resonance]
    ClearSlots: []
    Notes: '土克水'

  - Type: Counter
    BuffElement: Element.Buff.Water
    DebuffElement: Element.Debuff.Fire
    CloseGates: [StateSlot.Debuff.Element.Fire]
    OpenGates:  [StateSlot.Buff.Element.Water.Resonance]
    ClearSlots: []
    Notes: '水克火'

  - Type: Counter
    BuffElement: Element.Buff.Fire
    DebuffElement: Element.Debuff.Metal
    CloseGates: [StateSlot.Debuff.Element.Metal]
    OpenGates:  [StateSlot.Buff.Element.Fire.Resonance]
    ClearSlots: []
    Notes: '火克金'

  - Type: Counter
    BuffElement: Element.Buff.Metal
    DebuffElement: Element.Debuff.Wood
    CloseGates: [StateSlot.Debuff.Element.Wood]
    OpenGates:  [StateSlot.Buff.Element.Metal.Resonance]
    ClearSlots: []
    Notes: '金克木'

  # 相生（木生火，火生土，土生金，金生水，水生木）
  - Type: Generate
    FromElement: Element.Buff.Wood
    ToElement:   Element.Buff.Fire
    OpenGatesOnGenerate: [StateSlot.Buff.Element.Fire.Resonance]
    GenerateWindowSeconds: 3.0
    Notes: '木生火：开放火共鸣窗口'

  - Type: Generate
    FromElement: Element.Buff.Fire
    ToElement:   Element.Buff.Earth
    OpenGatesOnGenerate: [StateSlot.Buff.Element.Earth.Resonance]
    GenerateWindowSeconds: 3.0
    Notes: '火生土'

  - Type: Generate
    FromElement: Element.Buff.Earth
    ToElement:   Element.Buff.Metal
    OpenGatesOnGenerate: [StateSlot.Buff.Element.Metal.Resonance]
    GenerateWindowSeconds: 3.0
    Notes: '土生金'

  - Type: Generate
    FromElement: Element.Buff.Metal
    ToElement:   Element.Buff.Water
    OpenGatesOnGenerate: [StateSlot.Buff.Element.Water.Resonance]
    GenerateWindowSeconds: 3.0
    Notes: '金生水'

  - Type: Generate
    FromElement: Element.Buff.Water
    ToElement:   Element.Buff.Wood
    OpenGatesOnGenerate: [StateSlot.Buff.Element.Wood.Resonance]
    GenerateWindowSeconds: 3.0
    Notes: '水生木'
```


## 附：任务/条件调用清单
- 节点 '受击_硬直'（OnEnter/OnExit）
  - OnEnter 关闭 Gate: , 
  - OnExit 打开 Gate: , 
- 节点 '受击_破防'（OnEnter）
  - 关闭 Gate: , , 
  - 清槽: 
- 节点 '动作_重攻击'（OnEnter/OnExit）
  - OnEnter 打开 Gate: ; 关闭 Gate: 
  - OnExit 关闭 Gate: ; 打开 Gate: 
- 节点 '引导_施法'（OnEnter/中断/OnExit）
  - OnEnter 关闭 Gate: , 
  - 中断来源: ,  命中时→清槽 
  - OnExit 打开 Gate: 
- 节点 '元素_*_共鸣'（OnEnter/计时退出）
  - OnEnter 打开 Gate: 
  - 由 ElementRelations 的 GenerateWindowSeconds 或顶层计时任务关闭 Gate
- 元素关系（顶层逻辑）
  - Counter 命中: 关闭对应 Debuff Gate，打开克制方共鸣 Gate
  - Generate 命中: 打开 To 元素共鸣 Gate（带时窗）
- Cleanse（示例，YAML，仅示意）

```yaml
Task: Cleanse
  Params:
  Selector:
    Class: UTcsSelector_ByTags
    Params:
      IncludeTags: [Element.Debuff.*]
      MatchLogic: Any
      ExcludeSubtrees: []
      Exceptions: []
      MaxPriority: 999
  MaxRemoveCount: 2
```
- 轻量前置（可选，StateManager.CheckImmunity）
  - 策略仅做免疫判定（允许/阻断），不返回“缩放/抗性”。
