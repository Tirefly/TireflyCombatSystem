# TCS 宿主脚本插槽 E2E：单次 PIE 与 Unreal GC 证据

- **文档 ID**：`EVID-2026-09-28-scripting-e2e`
- **类型**：EVID / 证据
- **状态**：FROZEN
- **权威范围**：宿主脚本插槽单次 PIE 与原生 GC 后执行证据（SCRIPT-8 验收）
- **最后更新**：2026-09-28

- 验证日期：2026-09-28；地图 `L_UnrealSharpDev`，夹具 `BP_TcsHostScriptingE2EProbe`。
- 原始日志：`E:/Projects_Dev/LegendAutoChess/Saved/Logs/LegendAutoChess.log`；本证据从该文件一次读取中摘录。读取时大小 369893 字节，SHA-256 `b7dd472a70f6263e2f89497f274c89acd402f3d369766dcf35fc2d1940019140`。编辑器继续运行可能追加原始日志，不影响下述带原始行号的摘录。
- 用户粘贴片段：`C:/Users/TireflyPC/.codex/attachments/1ab5b86e-9434-452e-85b8-0f8cea257090/已粘贴的文本.txt`，只含 TCS 等筛选行，省略了原生 GC 命令/引擎输出。

## 判定

| 场景 | 结果 | 原始日志行 |
|---|---|---|
| Effect 同步写入和读回、`TSR_Running` 唤醒 | 通过 | 2570–2579 |
| 已释放/代际失配句柄 | 通过，写口 Warning 属预期 | 2586–2596 |
| Selector 顺序、空 selector/filter Host | 通过，空 selector Warning 属预期 | 2603、2610、2617 |
| Damage Flow false 中止、true 继续 | 通过，false 中止 Warning 属预期 | 2626、2634 |
| Unreal GC 命令 | 在 `GC_READY` 与 `GC_CHECK_BEGIN` 之间执行 | 2656、2663–2667 |
| GC 后 selector/filter + Effect executor | 通过 | 2677 |
| GC 后 Flow executor | 通过 | 2685 |
| GC 后 Damage delegate | 通过，`Final=7`、Health 100→93 | 2693、2696 |

本机 UE 5.8 `Engine/Source/Runtime/Engine/Private/UnrealEngine.cpp:9338–9348` 对 `Obj GC` 同步调用 `CollectGarbage(GARBAGE_COLLECTION_KEEPFLAGS, true)`。`Cmd: Obj GC`、`Collecting garbage...` 和对象哈希表压缩均早于 `GC_CHECK_BEGIN`，后者在下一阶段运行。当前日志级别没有单独的 `LogGarbage` 详细统计行，不妨碍结合命令路径和同步调用判定这一轮 GC 已执行。

## 原始日志关键摘录

