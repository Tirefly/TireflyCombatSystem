import { useEffect, useRef, useState } from 'react';
import type { VfxLayer, VfxProject } from '../../shared/types.js';
import { layerLabels } from '../../shared/types.js';
interface NumericProps { label: string; value: number; min: number; max: number; step?: number; unit?: string; change(value: number): void; }
export function Numeric(p: NumericProps) {
  const [draft, setDraft] = useState(String(p.value)); const cancelled = useRef(false);
  useEffect(() => setDraft(String(p.value)), [p.value]);
  const commit = () => { if (cancelled.current) { cancelled.current = false; setDraft(String(p.value)); return; } const n = Number(draft); if (draft.trim() && Number.isFinite(n)) { const value = Math.min(p.max, Math.max(p.min, p.step === 1 ? Math.round(n) : n)); p.change(value); setDraft(String(value)); } else setDraft(String(p.value)); };
  return <label className="numeric field"><span>{p.label}</span><div><input type="number" aria-label={p.label} value={draft} min={p.min} max={p.max} step={p.step ?? 0.01} onChange={e => setDraft(e.target.value)} onBlur={commit} onKeyDown={e => { if (e.key === 'Enter') e.currentTarget.blur(); if (e.key === 'Escape') { cancelled.current = true; e.currentTarget.blur(); } }}/>{p.unit && <small>{p.unit}</small>}</div></label>;
}
export function NameInput(p: { label: string; value: string; change(value: string): void }) {
  const [draft, setDraft] = useState(p.value); const cancelled = useRef(false);
  useEffect(() => setDraft(p.value), [p.value]);
  const commit = () => { const value = draft.trim(); if (!cancelled.current && value) { p.change(value); setDraft(value); } else setDraft(p.value); cancelled.current = false; };
  return <input aria-label={p.label} value={draft} maxLength={80} onChange={e => setDraft(e.target.value)} onBlur={commit} onKeyDown={e => { if (e.key === 'Enter') e.currentTarget.blur(); if (e.key === 'Escape') { cancelled.current = true; e.currentTarget.blur(); } }}/>;
}
interface Props { project: VfxProject; layer?: VfxLayer; edit(change: (draft: VfxProject) => void): void; tab: 'properties' | 'ai'; setTab(tab: 'properties' | 'ai'): void; }
export function Inspector(p: Props) {
  const layer = p.layer;
  const update = (change: (draft: VfxLayer) => void) => p.edit(project => { const target = project.layers.find(l => l.id === layer?.id); if (target) change(target); });
  return <aside className="inspector panel" aria-label="属性与AI面板"><div className="inspector-tabs"><button className={p.tab === 'properties' ? 'chosen' : ''} onClick={() => p.setTab('properties')}>属性 / MODULES</button><button className={p.tab === 'ai' ? 'chosen' : ''} onClick={() => p.setTab('ai')}>AI 创作</button></div>
    <div className="inspector-body">{p.tab === 'ai' ? <div className="ai-state"><span className="eyebrow">AI WORKFLOW</span><h2>先搭好创作底座。</h2><p>第一阶段尚未接入 AI 供应商，没有发送任何生成请求，也不会用预设替换冒充 AI 生成。</p><div className="status-pill">未配置 · 后续阶段</div><h3>规划中的双层流程</h3><ol><li>生成贴图、遮罩等素材</li><li>按模板与 Schema 装配候选</li><li>锁定检查、比较、应用与撤销</li></ol><p className="subtle">当前可使用三个内置模板，通过图层和基础模块手工精修。</p></div> : <>
      <details className="module project-module" open><summary><span>工程设置</span><small>PROJECT</small></summary><div className="module-content">
        <Numeric label="工程时长" value={p.project.duration} min={0.1} max={30} unit="s" change={v => p.edit(project => { project.duration = v; project.layers.forEach(l => { l.end = Math.min(l.end, v); l.start = Math.min(l.start, l.end - 0.001); }); })}/>
        <div className="two-fields"><label className="field"><span>帧率</span><select aria-label="工程帧率" value={p.project.fps} onChange={e => p.edit(project => { project.fps = Number(e.target.value); })}>{[24, 30, 60].map(f => <option key={f} value={f}>{f} FPS</option>)}</select></label><Numeric label="随机种子" value={p.project.seed} min={0} max={2147483647} step={1} change={v => p.edit(project => { project.seed = v; })}/></div>
      </div></details>
      {layer ? <><div className="layer-heading"><span style={{ background: layer.appearance.color }}/><div><h2>{layer.name}</h2><p>{layerLabels[layer.kind]} · 基础模块</p></div></div>
      <label className="field name-field"><span>图层名称</span><NameInput key={layer.id} label="图层名称" value={layer.name} change={value => update(l => { l.name = value; })}/></label>
      <details className="module" open><summary><span>01 · 时间与变换</span><small>INITIALIZE</small></summary><div className="module-content"><div className="two-fields"><Numeric label="开始时间" value={layer.start} min={0} max={layer.end - 0.001} unit="s" change={v => update(l => { l.start = v; })}/><Numeric label="结束时间" value={layer.end} min={layer.start + 0.001} max={p.project.duration} unit="s" change={v => update(l => { l.end = v; })}/></div>
      <div className="three-fields">{['X', 'Y', 'Z'].map((axis, i) => <Numeric key={axis} label={'位置 ' + axis} value={layer.transform.position[i]} min={-100} max={100} change={v => update(l => { l.transform.position[i] = v; })}/>)}</div>
      <div className="three-fields">{['X', 'Y', 'Z'].map((axis, i) => <Numeric key={axis} label={'旋转 ' + axis} value={layer.transform.rotation[i]} min={-360} max={360} unit="°" change={v => update(l => { l.transform.rotation[i] = v; })}/>)}</div>
      <Numeric label="整体缩放" value={layer.transform.scale} min={0.01} max={20} change={v => update(l => { l.transform.scale = v; })}/></div></details>
      <details className="module" open><summary><span>02 · 形状与生成</span><small>SHAPE</small></summary><div className="module-content"><div className="two-fields"><Numeric label={layer.kind === 'particles' ? '生成半径' : '半径'} value={layer.shape.radius} min={0.01} max={20} change={v => update(l => { l.shape.radius = v; })}/><Numeric label={layer.kind === 'particles' ? '粒子尺寸' : '宽度'} value={layer.shape.width} min={0.005} max={5} change={v => update(l => { l.shape.width = v; })}/></div>
      {(layer.kind === 'particles' || layer.kind === 'rays') && <Numeric label="数量" value={layer.shape.count} min={1} max={layer.kind === 'rays' ? 256 : 1000} step={1} change={v => update(l => { l.shape.count = v; })}/>}
      {(layer.kind === 'ring' || layer.kind === 'ribbon') && <Numeric label="弧度范围" value={layer.shape.arc} min={1} max={360} unit="°" change={v => update(l => { l.shape.arc = v; })}/>}
      {layer.kind === 'ribbon' && <p className="module-hint">当前为预制弧形条带。可编辑扫动轨迹与录制轨迹导入将在第二阶段接入。</p>}
      </div></details>
      <details className="module" open><summary><span>03 · 生命周期</span><small>UPDATE</small></summary><div className="module-content">
      {layer.kind !== 'ribbon' && <Numeric label={layer.kind === 'particles' ? '运动速度' : '扩张速度'} value={layer.motion.speed} min={-10} max={10} change={v => update(l => { l.motion.speed = v; })}/>}
      <Numeric label="旋转速度" value={layer.motion.spin} min={-720} max={720} unit="°/s" change={v => update(l => { l.motion.spin = v; })}/>
      {layer.kind === 'particles' && <Numeric label="发散范围" value={layer.motion.spread} min={0} max={2} change={v => update(l => { l.motion.spread = v; })}/>}
      <div className="two-fields"><Numeric label="淡入占比" value={layer.motion.fadeIn} min={0} max={0.5} change={v => update(l => { l.motion.fadeIn = v; })}/><Numeric label="淡出占比" value={layer.motion.fadeOut} min={0} max={0.5} change={v => update(l => { l.motion.fadeOut = v; })}/></div><p className="module-hint">占比相对于本图层生命周期；曲线关键帧不在本阶段。</p>
      </div></details>
      <details className="module" open><summary><span>04 · 外观</span><small>RENDER</small></summary><div className="module-content"><label className="field color-field"><span>主色</span><input aria-label="图层主色" type="color" value={layer.appearance.color} onChange={e => update(l => { l.appearance.color = e.target.value; })}/><code>{layer.appearance.color.toUpperCase()}</code></label>
      <Numeric label="不透明度" value={layer.appearance.opacity} min={0} max={1} change={v => update(l => { l.appearance.opacity = v; })}/><label className="field"><span>混合模式</span><select aria-label="混合模式" value={layer.appearance.blend} onChange={e => update(l => { l.appearance.blend = e.target.value as 'normal' | 'additive'; })}><option value="additive">加法 · 发光</option><option value="normal">普通 · Alpha</option></select></label>
      {layer.kind !== 'particles' && <label className="check-field"><input type="checkbox" checked={layer.appearance.wireframe} onChange={e => update(l => { l.appearance.wireframe = e.target.checked; })}/>线框检查</label>}
      </div></details></> : <p className="empty">选择或添加图层，编辑对应模块。</p>}
    </>}</div>
  </aside>;
}
