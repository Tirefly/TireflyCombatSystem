// check-docs.mjs —— TCS 文档变更检测（Level 1 文件指纹 + Level 2 章节锚点）
//
// 解决什么问题：TCS V2 文档改了之后，data/model.js 里的判定会悄悄失效，而工具照旧出报告。
// 这个脚本回答：哪些文档变了、变在哪个章节、因此哪些判定格必须人工复核。
//
// 锚点从哪来：判定格的备注里本来就写着「文件名.md §N.M」（如「数值聚合器.md §2.1 唯一算法」）。
// 本脚本直接解析这些备注，不另存一份锚点表，避免两处维护、两处漂。
//
// 用法：
//   node check-docs.mjs                   比对当前工作区与基线，输出变更与受影响判定格
//   node check-docs.mjs --update          把当前工作区设为新基线（人工复核完判定后执行）
//   node check-docs.mjs --fail-on-change  检出变更时以退出码 1 结束（给 CI 用）
//   node check-docs.mjs --quiet           少打印，只写产物
//
// 产物：
//   data/doc-snapshot.json   基线快照：文件 sha256 + 每个章节的 own/tree 哈希
//   data/doc-snapshot.js     同一份数据的 window.TCM_DOCS 全局，供页面读取
//   data/_doc-review.md      仅在检出变更时写出：待复核清单
//
// 边界：本脚本只判断「文档变了、对应哪几格要重看」，不做重判。
// 判定语义是否还成立，只能人读文档后改 data/model.js。

import { readFileSync, writeFileSync, existsSync, readdirSync, mkdirSync, rmSync } from 'node:fs';
import { join, dirname, resolve, relative, sep } from 'node:path';
import { fileURLToPath } from 'node:url';
import { createHash } from 'node:crypto';
import { createRequire } from 'node:module';
import { execFileSync } from 'node:child_process';

const require = createRequire(import.meta.url);
const HERE = dirname(fileURLToPath(import.meta.url));
// 工具住在 <Project>/Plugins/TireflyCombatSystem/Tools/TcsCapabilityMatrix，
// 故上溯四层到项目根。挪动工具位置要同步改这里，或用 TCM_PROJECT_ROOT 覆盖。
const PROJECT_ROOT = process.env.TCM_PROJECT_ROOT
  ? resolve(process.env.TCM_PROJECT_ROOT)
  : resolve(HERE, '..', '..', '..', '..');

const SNAP_JSON = join(HERE, 'data', 'doc-snapshot.json');
const SNAP_JS = join(HERE, 'data', 'doc-snapshot.js');
const REVIEW_MD = join(HERE, 'data', '_doc-review.md');

const ARGV = process.argv.slice(2);
const UPDATE = ARGV.includes('--update');
const QUIET = ARGV.includes('--quiet');
const FAIL_ON_CHANGE = ARGV.includes('--fail-on-change');

globalThis.window = globalThis;
require(join(HERE, 'data', 'model.js'));
const MODEL = globalThis.TCM_MODEL;

// ── 小工具 ────────────────────────────────────────────────────
function sha16(s) {
  return createHash('sha256').update(s, 'utf8').digest('hex').slice(0, 16);
}
function normLines(lines) {
  const out = [];
  for (const l of lines) out.push(l.replace(/[ \t]+$/, ''));
  let a = 0, b = out.length;
  while (a < b && out[a] === '') a++;
  while (b > a && out[b - 1] === '') b--;
  return out.slice(a, b).join('\n');
}
function toRel(p) {
  return relative(PROJECT_ROOT, p).split(sep).join('/');
}
function pad(s, n) {
  s = String(s);
  while (s.length < n) s = s + ' ';
  return s;
}

