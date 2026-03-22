# TCS Core Fixes Proposal Draft

This document is a draft to help create new OpenSpec proposals and tasks for several TCS correctness/maintainability fixes across the Attribute and State modules.

It intentionally does NOT modify code; it only describes scope, design decisions, and task breakdowns.

---

## 0. Goals / Non-goals

### Goals

- Fix Attribute SourceHandle query/removal correctness by removing dependence on unstable array indices.
- Make Attribute dynamic-range clamping behave correctly when the referenced attribute changes (e.g., HP clamped by MaxHP after MaxHP buff expires).
- Harden Attribute APIs (no silent overwrite on AddAttribute; strict validation for modifier creation inputs).
- Remove State slot update re-entrancy risks (merge removal + activation updates).
- Make same-priority tie-break in PriorityOnly slots configurable via strategy objects (CDO), not hardcoded rules.
- Simplify/centralize Slot Gate close logic to a single authoritative path.

### Non-goals

- State immunity, object pool integration, network replication of State instances (explicitly out of scope per earlier discussion).
- Forcing StateTree to stop on pending removal timeout (kept as developer responsibility).

---

## 1. Proposal Splitting Recommendation

These changes are cohesive but touch two different subsystems with different risk profiles.

### Recommended: split into 2 proposals (best reviewability)

1) **Attribute proposal** (larger but single-domain):
   - SourceHandle indexing refactor (stable IDs)
   - Strict modifier creation validation
   - AddAttribute overwrite prevention + AddAttributeByTag semantics
   - Dynamic range clamp propagation / range enforcement

2) **State proposal** (single-domain, behavior-sensitive):
   - Merge removal unified via RequestStateRemoval
   - Slot activation update: re-entrancy elimination via deferred requests
   - Same-priority tie-break strategy (UObject policy)
   - Gate close logic refactor (single authority function)

Why this split:
- Attribute and State can be reviewed/tested independently.
- Attribute indexing refactor is structural and benefits from focused review.
- State tie-break policy is gameplay-sensitive; keeping it isolated reduces risk.

### Alternative: 1 proposal (only if you want a single rollout)

Pros: one branch/PR, fewer release steps.
Cons: harder review, mixed concerns, easier to miss regressions.

### Alternative: 3 proposals (if you prefer very small steps)

1) Attribute indexing + validation
2) Attribute dynamic range enforcement
3) State activation/tie-break/gate refactor

---

## 2. Proposal A (Attribute): Stable SourceHandle Indexing + Range Enforcement + API Hardening

### 2.1. Problem: SourceHandle lookup/removal depends on unstable array indices

Current state:
- `UTcsAttributeComponent::SourceHandleIdToModifierIndices` maps SourceId -> array indices into `AttributeModifiers`.
- Deletions in `AttributeModifiers` shift indices; code currently patches indices by scanning all buckets, which is fragile and easy to desync.
- API like `RemoveModifiersBySourceHandle()` can “false fail” if the index cache drifts.

### 2.2. Design: replace “index-based cache” with “stable-id-based cache”

Key idea:
- Use `FTcsAttributeModifierInstance::ModifierInstId` as the stable identity.
- Keep `AttributeModifiers` as an array for iteration, but never store array indices in the SourceHandle cache.

#### Data structure changes (UTcsAttributeComponent)

- Replace:
  - `TMap<int32, TArray<int32>> SourceHandleIdToModifierIndices;`
- With:
  - `TMap<int32, TArray<int32>> SourceHandleIdToModifierInstIds;`  (SourceId -> ModifierInstId list)
  - `TMap<int32, int32> ModifierInstIdToIndex;`                    (ModifierInstId -> current array index)

Notes:
- These are runtime caches; they SHOULD NOT be UPROPERTY (same rationale as existing index cache).
- `SourceHandleIdToModifierInstIds` may contain stale ids; API calls should self-heal by pruning ids that no longer exist in `ModifierInstIdToIndex`.

#### Mutation rules

1) **Insert**
- After `AttributeModifiers.Add(...)`, set:
  - `ModifierInstIdToIndex[ModifierInstId] = NewIndex`
  - `SourceHandleIdToModifierInstIds[SourceId].AddUnique(ModifierInstId)` (only if SourceHandle valid)

2) **Update existing (refresh)**
- `ModifierInstId` remains stable.
- If SourceId changes:
  - Remove id from old SourceId bucket, add to new bucket.
- Always ensure `ModifierInstIdToIndex` is correct.

