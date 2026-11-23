// annotate-crawl.mjs —— 由 data/raw/ 的原始快照重建六份语料（data/requirements/*.json）。
//
// 用法：
//   node annotate-crawl.mjs           # 预演：只比对，打印差异，不改任何文件
//   node annotate-crawl.mjs --write   # 真正写入 data/requirements/
//
// 依赖：先跑 fetch-corpora.mjs 把原始数据抓到 data/raw/。
//
// 同时产出 data/raw/sidecar/{slug}.json —— 每条技能匹配到的 CDragon 对象的
// DataValues 与 mSpellCalculations 原文。不解析、不映射，仅供后续 Parameter 域
// 正式化后写公式求值判定用。

import { readFileSync, writeFileSync, existsSync, mkdirSync, copyFileSync } from 'node:fs';
import { join, dirname } from 'node:path';
import { fileURLToPath } from 'node:url';
import { TAG, CALC } from './prim-map.mjs';

const here = dirname(fileURLToPath(import.meta.url));
const rawDir = join(here, 'data', 'raw');
const reqDir = join(here, 'data', 'requirements');
const sidecarDir = join(rawDir, 'sidecar');
const legacyDir = join(rawDir, 'legacy');

// 与 fetch-corpora.mjs 的 PIN 保持一致
const VERSION = { aramkit: '16.22', augmentOld: '16.20', ddragon: '16.23.1', cdragon: '16.23' };
const SLOTS = ['Q', 'W', 'E', 'R', 'P'];
const write = process.argv.includes('--write');

// ── 读取原始快照 ────────────────────────────────────────────
function loadJson() {
  const p = join(rawDir, ...arguments);
  if (!existsSync(p)) throw new Error('缺少原始快照：' + p + '\n请先运行 node fetch-corpora.mjs');
  return JSON.parse(readFileSync(p, 'utf8'));
}

const ak = {
  champions: loadJson('aramkit', VERSION.aramkit, 'champions.json'),
  skills: loadJson('aramkit', VERSION.aramkit, 'champion-skills.json'),
  items: loadJson('aramkit', VERSION.aramkit, 'items.json'),
  augments: loadJson('aramkit', VERSION.aramkit, 'augments.json'),
  augmentsOld: loadJson('aramkit', VERSION.augmentOld, 'augments.json')
};
const dd = {
  champion: loadJson('ddragon', VERSION.ddragon, 'champion.json'),
  runes: loadJson('ddragon', VERSION.ddragon, 'runesReforged.json')
};

const cdragonCache = {};
function cdragon(slug) {
  if (!cdragonCache[slug]) cdragonCache[slug] = loadJson('cdragon', VERSION.cdragon, slug + '.bin.json');
  return cdragonCache[slug];
}

// ── 富文本工具 ──────────────────────────────────────────────
// aramkit 的 tooltip 是 { blocks: [{ spans: [...] }] }；icon 类型用 label 占位
function flattenTooltip(tooltip) {
  if (!tooltip || !Array.isArray(tooltip.blocks)) return [];
  return tooltip.blocks.map((block) => {
    const spans = Array.isArray(block.spans) ? block.spans : [];
    return spans.map((s) => (s.type === 'icon' ? (s.label || '') : (s.text || ''))).join('');
  });
}

// 语义标签：按首次出现顺序去重
function semanticsOf(tooltip) {
  if (!tooltip || !Array.isArray(tooltip.blocks)) return [];
  const out = [];
  for (const block of tooltip.blocks) {
    for (const s of (block.spans || [])) {
      if (s.semantic && !out.includes(s.semantic)) out.push(s.semantic);
    }
  }
  return out;
}

const numOr = (v) => (v === undefined || v === null ? null : v);

// ── 五、英雄技能（含槽位匹配） ──────────────────────────────
// 匹配顺序：exact（叶名 = 英雄名 + 槽位字母）→ 数值数组 → 被动名 → 未解析
const eqArr = (a, b) => {
  if (!Array.isArray(a) || !Array.isArray(b)) return null;
  const n = Math.min(a.length, b.length);
  if (!n) return null;
  for (let i = 0; i < n; i++) if (Math.abs((a[i] ?? 0) - (b[i] ?? 0)) > 0.01) return false;
  return true;
};

