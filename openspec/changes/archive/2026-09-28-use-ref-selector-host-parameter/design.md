# 设计

`UPARAM(ref)` 应使 UHT 形参同时具备引用和输出标记，UnrealSharp 的 UHT 导出器据此生成 `ref IList<FTcsCombatEntityHandle>`。C# 实现接收的是容器复制封送的列表，因此只做 `Add`，由回调出口写回原生数组；TCS 调用方在进入选择器前清空目标集。

静态生成与 C++ 编译不构成行为证明。需在重新生成的绑定中确认 `ref`，并在同一 PIE 装置中检查 selector 顺序、过滤器、空 Host、GC 后重入。当前未提交的 UnrealSharp `out` 修复保持原状；本变更不使用其 `RefKind.Out` 分支，后续仍需评估其他 API 是否依赖它。

## 生成器边界

静态检查显示，C++ 调用 C# 回调的 `Invoke_ResolveTargets` 会读取输入数组、调用 `ref` 实现并向原生缓冲区写回；但 C# 主动调用托管类自身 `ResolveTargets(ref ...)` 的生成方法只写入参数缓冲区、调用原生函数，没有随后读回 `ref` 参数。本提案的 selector 验收仅覆盖 C++ → C# 回调路径，不能据此宣称该反向调用路径已正确。若未来需要 C# 主动调用它，应另行处理生成器缺口。

## PIE 结果

`Documents/combat-system-design/evidence/2026-09-28-selector-ref-pie.md` 记录新签名下的目标保序 `[1,2]`、过滤结果 `[2]`、空 Host 路径，以及 `GC_READY → Cmd: Obj GC → GC_CHECK_BEGIN → POST_GC` 后的 selector 重入。此次验证只证明当前夹具的 C++ → C# 路径；随后撤销 UnrealSharp 的 `out` 修补，从原版源码重建生成器、重新发布 LAC 程序集并完成第二轮 PIE；完整证据见同一证据文件。全新克隆从零构建仍未执行。
