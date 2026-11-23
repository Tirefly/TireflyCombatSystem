/* TcsCapabilityMatrix · 判定知识库
 * 由人手工撰写；每个判定单元必须能回溯到 TCS 文档锚点或明确标注「无依据」。
 */
(function (root) {
  'use strict';

  var DOC = 'Plugins/TireflyCombatSystem/Documents/';

  // ── 判定状态 ──────────────────────────────────────────────
  var STATUS = {
    covered:   { id: 'covered',   label: '已覆盖',   mark: 'OK',  gap: null,      desc: '框架已有正式文档，且文档明确覆盖该原语' },
    ruling:    { id: 'ruling',    label: '需补裁决', mark: '!',   gap: 'ruling',  desc: '被未决项挡住，裁决后才能判定' },
    doc:       { id: 'doc',       label: '缺文档',   mark: 'D',   gap: 'doc',     desc: '已有草稿或已裁内容，只差正式化' },
    mechanism: { id: 'mechanism', label: '缺机制',   mark: 'X',   gap: 'mechanism', desc: '已判定框架不提供此类能力' },
    host:      { id: 'host',      label: '归宿主',   mark: 'H',   gap: 'host',    desc: '设计上明确由宿主实现，TCS 不应提供' },
    unknown:   { id: 'unknown',   label: '缺依据',   mark: '?',   gap: 'unknown', desc: '所属域无文档也无草稿，无法判定' }
  };

  // ── 判定单元（列）──────────────────────────────────────────
  var UNITS = [
    { id: 'core.entity',   kind: 'core',   name: '战斗实体',     status: 'formal',
      doc: DOC + 'tcs-remake-v2/02-核心基线/战斗实体.md',
      covers: '身份句柄、目标有效性、级联清理、实体查询契约 ITcsEntityQuery、存储形态判定' },
    { id: 'core.instance', kind: 'core',   name: '运行时实例',   status: 'formal',
      doc: DOC + 'tcs-remake-v2/02-核心基线/运行时实例.md',
      covers: '实例句柄、实例池 TTcsInstancePool、来源句柄 FTcsSourceHandle、观测面' },
    { id: 'core.clock',    kind: 'core',   name: '统一时钟',     status: 'formal',
      doc: DOC + 'tcs-remake-v2/02-核心基线/统一时钟.md',
      covers: '战斗时钟与泵、到期堆 FTcsExpiryHeap、延迟步骤、槽位态与取消窗口' },
    { id: 'core.eventbus', kind: 'core',   name: '事件总线',     status: 'formal',
      doc: DOC + 'tcs-remake-v2/02-核心基线/事件总线.md',
      covers: '类型擦除载荷、句柄式订阅、稳定下标派发、频道层级匹配' },
    { id: 'core.numeric',  kind: 'core',   name: '数值聚合器',   status: 'formal',
      doc: DOC + 'tcs-remake-v2/02-核心基线/数值聚合器.md',
      covers: '唯一算法、两级比较、ETcsAttrLayer 落点、OrderSeq、明细视图' },
    { id: 'core.tag',      kind: 'core',   name: 'GameplayTag',  status: 'formal',
      doc: DOC + 'tcs-remake-v2/02-核心基线/GameplayTag.md',
      covers: 'FTcsTagRequirements（require/ignore）、自建而不直接用 GAS' },
    { id: 'dom.attribute', kind: 'domain', name: 'Attribute 属性', status: 'formal',
      doc: DOC + 'tcs-remake-v2/04-领域/Attribute.md',
      covers: '三段式、两段聚合+全局层、依赖图、边界钳制、引用计数存在性、五个写入口' },
    { id: 'dom.parameter', kind: 'domain', name: 'Parameter 参数', status: 'draft',
      doc: DOC + 'tcs-remake-v2-drafts/领域-Parameter-草稿.md',
      covers: '求值器 FTcsParamResolver、表达式引用链、Born/Live 切片、布尔开关（R1–R22 已裁，未正式化）' },
    { id: 'dom.action',    kind: 'domain', name: 'Action 动作',   status: 'draft',
      doc: DOC + 'tcs-remake-v2-drafts/领域-Action-草稿.md',
      covers: '单步链、步骤类型的 Struct/Object 取舍、实例化策略（已裁，未正式化）' },
    { id: 'dom.state',     kind: 'domain', name: 'State 状态',    status: 'none', doc: null,
      covers: '无文档、无草稿' },
    { id: 'dom.skill',     kind: 'domain', name: 'Skill 技能',    status: 'none', doc: null,
      covers: '无文档、无草稿' },
    { id: 'dom.damage',    kind: 'domain', name: 'Damage 伤害',   status: 'none', doc: null,
      covers: '无文档、无草稿' },
    { id: 'dom.targeting', kind: 'domain', name: 'Targeting 选取', status: 'none', doc: null,
      covers: '无文档、无草稿' }
  ];

  // ── 判定档（同族原语共享同一组判定，单条可覆写）─────────────
  var PROFILES = {

    dmg: {
      'core.numeric':  ['covered', '伤害数值由聚合器算出：基值 + 修正器七带折叠（数值聚合器.md §2.1 唯一算法）'],
      'core.eventbus': ['covered', '伤害记录形状 + 投递事件由框架给；「什么值得统计」明确非目标（01-定位与判据.md §1.2 ③）'],
      'dom.damage':    ['unknown', '伤害域无文档、无草稿，减伤/类型分离/结算顺序均无可判依据'],
      'dom.targeting': ['unknown', '目标域无文档、无草稿']
    },

    cc: {
      'core.tag':      ['covered', '控制状态可用 tag 表达并由 FTcsTagRequirements 判定（GameplayTag.md §1）'],
      'core.clock':    ['covered', '持续时间由到期堆承载（统一时钟.md §1.2）'],
      'dom.state':     ['unknown', '状态域无文档、无草稿；控制免疫优先级、递减、互斥规则无依据'],
      'dom.targeting': ['unknown', '目标域无文档、无草稿']
    },

    buff: {
      'core.numeric':  ['covered', '增益数值走同一套聚合器，基值与修正器七带折叠（数值聚合器.md §2.1 唯一算法）'],
      'dom.attribute': ['covered', '属性修正器是唯一挂载点（Attribute.md §7.1 AttachMod / DetachMod）'],
      'core.clock':    ['covered', '持续时间由到期堆承载（统一时钟.md §1.2 到期堆）'],
      'dom.state':     ['unknown', '状态域无文档；层数叠加、刷新规则、互斥无依据']
    },

    cast: {
      'core.clock':    ['covered', '冷却与施法时间可由时钟表达（统一时钟.md §1.1 战斗时钟与泵）'],
      'core.instance': ['covered', '运行态可由实例句柄承载（运行时实例.md §1.1.4 需要句柄的类型）'],
      'dom.action':    ['doc', '单步链已裁但未正式化（领域-Action-草稿.md §1）'],
      'dom.skill':     ['unknown', '技能域无文档、无草稿；充能/重施/切换的契约无依据']
    },

    atk: {
      'core.entity':   ['covered', '攻击目标可经 ITcsEntityQuery 查询目标枚举（战斗实体.md §3 查询契约）'],
      'core.numeric':  ['covered', '伤害数值由聚合器算出（数值聚合器.md §2.1 唯一算法）'],
      'dom.damage':    ['unknown', '伤害域无文档'],
      'dom.skill':     ['unknown', '技能域无文档；攻击重置与动画通知的边界无依据']
    },

    tgt: {
      'core.entity':   ['covered', 'ITcsEntityQuery 提供目标枚举契约（战斗实体.md §3 查询契约）'],
      'core.tag':      ['covered', '筛选条件可由 FTcsTagRequirements 表达（GameplayTag.md §1）'],
      'dom.targeting': ['unknown', '目标域无文档、无草稿；形状判定、命中顺序、去重无依据']
    },

    def: {
      'core.tag':      ['covered', '免疫/不可选中可由 tag 表达（GameplayTag.md §1）'],
      'dom.state':     ['unknown', '状态域无文档；免疫优先级与窗口无依据']
    },

    stl: {
      'core.tag':      ['covered', '隐匿状态可由 tag 表达（GameplayTag.md §1）'],
      'dom.state':     ['unknown', '状态域无文档；破隐规则无依据']
    },

    mov: {
      'core.entity':   ['host', '位移明确归宿主：战斗实体.md §5.3「表现、反馈、位移、区域归宿主」'],
      'dom.targeting': ['unknown', '目标域无文档；冲刺落点与碰撞判定无依据']
    },

    sum: {
      'core.entity':   ['covered', '召唤物可作为子实体持有句柄（战斗实体.md §1 身份句柄）'],
      'core.instance': ['covered', '召唤物运行态可由实例句柄承载（运行时实例.md §1.1 实例句柄）'],
      'dom.state':     ['unknown', '状态域无文档；召唤物生命周期归属无依据']
    },

    trn: {
      'core.tag':      ['covered', '形态切换可由 tag 表达（GameplayTag.md §1）'],
      'dom.state':     ['unknown', '状态域无文档'],
      'dom.skill':     ['unknown', '技能域无文档；技能组替换的契约无依据']
    },

    prj: {
      'core.entity':   ['covered', '投射物可作为实体持有句柄（战斗实体.md §1 身份句柄）'],
      'core.clock':    ['covered', '投射物推进可由时钟驱动（统一时钟.md §1.1 战斗时钟与泵）'],
      'dom.targeting': ['unknown', '目标域无文档；飞行碰撞判定无依据']
    },

    spc: {
      'core.clock':    ['covered', '冷却异常（不享受急速、延迟冷却）可由时钟表达（统一时钟.md §1.1 战斗时钟与泵）'],
      'dom.skill':     ['unknown', '技能域无文档；特殊规则无归属判定依据']
    },

    fml: {
      'core.numeric':  ['covered', '七带折叠是公式运算的算术基座（数值聚合器.md §2.1）'],
      'core.entity':   ['covered', '等级来源由战斗实体契约提供（领域-Parameter-草稿.md §4.3.5）'],
      'dom.attribute': ['covered', '属性段值可作公式系数来源（Attribute.md §8.2 段值）'],
      'dom.parameter': ['doc', '求值器已裁 R1–R22，只差正式化（领域-Parameter-草稿.md §4）']
    },

    attr: {
      'dom.attribute': ['covered', '属性修正是 Attribute 域的核心能力（Attribute.md §7.1 属性集唯一增删入口 / §7.5 AttachMod）'],
      'core.numeric':  ['covered', '修正器进入两级聚合参与折叠（数值聚合器.md §2.1 唯一算法）']
    },

    trg: {
      'core.eventbus': ['covered', '触发以类型化事件投递与订阅实现（事件总线.md §1.1）'],
      'dom.state':     ['unknown', '状态域无文档、无草稿；触发条件与状态生命周期无依据'],
      'dom.action':    ['doc', '响应侧是动作链，单步链已裁未正式化（领域-Action-草稿.md §1）']
    },

    stack: {
      'dom.state':     ['unknown', '状态域无文档、无草稿；层数语义、叠加上限、衰减规则无依据'],
      'core.numeric':  ['covered', '层数可作为修正器来源计入聚合（数值聚合器.md §2.1 唯一算法）'],
      'dom.parameter': ['doc', '求值环境按 §4.3.1 封闭清单不含层数，需裁（领域-Parameter-草稿.md §4）']
    },

    rule: {
      'core.tag':   ['covered', '封印/改写可用 tag 标记并由 FTcsTagRequirements 判定（GameplayTag.md §1）'],
      'dom.skill':  ['unknown', '技能域无文档、无草稿；技能组替换与规则改写归属无依据']
    },

    atv: {
      'core.clock':    ['covered', '主动使用的冷却与持续由时钟承载（统一时钟.md §1.2 到期堆）'],
      'core.instance': ['covered', '主动使用的运行态可由实例句柄承载（运行时实例.md §1.1.4）'],
      'dom.action':    ['doc', '主动使用触发的动作链已裁未正式化（领域-Action-草稿.md §1）']
    },

    quest: {
      'core.eventbus': ['covered', '任务进度靠订阅事件累加（事件总线.md §1.1）'],
      'dom.state':     ['unknown', '状态域无文档、无草稿；进度量与分档的存储形态无依据'],
      'dom.parameter': ['doc', '进度作为取值来源需裁（领域-Parameter-草稿.md §4）']
    },

    grant: {
      'core.tag':      ['covered', '授予结果可由 tag 标记（GameplayTag.md §1）'],
      'dom.skill':     ['unknown', '技能域无文档、无草稿；技能槽授予与替换的契约无依据']
    },

    econ: {
      'core.entity':   ['host', '金币是宿主经济词汇，不是战斗能力：01-定位与判据.md §5.6 主判据「凡是需要宿主词汇的内置策略，框架一律不提供」'],
      'dom.attribute': ['host', '金币不参与战斗属性聚合（Attribute.md §9.2 它不做什么）']
    },

    roll: {
      'core.numeric':  ['covered', '随机结果的落点可由聚合承接（数值聚合器.md §2.1 唯一算法）'],
      'dom.parameter': ['doc', '求值器已裁，但随机数来源与确定性种子未裁（领域-Parameter-草稿.md §4）'],
      'dom.state':     ['unknown', '状态域无文档；随机池内容与重掷规则无依据']
    }
  };

  // ── 原语（行）────────────────────────────────────────────
  // src 记录该原语的官方来源标签，保证可回溯
  var P = function (id, name, group, profile, src, note) {
    return { id: id, name: name, group: group, profile: profile, src: src, note: note || null, cells: null };
  };

  var PRIMITIVES = [
    // 伤害
    P('dmg.instant', '即时伤害', 'dmg', 'dmg', ['Trait_DamageAbility', 'Trait_Strike']),
    P('dmg.aoe',     '范围伤害', 'dmg', 'dmg', ['Trait_AoE']),
    P('dmg.dot',     '持续伤害', 'dmg', 'dmg', ['Trait_DoT']),
    // 控制
    P('cc.immobilize', '定身/眩晕', 'cc', 'cc', ['Trait_ImmobilizingCCSpell', 'Trait_ImmobilizingCCAbility']),
    P('cc.knockback',  '击退',      'cc', 'cc', ['Trait_KnockBack']),
    P('cc.moveblock',  '移动限制',  'cc', 'cc', ['PositiveEffect_MoveBlock']),
    // 增益
    P('buff.boon',          '增益',        'buff', 'buff', ['PositiveEffect_Boon']),
    P('buff.heal',          '主动治疗',    'buff', 'buff', ['Trait_ActiveHeal']),
    P('buff.selfheal',      '自我治疗',    'buff', 'buff', ['Trait_SelfHeal']),
    P('buff.shield',        '护盾',        'buff', 'buff', ['Trait_Shield']),
    P('buff.shield_passive','被动护盾',    'buff', 'buff', ['Trait_PassiveShield']),
    P('buff.empower_attack','强化普攻',    'buff', 'buff', ['PositiveEffect_EmpowerAttack']),
    P('buff.attack_buff',   '攻击增益(限时)','buff', 'buff', ['Trait_AttackBuff_Duration']),
    // 施法形态
    P('cast.ultimate',      '终极技能',    'cast', 'cast', ['Trait_Ultimate']),
    P('cast.signature',     '标志性技能',  'cast', 'cast', ['Trait_SignatureSpell']),
    P('cast.recast',        '重施/替换',   'cast', 'cast', ['Trait_RecastOrReplaceSpell', 'Trait_SwapsIntoImmobilizingCCAbility']),
    P('cast.free_recast',   '免费重施',    'cast', 'cast', ['Trait_FreeRecast']),
    P('cast.channel',       '引导',        'cast', 'cast', ['Trait_ChannelSpell']),
    P('cast.toggle',        '开关式',      'cast', 'cast', ['Trait_Toggle']),
    P('cast.ammo',          '弹药/充能',   'cast', 'cast', ['Trait_Ammo']),
    P('cast.ult_reactivate','大招二次激活','cast', 'cast', ['Trait_UltimateReactivation']),
    P('cast.passive_slot',  '被动槽位',    'cast', 'cast', ['Trait_PassiveSlot']),
    // 攻击形态
    P('atk.reset',              '攻击重置',      'atk', 'atk', ['Trait_AttackReset']),
    P('atk.ranged_stop',        '远程首次命中停', 'atk', 'atk', ['Trait_Ranged_StopsFirstHit']),
    P('atk.piercing',           '穿透',          'atk', 'atk', ['Trait_Ranged_Piercing']),
    P('atk.melee_small_windup', '近战短前摇',    'atk', 'atk', ['Trait_Melee_SmallWindup']),
    P('atk.melee_big_windup',   '近战长前摇',    'atk', 'atk', ['Trait_Melee_BigWindup']),
    P('atk.multi_target',       '多目标攻击',    'atk', 'atk', ['Trait_MultiTargetAttack']),
    P('atk.non_primary',        '非主攻击',      'atk', 'atk', ['Trait_NonPrimaryAttack']),
    // 目标形状
    P('tgt.area',        '区域选取', 'tgt', 'tgt', ['Trait_Target_Area']),
    P('tgt.directional', '方向选取', 'tgt', 'tgt', ['Trait_Target_Directional']),
    P('tgt.zone',        '区域持续', 'tgt', 'tgt', ['Trait_Ranged_Zone']),
    // 防御
    P('def.parry',       '格挡',     'def', 'def', ['Trait_Parry']),
    P('def.untargetable','不可选中', 'def', 'def', ['Trait_Untargetable']),
    P('def.cc_immune',   '控制免疫', 'def', 'def', ['Trait_CCImmune']),
    P('def.immune',      '免疫',     'def', 'def', ['Trait_Immune']),
    P('def.stasis',      '凝滞',     'def', 'def', ['SpecialCase_StasisLocked']),
    // 隐匿
    P('stl.invisible',   '隐形',     'stl', 'stl', ['Trait_Invisibility']),
    P('stl.camouflage',  '伪装',     'stl', 'stl', ['Trait_Camouflage']),
    // 位移
    P('mov.dash',  '冲刺', 'mov', 'mov', ['Trait_PlayerSelectedDashDirection']),
    P('mov.blink', '闪现', 'mov', 'mov', ['PositiveEffect_Teleport']),
    // 召唤物
    P('sum.pet',     '召唤物', 'sum', 'sum', ['Trait_Pet']),
    P('sum.trap',    '陷阱',   'sum', 'sum', ['Trait_Trap']),
    P('sum.clone',   '分身',   'sum', 'sum', ['Trait_CreateClone']),
    P('sum.terrain', '造地形', 'sum', 'sum', ['Trait_CreateTerrain']),
    // 变身
    P('trn.transform', '变身',   'trn', 'trn', ['Trait_Transformation']),
    P('trn.upgrade',   '技能升级','trn', 'trn', ['Trait_Upgrade']),
    // 投射物
    P('prj.destroy', '投射物摧毁', 'prj', 'prj', ['Trait_ProjectileDestruction']),
    // 特殊规则
    P('spc.no_cooldown',     '无冷却',       'spc', 'spc', ['Trait_NoCooldown']),
    P('spc.no_haste',        '不享急速',     'spc', 'spc', ['Trait_NotBenefitFromHaste']),
    P('spc.delayed_cd',      '延迟冷却',     'spc', 'spc', ['SpecialCase_DelayedCooldown']),
    P('spc.cast_while_tp',   '传送中可施法', 'spc', 'spc', ['SpecialCase_CastWhileTeleporting']),
    P('spc.cast_override',   '施法覆盖',     'spc', 'spc', ['Trait_SylasSpecialAttackOverride']),
    P('spc.kinematic',       '演出锁定',     'spc', 'spc', ['Trait_Kinematic']),
    P('spc.smite',           '惩戒类',       'spc', 'spc', ['Trait_SummonerSmite']),
    P('spc.no_tear_trigger', '不触发女神泪', 'spc', 'spc', ['SpecialCase_CastNotTriggerTear']),
    // 公式原语
    P('fml.named_value',      '具名数值',     'fml', 'fml', ['NamedDataValueCalculationPart'], '对应 DataValues 里的一个具名数组'),
    P('fml.stat_coef',        '属性×系数',    'fml', 'fml', ['StatByCoefficientCalculationPart']),
    P('fml.stat_named',       '属性×具名值',  'fml', 'fml', ['StatByNamedDataValueCalculationPart']),
    P('fml.stat_subpart',     '属性×子式',    'fml', 'fml', ['StatBySubPartCalculationPart']),
    P('fml.level_breakpoint', '等级断点',     'fml', 'fml', ['ByCharLevelBreakpointsCalculationPart']),
    P('fml.level_interp',     '等级插值',     'fml', 'fml', ['ByCharLevelInterpolationCalculationPart']),
    P('fml.level_formula',    '等级公式',     'fml', 'fml', ['ByCharLevelFormulaCalculationPart']),
    P('fml.constant',         '常量',         'fml', 'fml', ['NumberCalculationPart']),
    P('fml.buff_counter',     '按层数取值',   'fml', 'fml', ['BuffCounterByNamedDataValueCalculationPart', 'BuffCounterByCoefficientCalculationPart']),
    P('fml.sum',              '求和',         'fml', 'fml', ['SumOfSubPartsCalculationPart']),
    P('fml.product',          '求积',         'fml', 'fml', ['ProductOfSubPartsCalculationPart']),
    P('fml.clamp',            '上下限钳制',   'fml', 'fml', ['ClampSubPartsCalculationPart']),
    P('fml.conditional',      '条件分支',     'fml', 'fml', ['GameCalculationConditional']),
    P('fml.modified',         '计算变体',     'fml', 'fml', ['GameCalculationModified']),
    P('fml.effect_value',     '效果值',       'fml', 'fml', ['EffectValueCalculationPart']),
    P('fml.resource_coef',    '资源×系数',    'fml', 'fml', ['AbilityResourceByCoefficientCalculationPart']),
    // 装备与增益的生效方式（非技能效果）
    P('attr.modifier',     '属性修正',   'attr', 'attr', ['stat', 'abilityPower', 'health', 'attackDamage', 'armor', 'magicResist'],
      '给属性挂修正器，是装备/增幅/符文最常见的生效方式'),
    P('trg.on_hit',        '命中触发',   'trg', 'trg', ['onHit']),
    P('trg.on_kill',       '击杀触发',   'trg', 'trg', ['onKill']),
    P('trg.on_damaged',    '受伤触发',   'trg', 'trg', ['onDamaged']),
    P('trg.periodic',      '周期触发',   'trg', 'trg', ['periodic']),
    P('trg.conditional',   '条件触发',   'trg', 'trg', ['passive']),
    P('state.stack',       '层数状态',   'stack', 'stack', ['status']),
    P('rule.override',     '规则改写',   'rule', 'rule', ['keywordMajor']),
    P('cast.activated',    '主动使用',   'cast', 'atv', ['active'], '装备/道具的主动技能，不占技能槽'),
    P('cast.duplicate',    '复制施放',   'cast', 'cast', ['recast'], '复制体重复施放同一技能'),
    // 数据供给与规则类
    P('attr.base',         '基础属性供给', 'attr', 'attr', ['champion-stats'], '英雄基础属性与每级成长，是属性系统的输入数据'),
    P('dmg.execute',       '处决/斩杀',   'dmg', 'dmg', ['execute'], '按阈值直接处决的伤害'),
    P('quest.counter',     '任务计数',   'quest', 'quest', ['quest'], '累计事件次数并按档位结算'),
    P('rule.grant',        '授予',       'rule', 'grant', ['grant'], '授予技能、符文、物品或资源'),
    P('roll.random',       '随机结果',   'roll', 'roll', ['random'], '随机抽取或随机数值'),
    P('econ.gold',         '金币',       'econ', 'econ', ['gold'], '经济产出，属宿主经济系统')
  ];

  // ── 单条覆写：与所在档不同、需要单独说明的原语 ─────────────
  PRIMITIVES.forEach(function (p) {
    if (p.id === 'sum.terrain') {
      p.cells = {
        'core.entity': ['host', '区域与地形归宿主：战斗实体.md §5.3'],
        'dom.state':   ['unknown', '状态域无文档；地形生命周期无依据']
      };
    }
    if (p.id === 'tgt.zone') {
      p.cells = {
        'core.entity':   ['host', '区域归宿主：战斗实体.md §5.3'],
        'dom.targeting': ['unknown', '目标域无文档；持续区域判定无依据']
      };
    }
    if (p.id === 'fml.clamp' || p.id === 'fml.conditional') {
      p.cells = {
        'core.numeric':  ['ruling', '聚合器的七带算术不含条件分支/钳制表达式；是否扩展聚合器未裁（数值聚合器.md §2.2 它不做什么）'],
        'core.entity':   ['covered', '等级来源走 ITcsEntityQuery::TryGetEntityLevel（领域-Parameter-草稿.md §4.3.5 等级从哪来）'],
        'dom.parameter': ['doc', '求值器已裁但对「表达式节点类型清单」未定（领域-Parameter-草稿.md §4）']
      };
    }
    if (p.id === 'trg.periodic') {
      p.cells = {
        'core.clock':    ['covered', '周期由到期堆驱动（统一时钟.md §1.2）'],
        'core.eventbus': ['covered', '到期后投递类型化事件（事件总线.md §1.1）'],
        'dom.action':    ['doc', '响应侧是动作链，已裁未正式化（领域-Action-草稿.md §1）']
      };
    }
    if (p.id === 'trg.on_hit' || p.id === 'trg.on_kill' || p.id === 'trg.on_damaged') {
      p.cells = {
        'core.eventbus': ['covered', '触发以类型化事件投递与订阅实现（事件总线.md §1.1）'],
        'dom.damage':    ['unknown', '伤害域无文档、无草稿；命中/击杀/受伤事件的形状与投递时机无依据'],
        'dom.action':    ['doc', '响应侧是动作链，已裁未正式化（领域-Action-草稿.md §1）']
      };
    }
    if (p.id === 'trg.conditional') {
      p.cells = {
        'core.tag':      ['covered', '条件可用 tag 表达并由 FTcsTagRequirements 判定（GameplayTag.md §1）'],
        'dom.state':     ['unknown', '状态域无文档；条件求值与重新求值时机无依据'],
        'dom.parameter': ['doc', '求值器已裁，但条件刷新触发未裁（领域-Parameter-草稿.md §4）']
      };
    }
    if (p.id === 'rule.override') {
      p.cells = {
        'core.tag':  ['covered', '封印/改写可用 tag 标记（GameplayTag.md §1）'],
        'dom.skill': ['unknown', '技能域无文档、无草稿；技能槽封印与技能组替换的契约无依据']
      };
    }
    if (p.id === 'fml.buff_counter') {
      p.cells = {
        'core.numeric':  ['covered', '层数可作为修正器来源计入聚合（数值聚合器.md §2.1 唯一算法）'],
        'dom.parameter': ['doc', '求值环境按 §4.3.1 封闭清单不含层数，需裁'],
        'dom.state':     ['unknown', '状态域无文档；层数语义无依据']
      };
    }
  });

  var GROUPS = [
    { id: 'dmg', name: '伤害' },
    { id: 'cc',  name: '控制' },
    { id: 'buff', name: '增益' },
    { id: 'cast', name: '施法形态' },
    { id: 'atk', name: '攻击形态' },
    { id: 'tgt', name: '目标形状' },
    { id: 'def', name: '防御' },
    { id: 'stl', name: '隐匿' },
    { id: 'mov', name: '位移' },
    { id: 'sum', name: '召唤与地形' },
    { id: 'trn', name: '变身' },
    { id: 'prj', name: '投射物' },
    { id: 'spc', name: '特殊规则' },
    { id: 'fml', name: '公式原语' },
    { id: 'attr',  name: '属性修正' },
    { id: 'trg',   name: '触发时机' },
    { id: 'stack', name: '层数状态' },
    { id: 'rule',  name: '规则改写' },
    { id: 'quest', name: '任务计数' },
    { id: 'roll',  name: '随机' },
    { id: 'econ',  name: '经济' }
  ];

  root.TCM_MODEL = {
    STATUS: STATUS, UNITS: UNITS, PROFILES: PROFILES,
    PRIMITIVES: PRIMITIVES, GROUPS: GROUPS
  };
})(typeof globalThis !== 'undefined' ? globalThis : this);