// ── 章节解析 ──────────────────────────────────────────────────
// 章节 id 取标题开头的编号（「### 7.5 AttachMod」→「7.5」）；无编号则用标题全文。
// own  = 本节直接内容（到下一个任意标题为止）
// tree = 本节含子节的全部内容（到下一个同级或更高级标题为止）
// 两者分开存：子节变了只让 tree 变，能区分「这节自己改了」与「它的子节改了」。
function parseDoc(text) {
  const lines = text.split(/\r?\n/);
  const heads = [];
  for (let i = 0; i < lines.length; i++) {
    const m = /^(#{1,6})[ \t]+(.*)$/.exec(lines[i]);
    if (!m) continue;
    const title = m[2].trim();
    const num = /^([0-9]+(?:\.[0-9]+)*)/.exec(title);
    heads.push({
      level: m[1].length,
      title: title,
      id: num ? num[1] : 'T:' + title.replace(/[*]/g, '').split(String.fromCharCode(96)).join('').replace(/\s+/g, ' ').trim(),
      line: i
    });
  }
  const dup = {};
  for (const h of heads) {
    if (dup[h.id] == null) { dup[h.id] = 1; }
    else { dup[h.id]++; h.id = h.id + '~' + dup[h.id]; }
  }
  const sections = {};
  for (let k = 0; k < heads.length; k++) {
    const h = heads[k];
    let ownEnd = lines.length;
    if (k + 1 < heads.length) ownEnd = heads[k + 1].line;
    let treeEnd = lines.length;
    for (let j = k + 1; j < heads.length; j++) {
      if (heads[j].level <= h.level) { treeEnd = heads[j].line; break; }
    }
    sections[h.id] = {
      level: h.level,
      title: h.title,
      line: h.line + 1,
      own: sha16(normLines(lines.slice(h.line + 1, ownEnd))),
      tree: sha16(normLines(lines.slice(h.line + 1, treeEnd)))
    };
  }
  return { lines: lines.length, sections: sections };
}

// ── 文档索引 ──────────────────────────────────────────────────
const DOC_ROOT = join(PROJECT_ROOT, 'Plugins', 'TireflyCombatSystem', 'Documents');
const SCAN_DIRS = [join(DOC_ROOT, 'tcs-remake-v2'), join(DOC_ROOT, 'tcs-remake-v2-drafts')];

function walkMd(d, out) {
  if (!existsSync(d)) return out;
  for (const e of readdirSync(d, { withFileTypes: true })) {
    const p = join(d, e.name);
    if (e.isDirectory()) walkMd(p, out);
    else if (e.name.toLowerCase().endsWith('.md')) out.push(p);
  }
  return out;
}

const ALL_MD = [];
for (const d of SCAN_DIRS) walkMd(d, ALL_MD);

const BY_NAME = {};
for (const p of ALL_MD) {
  const name = p.split(sep).pop();
  if (!BY_NAME[name]) BY_NAME[name] = [];
  BY_NAME[name].push(toRel(p));
}

function resolveDoc(relOrName) {
  if (!relOrName) return null;
  const direct = join(PROJECT_ROOT, relOrName);
  if (existsSync(direct)) return toRel(direct);
  const cands = BY_NAME[relOrName];
  if (cands && cands.length === 1) return cands[0];
  if (cands && cands.length > 1) {
    const v2 = cands.filter(function (c) { return c.indexOf('tcs-remake-v2/') >= 0; });
    return v2.length ? v2[0] : cands[0];
  }
  return null;
}

// ── 锚点收集 ──────────────────────────────────────────────────
// 备注里的写法：「数值聚合器.md §2.1 唯一算法」「Attribute.md §7.1 … / §7.5 AttachMod」
// 规则：每个 § 向前找最近的「xxx.md」，找不到就继承该格的判定单元自己的文档。
function parseNoteAnchors(note) {
  const res = [];
  if (!note) return res;
  const re = /§[ \t]*([0-9]+(?:\.[0-9]+)*)/g;
  let m;
  while ((m = re.exec(note)) !== null) {
    const prefix = note.slice(0, m.index);
    const fre = /([^ \t（）、，；:：]+\.md)/g;
    let last = null, fm;
    while ((fm = fre.exec(prefix)) !== null) last = fm[1];
    res.push({ fileHint: last, section: m[1] });
  }
  return res;
}

const UNIT_BY_ID = {};
for (const u of MODEL.UNITS) UNIT_BY_ID[u.id] = u;

// 每条锚点记录它被谁引用：owner 是判定格（profile/primitive + 单元），或判定单元本身
const ANCHORS = [];
function addAnchor(owner, fileHint, section, unitId) {
  let file = resolveDoc(fileHint);
  if (!file && !fileHint) {
    const u = UNIT_BY_ID[unitId];
    file = u && u.doc ? resolveDoc(u.doc) : null;
  }
  ANCHORS.push({
    owner: owner,
    unit: unitId,
    file: file,
    fileHint: fileHint || null,
    section: section,
    resolved: !!file
  });
}

const CELLS = [];
for (const pid of Object.keys(MODEL.PROFILES)) {
  const prof = MODEL.PROFILES[pid];
  for (const unitId of Object.keys(prof)) {
    const cell = prof[unitId];
    const rec = { profile: pid, unit: unitId, status: cell[0], note: cell[1], primitive: null };
    CELLS.push(rec);
    for (const a of parseNoteAnchors(cell[1])) addAnchor(rec, a.fileHint, a.section, unitId);
  }
}
for (const p of MODEL.PRIMITIVES) {
  if (!p.cells) continue;
  for (const unitId of Object.keys(p.cells)) {
    const cell = p.cells[unitId];
    const rec = { profile: p.profile, unit: unitId, status: cell[0], note: cell[1], primitive: p.id };
    CELLS.push(rec);
    for (const a of parseNoteAnchors(cell[1])) addAnchor(rec, a.fileHint, a.section, unitId);
  }
}

// 判定单元自己的文档 = 文件级锚点（章节号为空）
for (const u of MODEL.UNITS) {
  if (!u.doc) continue;
  const f = resolveDoc(u.doc);
  ANCHORS.push({ owner: { profile: null, unit: u.id, primitive: null, kind: 'unit' }, unit: u.id, file: f, fileHint: u.doc, section: null, resolved: !!f });
}

// ── 跟踪哪些文件 ──────────────────────────────────────────────
const TRACKED = [];
function addTracked(file, reason) {
  if (!file) return;
  const hit = TRACKED.find(function (t) { return t.path === file; });
  if (hit) { if (hit.reasons.indexOf(reason) < 0) hit.reasons.push(reason); return; }
  TRACKED.push({ path: file, reasons: [reason] });
}
for (const u of MODEL.UNITS) if (u.doc) addTracked(resolveDoc(u.doc), '判定单元 ' + u.id);
addTracked(resolveDoc('tcs-remake-v2/INDEX.md'), '文档族索引');
for (const a of ANCHORS) if (a.file) addTracked(a.file, '判定格引用');

// ── git 信息 ──────────────────────────────────────────────────
function gitInfo() {
  const repo = join(PROJECT_ROOT, 'Plugins', 'TireflyCombatSystem');
  function run(args) {
    return execFileSync('git', ['-C', repo].concat(args), {
      encoding: 'utf8', stdio: ['ignore', 'pipe', 'ignore']
    }).trim();
  }
  try {
    const head = run(['rev-parse', 'HEAD']);
    const branch = run(['rev-parse', '--abbrev-ref', 'HEAD']);
    const dirty = run(['status', '--porcelain']);
    const files = dirty ? dirty.split(/\r?\n/).filter(Boolean) : [];
    return { repo: 'Plugins/TireflyCombatSystem', branch: branch, head: head, headShort: head.slice(0, 7), dirty: files.length > 0, dirtyFiles: files };
  } catch (e) {
    return null;
  }
}

// ── 读文档、建快照 ────────────────────────────────────────────
function buildSnapshot() {
  const docs = {};
  for (const t of TRACKED) {
    const abs = join(PROJECT_ROOT, t.path);
    if (!existsSync(abs)) continue;
    const text = readFileSync(abs, 'utf8');
    const parsed = parseDoc(text);
    docs[t.path] = {
      sha256: sha16(text),
      lines: parsed.lines,
      sections: parsed.sections
    };
  }
  return {
    schema: 1,
    generatedAt: new Date().toISOString(),
    projectRoot: PROJECT_ROOT,
    git: gitInfo(),
    docs: docs
  };
}

// ── 比对 ──────────────────────────────────────────────────────
function compare(baseline, current) {
  const fileStates = {};
  const allPaths = new Set(Object.keys(baseline.docs).concat(Object.keys(current.docs)));
  for (const p of allPaths) {
    const b = baseline.docs[p], c = current.docs[p];
    if (!b) { fileStates[p] = { state: 'added', changed: [], gone: [], added: [] }; continue; }
    if (!c) { fileStates[p] = { state: 'deleted', changed: [], gone: [], added: [] }; continue; }
    if (b.sha256 === c.sha256) { fileStates[p] = { state: 'same', changed: [], gone: [], added: [] }; continue; }
    const changed = [], gone = [], added = [];
    for (const id of Object.keys(b.sections)) {
      const bs = b.sections[id], cs = c.sections[id];
      if (!cs) { gone.push(id); continue; }
      if (bs.own !== cs.own) changed.push(id);
      else if (bs.tree !== cs.tree) changed.push(id + ':child');
    }
    for (const id of Object.keys(c.sections)) if (!b.sections[id]) added.push(id);
    fileStates[p] = { state: 'changed', changed: changed, gone: gone, added: added };
  }
  return fileStates;
}

// 把文件级变更落到锚点上
function anchorState(anchor, fileStates, baseline) {
  if (!anchor.resolved) return 'unresolved-file';
  const fs = fileStates[anchor.file];
  if (!fs) return 'untracked';
  if (fs.state === 'deleted') return 'file-deleted';
  if (fs.state === 'added') return 'file-added';
  if (fs.state === 'same') return 'ok';
  if (!anchor.section) return 'file-changed';
  const baseDoc = baseline.docs[anchor.file];
  if (!baseDoc) return 'no-baseline';
  if (!baseDoc.sections[anchor.section]) return 'section-cited-missing';
  if (fs.gone.indexOf(anchor.section) >= 0) return 'section-deleted';
  if (fs.changed.indexOf(anchor.section) >= 0) return 'section-changed';
  if (fs.changed.indexOf(anchor.section + ':child') >= 0) return 'section-child-changed';
  return 'file-changed';
}

// ── 主流程 ────────────────────────────────────────────────────
const current = buildSnapshot();
const mode = (!existsSync(SNAP_JSON) || UPDATE) ? 'baseline' : 'check';
const baseline = (mode === 'baseline') ? current : JSON.parse(readFileSync(SNAP_JSON, 'utf8'));

const fileStates = (mode === 'baseline') ? null : compare(baseline, current);

const hits = [];
if (mode === 'check') {
  for (const a of ANCHORS) {
    const st = anchorState(a, fileStates, baseline);
    if (st === 'ok') continue;
    hits.push({ anchor: a, state: st });
  }
}

const STATE_LABEL = {
  'file-changed': '文件变了（本节之外）',
  'section-changed': '本节内容变了',
  'section-child-changed': '本节子章节变了',
  'section-deleted': '本节已不存在',
  'section-cited-missing': '备注引用的章节在基线里就不存在',
  'file-deleted': '文件已删除',
  'file-added': '文件是新增的',
  'unresolved-file': '备注里的文件名无法定位',
  'untracked': '该文件未被跟踪',
  'no-baseline': '无基线'
};

const affectedCells = [];
const seenCell = new Set();
const affectedPrims = new Set();
for (const h of hits) {
  const o = h.anchor.owner;
  const key = o.profile + '|' + o.unit + '|' + (o.primitive || '');
  if (!seenCell.has(key)) {
    seenCell.add(key);
    const prims = [];
    if (o.primitive) prims.push(o.primitive);
    else {
      for (const p of MODEL.PRIMITIVES) if (p.profile === o.profile) prims.push(p.id);
    }
    affectedCells.push({ profile: o.profile, unit: o.unit, primitive: o.primitive, primitives: prims, states: [], anchors: [] });
  }
  const cell = affectedCells.find(function (c) { return (c.profile + '|' + c.unit + '|' + (c.primitive || '')) === key; });
  if (cell) {
    if (cell.states.indexOf(h.state) < 0) cell.states.push(h.state);
    cell.anchors.push({ file: h.anchor.file, section: h.anchor.section, state: h.state });
    for (const p of cell.primitives) affectedPrims.add(p);
  }
}

// ── 锚点体检 ──────────────────────────────────────────────────
// 与「文档变没变」无关的另一件事：备注里引用的章节，在基线里是否真的存在。
// 这能抓出两类错：改了章节号忘了改备注；备注写了个从来不存在的节。
const anchorProblems = [];
for (const a of ANCHORS) {
  if (!a.resolved) {
    anchorProblems.push({ a: a, problem: 'unresolved-file', label: '备注里的文件名无法定位' });
    continue;
  }
  const bd = baseline.docs[a.file];
  if (!bd) { anchorProblems.push({ a: a, problem: 'no-baseline', label: '该文件无基线' }); continue; }
  if (a.section && !bd.sections[a.section]) {
    anchorProblems.push({ a: a, problem: 'section-cited-missing', label: '引用的章节 §' + a.section + ' 不存在' });
  }
}

// ── 产物 ──────────────────────────────────────────────────────
const changedFiles = [];
if (fileStates) {
  for (const p of Object.keys(fileStates)) {
    if (fileStates[p].state !== 'same') changedFiles.push({ path: p, state: fileStates[p].state, detail: fileStates[p] });
  }
}

const OUT = {
  schema: 1,
  mode: mode,
  generatedAt: current.generatedAt,
  baselineAt: baseline.generatedAt,
  projectRoot: PROJECT_ROOT,
  git: current.git,
  baselineGit: baseline.git,
  tracked: TRACKED.map(function (t) { return { path: t.path, reasons: t.reasons }; }),
  changedFiles: changedFiles.map(function (f) {
    return { path: f.path, state: f.state, changed: f.detail.changed, gone: f.detail.gone, added: f.detail.added };
  }),
  hits: hits.map(function (h) {
    return {
      file: h.anchor.file,
      section: h.anchor.section,
      state: h.state,
      stateLabel: STATE_LABEL[h.state] || h.state,
      profile: h.anchor.owner.profile,
      unit: h.anchor.owner.unit,
      primitive: h.anchor.owner.primitive
    };
  }),
  affectedCells: affectedCells.map(function (c) {
    return { profile: c.profile, unit: c.unit, primitive: c.primitive, primitives: c.primitives, states: c.states, anchors: c.anchors };
  }),
  anchorProblems: anchorProblems.map(function (x) {
    return {
      file: x.a.file, fileHint: x.a.fileHint, section: x.a.section,
      problem: x.problem, label: x.label,
      profile: x.a.owner.profile, unit: x.a.owner.unit, primitive: x.a.owner.primitive
    };
  }),
  docs: Object.keys(baseline.docs).map(function (p) {
    return { path: p, lines: baseline.docs[p].lines, sections: Object.keys(baseline.docs[p].sections).length };
  }),
  cells: {
    total: CELLS.length,
    anchored: CELLS.filter(function (c) { return parseNoteAnchors(c.note).length > 0; }).length,
    unitDocCells: CELLS.filter(function (c) { const u = UNIT_BY_ID[c.unit]; return u && u.doc; }).length
  },
  summary: {
    trackedFiles: TRACKED.length,
    anchorProblems: anchorProblems.length,
    changedFiles: changedFiles.length,
    anchors: ANCHORS.length,
    anchorHits: hits.length,
    affectedCells: affectedCells.length,
    affectedPrimitives: affectedPrims.size
  }
};

mkdirSync(join(HERE, 'data'), { recursive: true });
writeFileSync(SNAP_JS, '/* 自动生成，勿手改；重跑 node check-docs.mjs 刷新 */\nwindow.TCM_DOCS = ' + JSON.stringify(OUT) + ';\n', 'utf8');
if (mode === 'baseline') writeFileSync(SNAP_JSON, JSON.stringify(baseline, null, 1), 'utf8');

// ── 待复核清单 ────────────────────────────────────────────────
if (mode === 'check' && (hits.length || changedFiles.length)) {
  const L = [];
  L.push('# TCS 文档变更 · 待复核清单');
  L.push('');
  L.push('基线：' + baseline.generatedAt + (baseline.git ? ' · git ' + baseline.git.branch + '@' + baseline.git.headShort : ''));
  L.push('检测：' + current.generatedAt + (current.git ? ' · git ' + current.git.branch + '@' + current.git.headShort + (current.git.dirty ? '（工作区有未提交改动）' : '') : ''));
  L.push('');
  L.push('## 变更文件');
  L.push('');
  if (!changedFiles.length) L.push('- 无');
  for (const f of changedFiles) {
    L.push('- [' + f.state + '] ' + f.path);
    if (f.state === 'changed') {
      if (f.detail.changed.length) L.push('  - 变更章节：' + f.detail.changed.join('、'));
      if (f.detail.gone.length) L.push('  - 消失章节：' + f.detail.gone.join('、'));
      if (f.detail.added.length) L.push('  - 新增章节：' + f.detail.added.join('、'));
    }
  }
  L.push('');
  L.push('## 待复核判定格（' + affectedCells.length + ' 格，涉及 ' + affectedPrims.size + ' 条原语）');
  L.push('');
  L.push('| 判定档 | 判定单元 | 原语数 | 状态 | 锚点 |');
  L.push('| --- | --- | --- | --- | --- |');
  for (const c of affectedCells) {
    const st = c.states.map(function (s) { return STATE_LABEL[s] || s; }).join('、');
    const an = c.anchors.map(function (a) { return (a.file || '?').split('/').pop() + (a.section ? ' #' + a.section : ''); }).join('；');
    L.push('| ' + (c.primitive || c.profile) + ' | ' + c.unit + ' | ' + c.primitives.length + ' | ' + st + ' | ' + an + ' |');
  }
  L.push('');
  L.push('复核完判定后执行：node check-docs.mjs --update（把当前文档设为新基线），再 node test-core.mjs 回归。');
  writeFileSync(REVIEW_MD, L.join('\n') + '\n', 'utf8');
} else if (existsSync(REVIEW_MD)) {
  // 没有变更时不能留着上一次的清单——它会被误当成当前待办
  rmSync(REVIEW_MD, { force: true });
}

// ── 打印 ──────────────────────────────────────────────────────
if (!QUIET) {
  const g = current.git;
  console.log('== TCS 文档变更检测 ==');
  console.log('  项目根：' + PROJECT_ROOT);
  if (g) console.log('  git：' + g.branch + '@' + g.headShort + (g.dirty ? '  工作区有 ' + g.dirtyFiles.length + ' 处未提交改动' : '  工作区干净'));
  console.log('  跟踪文档 ' + TRACKED.length + ' 个，锚点 ' + ANCHORS.length + ' 条，判定格记录 ' + CELLS.length + ' 个');

  for (const t of TRACKED) {
    if (!existsSync(join(PROJECT_ROOT, t.path))) console.log('  ! 跟踪的文件不存在：' + t.path);
  }
  const anchoredCells = CELLS.filter(function (c) { return parseNoteAnchors(c.note).length > 0; }).length;
  const unitDocCells = CELLS.filter(function (c) { const u = UNIT_BY_ID[c.unit]; return u && u.doc; }).length;
  console.log('  备注含章节锚点：' + anchoredCells + '/' + CELLS.length + ' 格（' + Math.round(anchoredCells / CELLS.length * 100) + '%）；判定单元有文档的格 ' + unitDocCells + ' 个');

  if (anchorProblems.length) {
    console.log('  ! 锚点体检：' + anchorProblems.length + ' 条有问题');
    for (const x of anchorProblems.slice(0, 20)) {
      const who = x.a.owner.primitive || x.a.owner.profile || 'unit';
      console.log('      ' + pad(who, 20) + pad(x.a.owner.unit, 16) + x.label + '  (' + (x.a.fileHint || x.a.file) + ')');
    }
  } else {
    console.log('  锚点体检：全部锚点都能定位到现存章节');
  }

  if (mode === 'baseline') {
    console.log('\n  已建立基线（当前工作区）。之后改动文档再跑本脚本即可检出变更。');
    console.log('  写出 data/doc-snapshot.json 与 data/doc-snapshot.js');
  } else {
    console.log('\n  基线时间：' + baseline.generatedAt + (baseline.git ? '  git ' + baseline.git.headShort : ''));
    if (!changedFiles.length && !hits.length) {
      console.log('\n  结论：文档无变更，所有判定格的依据未动。');
    } else {
      console.log('\n  变更文件 ' + changedFiles.length + ' 个：');
      for (const f of changedFiles) {
        console.log('    [' + f.state + '] ' + f.path);
        if (f.state === 'changed') {
          if (f.detail.changed.length) console.log('        变更章节：' + f.detail.changed.join('、'));
          if (f.detail.gone.length) console.log('        消失章节：' + f.detail.gone.join('、'));
          if (f.detail.added.length) console.log('        新增章节：' + f.detail.added.join('、'));
        }
      }
      console.log('\n  待复核判定格 ' + affectedCells.length + ' 个，涉及 ' + affectedPrims.size + ' 条原语');
      for (const c of affectedCells.slice(0, 30)) {
        const st = c.states.map(function (s) { return STATE_LABEL[s] || s; }).join('、');
        console.log('    ' + pad(c.primitive || c.profile, 20) + pad(c.unit, 16) + st);
      }
      if (affectedCells.length > 30) console.log('    ... 另有 ' + (affectedCells.length - 30) + ' 格，见 data/_doc-review.md');
      console.log('\n  已写出 data/_doc-review.md');
      console.log('  下一步：按清单重读对应章节 → 改 data/model.js 判定 → node check-docs.mjs --update');
    }
  }
}

if (FAIL_ON_CHANGE && mode === 'check' && (changedFiles.length || hits.length)) process.exit(1);
