# Project Context

## Purpose
TireflyCombatSystem (TCS) is a generic, comprehensive combat system framework plugin for Unreal Engine, built on the concept of Data-Behavior Separation (定义资产 / 池化实例 / 集中执行). The legacy TCS repository was frozen (MD-3) and this repository is a from-scratch rebuild (branch `remake`), currently in the R3 implementation phase: a vertical slice covering six modules (Core / Attribute / Effect / Targeting / Damage / Integration). The plugin must serve multiple host projects — LegendAutoChess today, a future third-person ARPG later — and must remain extensible to server-authoritative networking.

## Tech Stack
- Unreal Engine 5.8, C++ plugin (`.uplugin` to be rewritten in R3 plan1 Task 0).
- Current modules: `TireflyCombatSystem` (Runtime), `TireflyCombatSystemEditor` (Editor). Target materialization is eleven compile modules (R0 §9): TcsCore, TcsNotation, TcsAttribute, TcsEffect, TcsDamage, TcsTargeting, TcsState, TcsSkill, TcsCue, TcsIntegration, TcsEditor.
- Plugin dependencies (current `.uplugin`): StateTree, GameplayStateTree, GameplayMessageRouter, TireflyObjectPool.
- Spec-driven development via OpenSpec (initialized 2026-09-10, openspec CLI ≥ 0.23).

## Project Conventions

### Code Style
- Follow the user-level `unreal-cpp-style` skill (`C:\Users\TireflyPC\.agents\skills`), including its「模块目录布局」chapter.
- Implementation types carry the `Tcs` prefix: `FTcsAttributeId`, `UTcsAttributeSubsystem`, `TTcsInstanceHandle`; enum values use abbreviation prefixes. Type names in design documents are conceptual names — plans win at execution time.
- Module directory layout (narrowed口径): module root holds only `Build.cs`, `Module.h`, `Module.cpp`; domain code lives under `Public/` / `Private/` with PascalCase domain subdirectories.
- Log channels are standalone files: `Public/<ModuleName>LogChannel.h` (DECLARE_LOG_CATEGORY_EXTERN) + `Private/<ModuleName>LogChannel.cpp` (DEFINE_LOG_CATEGORY), named `Tcs<ModuleName>LogChannel`; UE category names follow `LogTcs<Module>` (e.g. `LogTcsCore`). Code that logs includes the LogChannel header, never `Module.h`.
- Files are UTF-8 without BOM, LF line endings.

### Architecture Patterns
- Data-Behavior separation: definitions are UObject Def assets; instances are pooled USTRUCTs addressed by strong-typed handles (index + generation, D0-2); a single `TickableWorldSubsystem` tick pump with an injectable `FCombatClock` is the only driver of expiries (D0-5). Single game-thread assumption with asserts (D0-4).
- Minimal compile-set dependency rule (R0 §9): `Core←Attribute←Effect←{Damage,Targeting,State}←Skill`; the table column is a minimal compile set, the chain is direction permission only — TcsSkill compiles against Core/Attribute/State/Effect/Notation, and composition happens in TcsIntegration or the host.
- Registry-based step dispatch (D4-14): `TcsEffect` depends only on Core/Attribute; domain step types + executors self-register from their own modules (`UE_DEFINE_EFFECT_STEP_EXECUTOR`) — Damage/Heal/ModifyFlow→TcsDamage, SelectTargets→TcsTargeting, ApplyState→TcsState, PlayCue→TcsCue.
- Strategy unification (D3-7 v3, superseding the 2026-09-02 EditInlineNew Instanced UObject decision): every strategy is a reflected USTRUCT base with C++ virtual dispatch (StateTree-style), held as `TInstancedStruct<Base>` members on Def assets/steps; BP strategy-extension is intentionally dropped (R0 "blueprints not promised"); instances carry zero strategy/payload/subscription state.
- Custom escape bit规约: every policy enum has `Custom` fixed at value 1; value 0 is always the built-in default / None.
- Instance data structures forbid `TSubclassOf`; the hot path never re-queries Defs (D2-9 / D5-13).
- `FTcsParamValue{ TInstancedStruct<FTcsParamValueSource> Source }` is the unified numeric-config carrier in TcsCore (PV series 2026-09-11, superseding D2-12 `FTcsParamScalar`): abstract base evaluates via `virtual double Evaluate(...)` (D3-7 v3 form), built-in `Literal` / `ParamRef` sources; domain sources live in their owning modules — StateLevelArray/Map + InstigatorLevelArray/Map and the `ITcsEntityLevelProvider` interface (GetEntityLevel, host-implemented; TargetLevel sources deferred) live in TcsState, AttributeScaled lives in TcsAttribute; base damage = parameter-ledger resolution fed into the damage flow (D7-2 narrowed — the flow never computes base damage); attribute modifier operands keep separate Def/runtime shapes (D2-13, ledger stays resolved doubles).
- Parameter chains and attribute modifiers share **one** band-fold (D5-5 v3, 2026-09-14): ops `Add`/`PercentAdd`/`Mul`/`FlatAdd`/`Override`, order-independent within and across rows (`Override` group max replaces everything), a single pure fold function living in TcsAttribute and reused by M2, M5 chains and Damage flow attributes. Notation exposes **description view strategies** (D5-17 v3): `Def.Descriptions` holds per-description entries `{DescriptionId, TextKey, Views[]}` where each view slot is a `TInstancedStruct<FTcsParamView>` strategy (built-ins Value / Series / Range / Attribute); StringTable text carries only slot names (zero syntax), all machine semantics live in structured editor-validatable config — `IsCompatible(Probe)` rejects mismatched source×view pairs at save time, and host-defined views require zero public-code changes. Convention columns are restricted to a whitelist — configurable on `Literal`/table sources, forbidden on `ParamRef`/`AttributeScaled` (D5-18 v3).
- Server-authoritative network posture (D0-1): simulation core runs headless, clients hold mirror state, prediction covers presentation only. LAC itself is single-player today.
- TcsCore contains zero logging infrastructure (D0-6): logging uses UE native categories only; screen-display acceptance signals come from test rigs calling UE APIs directly.

