import { useState } from 'react';
import type { LayerKind, VfxLayer } from '../../shared/types.js';
import { layerLabels } from '../../shared/types.js';
interface Props { layers: VfxLayer[]; selected: string | null; solo: string | null; select(id: string): void; setSolo(id: string | null): void; toggle(id: string): void; add(kind: LayerKind): void; duplicate(): void; remove(): void; }
const symbols: Record<LayerKind, string> = { ring: '◎', rays: '✳', particles: '⠿', ribbon: '⌁', disc: '●' };
export function Layers(p: Props) {
  const [kind, setKind] = useState<LayerKind>('particles');
  return <section className="layers panel" aria-label="图层面板">
    <header className="panel-title"><h2>图层</h2><span>{p.layers.length.toString().padStart(2, '0')} LAYERS</span></header>
    <div className="layer-list">
      {!p.layers.length && <p className="empty">还没有图层。添加一个基础模块开始制作。</p>}
      {p.layers.map((layer, index) => <div className={'layer-row ' + (p.selected === layer.id ? 'selected' : '') + (!layer.visible ? ' muted-layer' : '')} key={layer.id}>
        <button className="visibility icon-button" aria-label={layer.name + (layer.visible ? ' 隐藏' : ' 显示')} aria-pressed={layer.visible} onClick={() => p.toggle(layer.id)}>{layer.visible ? '◉' : '○'}</button>
        <button className="layer-select" aria-label={'选择 ' + layer.name} onClick={() => p.select(layer.id)}><span className="layer-symbol" style={{ color: layer.appearance.color }}>{symbols[layer.kind]}</span><span><strong>{layer.name}</strong><small>{layerLabels[layer.kind]} <i>· {String(index + 1).padStart(2, '0')}</i></small></span></button>
        <button className={'solo icon-button ' + (p.solo === layer.id ? 'active' : '')} aria-label={layer.name + ' Solo'} aria-pressed={p.solo === layer.id} onClick={() => p.setSolo(p.solo === layer.id ? null : layer.id)}>S</button>
      </div>)}
    </div>
    <div className="layer-actions"><select aria-label="新增图层类型" value={kind} onChange={e => setKind(e.target.value as LayerKind)}>{Object.entries(layerLabels).map(([value, label]) => <option value={value} key={value}>{label}</option>)}</select><button disabled={p.layers.length >= 32} onClick={() => p.add(kind)}>＋ 添加</button></div>
    <div className="layer-footer"><button disabled={!p.selected || p.layers.length >= 32} onClick={p.duplicate}>复制图层</button><button disabled={!p.selected} onClick={p.remove}>删除图层</button></div>
    <div className="phase-note"><span>FOUNDATION / 01</span><p>图层与基础模块可编辑<br/>曲线、轨迹和素材编辑在后续阶段接入。</p></div>
  </section>;
}