function numericScore(akSkill, spell) {
  let s = 0;
  if (eqArr(akSkill.cooldown, spell.cooldownTime)) s += 2;
  const rgd = eqArr(akSkill.range, spell.castRangeDisplayOverride);
  const rg = rgd === null ? eqArr(akSkill.range, spell.castRange) : rgd;
  if (rg) s += 2;
  if (eqArr(akSkill.cost, spell.mana)) s += 1;
  return s;
}

// calcTypes 只取一层：具名计算节点自身 + 其 mFormulaParts 直接子项，不递归进子部件内部。
// 此规则用 536 条 exact 命中技能全量验证，536/536 一致。
function collectCalcTypes(calculations) {
  const acc = [];
  const add = (t) => { if (typeof t === 'string' && !acc.includes(t)) acc.push(t); };
  const walkCalc = (node) => {
    if (!node || typeof node !== 'object') return;
    add(node.__type);
    const parts = node.mFormulaParts;
    if (!Array.isArray(parts)) return;
    for (const p of parts) {
      if (!p || typeof p !== 'object') continue;
      add(p.__type);
      if (p.__type === 'GameCalculation' || p.__type === 'GameCalculationModified' || p.__type === 'GameCalculationConditional') walkCalc(p);
    }
  };
  if (calculations && typeof calculations === 'object') for (const k of Object.keys(calculations)) walkCalc(calculations[k]);
  return acc;
}

function primitivesOf(tags, calcTypes) {
  const set = new Set();
  for (const t of tags) (TAG[t] || []).forEach((x) => set.add(x));
  for (const c of calcTypes) (CALC[c] || []).forEach((x) => set.add(x));
  return [...set].sort();
}

// 给一个英雄解析五个槽位
function matchSlots(championSlug, akSkills) {
  const bin = cdragon(championSlug);
  const objects = Object.keys(bin).filter((p) => bin[p] && bin[p].mSpell);
  const champName = championSlug.charAt(0).toUpperCase() + championSlug.slice(1);
  const used = new Set();
  const result = {};

  // 第一遍：exact —— 必须先全部做完，否则数值匹配会抢占别的槽位的 exact 对象
  for (const slot of SLOTS) {
    if (!akSkills[slot]) continue;
    const want = (champName + slot).toLowerCase();
    const hit = objects.find((p) => !used.has(p) && p.split('/').pop().toLowerCase() === want);
    if (hit) { used.add(hit); result[slot] = { path: hit, match: 'exact' }; }
  }
  // 第二遍：数值数组比对；并列时取 DataValues 更多的那个
  for (const slot of SLOTS) {
    if (!akSkills[slot] || result[slot]) continue;
    const akSkill = akSkills[slot];
    let best = null;
    for (const p of objects) {
      if (used.has(p)) continue;
      const s = numericScore(akSkill, bin[p].mSpell);
      if (s <= 0) continue;
      const dv = Object.keys(bin[p].mSpell.DataValues || {}).length;
      if (!best || s > best.s || (s === best.s && dv > best.dv)) best = { path: p, s, dv };
    }
    if (best) { used.add(best.path); result[slot] = { path: best.path, match: 'numeric' }; }
  }
  // 第三遍：被动名
  for (const slot of SLOTS) {
    if (slot !== 'P' || !akSkills[slot] || result[slot]) continue;
    const cand = objects.find((p) => !used.has(p) && /passive/i.test(p.split('/').pop()));
    if (cand) { used.add(cand); result[slot] = { path: cand, match: 'passive' }; }
  }
  // 第四遍：仍未解析
  for (const slot of SLOTS) {
    if (akSkills[slot] && !result[slot]) result[slot] = { path: null, match: 'unresolved' };
  }
  return result;
}

