## MODIFIED Requirements
### Requirement: FinalizeStateRemoval 保持八步顺序

`UTcsStateComponent::FinalizeStateRemoval` SHALL 严格按以下顺序执行步骤，不允许重排，也不允许跳过：

1. Validate `StateInstance` and `StateDef`
2. If StateTree is running, call `StopStateTree()`
3. Attempt stage transition to `SS_Expired`; return early if it fails (idempotency)
4. Remove from local runtime caches: `StateTreeTickScheduler`, `DurationTracker`, `StateInstanceIndex`
5. If `SourceHandle.IsValid()` is true, clear modifiers created by this state via `StateInstance->GetOwnerAttributeComponent()->RemoveModifiersBySourceHandle(SourceHandle)`
6. Broadcast `NotifyStateStageChanged` and `NotifyStateRemoved`
7. Remove from slot containers and request slot activation refresh
8. `MarkPendingGC()`

#### Scenario: Modifier 清理使用 SourceHandle 有效谓词
- **WHEN** `FinalizeStateRemoval` executes Step 5 on a state whose `SourceHandle.IsValid()` is true, including a source handle whose `Id == 0`
- **THEN** 调用路径 MUST 直接是 `StateInstance->GetOwnerAttributeComponent()->RemoveModifiersBySourceHandle(...)`，而 NOT 是 `AttrMgr->RemoveModifiersBySourceHandle(OwnerActor, ...)`
- **AND** 该路径 MUST NOT 使用 `SourceHandle.Id > 0` 作为是否清理 modifier 的判断

#### Scenario: 子类覆写仍保持顺序
- **WHEN** 某个子类覆写 `FinalizeStateRemoval` 并调用 `Super::FinalizeStateRemoval(...)`
- **THEN** 上述八个步骤 MUST 仍按原顺序执行
