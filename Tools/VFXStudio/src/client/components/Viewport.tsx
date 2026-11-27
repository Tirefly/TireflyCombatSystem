import { forwardRef, useEffect, useImperativeHandle, useRef, useState } from 'react';
import { VfxScene, type SceneStats } from '../../engine/scene.js';
import type { CameraBookmark, VfxProject } from '../../shared/types.js';
export interface ViewportHandle { reset(): void; capture(name: string): CameraBookmark | undefined; use(bookmark: CameraBookmark): void; }
interface Props { project: VfxProject; time: number; solo: string | null; orbit: 'left' | 'right'; grid: boolean; }
export const Viewport = forwardRef<ViewportHandle, Props>(function Viewport(props, ref) {
  const canvas = useRef<HTMLCanvasElement>(null); const scene = useRef<VfxScene | null>(null); const latest = useRef(props); latest.current = props;
  const [error, setError] = useState(''); const [stats, setStats] = useState<SceneStats>({ calls: 0, triangles: 0, points: 0 });
  useImperativeHandle(ref, () => ({ reset: () => scene.current?.resetCamera(), capture: name => scene.current?.cameraBookmark(name), use: bookmark => scene.current?.useBookmark(bookmark) }), []);
  useEffect(() => {
    let animation = 0; let disposed = false; let lastStat = 0; let observer: ResizeObserver | undefined;
    const element = canvas.current!;
    const onLost = (event: Event) => { event.preventDefault(); setError('WebGL上下文已丢失。工程编辑仍保留，请先保存，再刷新页面恢复预览。'); cancelAnimationFrame(animation); };
    try {
      const instance = new VfxScene(element); scene.current = instance;
      instance.setProject(latest.current.project); instance.setOrbitButton(latest.current.orbit); instance.setGrid(latest.current.grid);
      observer = new ResizeObserver(entries => { const rect = entries[0].contentRect; instance.resize(rect.width, rect.height); }); observer.observe(element);
      const render = (now: number) => {
        if (disposed) return;
        try {
          const result = instance.render(latest.current.time, latest.current.solo);
          if (now - lastStat > 700) { setStats(result); lastStat = now; }
          animation = requestAnimationFrame(render);
        } catch (failure) { setError('三维绘制失败：' + String(failure)); }
      };
      animation = requestAnimationFrame(render); element.addEventListener('webglcontextlost', onLost);
    } catch (failure) { setError('无法创建WebGL 2预览。请启用浏览器硬件加速并使用桌面Chrome/Edge。详情：' + String(failure)); }
    return () => { disposed = true; cancelAnimationFrame(animation); observer?.disconnect(); element.removeEventListener('webglcontextlost', onLost); scene.current?.dispose(); scene.current = null; };
  }, []);
  useEffect(() => { scene.current?.setProject(props.project); }, [props.project]);
  useEffect(() => { scene.current?.setOrbitButton(props.orbit); }, [props.orbit]);
  useEffect(() => { scene.current?.setGrid(props.grid); }, [props.grid]);
  return <div className="viewport-stage">
    <canvas ref={canvas} aria-label="真实三维特效预览" onContextMenu={event => event.preventDefault()} />
    <div className="viewport-badge"><span className="live-dot"/> WEBGL 2 <span>·</span> UNLIT</div>
    <div className="axis-label"><b>X</b><i>Y</i><em>Z</em> 三维空间</div>
    <div className="render-stats" aria-label="绘制统计">{stats.calls} draws · {stats.triangles.toLocaleString()} tris · {stats.points} points</div>
    {error && <div role="alert" className="viewport-error"><strong>预览不可用</strong><p>{error}</p></div>}
  </div>;
});
