/* TcsCapabilityMatrix · 判定核心
 * 纯逻辑，无 DOM 依赖；浏览器挂到 window.TCM_CORE，Node 走 module.exports。
 */
(function (root, factory) {
  var api = factory();
  if (typeof module === 'object' && module.exports) { module.exports = api; }
  root.TCM_CORE = api;
})(typeof globalThis !== 'undefined' ? globalThis : this, function () {
  'use strict';

  // 判定优先级：数字越小越严重（机制缺失 > 无依据 > 待裁决 > 缺文档 > 归宿主 > 已覆盖）
  var PRIORITY = ['mechanism', 'unknown', 'ruling', 'doc', 'host', 'covered'];

  var KIND_LABEL = {
    skill: '英雄技能',
    champion: '英雄属性',
    item: '装备',
    augment: '海克斯增幅',
    rune: '符文'
  };

  function model() {
    return (typeof globalThis !== 'undefined' ? globalThis : this).TCM_MODEL;
  }

  function rank(status) {
    var i = PRIORITY.indexOf(status);
    return i < 0 ? 99 : i;
  }

  function isBlocking(status) {
    return status !== 'covered' && status !== 'host';
  }

  function primitiveById(id) {
    var list = model().PRIMITIVES;
    for (var i = 0; i < list.length; i++) { if (list[i].id === id) { return list[i]; } }
    return null;
  }

  // 取某原语在某判定单元上的格；未挂载返回 null
  function cellOf(p, unitId) {
    if (!p) { return null; }
    if (p.cells && p.cells[unitId]) {
      return { unit: unitId, status: p.cells[unitId][0], note: p.cells[unitId][1] };
    }
    var prof = model().PROFILES[p.profile];
    if (prof && prof[unitId]) {
      return { unit: unitId, status: prof[unitId][0], note: prof[unitId][1] };
    }
    return null;
  }

  // 某原语的完整行，按 UNITS 顺序
  function cellsOf(p) {
    var units = model().UNITS, out = [];
    for (var i = 0; i < units.length; i++) {
      var c = cellOf(p, units[i].id);
      if (c) { out.push(c); }
    }
    return out;
  }

  // 某原语最严重的状态
  function worstStatus(p) {
    var cells = cellsOf(p), worst = 'covered';
    for (var i = 0; i < cells.length; i++) {
      if (rank(cells[i].status) < rank(worst)) { worst = cells[i].status; }
    }
    return worst;
  }

  function annotator() {
    return (typeof globalThis !== 'undefined' ? globalThis : this).TCM_ANNOTATE || null;
  }

  // 解析一条需求的原语集合：优先用数据里已带标注，否则走标注规则
  function resolvePrimitives(req) {
    if (req.primitives && req.primitives.length) {
      return { ids: req.primitives.slice(), source: 'riot-tags', note: null };
    }
    var ann = annotator();
    if (!ann) { return { ids: [], source: null, note: '标注模块未加载' }; }
    return ann.resolve(req);
  }

  // ── 判定单条需求 ────────────────────────────────────────
  function evaluateRequirement(req) {
    var resolved = resolvePrimitives(req);
    var primIds = resolved.ids;
    var rows = [], counts = { covered: 0, host: 0, doc: 0, ruling: 0, unknown: 0, mechanism: 0 };
    var blocking = [], missing = [], total = 0;

    for (var i = 0; i < primIds.length; i++) {
      var pid = primIds[i];
      var p = primitiveById(pid);
      if (!p) {
        missing.push(pid);
        rows.push({ id: pid, name: null, unknown: true, cells: [], worst: 'unknown' });
        counts.unknown++;
        total++;
        blocking.push({ primitive: pid, unit: '-', status: 'unknown', note: '原语词表中不存在该 id，需补词表或修正标注' });
        continue;
      }
      var cells = cellsOf(p);
      var worst = 'covered';
      for (var k = 0; k < cells.length; k++) {
        counts[cells[k].status]++;
        total++;
        if (rank(cells[k].status) < rank(worst)) { worst = cells[k].status; }
        if (isBlocking(cells[k].status)) {
          blocking.push({ primitive: pid, primitiveName: p.name, unit: cells[k].unit, status: cells[k].status, note: cells[k].note });
        }
      }
      rows.push({ id: pid, name: p.name, group: p.group, unknown: false, cells: cells, worst: worst });
    }

    var worstReq = 'covered';
    for (var b = 0; b < blocking.length; b++) {
      if (rank(blocking[b].status) < rank(worstReq)) { worstReq = blocking[b].status; }
    }

    var verdict;
    if (!primIds.length) { verdict = 'untagged'; }
    else if (worstReq === 'covered') { verdict = 'ready'; }
    else if (worstReq === 'host') { verdict = 'host_only'; }
    else { verdict = 'blocked'; }

    // 阻塞所在判定单元（去重）
    var unitSet = {};
    for (var u = 0; u < blocking.length; u++) { unitSet[blocking[u].unit] = true; }
    var blockingUnits = Object.keys(unitSet);

    return {
      id: req.id,
      kind: req.kind,
      version: req.version || null,
      name: req.name || req.id,
      champion: req.champion || null,
      slot: req.slot || null,
      rarity: req.rarity || null,
      primitives: rows,
      primSource: resolved.source,
      primNote: resolved.note,
      missingPrimitives: missing,
      counts: counts,
      totalCells: total,
      coveredCells: counts.covered + counts.host,
      coveragePct: total ? Math.round((counts.covered / total) * 100) : 0,
      blocking: blocking,
      blockingUnits: blockingUnits,
      worst: worstReq,
      verdict: verdict
    };
  }

  function evaluateAll(items) {
    var out = [];
    for (var i = 0; i < items.length; i++) { out.push(evaluateRequirement(items[i])); }
    return out;
  }

  // ── 能力矩阵 ────────────────────────────────────────────
  function buildMatrix() {
    var units = model().UNITS, prims = model().PRIMITIVES, rows = [];
    for (var i = 0; i < prims.length; i++) {
      var p = prims[i], cells = cellsOf(p);
      var map = {};
      for (var j = 0; j < cells.length; j++) { map[cells[j].unit] = cells[j]; }
      rows.push({ primitive: p, cells: map, worst: worstStatus(p), count: cells.length });
    }
    return { units: units, rows: rows };
  }

  // ── 判定单元汇总 ────────────────────────────────────────
  function unitSummary() {
    var units = model().UNITS, prims = model().PRIMITIVES, out = [];
    for (var i = 0; i < units.length; i++) {
      var u = units[i];
      var stat = { covered: 0, host: 0, doc: 0, ruling: 0, unknown: 0, mechanism: 0, total: 0 };
      for (var j = 0; j < prims.length; j++) {
        var c = cellOf(prims[j], u.id);
        if (!c) { continue; }
        stat[c.status]++;
        stat.total++;
      }
      out.push({ unit: u, stat: stat });
    }
    return out;
  }

  // ── 单元影响面：解决哪个单元收益最大 ────────────────────
  function unitImpact(results) {
    var units = model().UNITS, buckets = {};
    for (var i = 0; i < units.length; i++) {
      buckets[units[i].id] = { unit: units[i], blocks: 0, exclusive: 0, blockedPrimitives: {} };
    }
    for (var r = 0; r < results.length; r++) {
      var res = results[r];
      if (res.verdict !== 'blocked') { continue; }
      for (var b = 0; b < res.blockingUnits.length; b++) {
        var uid = res.blockingUnits[b];
        if (!buckets[uid]) { continue; }
        buckets[uid].blocks++;
        if (res.blockingUnits.length === 1) { buckets[uid].exclusive++; }
      }
      for (var k = 0; k < res.blocking.length; k++) {
        var uid2 = res.blocking[k].unit;
        if (buckets[uid2]) { buckets[uid2].blockedPrimitives[res.blocking[k].primitive] = true; }
      }
    }
    var out = [];
    for (var key in buckets) {
      if (!Object.prototype.hasOwnProperty.call(buckets, key)) { continue; }
      var it = buckets[key];
      out.push({
        unit: it.unit,
        blocks: it.blocks,
        exclusive: it.exclusive,
        primitiveCount: Object.keys(it.blockedPrimitives).length,
        primitives: Object.keys(it.blockedPrimitives)
      });
    }
    out.sort(function (a, b) { return b.blocks - a.blocks; });
    return out;
  }

  // ── 需求级统计 ──────────────────────────────────────────
  function summarize(results) {
    var s = {
      total: results.length,
      ready: 0, host_only: 0, blocked: 0, untagged: 0,
      byKind: {},
      byWorst: { covered: 0, host: 0, doc: 0, ruling: 0, unknown: 0, mechanism: 0 },
      totalCells: 0, coveredCells: 0
    };
    for (var i = 0; i < results.length; i++) {
      var r = results[i];
      s[r.verdict] = (s[r.verdict] || 0) + 1;
      var k = r.kind || 'unknown';
      if (!s.byKind[k]) { s.byKind[k] = { total: 0, ready: 0, host_only: 0, blocked: 0, untagged: 0 }; }
      s.byKind[k].total++;
      s.byKind[k][r.verdict] = (s.byKind[k][r.verdict] || 0) + 1;
      if (r.verdict === 'blocked') { s.byWorst[r.worst]++; }
      s.totalCells += r.totalCells;
      s.coveredCells += r.coveredCells;
    }
    s.coveragePct = s.totalCells ? Math.round((s.coveredCells / s.totalCells) * 100) : 0;
    return s;
  }

  // ── 版本差分（海克斯 16.20 vs 16.22 一类）───────────────
  function diffResults(oldList, newList, oldResults, newResults) {
    var oldIds = {}, newIds = {};
    for (var i = 0; i < oldList.length; i++) { oldIds[oldList[i].id] = oldResults[i]; }
    for (var j = 0; j < newList.length; j++) { newIds[newList[j].id] = newResults[j]; }
    var added = [], removed = [], changed = [];
    for (var ni in newIds) {
      if (!Object.prototype.hasOwnProperty.call(newIds, ni)) { continue; }
      if (!oldIds[ni]) { added.push(newIds[ni]); }
      else {
        var a = oldIds[ni], b = newIds[ni];
        if (a.worst !== b.worst || a.coveragePct !== b.coveragePct) {
          changed.push({ id: ni, name: b.name, from: a.worst, to: b.worst, fromPct: a.coveragePct, toPct: b.coveragePct });
        }
      }
    }
    for (var oi in oldIds) {
      if (!Object.prototype.hasOwnProperty.call(oldIds, oi)) { continue; }
      if (!newIds[oi]) { removed.push(oldIds[oi]); }
    }
    return { added: added, removed: removed, changed: changed };
  }

  // ── Markdown 报告 ───────────────────────────────────────
  function markdown(report) {
    var L = [];
    var s = report.summary;
    var now = new Date(report.generatedAt);
    var stamp = now.getFullYear() + '-' + pad(now.getMonth() + 1) + '-' + pad(now.getDate()) + ' ' + pad(now.getHours()) + ':' + pad(now.getMinutes());

    L.push('# TCS 能力覆盖度报告');
    L.push('');
    L.push('- 生成时间：' + stamp);
    L.push('- 判定基准：TCS V2 文档族 ' + report.docBaseline);
    L.push('- 判定知识库：' + report.model.primitives + ' 条原语 / ' + report.model.units + ' 个判定单元 / ' + report.model.profiles + ' 个判定档');
    L.push('- 收录需求：' + s.total + ' 条（' + report.corpora.map(function (c) { return c.label + ' ' + c.count; }).join('、') + '）');
    if (report.docStatus) {
      var ds0 = report.docStatus;
      if (ds0.changedFiles.length || ds0.affectedCells.length) {
        L.push('- **判定依据已过期**：TCS 文档在基线之后有 ' + ds0.changedFiles.length + ' 个文件被改，' +
               ds0.affectedCells.length + ' 个判定格待复核。本报告结论建立在旧依据上，详见第七节。');
      } else if (ds0.baselineAt) {
        L.push('- 判定依据时效：与基线一致（基线 ' + String(ds0.baselineAt).slice(0, 10) + (ds0.baselineGit ? '，' + ds0.baselineGit : '') + '）');
      }
    }
    L.push('');

    L.push('## 一、总览');
    L.push('');
    L.push('| 裁决 | 条数 | 占比 |');
    L.push('| --- | ---: | ---: |');
    L.push('| 可实现（全部已覆盖） | ' + s.ready + ' | ' + pct(s.ready, s.total) + ' |');
    L.push('| 需宿主配合 | ' + s.host_only + ' | ' + pct(s.host_only, s.total) + ' |');
    L.push('| 受阻 | ' + s.blocked + ' | ' + pct(s.blocked, s.total) + ' |');
    if (s.untagged) { L.push('| 未标注原语 | ' + s.untagged + ' | ' + pct(s.untagged, s.total) + ' |'); }
    L.push('');
    L.push('原语格覆盖：' + s.coveredCells + ' / ' + s.totalCells + '（' + s.coveragePct + '%）。');
    L.push('');
    L.push('受阻需求按最严重缺口分布：缺机制 ' + s.byWorst.mechanism + '、缺依据 ' + s.byWorst.unknown + '、需补裁决 ' + s.byWorst.ruling + '、缺文档 ' + s.byWorst.doc + '。');
    L.push('');

    L.push('### 按数据类别');
    L.push('');
    L.push('| 类别 | 总数 | 可实现 | 需宿主 | 受阻 |');
    L.push('| --- | ---: | ---: | ---: | ---: |');
    for (var k in s.byKind) {
      if (!Object.prototype.hasOwnProperty.call(s.byKind, k)) { continue; }
      var b = s.byKind[k];
      L.push('| ' + (KIND_LABEL[k] || k) + ' | ' + b.total + ' | ' + (b.ready || 0) + ' | ' + (b.host_only || 0) + ' | ' + (b.blocked || 0) + ' |');
    }
    L.push('');

    L.push('## 二、判定单元缺口');
    L.push('');
    L.push('「阻塞需求」＝有该单元缺口的需求条数；「独占阻塞」＝仅靠该单元缺口就卡住的需求条数，补完它即可解锁。');
    L.push('');
    L.push('| 判定单元 | 文档状态 | 已覆盖/判定数 | 阻塞需求 | 独占阻塞 |');
    L.push('| --- | --- | ---: | ---: | ---: |');
    // impact 按阻塞数排序，units 按固定序，必须按 id 对齐而不能按下标
    var impById = {};
    for (var q = 0; q < report.impact.length; q++) { impById[report.impact[q].unit.id] = report.impact[q]; }
    for (var i = 0; i < report.units.length; i++) {
      var u = report.units[i];
      var imp = impById[u.unit.id] || { blocks: 0, exclusive: 0 };
      L.push('| ' + u.unit.name + ' | ' + docStatusLabel(u.unit.status) + ' | ' + u.stat.covered + '/' + u.stat.total + ' | ' + imp.blocks + ' | ' + imp.exclusive + ' |');
    }
    L.push('');

    L.push('## 三、按单元的最该补齐项');
    L.push('');
    for (var m = 0; m < report.unitGaps.length; m++) {
      var g = report.unitGaps[m];
      if (!g.primitives.length) { continue; }
      L.push('### ' + g.unit.name + '（' + docStatusLabel(g.unit.status) + '）');
      L.push('');
      L.push('| 原语 | 状态 | 依据 |');
      L.push('| --- | --- | --- |');
      for (var n = 0; n < g.primitives.length; n++) {
        var gp = g.primitives[n];
        L.push('| ' + gp.name + '（' + gp.id + '） | ' + statusLabel(gp.status) + ' | ' + esc(gp.note) + ' |');
      }
      L.push('');
    }

    L.push('## 四、需求明细');
    L.push('');
    var byKind = {};
    for (var q = 0; q < report.results.length; q++) {
      var r = report.results[q];
      var kk = r.kind || 'unknown';
      if (!byKind[kk]) { byKind[kk] = []; }
      byKind[kk].push(r);
    }
    for (var kk2 in byKind) {
      if (!Object.prototype.hasOwnProperty.call(byKind, kk2)) { continue; }
      L.push('### ' + (KIND_LABEL[kk2] || kk2));
      L.push('');
      L.push('| 名称 | 裁决 | 覆盖 | 缺口 |');
      L.push('| --- | --- | ---: | --- |');
      var arr = byKind[kk2];
      for (var a = 0; a < arr.length; a++) {
        var it = arr[a];
        L.push('| ' + esc(it.name) + ' | ' + verdictLabel(it.verdict) + ' | ' + it.coveragePct + '% | ' + esc(gapText(it)) + ' |');
      }
      L.push('');
    }

    L.push('## 五、方法与局限');
    L.push('');
    L.push('1. 判定的是**表达力**：某条需求用到的每个效果原语，TCS 现有文档能否承接。不是运行时验证——插件当前无可编译代码。');
    L.push('2. 需求到原语的映射来自 Riot 官方技能标签（mSpellTags）与伤害公式节点（mSpellCalculations），不来自人工阅读技能文案。技能与 CDragon 对象的匹配分四类：exact 叶名 536 条、numeric 数值数组 178 条、passive 被动名 129 条、unresolved 未解析 22 条；unresolved 的部分退回按文案推断，精度弱于前三类。');
    L.push('3. 「缺依据」不等于「框架不支持」，只表示该域尚无文档也无草稿，无法判定。');
    L.push('4. 「归宿主」不等于缺口：按 01-定位与判据.md §5.6，需要具体宿主词汇的能力由宿主实现，TCS 不提供也不需要提供。');
    L.push('5. LOL / DNF 是**表达力对标样本**，不是 TCS 的消费者。本报告的结论不能直接作为「TCS 该加什么机制」的排期依据。');
    L.push('');
    L.push('## 六、数据来源');
    L.push('');
    for (var d = 0; d < report.corpora.length; d++) {
      var c2 = report.corpora[d];
      L.push('- ' + c2.label + '：' + c2.count + ' 条，版本 ' + c2.version + '，来源 ' + c2.source);
    }
    L.push('');

    if (report.docStatus) {
      var ds2 = report.docStatus;
      L.push('## 七、判定依据的时效性');
      L.push('');
      L.push('判定格的依据写在 data/model.js 的备注里（形如「数值聚合器.md §2.1」），check-docs.mjs 把这些章节号解析成锚点，并记录每个章节的内容哈希。基线设好之后，文档一改就能落到「哪一格」。');
      L.push('');
      L.push('- 文档基线：' + (ds2.baselineAt || '—') + (ds2.baselineGit ? '，' + ds2.baselineGit : ''));
      L.push('- 检测时间：' + (ds2.generatedAt || '—') + (ds2.currentGit ? '，' + ds2.currentGit + (ds2.dirty ? '（工作区有未提交改动）' : '') : ''));
      L.push('- 跟踪文档 ' + ds2.trackedFiles + ' 篇，锚点 ' + ds2.anchors + ' 条，有章节锚点的判定格 ' + ds2.anchoredCells + '/' + ds2.totalCells);
      if (!ds2.changedFiles.length && !ds2.affectedCells.length) {
        L.push('- 结论：文档无变更，所有判定格的依据未动。');
      }
      if (ds2.changedFiles.length) {
        L.push('');
        L.push('### 变更文件');
        L.push('');
        L.push('| 文件 | 状态 | 变更章节 | 消失 | 新增 |');
        L.push('| --- | --- | --- | --- | --- |');
        for (var df = 0; df < ds2.changedFiles.length; df++) {
          var cf = ds2.changedFiles[df];
          L.push('| ' + esc(cf.path) + ' | ' + cf.state + ' | ' + esc(cf.changed.join('、') || '—') +
                 ' | ' + esc(cf.gone.join('、') || '—') + ' | ' + esc(cf.added.join('、') || '—') + ' |');
        }
      }
      if (ds2.affectedCells.length) {
        L.push('');
        L.push('### 待复核判定格');
        L.push('');
        L.push('| 判定档 / 原语 | 判定单元 | 波及原语 | 状态 | 锚点 |');
        L.push('| --- | --- | ---: | --- | --- |');
        for (var dc = 0; dc < ds2.affectedCells.length; dc++) {
          var cc = ds2.affectedCells[dc];
          L.push('| ' + (cc.primitive || cc.profile) + ' | ' + cc.unit + ' | ' + cc.primitives +
                 ' | ' + esc(cc.states.join('、')) + ' | ' + esc(cc.anchors.join('；') || '—') + ' |');
        }
        L.push('');
        L.push('**这份清单只能缩小范围，不能替人重判**：判定语义是否仍然成立，必须人读改动后的章节，再改 data/model.js，然后重跑本工具。');
      }
      L.push('');
    }

    return L.join('\n');
  }

  function pad(n) { return n < 10 ? '0' + n : String(n); }
  function pct(a, b) { return b ? (Math.round((a / b) * 1000) / 10) + '%' : '-'; }
  function esc(s) { return String(s == null ? '' : s).replace(/\|/g, '\\|').replace(/\n/g, ' '); }

  function docStatusLabel(s) {
    return s === 'formal' ? '正式文档' : (s === 'draft' ? '草稿' : '无文档');
  }
  function statusLabel(s) {
    return model().STATUS[s] ? model().STATUS[s].label : s;
  }
  function verdictLabel(v) {
    return v === 'ready' ? '可实现' : (v === 'host_only' ? '需宿主配合' : (v === 'untagged' ? '未标注' : '受阻'));
  }
  function gapText(r) {
    var seen = {}, out = [];
    for (var i = 0; i < r.blocking.length; i++) {
      var b = r.blocking[i];
      var key = b.primitive + '@' + b.unit;
      if (seen[key]) { continue; }
      seen[key] = true;
      var unitName = unitNameOf(b.unit);
      out.push((b.primitiveName || b.primitive) + '→' + unitName + '(' + statusLabel(b.status) + ')');
    }
    return out.slice(0, 6).join('；') + (out.length > 6 ? ' 等 ' + out.length + ' 项' : '');
  }
  function unitNameOf(id) {
    var units = model().UNITS;
    for (var i = 0; i < units.length; i++) { if (units[i].id === id) { return units[i].name; } }
    return id;
  }

  // 按单元聚合出「该单元下哪些原语不达标」
  function unitGaps(results) {
    var units = model().UNITS, out = [];
    for (var i = 0; i < units.length; i++) {
      var u = units[i], seen = {}, list = [];
      for (var r = 0; r < results.length; r++) {
        var bl = results[r].blocking;
        for (var b = 0; b < bl.length; b++) {
          if (bl[b].unit !== u.id) { continue; }
          if (seen[bl[b].primitive]) { continue; }
          seen[bl[b].primitive] = true;
          list.push({ id: bl[b].primitive, name: bl[b].primitiveName || bl[b].primitive, status: bl[b].status, note: bl[b].note });
        }
      }
      list.sort(function (a, b) { return rank(a.status) - rank(b.status); });
      out.push({ unit: u, primitives: list });
    }
    return out;
  }

  return {
    PRIORITY: PRIORITY,
    KIND_LABEL: KIND_LABEL,
    rank: rank,
    isBlocking: isBlocking,
    primitiveById: primitiveById,
    cellOf: cellOf,
    cellsOf: cellsOf,
    worstStatus: worstStatus,
    resolvePrimitives: resolvePrimitives,
    annotator: annotator,
    evaluateRequirement: evaluateRequirement,
    evaluateAll: evaluateAll,
    buildMatrix: buildMatrix,
    unitSummary: unitSummary,
    unitImpact: unitImpact,
    unitGaps: unitGaps,
    summarize: summarize,
    diffResults: diffResults,
    markdown: markdown,
    statusLabel: statusLabel,
    docStatusLabel: docStatusLabel,
    verdictLabel: verdictLabel,
    gapText: gapText,
    unitNameOf: unitNameOf
  };
});
