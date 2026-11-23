// test-core.mjs —— Node 冒烟测试：验证判定逻辑与数据自洽
import { createRequire } from 'node:module';
import { dirname, join } from 'node:path';
import { fileURLToPath } from 'node:url';

const require = createRequire(import.meta.url);
const here = dirname(fileURLToPath(import.meta.url));

globalThis.window = globalThis;
require(join(here, 'data', 'model.js'));
require(join(here, 'data', 'annotate.js'));
require(join(here, 'core.js'));
require(join(here, 'data', 'corpora.js'));

const MODEL = globalThis.TCM_MODEL;
const CORE = globalThis.TCM_CORE;
const DATA = globalThis.TCM_DATA;

let fail = 0;
function check(name, cond, detail) {
  if (cond) { console.log('  PASS  ' + name + (detail ? '  (' + detail + ')' : '')); }
  else { console.log('  FAIL  ' + name + (detail ? '  (' + detail + ')' : '')); fail++; }
}

console.log('== 1. 知识库自洽 ==');
check('原语数 = 88', MODEL.PRIMITIVES.length === 88, MODEL.PRIMITIVES.length);
check('判定单元数 = 13', MODEL.UNITS.length === 13, MODEL.UNITS.length);
check('判定档数 = 23', Object.keys(MODEL.PROFILES).length === 23);
check('原语 id 唯一', new Set(MODEL.PRIMITIVES.map(p => p.id)).size === MODEL.PRIMITIVES.length);

// 每个原语都有 profile 且 profile 存在
const badProfile = MODEL.PRIMITIVES.filter(p => !MODEL.PROFILES[p.profile]);
check('每个原语都有有效 profile', badProfile.length === 0, badProfile.map(p => p.id).join(','));

// 每条 profile 里的 unit 都是真实 UNITS
const unitIds = new Set(MODEL.UNITS.map(u => u.id));
const badUnits = [];
for (const [pid, prof] of Object.entries(MODEL.PROFILES)) {
  for (const uid of Object.keys(prof)) { if (!unitIds.has(uid)) badUnits.push(pid + ':' + uid); }
}
check('profile 引用的单元都存在', badUnits.length === 0, badUnits.join(','));

// 每个原语至少 2 格（单格判定没有意义：无法区分「已覆盖」与「碰巧」）
const thin = MODEL.PRIMITIVES.filter(p => CORE.cellsOf(p).length < 2);
check('每个原语至少 2 个判定格', thin.length === 0, thin.map(p => p.id + '=' + CORE.cellsOf(p).length).join(','));

console.log('\n== 2. 需求数据自洽 ==');
const all = [];
for (const [key, doc] of Object.entries(DATA.corpora)) {
  for (const it of doc.items) all.push(it);
}
console.log('  收录需求总数 ' + all.length + ' 条');

// 数据里用到的原语是否都在词表里
const used = new Set();
for (const it of all) for (const p of (it.primitives || [])) used.add(p);
const unknownPrims = [...used].filter(p => !CORE.primitiveById(p));
check('数据用到的原语全部在词表内', unknownPrims.length === 0, unknownPrims.join(',') || (used.size + ' 个原语被使用'));

const results = CORE.evaluateAll(all);
check('全部需求都产出了裁决', results.length === all.length);
const untagged = results.filter(r => r.verdict === 'untagged');
check('未标注率 < 1%', untagged.length / results.length < 0.01,
      'untagged=' + untagged.length + '/' + results.length + (untagged.length ? ' 例：' + untagged.slice(0,5).map(r => r.name).join(',') : ''));
const bySource = {};
for (const r of results) { bySource[r.primSource || 'none'] = (bySource[r.primSource || 'none'] || 0) + 1; }
console.log('  原语来源分布：' + Object.entries(bySource).map(([k,v]) => k + '=' + v).join('  '));

const s = CORE.summarize(results);
console.log('\n== 3. 裁决分布 ==');
console.log('  总 ' + s.total + ' / 可实现 ' + s.ready + ' / 需宿主 ' + s.host_only + ' / 受阻 ' + s.blocked);
console.log('  原语格覆盖 ' + s.coveredCells + '/' + s.totalCells + ' = ' + s.coveragePct + '%');
console.log('  受阻最严重缺口：缺机制 ' + s.byWorst.mechanism + ' 缺依据 ' + s.byWorst.unknown +
            ' 需补裁决 ' + s.byWorst.ruling + ' 缺文档 ' + s.byWorst.doc);

console.log('\n== 4. 按类别 ==');
for (const [k, v] of Object.entries(s.byKind)) {
  console.log('  ' + (CORE.KIND_LABEL[k] || k).padEnd(8) + ' 总 ' + String(v.total).padStart(4) +
              '  可实现 ' + String(v.ready || 0).padStart(3) +
              '  需宿主 ' + String(v.host_only || 0).padStart(3) +
              '  受阻 ' + String(v.blocked || 0).padStart(4));
}

console.log('\n== 5. 单元影响面（补哪个文档收益最大）==');
const impact = CORE.unitImpact(results);
for (const i of impact.slice(0, 8)) {
  console.log('  ' + i.unit.name.padEnd(14) + ' 阻塞 ' + String(i.blocks).padStart(4) +
              '  独占 ' + String(i.exclusive).padStart(4) + '  涉及原语 ' + i.primitiveCount);
}

