// fetch-corpora.mjs —— 抓取六份语料的原始数据到 data/raw/，不改动 data/requirements/。
//
// 用法：
//   node fetch-corpora.mjs                 # 增量抓取（已存在且非空的文件跳过）
//   node fetch-corpora.mjs --refresh       # 全量重抓
//   node fetch-corpora.mjs --only=ddragon  # 只抓某一类：aramkit | ddragon | cdragon
//
// 换版本：改下面的 PIN 常量，重跑本脚本，再跑 annotate-crawl.mjs。

import { mkdirSync, existsSync, statSync, writeFileSync, readFileSync } from 'node:fs';
import { join, dirname } from 'node:path';
import { fileURLToPath } from 'node:url';

const here = dirname(fileURLToPath(import.meta.url));
const rawDir = join(here, 'data', 'raw');

// ── 固定版本 ────────────────────────────────────────────────
// aramkit 的 resource 段是内容哈希，换补丁时必须重新取（取法见 README「换版本」）。
const PIN = {
  aramkit: {
    locale: 'zh-CN',
    base: 'https://data.aramkit.cn/resources',
    versions: {
      '16.22': { resource: '16.22-8a495cc12b20', files: ['champions', 'champion-skills', 'items', 'augments'] },
      '16.20': { resource: '16.20-6b24fc84026f', files: ['augments'] }
    }
  },
  ddragon: {
    version: '16.23.1',
    locale: 'zh_CN',
    base: 'https://ddragon.leagueoflegends.com/cdn',
    files: ['champion', 'runesReforged']
  },
  cdragon: {
    version: '16.23',
    base: 'https://raw.communitydragon.org',
    path: 'game/data/characters'
  }
};

const argv = process.argv.slice(2);
const refresh = argv.includes('--refresh');
const onlyArg = argv.find((a) => a.startsWith('--only='));
const only = onlyArg ? onlyArg.slice(7).split(',').map((s) => s.trim()) : null;
const want = (name) => !only || only.includes(name);

const CONCURRENCY = 8;
const RETRY = 3;

// ── 抓取原语 ────────────────────────────────────────────────
function ensureDir(p) { mkdirSync(p, { recursive: true }); }

const stats = { fetched: 0, skipped: 0, bytes: 0, failed: [] };

async function fetchText(url) {
  let lastErr;
  for (let attempt = 1; attempt <= RETRY; attempt++) {
    try {
      const res = await fetch(url, { headers: { 'user-agent': 'TcsCapabilityMatrix/1.0 (corpus crawl)' } });
      if (!res.ok) throw new Error('HTTP ' + res.status);
      const buf = Buffer.from(await res.arrayBuffer());
      if (buf.length === 0) throw new Error('空响应');
      return buf;
    } catch (e) {
      lastErr = e;
      if (attempt < RETRY) await new Promise((r) => setTimeout(r, 400 * attempt));
    }
  }
  throw lastErr;
}

// 抓一个 JSON，落盘；已存在且非空则跳过
async function fetchJson(url, outPath, validate) {
  if (!refresh && existsSync(outPath) && statSync(outPath).size > 0) {
    stats.skipped++;
    return false;
  }
  ensureDir(dirname(outPath));
  const buf = await fetchText(url);
  let parsed;
  try { parsed = JSON.parse(buf.toString('utf8')); }
  catch (e) {
    stats.failed.push({ url, reason: 'JSON 解析失败: ' + e.message });
    return false;
  }
  if (validate) {
    const bad = validate(parsed);
    if (bad) { stats.failed.push({ url, reason: bad }); return false; }
  }
  writeFileSync(outPath, JSON.stringify(parsed), 'utf8');
  stats.fetched++;
  stats.bytes += buf.length;
  return true;
}

// 并发池
async function pool(jobs, limit = CONCURRENCY) {
  const queue = jobs.slice();
  const workers = Array.from({ length: Math.min(limit, queue.length) }, async () => {
    while (queue.length) {
      const job = queue.shift();
      try { await job(); }
      catch (e) { stats.failed.push({ url: job.url || '(unknown)', reason: e.message }); }
    }
  });
  await Promise.all(workers);
}

