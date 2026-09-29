# TCS selector ref 容器小范围验证证据

- **文档 ID**：`EVID-2026-09-28-selector-ref`
- **类型**：EVID / 证据
- **状态**：FROZEN
- **权威范围**：selector ref 容器回写两轮 PIE 证据（含撤销修补复测）
- **最后更新**：2026-09-28

- 验证范围：TCS `ResolveTargets(..., UPARAM(ref) TArray<...>&)`；LAC C# `ResolveTargets_Implementation(..., ref IList<...>)` 依次追加 caster、secondary。地图 `L_UnrealSharpDev`，夹具 `BP_TcsHostScriptingE2EProbe`。
- 用户粘贴：`C:/Users/TireflyPC/.codex/attachments/ed6ed291-e1cd-4b72-ac49-563ee4eadbd1/已粘贴的文本.txt`；筛选片段省略原生 GC 命令。
- 完整来源：`E:/Projects_Dev/LegendAutoChess/Saved/Logs/LegendAutoChess.log`。取证时读取 364517 字节，SHA-256 `fc81b979ba42e4d1cb4364ec6aae07ef7f5d78f9ba14f119667e825c2ffad8c9`；这是首轮快照；编辑器重启后同名日志可能被覆盖，下面的第二轮独立记录另有哈希。以下行号来自取证时的完整日志，单次 PIE 窗口为 2511–2661 行。

| 验证点 | 完整日志行 | 判定 |
|---|---:|---|
| C# selector 先产出两个目标；两个 filter 淘汰 caster、保留 secondary；运行态只剩目标 2 | 2520–2524、2530 | 通过 |
| 未过滤 selector 保序 `[1,2]` | 2552–2556 | 通过 |
| 空 selector Host 警告 + 空集；空 filter Host 全通过 | 2559–2563、2566–2570 | 通过 |
| `GC_READY` 之后执行 `Cmd: Obj GC`、`Collecting garbage` 与对象哈希表压缩；之后才进入 `GC_CHECK_BEGIN` | 2612、2618–2621 | 原生 GC 顺序成立 |
| GC 后 C# selector 第四次调用、filter 再次执行并保留目标 2 | 2623–2631 | 通过 |
| GC 后 Flow、Damage delegate 回归 | 2639、2650 | 通过；Health 100→93 |

在 2511–2661 行的这次 PIE 窗口内未见探针 `FAIL`、`ensure` 或 `Fatal`。2559 行的空 selector Warning、以及无效句柄/流程中止等预期 Warning 属夹具刻意覆盖的降级路径；2643、2651 行的 `No world was found` 来自瞬态伤害公式对象输出屏显时的上下文噪声，伤害计算仍有 2650 行结果。

## 证据边界（首轮）

这次日志证明 **TCS 原生转发 → C# `ref IList` 回调 → 原生数组回写**在此夹具场景成立，也证明同一 PIE 世界内一次手动 `Obj GC` 后继续成立。生成绑定确认接口是 `ref IList`，因此这条 selector 路径未进入先前 UnrealSharp 的 `RefKind.Out` 修补分支。此次构建时本地 UnrealSharp 源码仍保留该未提交修补，没有在未修补的干净 checkout 中独立编译；C# 主动调用同名方法的反向 `ref` 回写亦未验证。跨 PIE 世界的注册表寿命与其他脚本语言均不在本次结论内。

## 原版 UnrealSharp 生成器复测

用户撤销 UnrealSharp 的 `out` 修补后，核对 `git diff --cached` 和 `git diff` 均为空；仅有先前留下的非跟踪 `Managed/UnrealSharp/Binaries/`。从此源码重新编译 `UnrealSharp.GlueGenerator` 成功（5 条既有 RS2000/RS2001 分析器发布追踪警告、0 错误），部署至插件的生成器 DLL SHA-256 为 `c3ae88cc153f38a4cf0a36c4efa63b735dea0ea5672f64aabb048a2780ea86b3`。用此生成器不增量编译 LAC C# 工程成功（0 警告、0 错误），生成的 `Invoke_ResolveTargets` 继续以 `ref` 调用实现并将列表写回原生缓冲区；UnrealSharp 的 `BuildUserSolution` 成功，将新 `LegendAutoChessCS.dll` 发布到 `Binaries/Managed/net10.0`。

第二轮 Editor 启动日志 1777、1780、1791 行明确从该路径加载 LAC 程序集；其启动时间晚于新 DLL 的发布。这轮粘贴文件为 `C:/Users/TireflyPC/.codex/attachments/40c925a4-1cb2-4818-a677-23b675e95871/已粘贴的文本.txt`；完整日志同路径 `E:/Projects_Dev/LegendAutoChess/Saved/Logs/LegendAutoChess.log`，本轮快照为 364653 字节，SHA-256 `d590a7784a19daffc97b13c51152ccda03b46e5416b417d6e851645b95ee8609`。

| 验证点 | 第二轮完整日志行 | 判定 |
|---|---:|---|
| Selector 产出两个目标，过滤后仅保留 secondary=2 | 2516–2520、2526 | 通过 |
| 未过滤 `[1,2]`、空 selector Host 空集 + Warning、空 filter Host 保留 `[1,2]` | 2548–2566 | 通过 |
| `GC_READY → Cmd: Obj GC → Collecting garbage → 哈希表压缩 → GC_CHECK_BEGIN` | 2608、2614–2617 | 原生 GC 顺序成立 |
| GC 后 selector 第四次抵达、filter 再次执行，结果目标=2 | 2619–2627 | 通过 |
| GC 后 Flow 和 Damage delegate 继续执行，Health 100→93 | 2635、2646 | 通过 |

这次 PIE 窗口 2507–2657 行未见探针 `FAIL`、`ensure` 或 `Fatal`；空 Host Warning 与瞬态 DamageFormula 的 `No world was found` 屏显上下文 Warning 不改变上述结果。

**结论**：本项目已在原版 UnrealSharp 生成器源码与重新发布的程序集上验证 TCS 原生调用 → LAC C# `ref IList` → 原生目标集回写及同次 PIE 的 GC 后重入，证明此 selector 场景不依赖原先的 `out` 修补。仍未进行全新克隆从零构建；C# 主动调用同名方法的反向 `ref` 回写、其他脚本语言和跨 PIE 寿命不在结论内。
