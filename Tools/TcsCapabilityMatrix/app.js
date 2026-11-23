/* TcsCapabilityMatrix · 界面层 */
(function () {
  'use strict';

  var MODEL = window.TCM_MODEL;
  var CORE = window.TCM_CORE;
  var DATA = window.TCM_DATA;
  var DOCS = window.TCM_DOCS || null;
  var STATUS = MODEL.STATUS;

  // 文档漂移索引：「原语id|单元id」→ 触发的锚点状态集合
  var DRIFT = (function () {
    var m = {};
    if (!DOCS || !DOCS.affectedCells) { return m; }
    DOCS.affectedCells.forEach(function (c) {
      var st = c.states || [];
      if (!c.primitive) {
        MODEL.PRIMITIVES.forEach(function (p) {
          if (p.profile === c.profile) {
            var k = p.id + '|' + c.unit;
            m[k] = (m[k] || []).concat(st);
          }
        });
      } else {
        var k2 = c.primitive + '|' + c.unit;
        m[k2] = (m[k2] || []).concat(st);
      }
    });
    return m;
  })();
  function driftOf(primId, unitId) { return DRIFT[primId + '|' + unitId] || null; }

  // 锚点状态的中文标签（优先后端给的 stateLabel）
  var DOC_STATE_LABEL = (function () {
    var m = {
      'file-changed': '文件变了（本节之外）',
      'section-changed': '本节内容变了',
      'section-child-changed': '本节子章节变了',
      'section-deleted': '本节已不存在',
      'section-cited-missing': '引用的章节不存在',
      'file-deleted': '文件已删除',
      'file-added': '文件是新增的',
      'unresolved-file': '文件名无法定位',
      'untracked': '未被跟踪',
      'no-baseline': '无基线',
      'ok': '未变'
    };
    if (DOCS && DOCS.hits) {
      DOCS.hits.forEach(function (h) { if (h.state && h.stateLabel) { m[h.state] = h.stateLabel; } });
    }
    return m;
  })();

  var STATE = {
    view: 'matrix',
    filter: { kind: '', verdict: '', source: '', q: '' },
    page: 0,
    perPage: 50,
    expanded: {}
  };

  var ALL = [];      // 全部需求条目，带 _corpus 与 _r（判定结果）
  var REPORTS = {};  // corpus key -> 判定结果数组

  // ── 工具 ───────────────────────────────────────────────
  function $(id) { return document.getElementById(id); }
  function esc(s) {
    return String(s == null ? '' : s)
      .replace(/&/g, '&amp;').replace(/</g, '&lt;').replace(/>/g, '&gt;')
      .replace(/"/g, '&quot;');
  }
  function pct(a, b) { return b ? (Math.round((a / b) * 1000) / 10) + '%' : '—'; }

  // ── 初始化 ─────────────────────────────────────────────
  function init() {
    var items = [];
    DATA.manifest.forEach(function (m) {
      var doc = DATA.corpora[m.key];
      var list = doc.items || [];
      list.forEach(function (it) { it._corpus = m.key; items.push(it); });
    });
    ALL = items;

    var results = CORE.evaluateAll(items);
    for (var i = 0; i < items.length; i++) { items[i]._r = results[i]; }

    DATA.manifest.forEach(function (m) {
      REPORTS[m.key] = ALL.filter(function (x) { return x._corpus === m.key; }).map(function (x) { return x._r; });
    });

    $('meta').textContent = MODEL.PRIMITIVES.length + ' 条原语 · ' + MODEL.UNITS.length + ' 个判定单元 · ' +
      ALL.length + ' 条需求 · TCS V2 文档族';

    renderLegend();
    renderMatrix();
    renderFilters();
    renderRequirements();
    renderGaps();
    renderDiff();
    renderDocs();
    renderAbout();
    renderBanner();
    bindTabs();
  }

  // 视图切换：支持 #view 深链接，便于分享与截图
  function activateView(v) {
    if (!document.getElementById('view-' + v)) { return; }
    STATE.view = v;
    var tabs = document.querySelectorAll('.tab');
    for (var k = 0; k < tabs.length; k++) {
      tabs[k].classList.toggle('active', tabs[k].getAttribute('data-view') === v);
    }
    var views = document.querySelectorAll('.view');
    for (var j = 0; j < views.length; j++) {
      views[j].classList.toggle('active', views[j].id === 'view-' + v);
    }
    window.scrollTo(0, 0);
  }

  function bindTabs() {
    var tabs = document.querySelectorAll('.tab');
    for (var i = 0; i < tabs.length; i++) {
      tabs[i].addEventListener('click', function () {
        activateView(this.getAttribute('data-view'));
        if (location.hash !== '#' + this.getAttribute('data-view')) {
          location.hash = this.getAttribute('data-view');
        }
      });
    }
    window.addEventListener('hashchange', function () {
      activateView((location.hash || '').replace('#', 'matrix'));
    });
    var initial = (location.hash || '').replace('#', '');
    if (initial) { activateView(initial); }
  }

  // ── 图例 ───────────────────────────────────────────────
  function renderLegend() {
    var order = ['covered', 'ruling', 'doc', 'mechanism', 'host', 'unknown'];
    var html = '';
    for (var i = 0; i < order.length; i++) {
      var s = STATUS[order[i]];
      html += '<span class="item s-' + s.id + '"><span class="swatch"></span>' + esc(s.label) +
              '<span class="hint" style="margin-left:2px">' + esc(s.desc) + '</span></span>';
    }
    $('legend').innerHTML = html;
  }

  // ── 能力矩阵 ───────────────────────────────────────────
  function renderMatrix() {
    var units = MODEL.UNITS;
    var prims = MODEL.PRIMITIVES;

    var head = '<thead><tr><th class="rowhead">效果原语</th>';
    for (var u = 0; u < units.length; u++) {
      head += '<th class="unit-h" title="' + esc(units[u].covers) + '">' + esc(units[u].name) +
              '<span class="st">' + esc(CORE.docStatusLabel(units[u].status)) + '</span></th>';
    }
    head += '<th class="unit-h" title="该原语最严重的判定">最差</th></tr></thead>';

    var groups = MODEL.GROUPS;
    var body = '<tbody>';
    for (var g = 0; g < groups.length; g++) {
      var grp = groups[g];
      var rows = prims.filter(function (p) { return p.group === grp.id; });
      if (!rows.length) { continue; }
      body += '<tr class="group-row"><td colspan="' + (units.length + 2) + '">' + esc(grp.name) +
              '　<span class="hint">' + rows.length + ' 条</span></td></tr>';
      for (var r = 0; r < rows.length; r++) {
        var p = rows[r];
        body += '<tr data-prim="' + esc(p.id) + '">';
        body += '<td class="rowhead pname">' + esc(p.name) + '<span class="id">' + esc(p.id) + '</span></td>';
        for (var u2 = 0; u2 < units.length; u2++) {
          var c = CORE.cellOf(p, units[u2].id);
          if (!c) {
            body += '<td class="cell na">—</td>';
          } else {
            var st = STATUS[c.status];
            var dr = driftOf(p.id, units[u2].id);
            body += '<td class="cell ' + c.status + (dr ? ' drift' : '') + '" data-prim="' + esc(p.id) + '" data-unit="' + esc(units[u2].id) + '" title="' +
                    esc(p.name + ' × ' + units[u2].name + ' → ' + st.label + '：' + c.note) +
                    (dr ? esc('\n⚠ 文档已变更，依据待复核：' + dr.join('、')) : '') + '">' + st.mark + '</td>';
          }
        }
        var worst = STATUS[CORE.worstStatus(p)];
        body += '<td class="cell ' + worst.id + '" style="cursor:default">' + worst.mark + '</td>';
        body += '</tr>';
      }
    }
    body += '</tbody>';

    var table = $('matrixTable');
    table.innerHTML = head + body;

    var cells = table.querySelectorAll('td.cell[data-unit]');
    for (var i = 0; i < cells.length; i++) {
      cells[i].addEventListener('click', function () { showCellInfo(this); });
    }
  }

  function showCellInfo(td) {
    var primId = td.getAttribute('data-prim');
    var unitId = td.getAttribute('data-unit');
    var p = CORE.primitiveById(primId);
    var c = CORE.cellOf(p, unitId);
    var u = null;
    for (var i = 0; i < MODEL.UNITS.length; i++) { if (MODEL.UNITS[i].id === unitId) { u = MODEL.UNITS[i]; } }
    var st = STATUS[c.status];
    var bar = $('cellInfo');
    if (!bar) {
      bar = document.createElement('div');
      bar.id = 'cellInfo';
      bar.className = 'note';
      bar.style.marginTop = '12px';
      var wrap = document.querySelector('#view-matrix .matrix-wrap');
      wrap.parentNode.insertBefore(bar, wrap);
    }
    bar.className = 'note' + (c.status === 'covered' ? '' : ' warn');
    bar.innerHTML = '<b>' + esc(p.name) + '</b> <span class="mono">' + esc(p.id) + '</span>' +
      ' × <b>' + esc(u ? u.name : unitId) + '</b> → <span class="chip ' + (st.gap ? 'blocked' : 'ready') + '">' + esc(st.label) + '</span>' +
      '<br>' + esc(c.note) +
      (u && u.doc ? '<br><span class="hint">文档：' + esc(u.doc) + '</span>' : '<br><span class="hint">该单元无文档</span>');
    bar.scrollIntoView({ block: 'nearest' });
  }

  // ── 需求筛选器 ─────────────────────────────────────────
  function renderFilters() {
    var kinds = [];
    var seen = {};
    ALL.forEach(function (x) {
      if (!seen[x.kind]) { seen[x.kind] = true; kinds.push(x.kind); }
    });
    var html = '';
    html += '<label>类别</label><select id="fKind"><option value="">全部</option>';
    kinds.forEach(function (k) {
      html += '<option value="' + esc(k) + '">' + esc(CORE.KIND_LABEL[k] || k) + '</option>';
    });
    html += '</select>';

    html += '<label>裁决</label><select id="fVerdict">' +
      '<option value="">全部</option>' +
      '<option value="ready">可实现</option>' +
      '<option value="host_only">需宿主配合</option>' +
      '<option value="blocked">受阻</option>' +
      '<option value="untagged">未标注</option></select>';

    html += '<label>原语来源</label><select id="fSource">' +
      '<option value="">全部</option>' +
      '<option value="riot-tags">Riot 官方标签</option>' +
      '<option value="semantics">数据商语义标签</option>' +
      '<option value="text">文案推断</option>' +
      '<option value="kind-rule">类别规则</option></select>';

    html += '<input type="search" id="fQ" placeholder="搜索名称 / id / 英雄…">';
    html += '<button class="btn" id="fReset">重置</button>';
    $('filters').innerHTML = html;

    $('fKind').addEventListener('change', function () { STATE.filter.kind = this.value; STATE.page = 0; renderRequirements(); });
    $('fVerdict').addEventListener('change', function () { STATE.filter.verdict = this.value; STATE.page = 0; renderRequirements(); });
    $('fSource').addEventListener('change', function () { STATE.filter.source = this.value; STATE.page = 0; renderRequirements(); });
    $('fReset').addEventListener('click', function () {
      STATE.filter = { kind: '', verdict: '', source: '', q: '' };
      STATE.page = 0;
      $('fKind').value = ''; $('fVerdict').value = ''; $('fSource').value = ''; $('fQ').value = '';
      renderRequirements();
    });
    var timer = null;
    $('fQ').addEventListener('input', function () {
      var v = this.value.trim();
      if (timer) { clearTimeout(timer); }
      timer = setTimeout(function () { STATE.filter.q = v; STATE.page = 0; renderRequirements(); }, 180);
    });
  }

  function filtered() {
    var f = STATE.filter;
    var q = f.q.toLowerCase();
    return ALL.filter(function (x) {
      var r = x._r;
      if (f.kind && x.kind !== f.kind) { return false; }
      if (f.verdict && r.verdict !== f.verdict) { return false; }
      if (f.source && (r.primSource || '') !== f.source) { return false; }
      if (q) {
        var hay = (x.name + ' ' + x.id + ' ' + (x.champion ? x.champion.name + ' ' + x.champion.title : '') + ' ' + (r.primitives || []).map(function (p) { return p.id; }).join(' ')).toLowerCase();
        if (hay.indexOf(q) < 0) { return false; }
      }
      return true;
    });
  }

  // ── 需求表 ─────────────────────────────────────────────
  function renderRequirements() {
    var list = filtered();
    var sum = CORE.summarize(list.map(function (x) { return x._r; }));

    $('reqSummary').innerHTML =
      card('筛选后需求', sum.total, '全部 ' + ALL.length + ' 条', 'total') +
      card('可实现', sum.ready, pct(sum.ready, sum.total), 'ok') +
      card('需宿主配合', sum.host_only, pct(sum.host_only, sum.total), 'info') +
      card('受阻', sum.blocked, pct(sum.blocked, sum.total), 'bad') +
      card('原语格覆盖', sum.coveragePct + '%', sum.coveredCells + ' / ' + sum.totalCells, 'warn');

    var start = STATE.page * STATE.perPage;
    var page = list.slice(start, start + STATE.perPage);

    var html = '<thead><tr>' +
      '<th>名称</th><th>类别</th><th>裁决</th><th class="num">覆盖</th><th>卡住的缺口</th><th>原语来源</th>' +
      '</tr></thead><tbody>';

    page.forEach(function (x, idx) {
      var r = x._r;
      var gi = start + idx;
      var open = !!STATE.expanded[r.id];
      html += '<tr class="expandable" data-idx="' + gi + '" data-id="' + esc(r.id) + '">';
      html += '<td class="name" title="' + esc(r.name + ' / ' + r.id) + '">' + esc(r.name) +
              (x.champion ? ' <span class="hint">' + esc(x.champion.name) + (x.slot ? ' ' + esc(x.slot) : '') + '</span>' : '') + '</td>';
      html += '<td>' + esc(CORE.KIND_LABEL[r.kind] || r.kind) + '</td>';
      html += '<td><span class="chip ' + r.verdict + '">' + esc(CORE.verdictLabel(r.verdict)) + '</span></td>';
      html += '<td class="num">' + r.coveragePct + '%</td>';
      html += '<td class="gap">' + (r.blocking.length ? esc(CORE.gapText(r)) : '<span class="hint">无</span>') + '</td>';
      html += '<td class="hint">' + esc((DATA.SOURCE_LABEL && DATA.SOURCE_LABEL[r.primSource]) || r.primSource || '—') + '</td>';
      html += '</tr>';
      if (open) {
        html += '<tr class="detail"><td colspan="6">' + detailOf(x) + '</td></tr>';
      }
    });
    html += '</tbody>';
    $('reqTable').innerHTML = html;

    var rows = $('reqTable').querySelectorAll('tr.expandable');
    for (var i = 0; i < rows.length; i++) {
      rows[i].addEventListener('click', function () {
        var id = this.getAttribute('data-id');
        STATE.expanded[id] = !STATE.expanded[id];
        renderRequirements();
      });
    }

    var pages = Math.max(1, Math.ceil(list.length / STATE.perPage));
    var phtml = '<button class="btn" ' + (STATE.page <= 0 ? 'disabled' : '') + ' id="prevPage">上一页</button>' +
      '<span>第 ' + (STATE.page + 1) + ' / ' + pages + ' 页　共 ' + list.length + ' 条</span>' +
      '<button class="btn" ' + (STATE.page >= pages - 1 ? 'disabled' : '') + ' id="nextPage">下一页</button>';
    $('pager').innerHTML = phtml;
    if ($('prevPage')) { $('prevPage').addEventListener('click', function () { STATE.page--; renderRequirements(); }); }
    if ($('nextPage')) { $('nextPage').addEventListener('click', function () { STATE.page++; renderRequirements(); }); }
  }

  function detailOf(x) {
    var r = x._r;
    var h = '<div class="detail-meta">';
    h += '<span class="mono">' + esc(r.id) + '</span>';
    if (r.version) { h += ' · 版本 ' + esc(r.version); }
    if (r.primSource) { h += ' · 原语来源：' + esc((DATA.SOURCE_LABEL && DATA.SOURCE_LABEL[r.primSource]) || r.primSource); }
    if (r.primNote) { h += ' · ' + esc(r.primNote); }
    h += '</div><div class="detail-box">';
    if (!r.primitives.length) {
      h += '<div class="detail-cell"><div class="note">该条目未识别出任何效果原语，不参与判定。</div></div>';
    }
    r.primitives.forEach(function (p) {
      var bad = 0;
      p.cells.forEach(function (c) { if (CORE.isBlocking(c.status)) { bad++; } });
      h += '<div class="detail-cell">';
      h += '<div class="top"><span class="pname">' + esc(p.name || p.id) + '</span><span class="chip ' + (bad ? 'blocked' : 'ready') + '">' +
           (bad ? bad + ' 处缺口' : '通过') + '</span></div>';
      p.cells.forEach(function (c) {
        if (!CORE.isBlocking(c.status)) { return; }
        var st = STATUS[c.status];
        h += '<div class="note"><span class="chip ' + (st.gap ? 'blocked' : 'ready') + '">' + esc(st.label) + '</span> ' +
             esc(CORE.unitNameOf(c.unit)) + '：' + esc(c.note) + '</div>';
      });
      h += '</div>';
    });
    h += '</div>';
    return h;
  }

  function card(k, v, d, cls) {
    return '<div class="card ' + (cls || '') + '"><div class="k">' + esc(k) + '</div><div class="v">' + esc(v) +
           '</div><div class="d">' + esc(d) + '</div></div>';
  }

  // ── 缺口视图 ───────────────────────────────────────────
  function renderGaps() {
    var results = ALL.map(function (x) { return x._r; });
    var sum = CORE.summarize(results);
    $('gapCards').innerHTML =
      card('受阻需求', sum.blocked, pct(sum.blocked, sum.total), 'bad') +
      card('缺机制', sum.byWorst.mechanism, '框架确实没有这类能力', 'bad') +
      card('缺裁决', sum.byWorst.ruling, '被未决项挡住', 'warn') +
      card('缺文档', sum.byWorst.doc, '已裁或有草稿，只差正式化', 'info') +
      card('缺依据', sum.byWorst.unknown, '该域无文档，现在判不了', 'warn');

    var impact = CORE.unitImpact(results);
    var ih = '<thead><tr><th>判定单元</th><th>文档状态</th><th class="num">阻塞需求</th><th class="num">独占阻塞</th><th class="num">涉及原语</th><th>说明</th></tr></thead><tbody>';
    impact.forEach(function (it) {
      if (!it.blocks && !it.exclusive) { return; }
      ih += '<tr>';
      ih += '<td>' + esc(it.unit.name) + ' <span class="mono hint">' + esc(it.unit.id) + '</span></td>';
      ih += '<td>' + esc(CORE.docStatusLabel(it.unit.status)) + '</td>';
      ih += '<td class="num">' + it.blocks + '</td>';
      ih += '<td class="num">' + (it.exclusive ? '<b style="color:var(--covered)">' + it.exclusive + '</b>' : '0') + '</td>';
      ih += '<td class="num">' + it.primitiveCount + '</td>';
      ih += '<td class="hint">' + esc(it.unit.covers) + '</td>';
      ih += '</tr>';
    });
    ih += '</tbody>';
    $('impactTable').innerHTML = ih;

    var unitGaps = CORE.unitGaps(results);
    var gh = '';
    unitGaps.forEach(function (g, gi) {
      if (!g.primitives.length) { return; }
      var cnt = { mechanism: 0, unknown: 0, ruling: 0, doc: 0 };
      g.primitives.forEach(function (p) { if (cnt[p.status] !== undefined) { cnt[p.status]++; } });
      var open = gi === 0 ? ' open' : '';
      gh += '<div class="gap-unit' + open + '">';
      gh += '<div class="head"><span class="name">' + esc(g.unit.name) + '</span>' +
            '<span class="chip ' + (g.unit.status === 'formal' ? 'ready' : (g.unit.status === 'draft' ? 'host_only' : 'blocked')) + '">' +
            esc(CORE.docStatusLabel(g.unit.status)) + '</span>' +
            '<span class="hint">缺机制 ' + cnt.mechanism + ' · 缺依据 ' + cnt.unknown + ' · 需补裁决 ' + cnt.ruling + ' · 缺文档 ' + cnt.doc + '</span>' +
            '<span class="meta">' + g.primitives.length + ' 条原语待处理</span></div>';
      gh += '<div class="body"><table><thead><tr><th>原语</th><th>状态</th><th>依据</th></tr></thead><tbody>';
      g.primitives.forEach(function (p) {
        var st = STATUS[p.status] || { label: p.status, id: 'unknown' };
        gh += '<tr><td>' + esc(p.name) + ' <span class="mono hint">' + esc(p.id) + '</span></td>' +
              '<td><span class="chip ' + (st.gap ? 'blocked' : 'ready') + '">' + esc(st.label) + '</span></td>' +
              '<td class="hint">' + esc(p.note) + '</td></tr>';
      });
      gh += '</tbody></table></div></div>';
    });
    $('gapUnits').innerHTML = gh;

    var heads = $('gapUnits').querySelectorAll('.gap-unit > .head');
    for (var i = 0; i < heads.length; i++) {
      heads[i].addEventListener('click', function () {
        this.parentNode.classList.toggle('open');
      });
    }
  }

  // ── 版本差分 ───────────────────────────────────────────
  function renderDiff() {
    var oldItems = ALL.filter(function (x) { return x._corpus === 'augments1620'; });
    var newItems = ALL.filter(function (x) { return x._corpus === 'augments1622'; });
    if (!oldItems.length || !newItems.length) {
      $('diffBody').innerHTML = '<div class="note warn">当前数据集缺少海克斯新旧版本语料。</div>';
      return;
    }
    var d = CORE.diffResults(oldItems.map(function (x) { return x; }), newItems.map(function (x) { return x; }),
      oldItems.map(function (x) { return x._r; }), newItems.map(function (x) { return x._r; }));

    $('diffCards').innerHTML =
      card('16.20 条数', oldItems.length, '旧版海克斯池', 'total') +
      card('16.22 条数', newItems.length, '新版海克斯池', 'total') +
      card('新增', d.added.length, '全部落在 3xxx 号段', 'ok') +
      card('移除', d.removed.length, '全部落在 12xx–21xx', 'bad') +
      card('沿用', newItems.length - d.added.length, '两版共同条目', 'info');

    var h = '<div class="note">海克斯增幅在新旧版本之间是整池替换：新版把旧池里的一批移除，同时引入一批新的，而不是两套并存。所以「旧版/新版」在数据层是按版本目录快照区分，工具不存两份数据，只存一次并按 version 切片。</div>';
    h += '<h2 style="font-size:14px;margin:18px 0 10px">新增条目（判定表现）</h2>';
    h += '<div style="overflow:auto"><table><thead><tr><th>名称</th><th>稀有度</th><th>裁决</th><th class="num">覆盖</th><th>缺口</th></tr></thead><tbody>';
    d.added.slice(0, 60).forEach(function (r) {
      h += '<tr><td>' + esc(r.name) + '</td><td>' + esc(r.rarity || '—') + '</td>' +
        '<td><span class="chip ' + r.verdict + '">' + esc(CORE.verdictLabel(r.verdict)) + '</span></td>' +
        '<td class="num">' + r.coveragePct + '%</td><td class="gap hint">' + esc(CORE.gapText(r)) + '</td></tr>';
    });
    h += '</tbody></table></div>';

    h += '<h2 style="font-size:14px;margin:22px 0 10px">移除条目</h2>';
    h += '<div style="overflow:auto"><table><thead><tr><th>名称</th><th>裁决</th><th>缺口</th></tr></thead><tbody>';
    d.removed.forEach(function (r) {
      h += '<tr><td>' + esc(r.name) + '</td><td><span class="chip ' + r.verdict + '">' + esc(CORE.verdictLabel(r.verdict)) + '</span></td>' +
        '<td class="gap hint">' + esc(CORE.gapText(r)) + '</td></tr>';
    });
    h += '</tbody></table></div>';
    $('diffBody').innerHTML = h;
  }

  // ── 文档状态 ───────────────────────────────────────────
  function renderBanner() {
    var el = $('docBanner');
    var dot = $('docDot');
    if (!el) { return; }
    if (!DOCS) {
      el.className = 'banner';
      el.style.display = '';
      el.innerHTML = '<b>尚无文档基线。</b>先运行 <span class="mono">node check-docs.mjs --update</span> 建立基线，之后文档改动才会被检出。';
      return;
    }
    var ch = (DOCS.summary && DOCS.summary.changedFiles) || 0;
    var hits = (DOCS.summary && DOCS.summary.anchorHits) || 0;
    var probs = (DOCS.summary && DOCS.summary.anchorProblems) || 0;
    el.style.display = '';
    if (ch || hits) {
      el.className = 'banner';
      el.innerHTML = '<b>TCS 文档有 ' + ch + ' 个文件变更，' + hits + ' 条判定依据受影响</b>' +
        '（涉及 ' + ((DOCS.summary && DOCS.summary.affectedCells) || 0) + ' 个判定格）。' +
        '本页结论仍按旧依据给出，请先复核。' +
        '<button class="btn" onclick="TCM.gotoDocs()">看待复核清单</button>';
      if (dot) { dot.style.display = ''; }
    } else if (probs) {
      el.className = 'banner';
      el.innerHTML = '<b>锚点体检：' + probs + ' 条备注引用的章节无法定位。</b>这不影响文档变更检测，但说明备注写错或文档改过名。' +
        '<button class="btn" onclick="TCM.gotoDocs()">查看</button>';
      if (dot) { dot.style.display = ''; }
    } else {
      el.className = 'banner ok';
      el.innerHTML = '文档无变更（基线 ' + String(DOCS.baselineAt || '').slice(0, 10) + '），当前判定依据全部对得上账。';
    }
  }

  function stateChip(s, label) {
    var cls = (s === 'file-changed' || s === 'section-changed' || s === 'section-child-changed') ? 'blocked' : 'untagged';
    if (s === 'ok') { cls = 'ready'; }
    return '<span class="chip ' + cls + '">' + esc(label || DOC_STATE_LABEL[s] || s) + '</span>';
  }

  function renderDocs() {
    if (!$('docBody')) { return; }
    if (!DOCS) {
      $('docCards').innerHTML = '';
      $('docBody').innerHTML = '<div class="note warn">没有 <span class="mono">data/doc-snapshot.js</span>。' +
        '在工具目录运行 <span class="mono">node check-docs.mjs --update</span> 生成。</div>';
      return;
    }
    var S = DOCS.summary || {};
    var C = DOCS.cells || {};
    var cards = [
      ['跟踪文档', (S.trackedFiles || 0), '含族内 10 篇与族外草稿', 'info'],
      ['锚点', (S.anchors || 0), '从判定格备注解析出的章节引用', 'info'],
      ['判定格锚定', (C.anchored || 0) + '/' + (C.total || 0), '有章节锚点的格 / 全部格', C.anchored === C.total ? 'ok' : 'warn'],
      ['变更文件', (S.changedFiles || 0), '对比基线', (S.changedFiles ? 'bad' : 'ok')],
      ['受影响判定格', (S.affectedCells || 0), '涉及 ' + (S.affectedPrimitives || 0) + ' 条原语', (S.affectedCells ? 'bad' : 'ok')],
      ['锚点体检', (S.anchorProblems || 0), '引用的章节定位失败数', (S.anchorProblems ? 'warn' : 'ok')]
    ];
    $('docCards').innerHTML = cards.map(function (c) {
      return '<div class="card ' + c[3] + '"><div class="k">' + esc(c[0]) + '</div><div class="v">' + esc(String(c[1])) +
        '</div><div class="d">' + esc(c[2]) + '</div></div>';
    }).join('');

    var out = '';

    // 基线信息
    out += '<h2 style="font-size:14px;margin:18px 0 10px">基线</h2>';
    out += '<table><tbody>';
    out += '<tr><td style="width:140px;color:var(--fg-faint)">基线时间</td><td class="mono">' + esc(DOCS.baselineAt || '—') + '</td></tr>';
    if (DOCS.baselineGit) {
      out += '<tr><td style="color:var(--fg-faint)">基线提交</td><td class="mono">' + esc(DOCS.baselineGit.branch + '@' + DOCS.baselineGit.headShort) + '</td></tr>';
    }
    out += '<tr><td style="color:var(--fg-faint)">检测时间</td><td class="mono">' + esc(DOCS.generatedAt || '—') + '</td></tr>';
    if (DOCS.git) {
      out += '<tr><td style="color:var(--fg-faint)">当前提交</td><td class="mono">' + esc(DOCS.git.branch + '@' + DOCS.git.headShort) +
        (DOCS.git.dirty ? '　<span class="hint">工作区有 ' + DOCS.git.dirtyFiles.length + ' 处未提交改动</span>' : '') + '</td></tr>';
    }
    out += '</tbody></table>';

    // 变更清单
    out += '<h2 style="font-size:14px;margin:22px 0 10px">变更清单</h2>';
    if (!DOCS.changedFiles || !DOCS.changedFiles.length) {
      out += '<div class="note">文档没有变化。基线之后没人动过这些文档，或者动完之后已经重跑过 <span class="mono">--update</span>。</div>';
    } else {
      out += '<table><thead><tr><th>文件</th><th>状态</th><th>变更章节</th><th>消失</th><th>新增</th></tr></thead><tbody>';
      DOCS.changedFiles.forEach(function (f) {
        out += '<tr><td class="mono">' + esc(f.path.split('/').slice(-2).join('/')) + '</td>' +
          '<td>' + stateChip(f.state, f.state === 'changed' ? '内容变了' : (f.state === 'deleted' ? '已删除' : '新增')) + '</td>' +
          '<td class="mono">' + esc((f.changed || []).join('、') || '—') + '</td>' +
          '<td class="mono">' + esc((f.gone || []).join('、') || '—') + '</td>' +
          '<td class="mono">' + esc((f.added || []).join('、') || '—') + '</td></tr>';
      });
      out += '</tbody></table>';
    }

    // 待复核判定格
    out += '<h2 style="font-size:14px;margin:22px 0 10px">待复核判定格</h2>';
    if (!DOCS.affectedCells || !DOCS.affectedCells.length) {
      out += '<div class="note">没有判定格受影响。</div>';
    } else {
      out += '<div class="note warn"><b>这些格必须人读文档后重判。</b>工具只负责把范围缩到这里：改动落在哪个章节、那个章节被哪些判定格引用。</div>';
      out += '<table><thead><tr><th>判定档 / 原语</th><th>判定单元</th><th>波及原语</th><th>状态</th><th>锚点</th></tr></thead><tbody>';
      DOCS.affectedCells.forEach(function (c) {
        var an = (c.anchors || []).map(function (a) {
          return (a.file || '?').split('/').pop() + (a.section ? ' §' + a.section : '');
        }).join('；');
        out += '<tr><td class="mono">' + esc(c.primitive || c.profile) + '</td>' +
          '<td>' + esc(CORE.unitNameOf ? CORE.unitNameOf(c.unit) : c.unit) + '</td>' +
          '<td class="num">' + (c.primitives || []).length + '</td>' +
          '<td>' + (c.states || []).map(function (s) { return stateChip(s); }).join(' ') + '</td>' +
          '<td class="mono" style="font-size:11px">' + esc(an || '—') + '</td></tr>';
      });
      out += '</tbody></table>';
    }

    // 锚点体检
    out += '<h2 style="font-size:14px;margin:22px 0 10px">锚点体检</h2>';
    if (!DOCS.anchorProblems || !DOCS.anchorProblems.length) {
      out += '<div class="note">全部 ' + (S.anchors || 0) + ' 条锚点都能定位到现存章节。</div>';
    } else {
      out += '<table><thead><tr><th>判定档 / 原语</th><th>判定单元</th><th>问题</th><th>备注里的文件名</th></tr></thead><tbody>';
      DOCS.anchorProblems.forEach(function (p) {
        out += '<tr><td class="mono">' + esc(p.primitive || p.profile || 'unit') + '</td>' +
          '<td>' + esc(CORE.unitNameOf ? CORE.unitNameOf(p.unit) : p.unit) + '</td>' +
          '<td style="color:var(--ruling)">' + esc(p.label) + '</td>' +
          '<td class="mono" style="font-size:11px">' + esc(p.fileHint || p.file || '—') + '</td></tr>';
      });
      out += '</tbody></table>';
    }

    // 文档清单
    out += '<h2 style="font-size:14px;margin:22px 0 10px">跟踪的文档</h2>';
    out += '<table><thead><tr><th>文件</th><th class="num">行数</th><th class="num">章节</th><th>状态</th></tr></thead><tbody>';
    var chMap = {};
    (DOCS.changedFiles || []).forEach(function (f) { chMap[f.path] = f.state; });
    (DOCS.docs || []).forEach(function (d) {
      var st = chMap[d.path];
      out += '<tr><td class="mono">' + esc(d.path.split('/').slice(-2).join('/')) + '</td>' +
        '<td class="num">' + d.lines + '</td><td class="num">' + d.sections + '</td>' +
        '<td>' + (st ? stateChip(st === 'changed' ? 'section-changed' : st, st === 'changed' ? '内容变了' : st) : '<span class="chip ready">未变</span>') + '</td></tr>';
    });
    out += '</tbody></table>';

    // 操作说明
    out += '<h2 style="font-size:14px;margin:22px 0 10px">怎么用</h2>';
    out += '<div class="note">' +
      '<b>1.</b> 平时：<span class="mono">node check-docs.mjs</span> —— 比对文档与基线，输出变更与待复核清单（同时刷新本页数据）。<br>' +
      '<b>2.</b> 复核完判定、改完 <span class="mono">data/model.js</span> 后：<span class="mono">node check-docs.mjs --update</span> 把当前文档设为新基线。<br>' +
      '<b>3.</b> 回归：<span class="mono">node test-core.mjs</span>；判断语义变了才需要重跑 <span class="mono">node build-corpora.mjs</span>。<br>' +
      '<b>边界：</b>「文档变了没有」可以自动判；「判定还成不成立」不能——那要人读文档。' +
      '</div>';

    $('docBody').innerHTML = out;
  }

  // ── 说明 ───────────────────────────────────────────────
  function renderAbout() {
    var h = '';
    h += '<div class="note"><b>这个工具回答一个问题：</b>把 LOL 里的技能、装备、海克斯、符文拆成「效果原语」，逐条对照 TCS 文档，判断 TCS 的表达力能不能覆盖它；不能的话，缺的是机制、裁决、文档，还是依据。</div>';

    h += '<h2 style="font-size:14px;margin:18px 0 10px">证据边界</h2>';
    h += '<div class="note warn"><b>这是设计期判定，不是运行时验证。</b>TCS 插件的 Source/ 已在 2026-11-18 随 V1 物理删除，当前 HEAD 没有可编译代码，任何「能跑通」的结论都无从验证。本工具判定的是「文档所描述的能力能否承接该效果」，属于表达力对标。</div>';

    h += '<h2 style="font-size:14px;margin:18px 0 10px">判定状态（六态）</h2>';
    h += '<div style="overflow:auto"><table><thead><tr><th>状态</th><th>含义</th><th>下一步动作</th></tr></thead><tbody>';
    var acts = {
      covered: '无需动作',
      host: '确认宿主侧已有实现；TCS 不需要提供',
      doc: '把已裁结论正式化进 tcs-remake-v2',
      ruling: '在族外草稿里把未决项裁掉',
      unknown: '先写该领域的文档（现在连判都没法判）',
      mechanism: '评估是否进入铁律①：找真实消费者，否则不建'
    };
    ['covered', 'host', 'doc', 'ruling', 'unknown', 'mechanism'].forEach(function (k) {
      var s = STATUS[k];
      h += '<tr><td><span class="chip ' + (s.gap ? 'blocked' : 'ready') + '">' + esc(s.label) + '</span></td>' +
        '<td>' + esc(s.desc) + '</td><td class="hint">' + esc(acts[k]) + '</td></tr>';
    });
    h += '</tbody></table></div>';

    h += '<h2 style="font-size:14px;margin:18px 0 10px">数据来源</h2>';
    h += '<div style="overflow:auto"><table><thead><tr><th>语料</th><th>条数</th><th>版本</th><th>来源</th></tr></thead><tbody>';
    DATA.manifest.forEach(function (m) {
      h += '<tr><td>' + esc(m.label) + '</td><td class="num">' + m.count + '</td><td>' + esc(m.version) + '</td>' +
        '<td class="hint mono">' + esc(m.source) + '</td></tr>';
    });
    h += '</tbody></table></div>';

    h += '<h2 style="font-size:14px;margin:18px 0 10px">原语的三级来源</h2>';
    h += '<div style="overflow:auto"><table><thead><tr><th>来源</th><th>精度</th><th>含义</th></tr></thead><tbody>';
    Object.keys(window.TCM_ANNOTATE.SOURCE).forEach(function (k) {
      var s = window.TCM_ANNOTATE.SOURCE[k];
      h += '<tr><td>' + esc(s.label) + '</td><td class="num">' + s.precision + '</td><td class="hint">' + esc(s.desc) + '</td></tr>';
    });
    h += '<tr><td>类别规则</td><td class="num">1</td><td class="hint">英雄数据本身不产生效果，对应属性系统的基础值输入</td></tr>';
    h += '</tbody></table></div>';

    h += '<h2 style="font-size:14px;margin:18px 0 10px">已知局限</h2>';
    h += '<ul style="color:var(--fg-dim);line-height:1.9;padding-left:22px">';
    h += '<li>需求到原语的映射来自 Riot 官方技能标签与伤害公式节点，不来自人工阅读技能文案。技能靠四遍扫描对上 CDragon 对象：<span class="mono">exact</span> 叶名 536 条、<span class="mono">numeric</span> 数值数组 178 条、<span class="mono">passive</span> 被动名 129 条、<span class="mono">unresolved</span> 未解析 22 条；只有 unresolved 那些退回按文案推断，精度弱于前三类。</li>';
    h += '<li>「缺依据」不等于「框架不支持」。七个领域里只有 Attribute 有正式文档，State / Skill / Damage / Targeting 连草稿都没有，因此大量条目落在这一档——这是文档进度问题，不是框架能力问题。</li>';
    h += '<li>「归宿主」不构成缺口。按 <span class="mono">01-定位与判据.md §5.6</span>，需要具体宿主词汇的能力由宿主实现。</li>';
    h += '<li>LOL 与 DNF 是<b>表达力对标样本</b>，不是 TCS 的消费者。本报告结论不能直接当作「TCS 该加什么机制」的排期依据——那需要真实消费者。</li>';
    h += '<li>海克斯增幅的原始逻辑配置在服务端，客户端资源里拿不到（map12.bin.json 里没有任何 augment 数据），所以海克斯只能判到「文案描述的效果」，判不到实现细节。</li>';
    h += '</ul>';
    $('aboutBody').innerHTML = h;
  }

  // ── 导出 ───────────────────────────────────────────────
  function buildReport() {
    var results = ALL.map(function (x) { return x._r; });
    return {
      generatedAt: Date.now(),
      docBaseline: 'tcs-remake-v2（INDEX.md 六核心基线 + Attribute 正式文档，其余六域为草稿或无文档）',
      docStatus: DOCS ? {
        baselineAt: DOCS.baselineAt || null,
        baselineGit: DOCS.baselineGit ? DOCS.baselineGit.branch + '@' + DOCS.baselineGit.headShort : null,
        generatedAt: DOCS.generatedAt || null,
        currentGit: DOCS.git ? DOCS.git.branch + '@' + DOCS.git.headShort : null,
        dirty: DOCS.git ? !!DOCS.git.dirty : false,
        trackedFiles: (DOCS.summary && DOCS.summary.trackedFiles) || 0,
        anchors: (DOCS.summary && DOCS.summary.anchors) || 0,
        anchoredCells: (DOCS.cells && DOCS.cells.anchored) || 0,
        totalCells: (DOCS.cells && DOCS.cells.total) || 0,
        anchorProblems: (DOCS.anchorProblems || []).length,
        changedFiles: (DOCS.changedFiles || []).map(function (f) {
          return { path: f.path, state: f.state, changed: f.changed || [], gone: f.gone || [], added: f.added || [] };
        }),
        affectedCells: (DOCS.affectedCells || []).map(function (c) {
          return { profile: c.profile, unit: c.unit, primitive: c.primitive,
                   primitives: (c.primitives || []).length,
                   states: c.states || [],
                   anchors: (c.anchors || []).map(function (a) {
                     return (a.file || '').split('/').pop() + (a.section ? ' §' + a.section : '');
                   }) };
        })
      } : null,
      model: {
        primitives: MODEL.PRIMITIVES.length,
        units: MODEL.UNITS.length,
        profiles: Object.keys(MODEL.PROFILES).length
      },
      summary: CORE.summarize(results),
      units: CORE.unitSummary(),
      impact: CORE.unitImpact(results),
      unitGaps: CORE.unitGaps(results),
      results: results,
      corpora: DATA.manifest
    };
  }

  function exportMarkdown() {
    var md = CORE.markdown(buildReport());
    var d = new Date();
    var stamp = d.getFullYear() + pad2(d.getMonth() + 1) + pad2(d.getDate()) + '-' + pad2(d.getHours()) + pad2(d.getMinutes());
    var blob = new Blob([md], { type: 'text/markdown;charset=utf-8' });
    var a = document.createElement('a');
    a.href = URL.createObjectURL(blob);
    a.download = 'TCS-能力覆盖度报告-' + stamp + '.md';
    document.body.appendChild(a);
    a.click();
    setTimeout(function () { URL.revokeObjectURL(a.href); a.remove(); }, 1000);
  }

  function pad2(n) { return n < 10 ? '0' + n : String(n); }

  function gotoDocs() {
    activateView('docs');
    if (location.hash !== '#docs') { location.hash = 'docs'; }
  }

  window.TCM = { exportMarkdown: exportMarkdown, buildReport: buildReport, state: STATE, gotoDocs: gotoDocs };

  if (document.readyState === 'loading') {
    document.addEventListener('DOMContentLoaded', init);
  } else {
    init();
  }
})();