function buildSkills() {
  const items = [];
  const sidecar = {};
  const matchCount = { exact: 0, numeric: 0, passive: 0, unresolved: 0 };

  for (const id of Object.keys(ak.skills)) {
    const champion = ak.champions[id];
    if (!champion) continue;
    const akSkills = ak.skills[id];
    const slots = matchSlots(champion.slug, akSkills);
    const bin = cdragon(champion.slug);
    const side = {};

    for (const slot of SLOTS) {
      const s = akSkills[slot];
      if (!s) continue;
      const m = slots[slot];
      const info = { match: m.match, path: m.path, alternateName: null, dataValues: null, calculations: null };
      let tags = [], calcTypes = [], dataValueKeys = [], primitives = [], sourceTags = null;

      if (m.path) {
        const spell = bin[m.path].mSpell;
        tags = [...new Set((spell.mSpellTags || []).filter(Boolean))];
        dataValueKeys = Object.keys(spell.DataValues || {});
        calcTypes = collectCalcTypes(spell.mSpellCalculations);
        primitives = primitivesOf(tags, calcTypes);
        sourceTags = m.path;
        info.alternateName = spell.mAlternateName || null;
        info.dataValues = spell.DataValues || null;
        info.calculations = spell.mSpellCalculations || null;
        matchCount[m.match]++;
      } else {
        matchCount.unresolved++;
      }

      items.push({
        id: 'lol.' + champion.slug + '.' + slot.toLowerCase(),
        game: 'lol',
        kind: 'skill',
        version: VERSION.aramkit,
        champion: {
          id: Number(id),
          slug: champion.slug,
          key: champion.riotId,
          name: champion.name,
          title: champion.title,
          roles: champion.roles
        },
        slot,
        name: s.name,
        maxRank: numOr(s.maxRank),
        cost: numOr(s.cost),
        cooldown: numOr(s.cooldown),
        range: numOr(s.range),
        semantics: semanticsOf(s.tooltip),
        tooltipText: flattenTooltip(s.tooltip),
        spellTags: tags,
        calcTypes,
        dataValueKeys,
        primitives,
        tagMatch: m.match,
        source: { text: 'aramkit/champion-skills.json#' + id + '.' + slot, tags: sourceTags }
      });

      side[slot] = info;
    }
    sidecar[champion.slug] = { champion: champion.slug, cdragonVersion: VERSION.cdragon, slots: side };
  }

  return {
    doc: {
      corpus: 'lol',
      kind: 'skill',
      version: VERSION.aramkit,
      generatedFrom: {
        aramkit: VERSION.aramkit,
        cdragon: VERSION.cdragon,
        note: 'spellTags/calcTypes/dataValueKeys 来自 CDragon bin；tagMatch 表示槽位如何命中：exact=叶名精确、numeric=冷却/射程/耗蓝数组比对、passive=被动名、unresolved=取不到对应对象'
      },
      items
    },
    sidecar,
    matchCount
  };
}

// ── 其余五份 ────────────────────────────────────────────────
function buildChampions() {
  const items = [];
  // aramkit 的 riotId 与 DDragon 的 data 键存在大小写差异（FiddleSticks vs Fiddlesticks），
  // 故先精确匹配，再退化为小写匹配。key 一律取 aramkit 的 riotId，与既有语料一致。
  const ddByLower = new Map(Object.keys(dd.champion.data).map((k) => [k.toLowerCase(), dd.champion.data[k]]));
  for (const id of Object.keys(ak.champions)) {
    const c = ak.champions[id];
    const d = dd.champion.data[c.riotId] || ddByLower.get(c.riotId.toLowerCase());
    if (!d) throw new Error('DDragon 缺少英雄：' + c.riotId);
    items.push({
      id: 'lol.champion.' + c.slug,
      game: 'lol',
      kind: 'champion',
      version: VERSION.aramkit,
      championId: Number(id),
      slug: c.slug,
      key: c.riotId,
      name: c.name,
      title: c.title,
      roles: c.roles,
      tags: d.tags,
      stats: d.stats,
      balance: numOr(c.balance),
      source: {
        stats: 'ddragon ' + VERSION.ddragon + ' champion.json#' + id,
        balance: 'aramkit/champions.json#' + id + '.balance'
      }
    });
  }
  return { corpus: 'lol', kind: 'champion', version: VERSION.aramkit, items };
}

function buildItems() {
  const items = [];
  for (const id of Object.keys(ak.items)) {
    const it = ak.items[id];
    items.push({
      id: 'lol.item.' + id,
      game: 'lol',
      kind: 'item',
      version: VERSION.aramkit,
      riotId: Number(id),
      name: it.name,
      icon: it.icon,
      priceTotal: it.priceTotal,
      sellPrice: it.sellPrice,
      purchasable: it.purchasable,
      isFinal: numOr(it.isFinal),
      maxStacks: numOr(it.maxStacks),
      requirements: numOr(it.requirements),
      from: numOr(it.from),
      to: numOr(it.to),
      categories: it.categories,
      acquisition: it.acquisition,
      tooltipText: flattenTooltip(it.tooltip),
      semantics: semanticsOf(it.tooltip),
      source: { text: 'aramkit/items.json#' + id }
    });
  }
  return { corpus: 'lol', kind: 'item', version: VERSION.aramkit, items };
}