```text
2570: [2026.09.28-03.12.19:750][400]LogBlueprintUserMessages: [L_UnrealSharpDev] [TcsProbeEffectExecutor] sync marker roundtrip=17
2571: [2026.09.28-03.12.19:754][400]LogBlueprintUserMessages: [L_UnrealSharpDev] [TcsProbeEffectExecutor] first call -> TSR_Running
2579: [2026.09.28-03.12.19:799][401]LogBlueprintUserMessages: [L_UnrealSharpDev] [TcsProbeEffectExecutor] resumed call -> TSR_Completed
2586: [2026.09.28-03.12.19:803][401]LogBlueprintUserMessages: [L_UnrealSharpDev] [TcsHostScriptingE2EProbe] 悬空句柄读口通过：返回空/无效值
2587: [2026.09.28-03.12.19:803][401]LogBlueprintUserMessages: [L_UnrealSharpDev] [TcsHostScriptingE2EProbe] 悬空句柄写口通过：false + Warning
2596: [2026.09.28-03.12.19:805][401]LogBlueprintUserMessages: [L_UnrealSharpDev] [TcsHostScriptingE2EProbe] 代际复用通过：Index=0 旧代=1 新代=3，旧句柄不能读写新运行态
2603: [2026.09.28-03.12.19:809][401]LogBlueprintUserMessages: [L_UnrealSharpDev] [TcsHostScriptingE2EProbe] 未过滤 selector 顺序 通过：targets=[1,2]
2610: [2026.09.28-03.12.19:809][401]LogBlueprintUserMessages: [L_UnrealSharpDev] [TcsHostScriptingE2EProbe] 空 selector Host 通过：targets=[]
2617: [2026.09.28-03.12.19:809][401]LogBlueprintUserMessages: [L_UnrealSharpDev] [TcsHostScriptingE2EProbe] 空 filter Host 通过：targets=[1,2]
2626: [2026.09.28-03.12.19:816][401]LogBlueprintUserMessages: [L_UnrealSharpDev] [TcsHostScriptingE2EProbe] Damage Flow false 路径通过：第二个步骤未执行
2634: [2026.09.28-03.12.19:816][401]LogBlueprintUserMessages: [L_UnrealSharpDev] [TcsHostScriptingE2EProbe] Damage Flow true 路径通过：两个步骤均执行
2656: [2026.09.28-03.12.20:805][491]LogBlueprintUserMessages: [L_UnrealSharpDev] [TcsHostScriptingE2EProbe] GC_READY：探针侧 UObject 字段已清空；请在本次 PIE 的控制台执行 obj gc，45 秒后自动复核
2663: [2026.09.28-03.12.41:737][613]Cmd: Obj GC
2664: [2026.09.28-03.12.41:737][613]Collecting garbage and resetting GC timer.
2665: [2026.09.28-03.12.41:772][613]LogUObjectHash: Compacting FUObjectHashTables data took   0.47ms
2667: [2026.09.28-03.13.06:052][291]LogBlueprintUserMessages: [L_UnrealSharpDev] [TcsHostScriptingE2EProbe] GC_CHECK_BEGIN：下列结果只有在此前日志能证明执行过 Unreal obj gc 时才是 GC 证据
2677: [2026.09.28-03.13.06:052][291]LogBlueprintUserMessages: [L_UnrealSharpDev] [TcsHostScriptingE2EProbe] POST_GC 插槽行为通过：selector/filter 与 Effect executor 再次调用，目标=2
2685: [2026.09.28-03.13.06:052][291]LogBlueprintUserMessages: [L_UnrealSharpDev] [TcsHostScriptingE2EProbe] POST_GC Flow executor 行为通过：两个脚本流程步骤再次调用
2689: [2026.09.28-03.13.06:055][291]LogScript: Warning: Script Msg: No world was found for object (/Engine/Transient.TcsProbeDamageFormula_C_0) passed in to UEngine::GetWorldFromContextObject().
2693: [2026.09.28-03.13.06:055][291]LogTcsDamage: UTcsDamageSubsystem: 伤害记录 #1（Flow=4 源=#1 目标=#1 Base=25.000 Final=7.000 Executed=7.000 暴击=0 击杀=0）
2696: [2026.09.28-03.13.06:055][291]LogBlueprintUserMessages: [TcsProbeDamageFormula_C_0] [TcsHostScriptingE2EProbe] POST_GC Damage delegate 行为通过：Health 100 → 93，C# 公式仍扣除 7
2697: [2026.09.28-03.13.06:055][291]LogScript: Warning: Script Msg: No world was found for object (/Engine/Transient.TcsProbeDamageFormula_C_0) passed in to UEngine::GetWorldFromContextObject().
2707: [2026.09.28-03.13.07:051][294]LogBlueprintUserMessages: [L_UnrealSharpDev] [TcsHostScriptingE2EProbe] GC_CHECK_END：请对照 GC_READY 与 GC_CHECK_BEGIN 之间的原生 obj gc 日志判定 GC 证据
```

## 边界

- 仅证明同一次 PIE 世界内，探针清空字段后经 Unreal GC 的保活；进程级动态执行器注册表的跨 PIE 旧世界指针问题仍由 `reflection-backlog.md` R-2 追踪，下一轮运行前须重启 Editor。
- 2697 等处 `No world was found` Warning 来源于使用瞬态 Outer 的 `UTcsProbeDamageFormula` 调用 `PrintString`，是夹具日志上下文噪声；伤害公式实际执行，`Final=7`。本次目标窗口未出现探针 `FAIL`、`ensure` 或 `Fatal`。
- AS、Luau、Puerts/TS 没有本项目可运行的独立 TCS 夹具，继续标记未验证；`ITcsEntityQuery` 仍为 C++ 专用。
