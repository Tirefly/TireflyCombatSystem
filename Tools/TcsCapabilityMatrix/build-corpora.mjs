// build-corpora.mjs —— 把 data/requirements/*.json 内联成 data/corpora.js
// 原因：file:// 下 fetch 本地 JSON 会被 CORS 拦截，只能用 script 标签加载。
import { readFileSync, writeFileSync, readdirSync } from 'node:fs';
import { join, dirname } from 'node:path';
import { fileURLToPath } from 'node:url';

const here = dirname(fileURLToPath(import.meta.url));
const reqDir = join(here, 'data', 'requirements');
const outFile = join(here, 'data', 'corpora.js');

const CORPUS_META = {
  'lol-skills':            { key: 'skills',       label: '英雄技能',   source: 'aramkit champion-skills + CDragon mSpellTags/mSpellCalculations' },
  'lol-champions':         { key: 'champions',    label: '英雄属性',   source: 'DDragon champion.json + aramkit 大乱斗平衡调整' },
  'lol-items':             { key: 'items',        label: '装备',       source: 'aramkit items.json' },
  'lol-augments-16.20':    { key: 'augments1620', label: '海克斯增幅 16.20', source: 'aramkit 16.20 augments.json' },
  'lol-augments-16.22':    { key: 'augments1622', label: '海克斯增幅 16.22', source: 'aramkit 16.22 augments.json' },
  'lol-runes':             { key: 'runes',        label: '符文',       source: 'DDragon runesReforged.json' }
};

const files = readdirSync(reqDir).filter(f => f.endsWith('.json')).sort();
const corpora = {};
const manifest = [];

for (const f of files) {
  const base = f.replace(/\.json$/, '');
  const meta = CORPUS_META[base];
  if (!meta) { console.warn('未登记语料，跳过：' + f); continue; }
  const doc = JSON.parse(readFileSync(join(reqDir, f), 'utf8'));
  corpora[meta.key] = doc;
  manifest.push({
    key: meta.key,
    label: meta.label,
    kind: doc.kind,
    version: doc.version,
    count: (doc.items || []).length,
    source: meta.source,
    file: 'data/requirements/' + f
  });
  console.log(meta.key.padEnd(14) + String((doc.items || []).length).padStart(5) + ' 条  v' + doc.version);
}

// 原语来源的显示名，供界面与报告共用
const SOURCE_LABEL = {
  'riot-tags': 'Riot 官方标签',
  'semantics': '数据商语义标签',
  'text': '文案推断',
  'kind-rule': '类别规则'
};

const header = '/* 自动生成，勿手改；改需求数据请改 data/requirements/*.json 后重跑 build-corpora.mjs */\n';
const body = 'window.TCM_DATA = ' + JSON.stringify({ corpora, manifest, SOURCE_LABEL }, null, 0) + ';\n';
writeFileSync(outFile, header + body, 'utf8');
console.log('\n写出 ' + outFile + '  ' + (Buffer.byteLength(header + body) / 1024 / 1024).toFixed(2) + ' MB');
