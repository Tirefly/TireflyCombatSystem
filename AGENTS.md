<!-- OPENSPEC:START -->
# OpenSpec Instructions

These instructions are for AI assistants working in the TireflyCombatSystem plugin.

Always open `@/openspec/AGENTS.md` when the request:
- Mentions planning or proposals (words like proposal, spec, change, plan)
- Introduces new capabilities, breaking changes, architecture shifts, or big performance/security work
- Sounds ambiguous and you need the authoritative spec before coding

Use `@/openspec/AGENTS.md` to learn:
- How to create and implement change proposals in this plugin-local OpenSpec workspace
- Spec format and conventions for TireflyCombatSystem
- Project structure and engineering guidelines for this plugin

Keep this managed block so 'openspec update' can refresh the instructions.

<!-- OPENSPEC:END -->

## Current Development Stage

- TireflyCombatSystem is currently in the architecture-design and development-practice stage.
- Assume there are currently no Blueprint assets referencing TCS APIs, events, assets, or editor authoring entries.
- Do not preserve Blueprint-asset compatibility based on speculation alone when reviewing or implementing TCS changes.
- If Blueprint assets start referencing TCS in the future, wait for the user to explicitly state that new fact before changing this assumption.