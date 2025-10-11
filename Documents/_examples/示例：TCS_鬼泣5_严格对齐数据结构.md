# TCS 示例文档：'鬼泣5' 场景（严格对齐数据结构）

说明：本示例用于评估 TCS（顶层 StateTree + 合并器 + 策略类）的覆盖性，贴近 DMC5 的“硬核动作”结构（风格切换、取消窗口、恶魔化、霸体/硬直管理等）。中文名称用引号包裹；标签与枚举使用英文。

## 1. 槽位定义（FTcsStateSlotDefinition）
- 槽位
  - SlotTag: StateSlot.Mobility.Walk
  - ActivationMode: PriorityOnly
  - StateTreeStateName: '移动_行走'
- 槽位
  - SlotTag: StateSlot.Mobility.Dodge
  - ActivationMode: PriorityOnly
  - StateTreeStateName: '移动_闪避'
- 槽位
  - SlotTag: StateSlot.Mobility.Jump
  - ActivationMode: PriorityOnly
  - StateTreeStateName: '移动_跳跃'
- 槽位
  - SlotTag: StateSlot.Action.Attack.Light
  - ActivationMode: PriorityHangUp
  - StateTreeStateName: '动作_轻攻击'
- 槽位
  - SlotTag: StateSlot.Action.Attack.Heavy
  - ActivationMode: PriorityHangUp
  - StateTreeStateName: '动作_重攻击'
- 槽位
  - SlotTag: StateSlot.Action.Style
  - ActivationMode: PriorityOnly
  - StateTreeStateName: '风格_切换'
- 槽位
  - SlotTag: StateSlot.Buff.DevilTrigger
  - ActivationMode: AllActive
  - StateTreeStateName: '增益_恶魔化'
- 槽位
  - SlotTag: StateSlot.CC.Stagger
  - ActivationMode: AllActive
  - StateTreeStateName: '受击_硬直'

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
  - StateDefId: '风格_切换_魔剑士'
  - StateSlotType: StateSlot.Action.Style
  - Priority: 70
  - DurationType: SDT_None
  - Duration: 0.0
  - SameInstigatorMergerType: UTcsStateMerger_UseNewest
  - DiffInstigatorMergerType: UTcsStateMerger_UseNewest
  - StateTreeRef: /Game/TCS/StateTrees/Style/Swordmaster
  - Tags: State.Style.Swordmaster
- 状态
  - StateDefId: '风格_切换_枪神'
  - StateSlotType: StateSlot.Action.Style
  - Priority: 70
  - DurationType: SDT_None
  - Duration: 0.0
  - SameInstigatorMergerType: UTcsStateMerger_UseNewest
  - DiffInstigatorMergerType: UTcsStateMerger_UseNewest
  - StateTreeRef: /Game/TCS/StateTrees/Style/Gunslinger
  - Tags: State.Style.Gunslinger
- 状态
  - StateDefId: '风格_切换_皇家护卫'
  - StateSlotType: StateSlot.Action.Style
  - Priority: 70
  - DurationType: SDT_None
  - Duration: 0.0
  - SameInstigatorMergerType: UTcsStateMerger_UseNewest
  - DiffInstigatorMergerType: UTcsStateMerger_UseNewest
  - StateTreeRef: /Game/TCS/StateTrees/Style/RoyalGuard
  - Tags: State.Style.RoyalGuard
- 状态
  - StateDefId: '增益_恶魔化'
  - StateSlotType: StateSlot.Buff.DevilTrigger
  - Priority: 100
  - DurationType: SDT_Duration
  - Duration: 15.0
  - SameInstigatorMergerType: UTcsStateMerger_NoMerge
  - DiffInstigatorMergerType: UTcsStateMerger_NoMerge
  - StateTreeRef: /Game/TCS/StateTrees/Buff/DevilTrigger
  - Tags: State.Buff.DevilTrigger
- 状态
  - StateDefId: '受击_硬直'
  - StateSlotType: StateSlot.CC.Stagger
  - Priority: 120
  - DurationType: SDT_Duration
  - Duration: 0.5
  - SameInstigatorMergerType: UTcsStateMerger_UseNewest
  - DiffInstigatorMergerType: UTcsStateMerger_UseNewest
  - StateTreeRef: /Game/TCS/StateTrees/CC/Stagger
  - Tags: State.CC.Stagger

