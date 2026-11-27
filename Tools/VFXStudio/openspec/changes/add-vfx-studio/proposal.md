# Change: Add VFXStudio authoring foundation

## Why
为独立的本机 Web 游戏特效制作工具建立正式第一阶段，替代一次性界面草图。工具位于 TCS Tools，但不依赖或修改 UE 战斗系统。

## What Changes
- 提供真实三维四区工作台、法阵/刀光/Impact 模板、图层基础模块、时间定位、撤销重做和本机工程保存重开。
- 建立严格工程契约和仅监听回环地址的文件服务。
- 明确 AI 未配置、完整烘焙导出与曲线轨迹高级编辑为后续阶段，不伪造结果。

## Impact
- Affected specs: vfx-authoring-foundation（新增，工具内管理）。
- Affected code: 本工具 src/client、src/engine、src/shared、src/server 及构建配置。
- 不修改插件描述、UE Source、现有工具、TCS V2 文档或宿主规格。
- 用户已在会话批准第一阶段及主代理实现、独立评审；无提交/推送授权。
