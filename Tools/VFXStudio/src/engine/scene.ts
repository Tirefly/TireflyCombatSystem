import * as THREE from 'three';
import { OrbitControls } from 'three/addons/controls/OrbitControls.js';
import type { CameraBookmark, VfxLayer, VfxProject } from '../shared/types.js';
import { evaluateLayer } from './evaluate.js';
interface LayerObject { group: THREE.Group; object: THREE.Mesh | THREE.Points; material: THREE.MeshBasicMaterial | THREE.PointsMaterial; layer: VfxLayer; }
export interface SceneStats { calls: number; triangles: number; points: number; }
export class VfxScene {
  readonly renderer: THREE.WebGLRenderer;
  readonly camera = new THREE.PerspectiveCamera(42, 1, 0.05, 200);
  readonly controls: OrbitControls;
  private scene = new THREE.Scene();
  private layers: LayerObject[] = [];
  private project: VfxProject | null = null;
  private grid: THREE.GridHelper;
  private particleTexture: THREE.CanvasTexture;
  constructor(canvas: HTMLCanvasElement) {
    this.renderer = new THREE.WebGLRenderer({ canvas, antialias: true, alpha: false, powerPreference: 'high-performance' });
    this.renderer.setPixelRatio(Math.min(window.devicePixelRatio, 2));
    this.renderer.outputColorSpace = THREE.SRGBColorSpace;
    this.renderer.toneMapping = THREE.NoToneMapping;
    this.scene.background = new THREE.Color('#10171e');
    this.controls = new OrbitControls(this.camera, canvas);
    this.controls.enableDamping = false;
    this.controls.minDistance = 0.5;
    this.controls.maxDistance = 40;
    this.controls.maxPolarAngle = Math.PI * 0.94;
    this.grid = new THREE.GridHelper(12, 24, '#38515b', '#24343d');
    this.grid.position.y = -0.08;
    (this.grid.material as THREE.Material).transparent = true;
    (this.grid.material as THREE.Material).opacity = 0.5;
    this.scene.add(this.grid);
    const image = document.createElement('canvas'); image.width = image.height = 64;
    const context = image.getContext('2d');
    if (!context) throw new Error('无法创建粒子纹理');
    const gradient = context.createRadialGradient(32, 32, 0, 32, 32, 32);
    gradient.addColorStop(0, 'rgba(255,255,255,1)'); gradient.addColorStop(0.25, 'rgba(255,255,255,0.9)'); gradient.addColorStop(1, 'rgba(255,255,255,0)');
    context.fillStyle = gradient; context.fillRect(0, 0, 64, 64);
    this.particleTexture = new THREE.CanvasTexture(image);
    this.particleTexture.colorSpace = THREE.SRGBColorSpace;
    this.resetCamera();
  }
  setOrbitButton(button: 'left' | 'right'): void {
    this.controls.mouseButtons.LEFT = button === 'left' ? THREE.MOUSE.ROTATE : THREE.MOUSE.PAN;
    this.controls.mouseButtons.RIGHT = button === 'right' ? THREE.MOUSE.ROTATE : THREE.MOUSE.PAN;
    this.controls.mouseButtons.MIDDLE = THREE.MOUSE.DOLLY;
  }
  resize(width: number, height: number): void {
    if (!width || !height) return;
    this.camera.aspect = width / height; this.camera.updateProjectionMatrix(); this.renderer.setSize(width, height, false);
  }
  resetCamera(): void { this.camera.position.set(5, 4.8, 7); this.controls.target.set(0, 0.35, 0); this.controls.update(); }
  cameraBookmark(name: string): CameraBookmark { return { id: crypto.randomUUID(), name, position: this.camera.position.toArray(), target: this.controls.target.toArray() }; }
  useBookmark(bookmark: CameraBookmark): void { this.camera.position.fromArray(bookmark.position); this.controls.target.fromArray(bookmark.target); this.controls.update(); }
  setGrid(visible: boolean): void { this.grid.visible = visible; }
  private clearLayers(): void {
    for (const item of this.layers) { this.scene.remove(item.group); item.object.geometry.dispose(); item.material.dispose(); }
    this.layers = [];
  }
  setProject(project: VfxProject): void {
    this.project = project; this.clearLayers();
    for (const layer of project.layers) {
      let geometry: THREE.BufferGeometry;
      if (layer.kind === 'ring') { geometry = new THREE.RingGeometry(Math.max(0.001, layer.shape.radius - layer.shape.width), layer.shape.radius, 128, 1, 0, layer.shape.arc * Math.PI / 180); geometry.rotateX(-Math.PI / 2); }
      else if (layer.kind === 'disc') { geometry = new THREE.CircleGeometry(layer.shape.radius, 96); geometry.rotateX(-Math.PI / 2); }
      else if (layer.kind === 'particles') { geometry = new THREE.BufferGeometry(); geometry.setAttribute('position', new THREE.BufferAttribute(new Float32Array(layer.shape.count * 3), 3)); }
      else if (layer.kind === 'rays') {
        const positions: number[] = [];
        for (let i = 0; i < Math.min(layer.shape.count, 256); i++) {
          const a = i / layer.shape.count * Math.PI * 2;
          const length = 0.16 + (i % 3) * layer.shape.width * 2;
          const r = layer.shape.radius, w = layer.shape.width;
          for (const [radius, offset] of [[r, -w], [r, w], [r + length, 0]]) positions.push(Math.cos(a) * radius + Math.sin(a) * offset, 0.015, Math.sin(a) * radius - Math.cos(a) * offset);
        }
        geometry = new THREE.BufferGeometry(); geometry.setAttribute('position', new THREE.Float32BufferAttribute(positions, 3));
      } else {
        const vertices: number[] = []; const indices: number[] = [];
        const segments = 96, arc = layer.shape.arc * Math.PI / 180;
        for (let i = 0; i <= segments; i++) {
          const u = i / segments, a = -arc * 0.5 + u * arc;
          const taper = Math.pow(Math.sin(Math.PI * u), 0.65);
          for (const inner of [false, true]) {
            const r = layer.shape.radius - (inner ? layer.shape.width * taper : 0);
            vertices.push(Math.cos(a) * r - layer.shape.radius * 0.45, Math.sin(a) * r, Math.sin(u * Math.PI) * 0.25);
          }
          if (i < segments) { const n = i * 2; indices.push(n, n + 1, n + 2, n + 1, n + 3, n + 2); }
        }
        geometry = new THREE.BufferGeometry(); geometry.setAttribute('position', new THREE.Float32BufferAttribute(vertices, 3)); geometry.setIndex(indices);
      }
      const common = { color: layer.appearance.color, transparent: true, opacity: layer.appearance.opacity, depthWrite: false,
        blending: layer.appearance.blend === 'additive' ? THREE.AdditiveBlending : THREE.NormalBlending };
      const material = layer.kind === 'particles' ? new THREE.PointsMaterial({ ...common, map: this.particleTexture, size: layer.shape.width * 2.2, sizeAttenuation: true })
        : new THREE.MeshBasicMaterial({ ...common, side: THREE.DoubleSide, wireframe: layer.appearance.wireframe });
      const object = layer.kind === 'particles' ? new THREE.Points(geometry, material as THREE.PointsMaterial) : new THREE.Mesh(geometry, material as THREE.MeshBasicMaterial);
      object.frustumCulled = false; // Analytic particles move outside their initial bounds; foundation scenes are intentionally small.
      const group = new THREE.Group(); group.add(object); group.position.fromArray(layer.transform.position);
      group.rotation.set(...layer.transform.rotation.map(v => v * Math.PI / 180) as [number, number, number]);
      this.scene.add(group); this.layers.push({ group, object, material, layer });
    }
  }
  render(time: number, soloId: string | null): SceneStats {
    if (this.project) for (const item of this.layers) {
      const evaluated = evaluateLayer(item.layer, time, this.project.seed);
      item.group.visible = evaluated.active && (!soloId || item.layer.id === soloId);
      item.group.scale.setScalar(evaluated.scale);
      item.object.rotation.y = item.layer.kind === 'ribbon' ? 0 : evaluated.spin;
      if (item.layer.kind === 'ribbon') item.object.rotation.z = evaluated.spin;
      item.material.opacity = evaluated.opacity;
      if (evaluated.particles) { const attribute = item.object.geometry.getAttribute('position') as THREE.BufferAttribute; attribute.array.set(evaluated.particles); attribute.needsUpdate = true; }
    }
    this.controls.update(); this.renderer.render(this.scene, this.camera);
    return { calls: this.renderer.info.render.calls, triangles: this.renderer.info.render.triangles, points: this.renderer.info.render.points };
  }
  dispose(): void { this.clearLayers(); this.controls.dispose(); this.grid.geometry.dispose(); (this.grid.material as THREE.Material).dispose(); this.particleTexture.dispose(); this.renderer.dispose(); }
}
