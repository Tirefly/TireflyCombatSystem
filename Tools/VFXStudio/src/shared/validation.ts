import { Ajv } from 'ajv';
import type { ValidationResult, VfxProject } from './types.js';
const num = (minimum: number, maximum: number) => ({ type: 'number', minimum, maximum });
const integer = (minimum: number, maximum: number) => ({ type: 'integer', minimum, maximum });
const text = (maxLength = 80) => ({ type: 'string', minLength: 1, maxLength });
const id = { ...text(64), pattern: '^[a-zA-Z0-9_-]+$' };
const vec = { type: 'array', items: num(-1000, 1000), minItems: 3, maxItems: 3 };
const object = (properties: Record<string, unknown>) => ({ type: 'object', properties, required: Object.keys(properties), additionalProperties: false });
export const projectSchema = object({
  schemaVersion: { const: 1 }, id, name: text(), revision: integer(0, 2147483647),
  template: object({ id: { enum: ['magic', 'slash', 'impact'] }, version: integer(1, 9999) }),
  duration: num(0.1, 30), fps: { enum: [24, 30, 60] }, seed: integer(0, 2147483647), loop: { type: 'boolean' },
  layers: { type: 'array', maxItems: 32, items: object({
    id, name: text(), kind: { enum: ['ring', 'rays', 'particles', 'ribbon', 'disc'] }, visible: { type: 'boolean' },
    start: num(0, 30), end: num(0.001, 30),
    transform: object({ position: vec, rotation: vec, scale: num(0.01, 20) }),
    appearance: object({ color: { type: 'string', pattern: '^#[0-9a-fA-F]{6}$' }, opacity: num(0, 1), blend: { enum: ['normal', 'additive'] }, wireframe: { type: 'boolean' } }),
    shape: object({ radius: num(0.01, 20), width: num(0.005, 5), count: integer(1, 1000), arc: num(1, 360) }),
    motion: object({ speed: num(-10, 10), spin: num(-720, 720), spread: num(0, 2), fadeIn: num(0, 0.5), fadeOut: num(0, 0.5) }),
  }) },
  bookmarks: { type: 'array', maxItems: 16, items: object({ id, name: text(), position: vec, target: vec }) },
});
const ajv = new Ajv({ strict: true, allErrors: true, coerceTypes: false, useDefaults: false, removeAdditional: false });
const check = ajv.compile<VfxProject>(projectSchema);
/** Structural and semantic validation never rewrites an AI candidate or user document. */
export function validateProject(value: unknown): ValidationResult {
  if (!check(value)) return { valid: false, errors: (check.errors ?? []).slice(0, 8).map(e => (e.instancePath || '/') + ' ' + e.message) };
  const project = value as VfxProject;
  const errors: string[] = [];
  const ids = new Set<string>();
  for (const item of [...project.layers, ...project.bookmarks]) {
    if (ids.has(item.id)) errors.push('重复ID: ' + item.id);
    ids.add(item.id);
  }
  for (const layer of project.layers) if (layer.start >= layer.end || layer.end > project.duration) errors.push(layer.name + ': 时间区间必须位于工程时长内且起点早于终点');
  for (const b of project.bookmarks) if (Math.hypot(...b.position.map((v, i) => v - b.target[i])) < 0.01) errors.push(b.name + ': 相机不能与目标重合');
  return errors.length ? { valid: false, errors } : { valid: true, project };
}
