# Tasks: 参数源族宿主插槽

> 内容对应 [`PLN-R5`](../../../Documents/combat-system-design/plans/plan-r5-state-layer.md) Task 3 Step 9（R4.5-b）；调研结论见 `Documents/combat-system-design/ledger/reflection-backlog.md` 的《调研 R-1》。

## 1. 接口与转发器

- [x] 1.1 新建 `Source/TcsCore/Public/Parameter/TcsParamSourceHost.h`：`UINTERFACE(MinimalAPI, Blueprintable) ITcsParamSourceHost` + 两个 `UFUNCTION(BlueprintNativeEvent)`（`Evaluate(const FTcsParamEvaluateContext&) -> double` / `AllowsValueConvention() -> bool`），接口内声明 `_Implementation` 中性默认体
- [x] 1.2 **同文件**放 `FTcsParamSource_HostDelegate : FTcsParamValueSource`（`UPROPERTY TScriptInterface<ITcsParamSourceHost> Host` + 两个虚函数纯转发 + 空 Host 守卫 0 / true）——**接口与转发器同住一文件**（同族先例 `TcsParamSource_AttributeScaled.h`；两者是同一机制的两半）
- [x] 1.3 头注释写明"本类是全插件**第一个持 `TScriptInterface` 的参数源 USTRUCT**"（形态先例在转发器族与求值上下文，不在参数源族）
- [x] 1.4 补 `FTcsParamValueSource` 的虚析构（`C4265` 的根因修复；多态基类的正确性要求）

## 2. 编译与核验

- [x] 2.1 UBT Development Editor + Game Shipping 双配置编译，零 error 零 warning —— **✅ 2026-10-04 双绿**
- [x] 2.2 glue 产物核验：两个方法生成真实方法体（非空壳）——"能导出 ≠ 能往返" —— **✅** `TcsParamSourceHost.generated.cs`：`Evaluate` / `AllowsValueConvention` 均含 `CallGetNativeFunctionFromClassAndName` + 参数偏移 + 返回位偏移；`FTcsParamSource_HostDelegate.generated.cs` 的 `Host` 字段含 `ITcsParamSourceHostMarshaller.FromNative` / `ToNative`（**双向真实读写**）
- [ ] 2.3 C# 实现实测：宿主源配进 `FTcsParamValue.Source` 后求值走脚本实现；**复用两段式命令驱动的既有探针形态**（C++ 驱动基类 + C# 子类），不得另写一套 —— **顺延**：本步需要宿主脚本夹具（`Script/LegendAutoChessCS/TcsDevGcFixtures/` + `Tcs.Test.Gc.Arm/Verify` 那条链路），已记入 `EVID-2026-10-04-state-param-snapshot-and-level-sources` §5 边界⑦，**归 R5 Task 7 的端到端验收**（或单独补一次探针）
- [x] 2.4 回归：既有求值路径未受影响（常规命令零红字）—— **✅** `Tcs.Test.Slice.Run` 即时 32/0 + 延迟 39/0、`Tcs.Test.Slice.Reject` 9/0；既有求值路径（链步骤数值 / 修正器操作数 / 流程操作数）读数与 Task 2 收束时一致