function buildRunes() {
  const items = [];
  for (const tree of dd.runes) {
    (tree.slots || []).forEach((slot, idx) => {
      for (const rune of (slot.runes || [])) {
        items.push({
          id: 'lol.rune.' + rune.id,
          game: 'lol',
          kind: 'rune',
          version: VERSION.ddragon,
          riotId: rune.id,
          treeId: tree.id,
          treeName: tree.name,
          slotIndex: idx,
          name: rune.name,
          icon: rune.icon,
          shortDesc: rune.shortDesc,
          longDesc: rune.longDesc,
          source: { text: 'ddragon ' + VERSION.ddragon + ' runesReforged.json#' + rune.id }
        });
      }
    });
  }
  return {
    corpus: 'lol',
    kind: 'rune',
    version: VERSION.ddragon,
    dataQuality: '符文数值只存在于 shortDesc/longDesc 富文本，DDragon 无结构化数值字段',
    items
  };
}

function buildAugments(src, version) {
  const items = [];
  for (const id of Object.keys(src)) {
    const a = src[id];
    items.push({
      id: 'lol.augment.' + a.slug,
      game: 'lol',
      kind: 'augment',
      version,
      riotId: Number(id),
      slug: a.slug,
      name: a.name,
      icon: a.icon,
      rarity: a.rarity,
      tooltipText: flattenTooltip(a.tooltip),
      semantics: semanticsOf(a.tooltip),
      source: { text: 'aramkit/' + version + '/augments.json#' + id }
    });
  }
  return { corpus: 'lol', kind: 'augment', version, items };
}

// ── 构建 + 比对 ─────────────────────────────────────────────
const skillsResult = buildSkills();
const built = {
  'lol-champions': buildChampions(),
  'lol-items': buildItems(),
  'lol-runes': buildRunes(),
  'lol-augments-16.20': buildAugments(ak.augmentsOld, VERSION.augmentOld),
  'lol-augments-16.22': buildAugments(ak.augments, VERSION.aramkit),
  'lol-skills': skillsResult.doc
};

// 规范化比较：键序无关
function canonical(v) {
  if (Array.isArray(v)) return '[' + v.map(canonical).join(',') + ']';
  if (v && typeof v === 'object') {
    return '{' + Object.keys(v).sort().map((k) => JSON.stringify(k) + ':' + canonical(v[k])).join(',') + '}';
  }
  return JSON.stringify(v);
}

const report = [];
let totalChanged = 0;
for (const name of Object.keys(built)) {
  const doc = built[name];
  const file = join(reqDir, name + '.json');
  const nextText = JSON.stringify(doc, null, 2) + '\n';
  if (!existsSync(file)) {
    report.push({ name, status: '新建', detail: (doc.items || []).length + ' 条' });
    totalChanged++;
    if (write) writeFileSync(file, nextText, 'utf8');
    continue;
  }
  const prevText = readFileSync(file, 'utf8');
  const prevDoc = JSON.parse(prevText);
  if (canonical(prevDoc) === canonical(doc)) {
    report.push({ name, status: '一致', detail: (doc.items || []).length + ' 条' + (prevText === nextText ? '' : '（仅格式差异）') });
    if (write && prevText !== nextText) writeFileSync(file, nextText, 'utf8');
  } else {
    const prevById = new Map((prevDoc.items || []).map((x) => [x.id, x]));
    const nextById = new Map((doc.items || []).map((x) => [x.id, x]));
    const added = [...nextById.keys()].filter((k) => !prevById.has(k));
    const removed = [...prevById.keys()].filter((k) => !nextById.has(k));
    const changed = [...nextById.keys()].filter((k) => prevById.has(k) && canonical(prevById.get(k)) !== canonical(nextById.get(k)));
    report.push({ name, status: '有差异', detail: '新增 ' + added.length + ' / 移除 ' + removed.length + ' / 改动 ' + changed.length, prevById, nextById, added, removed, changed });
    if (process.argv.includes('--dump-diff')) {
      const dump = {};
      for (const k of changed) {
        const a = prevById.get(k), b = nextById.get(k);
        const fields = {};
        for (const f of new Set([...Object.keys(a), ...Object.keys(b)])) {
          if (canonical(a[f]) !== canonical(b[f])) fields[f] = { was: a[f], now: b[f] };
        }
        dump[k] = fields;
      }
      writeFileSync(join(here, 'data', '_diff-' + name + '.json'), JSON.stringify(dump, null, 2), 'utf8');
      console.log('  字段级差异：data/_diff-' + name + '.json');
    }
    totalChanged++;
    if (write) {
      // Tools 目录不在版本控制内，覆盖前先留一份旧语料
      mkdirSync(legacyDir, { recursive: true });
      const bak = join(legacyDir, name + '.json');
      if (!existsSync(bak)) copyFileSync(file, bak);
      writeFileSync(file, nextText, 'utf8');
      console.log('  写入 ' + name + '.json');
    }
  }
}

