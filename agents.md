# Client / Server / Shared

Organize like a thin multiplayer game stack (gardn-style), not by “whatever file is open.”

## Layers
- **Shared** — single source of truth: IDs/enums, balance numbers, protocol, entity schema, map/spawn tables. Both sides consume; neither invents duplicates.
- **Server** — authority and simulation. Tick systems and entity events live here.
- **Client** — render, input, UI, presentation-only state.

## Where new code goes
- New **tick/system concern** → new server process/system module (do not append to an unrelated system).
- New **entity event/helper** (damage, death, targeting) → entity-functions style module, not inside a tick file.
- New **renderer / asset drawer / UI surface** → its own client file under the matching folder.
- **Numbers both sides need** → Shared only. Duplicating a radius, cooldown, or rarity on client and server is a bug.

## Orchestration stays thin
The main tick/loop file should mostly **order** systems. If it grows business logic, extract a system module.

## Data tables are the exception
Large pure-data files (static balance, generated drawings) may exceed normal size limits. Do not use that as permission to mix systems into them.

# Code health

Heuristics, not gates — use judgment; don’t block a fix over a rule.

## Split before you grow
If a file is already >400–500 lines and the change adds a **new concern**, put that concern in a new module instead of appending.

## New feature = new file
Default to a new module for a new mob type, mechanic, or system. Only inline if it’s genuinely a couple of lines.

## No copy-paste beyond ~10 lines
Near-duplicate blocks → extract a helper. Don’t wait for a third copy.

## Delete, don’t deprecate
No feature flags, `_old`, commented-out blocks, or back-compat shims for internal-only code. Unused → delete.

## Comments explain why, not what
Skip restating the code. Comment only non-obvious constraints, ordering, or invariants.

## Review and prune
Before non-trivial deploy, run a code-review (or simplify for pure cleanup) on the diff.
Periodically prune dead code, stale TODOs, and files past the size threshold — in a dedicated pass, not mixed into features.
