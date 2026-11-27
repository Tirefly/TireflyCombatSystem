import { mkdir, readFile, readdir, rename, stat, unlink, writeFile } from 'node:fs/promises';
import path from 'node:path';
import { randomUUID } from 'node:crypto';
import type { ProjectSummary, VfxProject } from '../shared/types.js';
import { validateProject } from '../shared/validation.js';
export class StorageError extends Error { constructor(message: string, public statusCode = 500) { super(message); } }
export interface StoredProject { project: VfxProject; version: string; }
export class ProjectStorage {
  private queues = new Map<string, Promise<unknown>>();
  constructor(private readonly root: string) {}
  private file(id: string): string {
    if (!/^[a-zA-Z0-9_-]{1,64}$/.test(id)) throw new StorageError('工程ID不合法', 400);
    return path.join(this.root, id + '.json');
  }
  async load(id: string): Promise<StoredProject> {
    try {
      const raw = JSON.parse(await readFile(this.file(id), 'utf8')) as StoredProject;
      const result = validateProject(raw.project);
      if (!result.valid || result.project.id !== id || typeof raw.version !== 'string') throw new StorageError('磁盘工程格式无效，原文件已保留', 422);
      return { project: result.project, version: raw.version };
    } catch (error) {
      if ((error as NodeJS.ErrnoException).code === 'ENOENT') throw new StorageError('未找到工程', 404);
      throw error;
    }
  }
  async list(): Promise<ProjectSummary[]> {
    await mkdir(this.root, { recursive: true });
    const entries = await readdir(this.root);
    const result: ProjectSummary[] = [];
    for (const entry of entries.filter(e => /^[a-zA-Z0-9_-]{1,64}\.json$/.test(e))) {
      try {
        const id = entry.slice(0, -5);
        const [{ project }, info] = await Promise.all([this.load(id), stat(this.file(id))]);
        result.push({ id, name: project.name, templateId: project.template.id, updatedAt: info.mtime.toISOString() });
      } catch { /* A corrupt file must not prevent opening other projects; loading it by ID still reports the error. */ }
    }
    return result.sort((a, b) => b.updatedAt.localeCompare(a.updatedAt));
  }
  async save(project: VfxProject, expectedVersion: string | null): Promise<string> {
    const previous = this.queues.get(project.id) ?? Promise.resolve();
    const current = previous.catch(() => undefined).then(async () => {
      await mkdir(this.root, { recursive: true });
      let existing: StoredProject | null = null;
      try { existing = await this.load(project.id); } catch (error) { if (!(error instanceof StorageError && error.statusCode === 404)) throw error; }
      if ((existing?.version ?? null) !== expectedVersion) throw new StorageError('磁盘版本已变化，请先另存工程或重新打开，避免覆盖其他窗口的修改', 409);
      const version = randomUUID();
      const destination = this.file(project.id);
      const temporary = destination + '.' + randomUUID() + '.tmp';
      try {
        await writeFile(temporary, JSON.stringify({ version, project }, null, 2) + '\n', { encoding: 'utf8', flag: 'wx' });
        await rename(temporary, destination);
      } finally { await unlink(temporary).catch(() => undefined); }
      return version;
    });
    this.queues.set(project.id, current);
    try { return await current; } finally { if (this.queues.get(project.id) === current) this.queues.delete(project.id); }
  }
}