3) **Remove**
- Use `ModifierInstIdToIndex` to locate the element.
- Use `RemoveAtSwap(Index)` to avoid shifting the whole array.
- Update only one swapped element’s index mapping:
  - `ModifierInstIdToIndex[SwappedId] = Index`
- Remove the deleted id from:
  - `ModifierInstIdToIndex`
  - `SourceHandleIdToModifierInstIds[SourceId]` bucket (remove bucket if empty)

Tradeoff:
- `RemoveAtSwap` changes iteration order of `AttributeModifiers`. This should be acceptable because your recomputation logic already uses priority/timestamps to determine effective results; do not rely on physical array order for semantics.

#### Query/Removal APIs behavior

- `GetModifiersBySourceHandle`:
  - Read ids from `SourceHandleIdToModifierInstIds[SourceId]`
  - For each id, resolve to index via `ModifierInstIdToIndex`
  - If missing, prune the stale id from the bucket (self-heal)

- `RemoveModifiersBySourceHandle`:
  - Copy the bucket ids first (avoid mutation while iterating)
  - Remove each by id (or collect instances then call existing RemoveModifier pipeline)

### 2.3. Strict validation: modifier creation MUST reject invalid inputs

Apply these rules in:
- `UTcsAttributeManagerSubsystem::CreateAttributeModifier(...)`
- `UTcsAttributeManagerSubsystem::CreateAttributeModifierWithOperands(...)`

Rules:
- `SourceName == NAME_None`: fail + error log
- `!IsValid(Instigator) || !IsValid(Target)`: fail + error log (already exists)
- `Instigator` and `Target` MUST implement `UTcsEntityInterface`: fail + error log
- If using a RowHandle source definition in the future:
  - RowHandle not null but row missing: fail + error log (prefer failing for content correctness)

### 2.4. AddAttribute overwrite prevention

Change behavior:
- `AddAttribute`: if attribute already exists, do not overwrite; log warning; return.
- `AddAttributes`: same, per item.
- `AddAttributeByTag`: should return whether the attribute was actually added (not just “tag resolved”).

### 2.5. Dynamic Range Clamp Propagation (MaxHP -> HP scenario)

#### Observed issue

`ClampAttributeValueInRange` resolves dynamic min/max by calling `UTcsAttributeComponent::GetAttributeValue`, which reads **committed** `CurrentValue` only.
During a recalculation, this means:
- When MaxHP changes in-flight, HP’s clamp may still see the *old* MaxHP because MaxHP has not been committed yet.
- After MaxHP decreases (buff removed), HP BaseValue may remain > MaxHP because BaseValue recalculation is not triggered by CurrentValue modifier removal.

This breaks the expected invariant:
- If HP has dynamic max bound to MaxHP, then **HP (base/current)** must be <= **MaxHP (current)** at all times.

#### Proposed design: enforce attribute ranges as a first-class “post-change invariant”

Add an explicit range enforcement step that runs whenever attribute values change:

1) Recalculate values (existing behavior):
   - `RecalculateAttributeBaseValues(...)` (when base modifiers applied)
   - `RecalculateAttributeCurrentValues(...)` (when persistent modifiers added/removed/updated)

2) After (1), run **Range Enforcement Pass**:
   - Iterate attributes and clamp:
     - `BaseValue` against resolved range
     - `CurrentValue` against resolved range
   - Because ranges may depend on other attributes, run iterative passes until stable:
     - stop when no value changes OR hit `MaxIterations` (e.g., 4-8)
     - on max-iteration hit: log a warning about potential cyclic dependencies

Dynamic range resolution MUST use “latest values”:
- During the enforcement pass, resolve dynamic min/max from the *current working values* (not stale committed values).

Implementation approach options (choose one in proposal):

Option A (preferred): “in-flight resolver” support
- Extend clamp logic to accept a value resolver callback:
  - `ResolveAttributeValue(AttributeName) -> float` (reads from the working map first, fallback to component).
- Use it for both:
  - in-flight clamp during recalculation (so HP clamp can see updated MaxHP in the same pass)
  - post-pass enforcement (so dependencies are stable within the iteration)

Option B: commit-first, then enforce
- Commit recalculated values first.
- Then run enforcement using `GetAttributeValue` which now sees updated committed values.
- Still iterative to handle multi-hop dependencies.

Option A is more precise and avoids transient incorrect commits; Option B is simpler but may “momentarily commit out-of-range values” before correction.

#### Event semantics (to decide explicitly)

When enforcement clamps values:
- `OnAttributeValueChanged` SHOULD fire when `CurrentValue` changes (even if no modifier directly touched that attribute in the current batch).
- `OnAttributeBaseValueChanged` behavior can be:
  - Either: fire only when base modifiers change it (current design intent),
  - Or: also fire when enforcement changes BaseValue.

