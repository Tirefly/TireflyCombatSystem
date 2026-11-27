# VFXStudio Foundation Implementation Plan

> For agentic workers: 使用 executing-plans 按顺序执行，完成后进行整体评审。独立评审工具受阻后，用户已批准改为主代理专项自审，交付必须注明未经独立评审。用户规则覆盖技能默认：不采用TDD、不新增自动测试套件、不提交推送。

**Goal:** 交付可实际操作、可本机保存重开的特效编辑器第一阶段。
**Architecture:** 独立工具；shared权威契约，engine显式求值/绘制，client工作台，server受限文件服务。
**Tech Stack:** TypeScript / React / Three.js WebGL2 / Node / Fastify / Ajv / Vite / pnpm。
**Spec:** specs/vfx-authoring-foundation/spec.md；设计见同目录design.md。

## Global Constraints
- 代码仅本工具目录；当前工作区直接增量实现，不切分支不迁移用户目录；已核对插件工作区无既有改动。
- UTF-8无BOM、LF、末尾单换行；不改TCS运行时代码和现有工具。
- 使用类型检查、生产构建、定向浏览器人工操作和临时验证调用，不创建测试套件。
- 真实AI和烘焙导出不在第一阶段；状态必须如实标注。

## Review Focus
- 极端参数/NaN/未知字段 → 拒绝非法输入，避免无界粒子分配。
- 保存期间继续编辑/切换工程 → 不把新版本误标已保存，不被旧响应替换。
- WebGL创建失败/窗口尺寸改变 → 可见错误或正确重排，不残留WebGL资源。
- 删除最后一层/无选中层 → 空状态仍可添加，不崩溃。
- 跨站请求/路径注入/同ID并发保存 → 服务保护和原子替换有效。

## 1. 工程契约与基础服务
Files: package.json、pnpm-lock.yaml、tsconfig.json、tsconfig.server.json、vite.config.ts、index.html、.gitignore；src/shared/types.ts、validation.ts、templates.ts；src/server/index.ts、storage.ts。
Interfaces: createProject(kind: TemplateKind): VfxProject；validateProject(value: unknown): ValidationResult；listProjects(): Promise<ProjectSummary[]>；saveProject(project: VfxProject, expectedVersion: string | null): Promise<string>。
- [x] 1.1 创建工具脚手架、严格工程契约及三模板。
- [x] 1.2 实现受限列表/读取/保存API、原子存储、Host/Origin/token校验。
- [x] 1.3 pnpm.cmd run typecheck通过；定向请求核对合法/拒绝分支。

## 2. 确定性3D求值与工作台
Files: src/engine/evaluate.ts、scene.ts；src/client/main.tsx、App.tsx、store.ts、api.ts、components/Viewport.tsx、Layers.tsx、Inspector.tsx、Timeline.tsx、styles.css。
Interfaces: evaluateLayer(layer: VfxLayer, time: number, seed: number): EvaluatedLayer；VfxScene.setProject(project: VfxProject): void；VfxScene.render(time: number, soloId: string | null): void；VfxScene.dispose(): void。
- [x] 2.1 创建真实WebGL2场景、环绕映射、缩放平移、重置与机位书签。
- [x] 2.2 接入四区UI、三模板创建、层编辑、增删复制显隐Solo、撤销重做。
- [x] 2.3 接入固定帧播放/暂停/seek/单步、基础模块属性、保存/打开状态保护。
- [x] 2.4 pnpm.cmd run build通过；浏览器检查主操作、空态、窄屏、主题、错误。

## 3. 交付与评审
Files: README.md；本清单进度与结果记录。
- [x] 3.1 写明安装/构建/启动/数据位置/阶段边界及人工验证步骤。
- [x] 3.2 核对同帧回放、保存重开、未授权/非法数据拒绝、AI未配置提示。
- [x] 3.3 按用户批准改为主代理专项自审最终代码；修复重要问题后重新构建与相关验证（未经独立评审）。
- [x] 3.4 核对最终diff、编码及改动范围；交付运行地址、证据与未验证项。

## Execution ledger
- 用户批准第一阶段与native执行；提案据获批范围起草；未授权任何版本控制提交。
- Pre-flight: shared VfxProject供server验证/存储和engine/client共同消费，显示名不作引用键；scene只读工程，client命令是唯一编辑入口。
- Ruling: 直接在指定工具新目录实现，不用worktree迁移路径；工作区基线已核对且本工具目录不存在，防止隔离路径与用户指定落位不一致。
- Ruling: 本阶段采用解析基础模块，不预建物理快照；高级能力仍保留后续范围。
- Review ruling: 独立评审子代理的执行入口被策略拒绝，未读取代码；经用户批准改为主代理专项自审，未经独立评审。已定向检查契约边界、保存并发/状态、三维资源释放、空图层和请求安全，不宣称第三方审查通过。
- Build: 2026-11-25，Node 24.21.0 / pnpm 12.6.0，pnpm.cmd run build通过前后端严格类型检查与生产构建；openspec.cmd validate add-vfx-studio --strict --no-interactive通过。Vite提示810.27KB的JS主块超500KB（gzip217.71KB），未做性能基准，不当作性能验收通过。
- Browser evidence: Edge实页创建法阵/刀光/Impact；真实左右键环绕后书签位置变化且保存；名称输入、半径2.1→2.5、撤销重做、保存重开、刀光末帧47/47实测。Impact显隐/Solo/复制、删至0层再添加通过。明暗主题和862×677内容视口窄屏重排通过，窄屏属性区移到下方；当前会话控制台无应用错误。
- Directed calls: 三模板结构、同帧回访、半开采样；未知字段不改写、非法时间/粒子上限/NaN拒绝；保存读取及同版本并发200/409、非法工程422、无token/外域Origin/跨站API403已实测。Node fetch会改写sec-fetch-mode，入口导航分支改用node:http验证document200、iframe403及非法ID400；不是降低服务保护。
- Final read-only check: 20项全部通过（三模板schema/replay/半开范围共9项、4项契约拒绝、6项HTTP入口/安全、28文件UTF-8无BOM/LF/末尾单换行/无行尾空白）。一次脚本转义错误修正后重跑通过；未添加自动测试套件。
- Fixes verified: strict Ajv导入和构建语法；首页顶层导航不误拦且API仍拒跨站；名称输入允许中间空值、Escape取消数值、保存先提交活动输入；粒子移动不被初始包围体错误剔除；最小字号12px。
- Limits: 未实测WebGL创建失败/上下文丢失恢复、磁盘写失败/关服务时UI、保存过程中继续编辑的人工竞态、32层上限UI、多GPU/浏览器矩阵及长期资源压力；这些仅静态审阅或留待后续。列表目前跳过损坏工程文件，未提供恢复界面。未实现真实AI、素材/PBR、曲线轨迹、自定义模板或任何烘焙导出。
- Scope: 新增28个源码/配置/文档/锁文件仅位于本工具；生成的node_modules/dist/.local已忽略。未改UE/TCS运行时代码和其他工具，未commit/push。验收用工程保留在本机数据目录，未删除用户数据。
- Delivery: 本机服务http://127.0.0.1:4317保持运行供用户验收；服务关闭后按README重新启动。