console.log('\n== 6. 矩阵与单元汇总 ==');
const m = CORE.buildMatrix();
check('矩阵行数 = 原语数', m.rows.length === MODEL.PRIMITIVES.length);
const us = CORE.unitSummary();
let sumCells = 0;
for (const u of us) sumCells += u.stat.total;
check('单元汇总的格数 = 全部原语格数', sumCells === m.rows.reduce((a, r) => a + r.count, 0), sumCells);
for (const u of us) {
  const st = u.stat;
  console.log('  ' + u.unit.name.padEnd(14) + ' ' + String(u.unit.status).padEnd(7) +
              ' 已覆盖 ' + String(st.covered).padStart(3) + '/' + String(st.total).padStart(3) +
              '  裁决 ' + String(st.ruling).padStart(2) + ' 缺文档 ' + String(st.doc).padStart(2) +
              ' 缺机制 ' + String(st.mechanism).padStart(2) + ' 归宿主 ' + String(st.host).padStart(2) +
              ' 缺依据 ' + String(st.unknown).padStart(2));
}

console.log('\n== 7. 版本差分（海克斯 16.20 → 16.22）==');
const oldDoc = DATA.corpora.augments1620, newDoc = DATA.corpora.augments1622;
const oldRes = CORE.evaluateAll(oldDoc.items), newRes = CORE.evaluateAll(newDoc.items);
const diff = CORE.diffResults(oldDoc.items, newDoc.items, oldRes, newRes);
console.log('  新增 ' + diff.added.length + '，移除 ' + diff.removed.length + '，判定变化 ' + diff.changed.length);
check('新增 + 共同 = 新版总数', diff.added.length + (newRes.length - diff.added.length) === newRes.length);

console.log('\n== 8. 报告生成 ==');
const report = {
  generatedAt: Date.now(),
  docBaseline: 'tcs-remake-v2 (HEAD 17f18c7)',
  model: { primitives: MODEL.PRIMITIVES.length, units: MODEL.UNITS.length, profiles: Object.keys(MODEL.PROFILES).length },
  summary: s,
  units: us,
  impact,
  unitGaps: CORE.unitGaps(results),
  results,
  corpora: DATA.manifest
};
const md = CORE.markdown(report);
check('报告非空', md.length > 3000, md.length + ' 字符');
check('报告含六个一级小节', (md.match(/^## /gm) || []).length === 6, (md.match(/^## /gm) || []).length);

// 8b. 带文档基线时多出第七节，且小节顺序必须是 一..七
const docStatus = {
  baselineAt: '2026-11-19T10:00:00+08:00', baselineGit: 'remake@17f18c7',
  generatedAt: '2026-11-19T12:00:00+08:00', currentGit: 'remake@17f18c7', dirty: false,
  trackedFiles: 10, anchors: 77, anchoredCells: 66, totalCells: 97, anchorProblems: 0,
  changedFiles: [], affectedCells: []
};
const mdClean = CORE.markdown(Object.assign({}, report, { docStatus }));
const heads = (mdClean.match(/^## .+$/gm) || []).map(function (x) { return x.replace(/^## /, '').split('、')[0]; });
check('带基线时报告含七个小节', heads.length === 7, heads.length);
check('小节顺序为 一..七', heads.join('') === '一二三四五六七', heads.join(''));
check('无变更时头部写明依据一致', mdClean.indexOf('判定依据时效：与基线一致') > 0);
check('无变更时第七节给出结论行', mdClean.indexOf('结论：文档无变更，所有判定格的依据未动。') > 0);

const staleStatus = Object.assign({}, docStatus, {
  dirty: true,
  changedFiles: [{ path: 'Documents/tcs-remake-v2/02-核心基线/数值聚合器.md', state: 'changed', changed: ['2.1'], gone: [], added: [] }],
  affectedCells: [{ profile: 'dmg', unit: 'core.numeric', primitive: null, primitives: 4, states: ['section-changed'], anchors: [{ file: 'Documents/tcs-remake-v2/02-核心基线/数值聚合器.md', section: '2.1' }] }]
});
const mdStale = CORE.markdown(Object.assign({}, report, { docStatus: staleStatus }));
check('有变更时头部给出过期警告', mdStale.indexOf('判定依据已过期') > 0);
check('有变更时列出变更文件', mdStale.indexOf('| Documents/tcs-remake-v2/02-核心基线/数值聚合器.md | changed |') > 0);
check('有变更时列出待复核判定格', mdStale.indexOf('### 待复核判定格') > 0 && mdStale.indexOf('| dmg | core.numeric | 4 |') > 0);
check('有变更时声明工具边界', mdStale.indexOf('不能替人重判') > 0);
require('node:fs').writeFileSync(join(here, 'data', '_report-sample.md'), md, 'utf8');
console.log('  样例报告已写出 data/_report-sample.md');

console.log('\n' + (fail === 0 ? '全部通过' : fail + ' 项失败'));
process.exit(fail === 0 ? 0 : 1);
