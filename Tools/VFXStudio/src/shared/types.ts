export type TemplateKind = 'magic' | 'slash' | 'impact';
export type LayerKind = 'ring' | 'rays' | 'particles' | 'ribbon' | 'disc';
export type Vec3 = [number, number, number];
export interface VfxLayer {
  id: string;
  name: string;
  kind: LayerKind;
  visible: boolean;
  start: number;
  end: number;
  transform: { position: Vec3; rotation: Vec3; scale: number };
  appearance: { color: string; opacity: number; blend: 'additive' | 'normal'; wireframe: boolean };
  shape: { radius: number; width: number; count: number; arc: number };
  motion: { speed: number; spin: number; spread: number; fadeIn: number; fadeOut: number };
}
export interface CameraBookmark { id: string; name: string; position: Vec3; target: Vec3; }
export interface VfxProject {
  schemaVersion: 1;
  id: string;
  name: string;
  revision: number;
  template: { id: TemplateKind; version: number };
  duration: number;
  fps: number;
  seed: number;
  loop: boolean;
  layers: VfxLayer[];
  bookmarks: CameraBookmark[];
}
export interface ProjectSummary { id: string; name: string; updatedAt: string; templateId: TemplateKind; }
export type ValidationResult = { valid: true; project: VfxProject } | { valid: false; errors: string[] };
export const layerLabels: Record<LayerKind, string> = { ring: '环形', rays: '放射线', particles: '空间粒子', ribbon: '刀光条带', disc: '核心光斑' };
export const templateLabels: Record<TemplateKind, string> = { magic: '法阵', slash: '刀光', impact: 'Impact' };
export function lastFrame(project: Pick<VfxProject, 'duration' | 'fps'>): number {
  return Math.max(0, Math.ceil(project.duration * project.fps - 1e-8) - 1);
}
