import Fastify from 'fastify';
import fastifyStatic from '@fastify/static';
import { randomBytes } from 'node:crypto';
import path from 'node:path';
import { fileURLToPath } from 'node:url';
import { validateProject } from '../shared/validation.js';
import { ProjectStorage, StorageError } from './storage.js';
const toolRoot = fileURLToPath(new URL('../../', import.meta.url));
const port = Number(process.env.VFX_PORT ?? 4317);
if (!Number.isInteger(port) || port < 1024 || port > 65535) throw new Error('VFX_PORT 必须为1024–65535的整数');
const token = randomBytes(32).toString('hex');
const app = Fastify({ logger: true, bodyLimit: 1024 * 1024 });
const storage = new ProjectStorage(path.join(toolRoot, '.local', 'projects'));
const origins = new Set(['http://127.0.0.1:' + port, 'http://localhost:' + port]);
if (process.env.VFX_DEV === '1') { origins.add('http://127.0.0.1:5173'); origins.add('http://localhost:5173'); }
app.addHook('onRequest', async (request, reply) => {
  if (!new Set(['127.0.0.1:' + port, 'localhost:' + port]).has(request.headers.host ?? '')) return reply.code(403).send({ message: 'Host不受信任' });
  const origin = request.headers.origin;
  if (origin && !origins.has(origin)) return reply.code(403).send({ message: 'Origin不受信任' });
  // External links may navigate to the workbench, but must never read the local APIs.
  const entryNavigation = request.method === 'GET' && request.url.split('?')[0] === '/' && request.headers['sec-fetch-mode'] === 'navigate' && request.headers['sec-fetch-dest'] === 'document';
  if (request.headers['sec-fetch-site'] === 'cross-site' && !entryNavigation) return reply.code(403).send({ message: '拒绝跨站请求' });
  if (request.method !== 'GET' && request.method !== 'HEAD' && request.headers['x-vfx-token'] !== token) return reply.code(403).send({ message: '会话授权无效，请刷新页面' });
});
app.addHook('onSend', async (_request, reply) => {
  reply.header('X-Content-Type-Options', 'nosniff');
  reply.header('Referrer-Policy', 'no-referrer');
  reply.header('Content-Security-Policy', "default-src 'self'; script-src 'self'; style-src 'self' 'unsafe-inline'; img-src 'self' data: blob:; connect-src 'self'; object-src 'none'; base-uri 'self'; frame-ancestors 'none'");
  reply.header('Cache-Control', 'no-store');
});
app.setErrorHandler((error, _request, reply) => {
  const status = error instanceof StorageError ? error.statusCode : (error as { statusCode?: number }).statusCode ?? 500;
  app.log.error(error);
  reply.code(status).send({ message: status >= 500 ? '本机服务发生错误，请检查终端和存储目录权限；当前编辑不会被覆盖' : error instanceof Error ? error.message : String(error) });
});
app.get('/api/session', async () => ({ token, version: '0.1.0' }));
app.get('/api/projects', async () => ({ projects: await storage.list() }));
app.get<{ Params: { id: string } }>('/api/projects/:id', async request => storage.load(request.params.id));
app.put<{ Params: { id: string } }>('/api/projects/:id', async (request, reply) => {
  const body = request.body as { project?: unknown; expectedVersion?: unknown } | null;
  if (!body || typeof body !== 'object' || Array.isArray(body) || Object.keys(body).some(k => !['project', 'expectedVersion'].includes(k)) || !(body.expectedVersion === null || typeof body.expectedVersion === 'string')) return reply.code(400).send({ message: '保存请求格式无效' });
  const result = validateProject(body.project);
  if (!result.valid) return reply.code(422).send({ message: result.errors.join('；') });
  if (result.project.id !== request.params.id) return reply.code(400).send({ message: '路径ID与工程ID不一致' });
  const version = await storage.save(result.project, body.expectedVersion as string | null);
  return { version };
});
await app.register(fastifyStatic, { root: path.join(toolRoot, 'dist', 'client'), index: 'index.html' });
try { await app.listen({ port, host: '127.0.0.1' }); console.log('VFXStudio http://127.0.0.1:' + port); }
catch (error) { app.log.error(error); process.exitCode = 1; }
const shutdown = async () => { await app.close(); };
process.once('SIGINT', () => void shutdown());
process.once('SIGTERM', () => void shutdown());