## 3. 顶层 StateTree（树形结构与转换）
```yaml
StateTree:
  Name: '顶层_鬼泣5_示例'
  Root:
    Children:
      - Name: '战斗_姿态'
        Children:
          - Name: '移动_行走'
            OnEnter: ['打开Gate: StateSlot.Mobility.Walk']
            OnExit:  ['关闭Gate: StateSlot.Mobility.Walk']
          - Name: '移动_闪避'
            OnEnter: ['打开Gate: StateSlot.Mobility.Dodge']
            OnExit:  ['关闭Gate: StateSlot.Mobility.Dodge']
          - Name: '移动_跳跃'
            OnEnter: ['打开Gate: StateSlot.Mobility.Jump']
            OnExit:  ['关闭Gate: StateSlot.Mobility.Jump']

          - Name: '动作_轻攻击'
            OnEnter: ['打开Gate: StateSlot.Action.Attack.Light']
            OnExit:  ['关闭Gate: StateSlot.Action.Attack.Light']
          - Name: '动作_重攻击'
            OnEnter: ['打开Gate: StateSlot.Action.Attack.Heavy']
            OnExit:  ['关闭Gate: StateSlot.Action.Attack.Heavy']

      - Name: '风格_域'
        Children:
          - Name: '风格_切换'
            OnEnter: ['打开Gate: StateSlot.Action.Style']
            OnExit:  ['关闭Gate: StateSlot.Action.Style']
            Transitions:
              - When: '输入: 切换到 魔剑士'  To: '风格_切换_魔剑士'
              - When: '输入: 切换到 枪神'    To: '风格_切换_枪神'
              - When: '输入: 切换到 皇家护卫' To: '风格_切换_皇家护卫'
          - Name: '风格_切换_魔剑士'
            OnEnter: ['打开Gate: StateSlot.Action.Style']
          - Name: '风格_切换_枪神'
            OnEnter: ['打开Gate: StateSlot.Action.Style']
          - Name: '风格_切换_皇家护卫'
            OnEnter: ['打开Gate: StateSlot.Action.Style']

      - Name: '恶魔化_域'
        Children:
          - Name: '增益_恶魔化'
            OnEnter: ['打开Gate: StateSlot.Buff.DevilTrigger']
            OnExit:  ['关闭Gate: StateSlot.Buff.DevilTrigger']

      - Name: '受击_域'
        Children:
          - Name: '受击_硬直'
            OnEnter: ['关闭Gate: StateSlot.Action.*']
            OnExit:  ['打开Gate: StateSlot.Action.Attack.Light']
```

## 4. 顶层联动说明
- 攻击槽（轻/重）采用 'PriorityHangUp'：进行重攻击时，挂起轻攻击分支，结束后自动恢复；
- 风格切换为独立槽：OnEnter 打开风格相关 Gate，风格间互斥由同槽策略保证；
- 恶魔化：OnEnter 打开 Buff 槽 Gate；
- 受击域：进入硬直临时关闭攻击 Gate；
- 取消窗口与步法（可选）：在攻击节点 OnExit 的 TransitionRule 上设置输入与时序，切换到 '移动_闪避' 或下一连段。

## 5. 策略资产（Policies）
```YAML
Policies:
  ImmunityPolicies:
    - Class: /Game/TCS/Policies/Pol_BossStaggerImmunity  # 示例：Boss 对硬直类状态免疫（或部分子类）
    - Class: /Game/TCS/Policies/Pol_DevilTriggerImmunity # 示例：恶魔化时对轻度硬直免疫
  CleansePolicies:
    - Class: /Game/TCS/Policies/Pol_None                # 本示例不使用净化，可留空或占位
```

说明：免疫仅返回“允许/阻断”，不返回缩放/抗性；顶层 StateTree 负责 Gate/时序/依赖与互斥。



## 附：任务/条件调用清单
- 节点 '动作_轻攻击'（OnExit 的 TransitionRule）
  - 取消窗口: 在允许的时间窗内收到 '闪避' 输入 → 转到 '移动_闪避'
  - 连段示例: 收到 '轻攻' 输入 → 转到 '动作_轻攻击(下一段)'
- 节点 '动作_重攻击'（与轻攻同理，可设独立取消/连段窗口）
- 节点 '风格_切换'（Transitions）
  - 输入: 切换到 '魔剑士' / '枪神' / '皇家护卫' → 转到对应节点
- 节点 '增益_恶魔化'（OnEnter/OnExit）
  - OnEnter 打开 Gate: 
  - OnExit 关闭 Gate: 
- 节点 '受击_硬直'（OnEnter/OnExit）
  - OnEnter 关闭 Gate: 
  - OnExit 打开 Gate: 
- 同槽策略（说明）
  - '动作_轻攻击'/'动作_重攻击' 槽采用 'PriorityHangUp'：高优先激活时低优先被挂起，退出后自动恢复
- 轻量前置（可选，StateManager.CheckImmunity）
  - 策略仅做免疫判定（允许/阻断），不返回“缩放/抗性”。