// ── 各数据源 ────────────────────────────────────────────────
export function aramkitUrl(version, file, locale = PIN.aramkit.locale) {
  const v = PIN.aramkit.versions[version];
  if (!v) throw new Error('未登记的 aramkit 版本: ' + version);
  return PIN.aramkit.base + '/' + v.resource + '/' + locale + '/resources/' + file + '.json';
}

async function fetchAramkit() {
  console.log('\n[aramkit]');
  for (const [version, v] of Object.entries(PIN.aramkit.versions)) {
    for (const file of v.files) {
      const url = aramkitUrl(version, file);
      const out = join(rawDir, 'aramkit', version, file + '.json');
      await fetchJson(url, out, (d) => (d && typeof d === 'object' && Object.keys(d).length ? null : '结构为空'));
      console.log('  ' + version + ' ' + file.padEnd(16) + ' ok');
    }
  }
}

async function fetchDdragon() {
  console.log('\n[ddragon]');
  for (const file of PIN.ddragon.files) {
    const url = PIN.ddragon.base + '/' + PIN.ddragon.version + '/data/' + PIN.ddragon.locale + '/' + file + '.json';
    const out = join(rawDir, 'ddragon', PIN.ddragon.version, file + '.json');
    await fetchJson(url, out, (d) => (file === 'champion' ? (d.data && Object.keys(d.data).length ? null : '缺 data') : null));
    console.log('  ' + file);
  }
}

// CDragon 需要英雄 slug 列表 → 依赖已抓好的 aramkit champions.json
function readAramkitChampions() {
  const p = join(rawDir, 'aramkit', '16.22', 'champions.json');
  if (!existsSync(p)) throw new Error('缺少 ' + p + '，请先抓 aramkit');
  return JSON.parse(readFileSync(p, 'utf8'));
}

async function fetchCdragon() {
  console.log('\n[cdragon]');
  const champs = readAramkitChampions();
  const slugs = Object.values(champs).map((c) => c.slug).filter(Boolean).sort();
  console.log('  英雄数 ' + slugs.length + '，版本目录 ' + PIN.cdragon.version);
  const outDir = join(rawDir, 'cdragon', PIN.cdragon.version);
  ensureDir(outDir);

  const jobs = slugs.map((slug) => {
    const url = PIN.cdragon.base + '/' + PIN.cdragon.version + '/' + PIN.cdragon.path + '/' + slug + '/' + slug + '.bin.json';
    const job = () => fetchJson(url, join(outDir, slug + '.bin.json'), (d) => (d && typeof d === 'object' ? null : '结构为空'));
    job.url = url;
    return job;
  });
  await pool(jobs);
  console.log('  完成：新抓 ' + stats.fetched + '，跳过 ' + stats.skipped);
}

// ── 主流程 ──────────────────────────────────────────────────
const t0 = Date.now();
ensureDir(rawDir);

try {
  if (want('aramkit')) await fetchAramkit();
  if (want('ddragon')) await fetchDdragon();
  if (want('cdragon')) await fetchCdragon();
} catch (e) {
  console.error('\n抓取中断: ' + e.message);
  process.exitCode = 1;
}

// 快照元数据
const snapshot = {
  fetchedAt: new Date().toISOString(),
  pin: PIN,
  stats: { fetched: stats.fetched, skipped: stats.skipped, bytes: stats.bytes, failed: stats.failed.length },
  failures: stats.failed
};
writeFileSync(join(rawDir, 'snapshot.json'), JSON.stringify(snapshot, null, 2), 'utf8');

console.log('\n' + '─'.repeat(56));
console.log('新抓 ' + stats.fetched + ' 个文件，跳过 ' + stats.skipped + '，共 ' + (stats.bytes / 1024 / 1024).toFixed(1) + ' MB，耗时 ' + ((Date.now() - t0) / 1000).toFixed(1) + 's');
if (stats.failed.length) {
  console.log('失败 ' + stats.failed.length + ' 个：');
  stats.failed.slice(0, 10).forEach((f) => console.log('  ' + f.url + '  →  ' + f.reason));
  if (!refresh && stats.failed.length) {
    console.log('提示：若只是网络抖动，重跑本脚本即可（已存在的文件会跳过）。');
  }
  process.exitCode = 1;
} else {
  console.log('全部成功。下一步：node annotate-crawl.mjs');
}
