# Change: 收束 TCS 代码、设计文档与 OpenSpec 契约漂移

## Why

TCS 当前代码已推进到 R3 收束与 R4 宿主脚本插槽实施阶段，但插件描述、项目上下文、设计文档、生效规格和未归档提案的完成状态不一致。现状会让后续实施者无法判断哪些内容已经实现、哪些只完成静态验证、哪些仍等待 PIE 或 GC 证据。

## What Changes

- 将插件描述规格更新为当前实际的七个 Runtime 模块，并保留十一模块目标架构与当前 R3 子集的区分。
- 清理 `openspec/project.md` 中过期的 current 模块、依赖和 `FName` 身份口径。
- 在 `add-host-scripting-slots` 完成并归档后，回写其 S-8 相关生效规格：运行态句柄反射性、DamageFlow 反射视图、门面访问器和 Targeting 宿主插槽。
- 明确 `ITcsEntityQuery` 仍是 C++ 专用能力；宿主脚本插槽只承诺已落地的句柄访问器，不把“脚本遍历世界”写成已完成能力。
- 将设计文档与遗留台账改为三态证据：静态实现完成、glue 产物完成、PIE/GC 行为证据完成；未验证项不得标为已落地。
- 增加代码位置、设计章节、OpenSpec requirement 和验证证据之间的追踪矩阵，作为后续归档门槛。

## Impact

- Affected specs: `plugin-descriptor`, `effect-chain`, `effect-interpreter`, `damage-primitive`, `targeting-strategy`。
- Affected documents: `openspec/project.md`、TCS combat-system-design 文档、`deferred-inputs-ledger.md`。
- Related change: `add-host-scripting-slots` MUST be reviewed and completed before this change is archived; this proposal does not silently archive or rewrite that change.
- Affected code: no implementation change is authorized by this proposal; code is inspected only as the current truth source.

## Non-Goals

- 不在本变更中实现 `TcsState`、`TcsSkill`、`TcsCue`、`TcsEditor` 或剩余 R4/R5 原语。
- 不在本变更中反射化 `ITcsEntityQuery` 的 `TFunctionRef` 接口。
- 不把未执行的 UBT、PIE、GC 或跨语言往返检查改写成已通过。
