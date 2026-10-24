## REMOVED Requirements

### Requirement: 状态施加步骤（ApplyState）

**Reason**: 本能力 1 条需求的主语是 `TcsBuff`（`FTcsStepApplyState` 住 `TcsBuff`——`effect-step-dispatch` 的领域自持步骤），不是共享层 ⇒ 名字谎报主语（`ARCH-05` D-1）。

**Migration**: 需求全文已**逐字迁入** `buff-step-library`（本变更的 `ADDED`，正文除能力名互引外无改动）。
