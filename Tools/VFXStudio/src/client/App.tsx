import { useCallback, useEffect, useRef, useState } from 'react';
import { flushSync } from 'react-dom';
import type { LayerKind, ProjectSummary, TemplateKind, VfxProject } from '../shared/types.js';
import { lastFrame, templateLabels } from '../shared/types.js';
import { createLayer, createProject, newId } from '../shared/templates.js';
import { connect, listProjects, loadProject, saveProject } from './api.js';
import { fingerprint, useEditor } from './store.js';
import { Layers } from './components/Layers.js';
import { Inspector, NameInput } from './components/Inspector.js';
import { Timeline } from './components/Timeline.js';
import { Viewport, type ViewportHandle } from './components/Viewport.js';
export function App() {
  const editor = useEditor(); const project = editor.project;
  const [selected, setSelected] = useState<string | null>(project.layers[0]?.id ?? null);
  const [solo, setSolo] = useState<string | null>(null);
  const [frame, setFrame] = useState(20); const [playing, setPlaying] = useState(false);
  const [orbit, setOrbit] = useState<'left' | 'right'>('left'); const [grid, setGrid] = useState(true);
  const [theme, setTheme] = useState(() => localStorage.getItem('vfx-theme') === 'light' ? 'light' : 'dark');
  const [tab, setTab] = useState<'properties' | 'ai'>('properties');
  const [savedStamp, setSavedStamp] = useState(''); const [diskVersion, setDiskVersion] = useState<string | null>(null);
  const [saving, setSaving] = useState(false); const [message, setMessage] = useState('正在连接本机工程服务…'); const [error, setError] = useState('');
  const [modal, setModal] = useState<'new' | 'open' | null>(null); const [projects, setProjects] = useState<ProjectSummary[]>([]); const [listing, setListing] = useState(false); const [loading, setLoading] = useState(false);
  const viewport = useRef<ViewportHandle>(null); const dialog = useRef<HTMLDialogElement>(null); const inFlight = useRef(false);
  const current = useRef(project); current.current = project;
  const dirty = fingerprint(project) !== savedStamp;
  const layer = project.layers.find(l => l.id === selected);
  useEffect(() => { document.documentElement.dataset.theme = theme; localStorage.setItem('vfx-theme', theme); }, [theme]);
  useEffect(() => { let alive = true; void connect().then(() => { if (alive) setMessage('本机服务已连接 · 工程尚未保存'); }).catch(e => { if (alive) setError(String(e.message ?? e)); }); return () => { alive = false; }; }, []);
  useEffect(() => { if (!project.layers.some(l => l.id === selected)) setSelected(project.layers[0]?.id ?? null); if (!project.layers.some(l => l.id === solo)) setSolo(null); }, [project.layers, selected, solo]);
  useEffect(() => { setFrame(v => Math.min(v, lastFrame(project))); }, [project.duration, project.fps]);
  useEffect(() => {
    if (!playing) return;
    let animation = 0; let start: number | null = null; const initial = frame;
    const animate = (now: number) => { if (start === null) start = now; const next = initial + Math.floor((now - start) / 1000 * project.fps); const max = lastFrame(project);
      if (!project.loop && next > max) { setFrame(max); setPlaying(false); return; }
      setFrame(project.loop ? next % (max + 1) : Math.min(next, max)); animation = requestAnimationFrame(animate); };
    animation = requestAnimationFrame(animate); return () => cancelAnimationFrame(animation);
    // Playback captures its start frame only when starting; seeking stops playback explicitly.
    // eslint-disable-next-line react-hooks/exhaustive-deps
  }, [playing, project.duration, project.fps, project.loop]);
  useEffect(() => { const warn = (event: BeforeUnloadEvent) => { if (dirty || saving) { event.preventDefault(); event.returnValue = ''; } }; window.addEventListener('beforeunload', warn); return () => window.removeEventListener('beforeunload', warn); }, [dirty, saving]);
  const save = useCallback(async () => {
    if (inFlight.current) return;
    // Commit an active draft (including Ctrl+S from a number/name field) before taking the save snapshot.
    flushSync(() => { if (document.activeElement instanceof HTMLInputElement) document.activeElement.blur(); });
    inFlight.current = true; setSaving(true); setError('');
    const snapshot = structuredClone(current.current); const stamp = fingerprint(snapshot);
    try { const version = await saveProject(snapshot, diskVersion); if (current.current.id === snapshot.id) { setDiskVersion(version); setSavedStamp(stamp); setMessage('已保存到本机 · ' + new Date().toLocaleTimeString()); } }
    catch (failure) { setError(failure instanceof Error ? failure.message : String(failure)); }
    finally { inFlight.current = false; setSaving(false); }
  }, [diskVersion]);
  useEffect(() => { const key = (event: KeyboardEvent) => {
    const target = event.target as HTMLElement; const typing = ['INPUT', 'TEXTAREA', 'SELECT'].includes(target.tagName) || target.isContentEditable;
    if ((event.ctrlKey || event.metaKey) && event.key.toLowerCase() === 's') { event.preventDefault(); if (!modal && !loading) void save(); }
    if (typing || modal || loading) return;
    if ((event.ctrlKey || event.metaKey) && event.key.toLowerCase() === 'z') { event.preventDefault(); if (event.shiftKey) editor.redo(); else editor.undo(); }
    if (event.code === 'Space') { event.preventDefault(); setPlaying(v => !v); }
  }; window.addEventListener('keydown', key); return () => window.removeEventListener('keydown', key); }, [save, editor, modal, loading]);
  useEffect(() => { if (modal) dialog.current?.showModal(); else dialog.current?.close(); }, [modal]);
  const switchProject = (next: VfxProject, version: string | null, stamp: string) => { editor.replace(next); setDiskVersion(version); setSavedStamp(stamp); setSelected(next.layers[0]?.id ?? null); setSolo(null); setPlaying(false); setFrame(Math.min(Math.round(next.fps * 0.25), lastFrame(next))); setError(''); setModal(null); viewport.current?.reset(); };
  const safeSwitch = () => !dirty || window.confirm('当前工程有未保存修改，继续将放弃这些修改。是否继续？');
  const openList = async () => { if (saving || loading) return; setModal('open'); setListing(true); setError(''); try { setProjects(await listProjects()); } catch (e) { setError(e instanceof Error ? e.message : String(e)); } finally { setListing(false); } };
  const open = async (id: string) => { if (loading || !safeSwitch()) return; setLoading(true); try { const result = await loadProject(id); switchProject(result.project, result.version, fingerprint(result.project)); setMessage('已从本机打开工程'); } catch (e) { setError(e instanceof Error ? e.message : String(e)); } finally { setLoading(false); } };
  const addLayer = (kind: LayerKind) => { const next = createLayer(kind, project.duration); editor.edit(p => { p.layers.push(next); }); setSelected(next.id); };
  const copyLayer = () => { if (!layer) return; const next = structuredClone(layer); next.id = newId(); next.name = (next.name + ' 副本').slice(0, 80); editor.edit(p => { p.layers.push(next); }); setSelected(next.id); };
  const saveCopy = () => { if (saving || loading) return; const next = structuredClone(project); next.id = newId(); next.name = (next.name + ' 副本').slice(0, 80); next.revision = 0; switchProject(next, null, ''); setMessage('已创建独立副本，请保存'); };
  return <main className="workbench">
    <header className="app-header"><div className="brand"><div className="brand-mark">V<span>✦</span></div><div><strong>VFX<span>STUDIO</span></strong><small>TIRE FLY / AUTHORING LAB</small></div><span className="version">0.1 / FOUNDATION</span></div><div className="header-actions"><button disabled={saving || loading} onClick={() => setModal('new')}>新建</button><button disabled={saving || loading} onClick={() => void openList()}>打开</button><button disabled={saving || loading} onClick={saveCopy}>另存副本</button><button className="primary" disabled={saving || loading} onClick={() => void save()}>{saving ? '保存中…' : '保存工程'}</button><button className="theme-button" aria-label="切换明暗主题" onClick={() => setTheme(v => v === 'dark' ? 'light' : 'dark')}>{theme === 'dark' ? '☀' : '☾'}</button></div></header>
    <div className="project-bar"><div className="project-identity"><span className="project-dot"/><NameInput key={project.id} label="工程名称" value={project.name} change={value => editor.edit(p => { p.name = value; })}/><span className="dirty-label">{dirty ? '未保存' : '已保存'}</span></div><div className="history"><button disabled={!editor.past.length || loading} onClick={editor.undo}>↶ 撤销</button><button disabled={!editor.future.length || loading} onClick={editor.redo}>↷ 重做</button><span>{templateLabels[project.template.id]} / TEMPLATE v{project.template.version}</span></div></div>
    <Layers layers={project.layers} selected={selected} solo={solo} select={setSelected} setSolo={setSolo} toggle={id => editor.edit(p => { const l = p.layers.find(item => item.id === id); if (l) l.visible = !l.visible; })} add={addLayer} duplicate={copyLayer} remove={() => editor.edit(p => { p.layers = p.layers.filter(item => item.id !== selected); })}/>
    <section className="preview panel" aria-label="预览面板"><header className="preview-toolbar"><h2>预览 <span>/ PERSPECTIVE</span></h2><div><label>环绕 <select aria-label="环绕鼠标键" value={orbit} onChange={e => setOrbit(e.target.value as 'left' | 'right')}><option value="left">左键拖动</option><option value="right">右键拖动</option></select></label><button aria-pressed={grid} className={grid ? 'active' : ''} onClick={() => setGrid(v => !v)}>网格</button><button onClick={() => viewport.current?.reset()}>重置视角</button></div></header>
    <Viewport ref={viewport} project={project} time={frame / project.fps} solo={solo} orbit={orbit} grid={grid}/>
    <div className="viewport-bottom"><span>{orbit === 'left' ? '左拖环绕 · 右拖平移' : '右拖环绕 · 左拖平移'} · 滚轮距离</span><div><select aria-label="相机书签" defaultValue="" onChange={e => { const b = project.bookmarks.find(item => item.id === e.target.value); if (b) viewport.current?.use(b); e.target.value = ''; }}><option value="">机位书签 ({project.bookmarks.length})</option>{project.bookmarks.map(b => <option value={b.id} key={b.id}>{b.name}</option>)}</select><button disabled={project.bookmarks.length >= 16} onClick={() => { const bookmark = viewport.current?.capture('机位 ' + (project.bookmarks.length + 1)); if (bookmark) editor.edit(p => { p.bookmarks.push(bookmark); }); }}>＋ 记机位</button><button disabled={!project.bookmarks.length} onClick={() => editor.edit(p => { p.bookmarks.pop(); })}>移除末项</button></div></div>
    </section>
    <Inspector project={project} layer={layer} edit={editor.edit} tab={tab} setTab={setTab}/>
    <Timeline project={project} frame={frame} playing={playing} selected={selected} seek={v => { setPlaying(false); setFrame(Math.max(0, Math.min(lastFrame(project), v))); }} play={() => setPlaying(v => !v)} select={setSelected} loop={() => editor.edit(p => { p.loop = !p.loop; })}/>
    <footer className="statusbar"><span className={error ? 'error-text' : ''} role={error ? 'alert' : 'status'}>{error || message}{!error && dirty && !saving ? ' · 当前有未保存修改' : ''}</span><span>Ctrl+S 保存 · Space 播放 · Ctrl+Z 撤销 <b>真实 AI / 烘焙导出未接入</b></span></footer>
    <dialog ref={dialog} className="project-dialog" onCancel={e => { if (loading) e.preventDefault(); else setModal(null); }}><header><div><span className="eyebrow">{modal === 'new' ? 'CREATE FROM TEMPLATE' : 'LOCAL PROJECTS'}</span><h2>{modal === 'new' ? '从一份灵感开始。' : '继续你的创作。'}</h2></div><button aria-label="关闭工程对话框" disabled={loading} onClick={() => setModal(null)}>×</button></header>
    {modal === 'new' ? <div className="template-cards">{(['magic', 'slash', 'impact'] as TemplateKind[]).map(kind => <button key={kind} className={'template-card ' + kind} onClick={() => { if (safeSwitch()) { switchProject(createProject(kind), null, ''); setMessage('已创建独立工程 · 请保存'); } }}><span className="template-visual">{kind === 'magic' ? '◎' : kind === 'slash' ? '⌁' : '✳'}</span><strong>{templateLabels[kind]}</strong><small>{kind === 'magic' ? '循环环形 / 空间星尘' : kind === 'slash' ? '弧形条带 / 锋线余迹' : '闪光核心 / 空间爆发'}</small><em>程序化示例 · 非 AI 生成</em></button>)}</div> : <div className="project-list">{listing ? <p>正在读取本机工程…</p> : projects.length ? projects.map(item => <button disabled={loading} key={item.id} onClick={() => void open(item.id)}><span><strong>{item.name}</strong><small>{templateLabels[item.templateId]} · {new Date(item.updatedAt).toLocaleString()}</small></span><span>打开 →</span></button>) : <p className="empty">尚无已保存工程。保存后会显示在这里。</p>}</div>}
    {error && <p role="alert" className="error-text">{error}</p>}<p className="dialog-note">工程保存在本机工具目录 .local/projects，不上传云端。</p></dialog>
  </main>;
}