// ── 输出 ────────────────────────────────────────────────────
console.log('== 语料重建' + (write ? '（已写入语料）' : '（预演，不写语料；sidecar 仍会刷新）') + ' ==');
for (const r of report) console.log('  ' + r.name.padEnd(22) + r.status.padEnd(6) + r.detail);

const mc = skillsResult.matchCount;
console.log('\n  槽位匹配：exact ' + mc.exact + ' / numeric ' + mc.numeric + ' / passive ' + mc.passive + ' / unresolved ' + mc.unresolved);

const skillsReport = report.find((r) => r.name === 'lol-skills' && r.status === '有差异');
if (skillsReport) {
  const lines = [];
  lines.push('# 技能语料重建差异');
  lines.push('');
  lines.push('由 node annotate-crawl.mjs 生成。旧值来自早期临时脚本，新值由 CDragon 原始对象按槽位匹配重新推导。');
  lines.push('');
  lines.push('## 槽位匹配分布');
  lines.push('');
  lines.push('| 方式 | 条数 |');
  lines.push('| --- | ---: |');
  lines.push('| exact 叶名 | ' + mc.exact + ' |');
  lines.push('| numeric 数值数组 | ' + mc.numeric + ' |');
  lines.push('| passive 被动名 | ' + mc.passive + ' |');
  lines.push('| unresolved 未解析 | ' + mc.unresolved + ' |');
  lines.push('');
  lines.push('## 改动明细（' + skillsReport.changed.length + ' 条）');
  lines.push('');
  lines.push('| 技能 | 旧原语数 | 新原语数 | 旧匹配 | 新匹配 |');
  lines.push('| --- | ---: | ---: | --- | --- |');
  for (const id of skillsReport.changed) {
    const a = skillsReport.prevById.get(id);
    const b = skillsReport.nextById.get(id);
    lines.push('| ' + id + ' | ' + (a.primitives || []).length + ' | ' + (b.primitives || []).length + ' | ' + (a.tagMatch || '') + ' | ' + (b.tagMatch || '') + ' |');
  }
  writeFileSync(join(here, 'data', '_rematch-diff.md'), lines.join('\n') + '\n', 'utf8');
  console.log('  差异清单：data/_rematch-diff.md（' + skillsReport.changed.length + ' 条改动）');
}

if (process.argv.includes('--dump-rebuilt')) {
  const outDir = join(here, 'data', '_rebuilt');
  mkdirSync(outDir, { recursive: true });
  for (const name of Object.keys(built)) writeFileSync(join(outDir, name + '.json'), JSON.stringify(built[name], null, 2) + '\n', 'utf8');
  console.log('  重建结果：data/_rebuilt/ 共 ' + Object.keys(built).length + ' 份');
}

// sidecar 是新增数据（不覆盖任何既有语料），预演时也产出
mkdirSync(sidecarDir, { recursive: true });
let sideCount = 0;
let sideBytes = 0;
for (const slug of Object.keys(skillsResult.sidecar)) {
  const text = JSON.stringify(skillsResult.sidecar[slug]);
  writeFileSync(join(sidecarDir, slug + '.json'), text, 'utf8');
  sideCount++;
  sideBytes += Buffer.byteLength(text);
}
console.log('  sidecar：data/raw/sidecar/ 共 ' + sideCount + ' 个英雄，' + (sideBytes / 1024 / 1024).toFixed(2) + ' MB（DataValues + mSpellCalculations 原文）');

if (!write && totalChanged) {
  console.log('\n确认无误后运行：node annotate-crawl.mjs --write（会先把旧语料备份到 data/raw/legacy/）');
}
