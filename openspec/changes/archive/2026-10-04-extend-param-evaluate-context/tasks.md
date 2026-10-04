## 1. 实现

- [x] 1.1 `FTcsParamEvaluateContext` 追加 `Subject`（`FTcsCombatEntityHandle`）与 `EffectiveLevel`（`int32`，0 = 无等级语义）两字段，均 `UPROPERTY(BlueprintReadWrite)`，附取值归属注释
- [x] 1.2 `TcsParamValueSource.h` 头部口径注释改写：`Subject` 句柄/级别位字段"M2/M5 轮补齐" → "随 TcsState 等级源同批补齐（2026-10-04，M2 已收束，原文过期）"
- [x] 1.3 新增 `Source/TcsCore/Public/Parameter/TcsParamEnumerableSource.h`：`FTcsParamEnumerableSource : FTcsParamValueSource`（`USTRUCT(meta = (Hidden))` + `TCSCORE_API`），一个中性默认实现（**提交前修正**：原写的 `GetIndexCount` 全库无设计出处且无消费者，已删——PV-10 的 `Enumerate` 与其派生形态统一留白到展示层落地时）

## 2. 验证

- [x] 2.1 编译：UBT Development Editor 零 error / 零 warning（含 `UPROPERTY` ⇒ 补跑 Shipping）——双配置均 Succeeded，日志零 warning 零 error；UHT 产物 `NewProp_Subject` / `NewProp_EffectiveLevel` 已进反射表
- [ ] 2.2 回归：宿主 `Tcs.Test.Slice.Run` 19/0 且零红字（零行为变更的直接证据）——**PIE 内人工验证点，待用户执行**

## 3. 收束

- [x] 3.1 台账 `STAT-2` 改记"部分消费"（基类 + 两个索引入口已落地；`Enumerate` 留待展示层）
- [x] 3.2 提案归档 + `specs/param-value/` 同步——**2026-10-04 归档为 `changes/archive/2026-10-04-extend-param-evaluate-context`**（`param-value`：+1 ADDED / ~1 MODIFIED；`openspec validate --all --strict` = 26 passed / 0 failed、`changes/` 零活动提案）
