/* TcsCapabilityMatrix · 标注规则
 * 把数据源给出的语义标签 / 文案，映射到效果原语。
 * 三级来源，精度递减：riot-tags（Riot 官方 mSpellTags + 公式节点）> semantics（数据商语义标签）> text（文案正则推断）
 */
(function (root) {
  'use strict';

  var SOURCE = {
    'riot-tags': { id: 'riot-tags', label: 'Riot 官方标签', precision: 1, desc: '来自 mSpellTags 与 mSpellCalculations，是游戏实现里真实存在的标记' },
    'semantics': { id: 'semantics', label: '数据商语义标签', precision: 2, desc: '来自 aramkit 富文本 span 的 semantic 字段，由数据商人工标注' },
    'text':      { id: 'text',      label: '文案推断',       precision: 3, desc: '由技能/装备文案正则匹配得到，只作补充，可能漏标误标' }
  };

  // aramkit semantic → 原语。未列出的语义（keyword/legacyText/rarity 等）不产生原语。
  var SEMANTIC_RULES = {
    heal: 'buff.heal',
    shield: 'buff.shield',
    magicDamage: 'dmg.instant',
    physicalDamage: 'dmg.instant',
    trueDamage: 'dmg.instant',
    totalValue: 'dmg.instant',
    onHit: 'trg.on_hit',
    active: 'cast.activated',
    status: 'state.stack',
    passive: 'trg.conditional',
    recast: 'cast.recast',
    stat: 'attr.modifier',
    abilityPower: 'attr.modifier',
    health: 'attr.modifier',
    attackDamage: 'attr.modifier',
    attackSpeed: 'attr.modifier',
    mana: 'attr.modifier',
    armor: 'attr.modifier',
    magicResist: 'attr.modifier',
    movementSpeed: 'attr.modifier',
    criticalStrike: 'attr.modifier',
    armorPenetration: 'attr.modifier',
    magicPenetration: 'attr.modifier',
    abilityHaste: 'attr.modifier',
    lifeSteal: 'attr.modifier',
    omnivamp: 'attr.modifier',
    criticalStrike: 'attr.modifier',
    gold: 'econ.gold'
  };

  // 文案正则 → 原语。用于没有语义标签的语料（符文），以及补充语义标签漏掉的部分。
  var TEXT_RULES = [
    { re: /造成[^。]{0,20}伤害|伤害值|自适应伤害|真实伤害|魔法伤害|物理伤害/, prims: ['dmg.instant'], why: '文案提到造成伤害' },
    { re: /持续[^。]{0,10}伤害|灼烧|每秒[^。]{0,6}伤害/, prims: ['dmg.dot'], why: '文案提到持续伤害' },
    { re: /治疗|回复生命|生命偷取/, prims: ['buff.heal'], why: '文案提到治疗' },
    { re: /护盾/, prims: ['buff.shield'], why: '文案提到护盾' },
    { re: /眩晕|定身|击飞|击退|沉默|禁锢|嘲讽|减速/, prims: ['cc.immobilize'], why: '文案提到控制效果' },
    { re: /位移|冲刺|突进|闪现|传送/, prims: ['mov.dash'], why: '文案提到位移' },
    { re: /召唤|分身|陷阱/, prims: ['sum.pet'], why: '文案提到召唤物' },
    { re: /冷却时间|冷却/, prims: ['trg.periodic'], why: '文案提到该效果自身有冷却，属周期性触发' },
    { re: /层数|叠加|每层/, prims: ['state.stack'], why: '文案提到层数' },
    { re: /封印|禁用|无法使用/, prims: ['rule.override'], why: '文案提到对规则本身的改写' },
    { re: /攻击力|法术强度|护甲|魔法抗性|生命值|攻击速度|韧性|全能吸血|技能急速|装备急速|适应之力|暴击几率|暴击|攻击距离|护甲穿透|法术穿透|法力值/, prims: ['attr.modifier'], why: '文案提到属性数值' },
    { re: /处决|斩杀/, prims: ['dmg.execute'], why: '文案提到按阈值处决' },
    { re: /随机/, prims: ['roll.random'], why: '文案提到随机抽取' },
    { re: /任务[:：]|任务进度|累计命中|累计[^。]{0,6}次/, prims: ['quest.counter'], why: '文案提到任务计数' },
    { re: /金币/, prims: ['econ.gold'], why: '文案提到金币产出' },
    { re: /获得[^。]{0,12}技能|获得[^。]{0,12}符文|获得[^。]{0,8}锻造器|授予/, prims: ['rule.grant'], why: '文案提到授予技能或符文' },
    { re: /复制体|再次施放|额外飞弹|额外[^。]{0,6}弹/, prims: ['cast.duplicate'], why: '文案提到复制施放或额外投射' },
    { re: /额外[^。]{0,8}敌人|多个敌人|附近敌人/, prims: ['atk.multi_target'], why: '文案提到多目标' }
  ];

  function fromSemantics(item) {
    var out = {}, hits = [];
    var sems = item.semantics || [];
    for (var i = 0; i < sems.length; i++) {
      var p = SEMANTIC_RULES[sems[i]];
      if (p && !out[p]) { out[p] = true; hits.push(sems[i] + '→' + p); }
    }
    return { ids: Object.keys(out), hits: hits };
  }

  function textOf(item) {
    var parts = [];
    if (item.tooltipText) { parts = parts.concat(item.tooltipText); }
    if (item.shortDesc) { parts.push(item.shortDesc); }
    if (item.longDesc) { parts.push(String(item.longDesc).replace(/<[^>]*>/g, ' ')); }
    return parts.join(' ');
  }

  function fromText(item, existing) {
    var text = textOf(item);
    var out = {}, hits = [];
    if (!text) { return { ids: [], hits: hits }; }
    for (var i = 0; i < TEXT_RULES.length; i++) {
      var r = TEXT_RULES[i];
      if (!r.re.test(text)) { continue; }
      for (var k = 0; k < r.prims.length; k++) {
        var p = r.prims[k];
        if (existing[p] || out[p]) { continue; }
        out[p] = true;
        hits.push(r.why + '→' + p);
      }
    }
    return { ids: Object.keys(out), hits: hits };
  }

  // 解析一条需求的最终原语集合与来源
  function resolve(item) {
    // 英雄本身不产生效果，它的可判定内容是「基础属性 + 每级成长」这一份数据供给
    if (item.kind === 'champion') {
      return { ids: ['attr.base'], source: 'kind-rule', note: '英雄数据对应属性系统的基础值输入' };
    }
    var declared = item.primitives || [];
    if (declared.length) {
      var matchNote = null;
      if (item.tagMatch === 'numeric') { matchNote = '按技能数值数组匹配到 CDragon 对象'; }
      else if (item.tagMatch === 'passive') { matchNote = '按技能名含 passive 匹配到 CDragon 对象'; }
      else if (item.tagMatch === 'unresolved') { matchNote = '未能匹配到 CDragon 对象，原语集合为空'; }
      return { ids: declared.slice(), source: 'riot-tags', note: matchNote };
    }
    var sem = fromSemantics(item);
    var seen = {};
    for (var i = 0; i < sem.ids.length; i++) { seen[sem.ids[i]] = true; }
    var txt = fromText(item, seen);
    var ids = sem.ids.concat(txt.ids);
    if (!ids.length) { return { ids: [], source: null, note: '无语义标签、文案也未命中任何规则' }; }
    return {
      ids: ids,
      source: sem.ids.length ? 'semantics' : 'text',
      note: sem.hits.concat(txt.hits).join('；')
    };
  }

  root.TCM_ANNOTATE = {
    SOURCE: SOURCE,
    SEMANTIC_RULES: SEMANTIC_RULES,
    TEXT_RULES: TEXT_RULES,
    resolve: resolve,
    fromSemantics: fromSemantics,
    fromText: fromText
  };
})(typeof globalThis !== 'undefined' ? globalThis : this);