Given HP is a typical “base-stored” attribute, it is safer to broadcast base changes too if enforcement alters BaseValue, but this is a product decision. If you keep base-change events attribution-only, document that invariant enforcement may alter BaseValue silently.

---

## 3. Proposal B (State): Removal Unification + Re-entrancy Fix + Tie-break Strategy + Gate Refactor

### 3.1. Merge removal should be unified via RequestStateRemoval

Current risk:
- Merge logic directly calls Finalize removal, and Finalize triggers slot activation update, causing nested `UpdateStateSlotActivation` calls.

Design:
- For “merged out” states, do NOT directly finalize.
- Instead call `RequestStateRemoval` with:
  - `Reason = Custom`
  - `CustomReason = "MergedOut"`
- This ensures:
  - StateTree has a chance to run removal logic (if applicable)
  - removal pipeline is single-path

### 3.2. Eliminate UpdateStateSlotActivation re-entrancy via deferred requests

Design:
- Add internal “deferred activation update” mechanism in `UTcsStateManagerSubsystem`:
  - `bIsUpdatingSlotActivation`
  - `PendingSlotActivationUpdates` (TSet<FGameplayTag>)
  - `RequestUpdateStateSlotActivation(SlotTag)`

Rules:
- If currently updating, only enqueue SlotTag.
- If not updating, perform update and drain the queue afterwards.
- This ensures slot activation converges without recursion.

### 3.3. Same-priority tie-break: strategy object (CDO), not enum

Requirement:
- In `SSAM_PriorityOnly`, when multiple states share the same priority, ordering MUST be deterministic.
- But tie-break rules are domain-specific (buff vs skill), and must be extensible per project.

Design:
- Add a policy field to `FTcsStateSlotDefinition`, e.g.:
  - `TSubclassOf<UTcsStateSamePriorityPolicy> SamePriorityPolicy;`
- The policy is a UObject CDO that can:
  - Decide ordering among equal-priority candidates
  - Optionally reject new states (queue limit) or decide how to treat overflow

Minimal interface (suggested):
- `int64 GetOrderKey(const UTcsStateInstance* State) const;`
  - Higher key => earlier in ordering (or document inverse)
- Optional extension (for queue limit):
  - `bool ShouldAcceptNewState(const UTcsStateInstance* NewState, const TArray<UTcsStateInstance*>& ExistingSamePriority, FName& OutFailReason) const;`

Default built-in policies to ship with TCS:
- `UTcsStateSamePriorityPolicy_UseNewest` (buff-friendly)
  - key = ApplyTimestamp (or InstanceId fallback)
- `UTcsStateSamePriorityPolicy_UseOldest` (skill queue-friendly)
  - key = -ApplyTimestamp (or inverse compare)

How it integrates with current pipeline:
- Keep “sort by Priority” as primary key.
- For each equal-priority group, apply the policy ordering.
- PriorityOnly activation then picks the first element as active; others remain inactive unless already active (preemption policy behavior remains unchanged).

### 3.4. Gate close refactor: single authoritative function

Current: multiple functions enforce gate-close behavior and invariants.

Design:
- Create one authoritative internal function:
  - `HandleGateClosed(StateComponent, SlotTag, SlotDef, Slot)`
- It must:
  - Apply `GateCloseBehavior` (HangUp/Pause/Cancel) consistently
  - Ensure invariant: gate closed => no Active state remains in slot
- Remove duplicated enforcement paths; keep only one call site from `UpdateStateSlotActivation`.

---

## 4. Suggested Change IDs (new proposals)

If using 2 proposals:

1) `refactor-attribute-runtime-integrity`
2) `refactor-state-slot-activation-integrity`

If using 3 proposals:
- `refactor-attribute-sourcehandle-indexing`
- `fix-attribute-dynamic-range-enforcement`
- `refactor-state-slot-activation-integrity`

---

## 5. Testing Checklist (automation-level, no networking)

Attribute:
- SourceHandle remove/get remains correct after:
  - many inserts
  - random removals (including middle removals)
  - cache self-heal when stale ids exist
- AddAttribute does not overwrite existing attributes (warning expected).
- Strict modifier creation rejects invalid SourceName / non-entity instigator/target.
- Dynamic range enforcement:
  - MaxHP buff removed => HP clamps down accordingly (base and/or current per chosen semantics).

State:
- Merge removal uses RequestStateRemoval and does not cause recursive activation updates.
- PriorityOnly + equal priority ordering deterministic with policy.
- Gate close uses single function and no Active remains when closed.

