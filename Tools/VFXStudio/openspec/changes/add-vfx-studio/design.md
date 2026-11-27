## Context
正式工具第一阶段。Node 24 已可用；桌面 Chrome/Edge 为本轮人工验证目标，不承诺移动端或跨 GPU 像素一致。

## Goals / Non-Goals
- Goals: 真3D、三模板精修、时间seek、工程持久化、可观察错误。
- Non-Goals: 游戏 SDK、UE/Godot 插件、通用节点图、付费AI、完整烘焙导出、曲线关键帧、录制轨迹导入（后续阶段）。

## Decisions
- TypeScript + React + Three.js/WebGL2；Node + Fastify + Ajv。Vite 构建前端，tsc 构建服务；pnpm 锁文件固定安装结果。
- 编辑器粒子与轨迹按工程时间和固定seed直接计算，不受实时帧率影响。第一阶段不提供需要历史积分的物理模块，因此不预建快照系统。
- 工程JSON为权威，不序列化Three对象。稳定图层ID；相机与Solo归工作台状态，不改图层定义。保存工程时可保存相机书签。
- 服务只监听127.0.0.1；校验Host与Origin、会话写token、输入长度/数值/未知字段；限定项目ID和存储根，写入临时文件后原子替换。凭据不进入工程；不支持任意路径API。
- 本地项目目录 .local/projects，忽略依赖、构建及用户工程。无密钥、无第三方付费调用。
- 一套工具内OpenSpec，不将独立工具规格混入宿主游戏能力。现有载体没有本工具专属位置，故新建该工具的规格目录。

## Risks / Trade-offs
- WebGL2不可用：显示明确错误，不用2D冒充3D。
- 程序化素材是首批示例，不代表AI已接入；AI页明确未配置。
- 同帧确定性只承诺定义求值，不承诺跨GPU像素相同。
- 第一阶段只有基础模块数值编辑，不把高级曲线或外部轨迹宣告完成。

## Migration Plan
全新目录增量，无旧工程迁移。保留schemaVersion并拒绝不支持的版本；不改用户其他文件。

## Open Questions
后续AI供应商/模型/费用、视频编码器与高负载指标在相应阶段前另定。
