import type { VfxLayer } from '../shared/types.js';
export interface EvaluatedLayer { active: boolean; opacity: number; scale: number; spin: number; progress: number; particles: Float32Array | null; }
export function layerSeed(seed: number, id: string): number {
  let hash = seed | 0;
  for (let i = 0; i < id.length; i++) hash = Math.imul(hash ^ id.charCodeAt(i), 16777619);
  return hash >>> 0;
}
function random(seed: number, index: number): number {
  let x = Math.imul(seed ^ Math.imul(index + 1, 374761393), 668265263);
  x = Math.imul(x ^ (x >>> 13), 1274126177);
  return ((x ^ (x >>> 16)) >>> 0) / 4294967296;
}
/** All supported foundation modules are analytic; no wall-clock or previous-frame state enters this function. */
export function evaluateLayer(layer: VfxLayer, time: number, seed: number): EvaluatedLayer {
  const progress = Math.max(0, Math.min(1, (time - layer.start) / (layer.end - layer.start)));
  const active = layer.visible && time >= layer.start && time < layer.end;
  const fadeIn = layer.motion.fadeIn === 0 ? 1 : Math.min(1, progress / layer.motion.fadeIn);
  const fadeOut = layer.motion.fadeOut === 0 ? 1 : Math.min(1, (1 - progress) / layer.motion.fadeOut);
  const elapsed = Math.max(0, time - layer.start);
  const particles = layer.kind === 'particles' ? new Float32Array(layer.shape.count * 3) : null;
  if (particles) {
    const hashed = layerSeed(seed, layer.id);
    for (let i = 0; i < layer.shape.count; i++) {
      const a = random(hashed, i * 5) * Math.PI * 2;
      const y = 1 - random(hashed, i * 5 + 1) * layer.motion.spread;
      const radial = Math.sqrt(Math.max(0, 1 - y * y));
      const speed = layer.motion.speed * (0.35 + random(hashed, i * 5 + 2) * 0.65);
      const originRadius = layer.shape.radius * Math.sqrt(random(hashed, i * 5 + 3));
      const travel = elapsed * speed;
      particles[i * 3] = Math.cos(a) * (originRadius + travel * radial);
      particles[i * 3 + 1] = travel * y;
      particles[i * 3 + 2] = Math.sin(a) * (originRadius + travel * radial);
    }
  }
  const expansion = ['ring', 'rays', 'disc'].includes(layer.kind) ? Math.max(0.01, 1 + elapsed * layer.motion.speed) : 1;
  return { active, opacity: layer.appearance.opacity * fadeIn * fadeOut, scale: layer.transform.scale * expansion,
    spin: elapsed * layer.motion.spin * Math.PI / 180, progress, particles };
}
