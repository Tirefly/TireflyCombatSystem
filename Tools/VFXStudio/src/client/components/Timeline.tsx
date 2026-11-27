import type { VfxProject } from '../../shared/types.js';
import { lastFrame } from '../../shared/types.js';
interface Props { project: VfxProject; frame: number; playing: boolean; selected: string | null; seek(frame: number): void; play(): void; select(id: string): void; loop(): void; }
export function Timeline(p: Props) {
  const max = lastFrame(p.project);
  return <section className="timeline panel" aria-label="时间轴">
    <header className="timeline-toolbar"><div className="playback"><button aria-label="首帧" onClick={() => p.seek(0)}>⏮</button><button aria-label="上一帧" onClick={() => p.seek(p.frame - 1)}>‹</button><button className="play-button" aria-label={p.playing ? '暂停' : '播放'} onClick={p.play}>{p.playing ? 'Ⅱ' : '▶'}</button><button aria-label="下一帧" onClick={() => p.seek(p.frame + 1)}>›</button><button aria-label="末帧" onClick={() => p.seek(max)}>⏭</button><button className={p.project.loop ? 'active' : ''} aria-pressed={p.project.loop} onClick={p.loop}>循环</button></div><div className="timecode"><strong>{(p.frame / p.project.fps).toFixed(3)}<small> s</small></strong><span>帧 {p.frame} / {max} · {p.project.fps} FPS</span></div></header>
    <div className="time-ruler"><span>0.000 s</span><span>{(p.project.duration / 2).toFixed(3)} s</span><span>{p.project.duration.toFixed(3)} s</span></div>
    <input className="seek" aria-label="时间定位帧" type="range" min="0" max={max} step="1" value={p.frame} onChange={e => p.seek(Number(e.target.value))}/>
    <div className="tracks">{p.project.layers.map(layer => <button className={'track ' + (layer.id === p.selected ? 'selected-track' : '')} key={layer.id} onClick={() => p.select(layer.id)} aria-label={'时间轨 ' + layer.name}><span>{layer.name}</span><div className="track-lane"><i style={{ left: layer.start / p.project.duration * 100 + '%', width: (layer.end - layer.start) / p.project.duration * 100 + '%', backgroundColor: layer.appearance.color }}/><b style={{ left: (max ? p.frame / max : 0) * 100 + '%' }}/></div></button>)}</div>
  </section>;
}
