import type { LayerKind, TemplateKind, VfxLayer, VfxProject } from './types.js';
import { layerLabels, templateLabels } from './types.js';
export function newId(): string { return crypto.randomUUID(); }
export function createLayer(kind: LayerKind, duration: number): VfxLayer {
  return { id: newId(), name: layerLabels[kind], kind, visible: true, start: 0, end: duration,
    transform: { position: [0, 0, 0], rotation: [0, 0, 0], scale: 1 },
    appearance: { color: '#ffbf69', opacity: 0.9, blend: 'additive', wireframe: false },
    shape: { radius: 1.5, width: 0.08, count: kind === 'particles' ? 100 : 24, arc: 360 },
    motion: { speed: kind === 'particles' ? 1.8 : 0, spin: 25, spread: 1, fadeIn: 0.08, fadeOut: 0.3 } };
}
export function createProject(kind: TemplateKind): VfxProject {
  const duration = kind === 'magic' ? 3 : 0.8;
  const project: VfxProject = { schemaVersion: 1, id: newId(), name: templateLabels[kind] + ' · 未命名', revision: 0,
    template: { id: kind, version: 1 }, duration, fps: 60, seed: 2718, loop: true, layers: [], bookmarks: [] };
  const add = (type: LayerKind, name: string, color: string, radius: number, width: number) => {
    const l = createLayer(type, duration); l.name = name; l.appearance.color = color; l.shape.radius = radius; l.shape.width = width; project.layers.push(l); return l;
  };
  if (kind === 'magic') {
    add('ring', '外环 · 回旋边界', '#f4bb64', 2.1, 0.025);
    const inner = add('ring', '内环 · 聚能', '#ffde9e', 1.65, 0.07); inner.motion.spin = -18;
    const runes = add('rays', '刻线 · 符文节律', '#f6c879', 1.88, 0.055); runes.shape.count = 32; runes.motion.spin = 12;
    const core = add('disc', '阵心 · 呼吸光', '#ff9d47', 0.7, 0.1); core.appearance.opacity = 0.25;
    const sparks = add('particles', '上浮 · 星尘', '#ffde9e', 1.8, 0.045); sparks.motion.speed = 0.5; sparks.shape.count = 100;
  } else if (kind === 'slash') {
    const slash = add('ribbon', '刃面 · 弧光', '#64d9ed', 2.3, 0.5); slash.shape.arc = 155; slash.motion.spin = 0;
    const edge = add('ribbon', '锋线 · 高光', '#d5fbff', 2.38, 0.045); edge.shape.arc = 155; edge.motion.spin = 0;
    const trail = add('ribbon', '余迹 · 外沿', '#4185b4', 2.12, 0.72); trail.shape.arc = 140; trail.appearance.opacity = 0.25; trail.start = 0.05; trail.motion.spin = 0;
    const p = add('particles', '碎光 · 散落', '#b2f7ff', 0.25, 0.04); p.shape.count = 70; p.motion.speed = 3; p.motion.spread = 0.25;
  } else {
    const flash = add('disc', '闪光 · 核心', '#fff3cf', 0.65, 0.1); flash.end = 0.32;
    const shock = add('ring', '冲击 · 扩散环', '#ffaa55', 1.8, 0.07); shock.motion.speed = 2;
    const rays = add('rays', '放射 · 尖刺', '#ffdd94', 0.45, 0.04); rays.shape.count = 18; rays.motion.speed = 2.5;
    const p = add('particles', '碎片 · 空间爆发', '#ffac66', 0.1, 0.055); p.shape.count = 180; p.motion.speed = 4;
  }
  return project;
}