### Testing Strategy
- **TDD is forbidden** (user-level highest discipline): no failing-test-first steps. Verification = UBT compile pass + directed manual checkpoints.
- R3 acceptance is the seven-item manual checklist in `Documents/combat-system-design/2026-09-02-r3-vertical-slice-script.md`, driven by a PIE test map and a test rig (plan2 Task 6).

### Git Workflow
- Active branch: `remake`. The legacy implementation is frozen on the old repository/branch for reference only.
- Commits happen only after explicit user authorization; task boundaries are stop points that wait for user review.
- Commit message style follows the existing repo convention: `【ADD】…` / `【MOD】…` / `【DEL】…`.

## Domain Context
- **Single source of truth**: `Documents/combat-system-design/` — start from `README.md` (decision log + document index) and `2026-09-02-r0-rebuild-position-paper.md` §9 (module materialization). Conflict resolution order: user's live decision > design docs > plan docs > session startup prompt.
- R3 plans: `2026-09-02-r3-plan1-core-attributes.md` (TcsCore + TcsNotation shell + TcsAttribute) and `2026-09-02-r3-plan2-damage-chain.md` (TcsEffect + TcsTargeting + TcsDamage + TcsIntegration). M3 states / M5 skill / M7 cue / M8 editor tooling are designed but out of the R3 slice.
- Core domain concepts (each has a formal module doc 01–11): attribute aggregation pipeline (bands, transactions, read-registers-dependency, SCC cycle detection), state central registry with five-axis `FStateStackPolicy`, effect chains (15 primitives, trigger-row evaluator, async four-wake-source), damage flow templates (interpreter + standard step library + official default template), skill cast ledger with multi-track cooldowns, targeting selector/filter strategies (default Self / EventTarget), and the TcsNotation layer (ValueConvention + description text binding via `FText::Format` placeholders).
- Host integration contract: two-layer bootstrap (GameInstance DefLibrary + World driver), `CombatEntity` component as the three-responsibility adapter.

## Important Constraints
- Determinism is a lightweight discipline (D0-1) with the single tick pump as the only driver of time.
- Mass/ECS memory-layout work is explicitly out of current scope (future TcsMass); revisit with the 1000-unit stress test per `2026-09-02-mass-连续内存数据布局备忘.md`.
- Load agent skills before working: `unreal-development-workflow` (execution philosophy), `unreal-cpp-style`, `unreal-cpp-compile` (engine path probing + UBT builds).
- For new capabilities, breaking changes, architecture shifts, or major performance work, create and validate an OpenSpec proposal in this `openspec/` directory before implementing.

## External Dependencies
- Host project: LegendAutoChess — this repository is mounted as a git submodule at `Plugins/Tirefly/TireflyCombatSystem`.
- Engine plugins: StateTree (decision/orchestration only — never buff/skill execution), GameplayStateTree, GameplayMessageRouter, TireflyObjectPool (sibling Tirefly plugin).
- Engine installations are expected under `E:\UnrealEngine\`; probe the exact path per the `unreal-cpp-compile` skill.
