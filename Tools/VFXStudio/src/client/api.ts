import type { ProjectSummary, VfxProject } from '../shared/types.js';
let token: string | null = null;
async function request<T>(url: string, options?: RequestInit): Promise<T> {
  const controller = new AbortController(); const timeout = window.setTimeout(() => controller.abort(), 15000);
  try {
    const response = await fetch(url, { ...options, signal: controller.signal });
    const value = await response.json() as T & { message?: string };
    if (!response.ok) throw new Error(value.message ?? '本机服务请求失败');
    return value;
  } catch (error) { if (error instanceof DOMException && error.name === 'AbortError') throw new Error('本机服务响应超时，请确认服务仍在运行'); throw error; }
  finally { window.clearTimeout(timeout); }
}
export async function connect(): Promise<void> { token = (await request<{ token: string }>('/api/session')).token; }
export async function listProjects(): Promise<ProjectSummary[]> { return (await request<{ projects: ProjectSummary[] }>('/api/projects')).projects; }
export async function loadProject(id: string): Promise<{ project: VfxProject; version: string }> { return request('/api/projects/' + encodeURIComponent(id)); }
export async function saveProject(project: VfxProject, expectedVersion: string | null): Promise<string> {
  if (!token) await connect();
  const response = await request<{ version: string }>('/api/projects/' + encodeURIComponent(project.id), { method: 'PUT', headers: { 'Content-Type': 'application/json', 'X-VFX-Token': token! }, body: JSON.stringify({ project, expectedVersion }) });
  return response.version;
}
