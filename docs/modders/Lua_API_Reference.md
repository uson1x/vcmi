# Lua API Reference

## TODO

- review that existing API follows rules described here
- decide what to do with "Id classes" or other simple wrappers around integers. Pass them as integers? As pointer to class? As copy of integer as lua user data?
  - PlayerColor
  - BattleHex
- Add error handling instead of returning nil values whenever something goes wrong. Lua already provides `luaL_error` for such cases
- Mocking of library classes is often problematic - drop?
- add "preprocess" function to initialize parameters (e.g. string -> Creature)
- json schema support? Or "validate" function? Or have Lua generate json schema? 
- ensure that there is debug logging available to scripts

## General rules

Scripts must be constant and should not generate any side effects.

Exception are scripts that are executed as result of netpack apply (such scripts should be marked as such)

Global state of a Lua script must never change - script should not make assumptions on how many times it was run or in what order were functions called

## General naming rules

- Classes must be in a namespace.
- Namespace names are single word and in lowercase.
- Classes names are in PascalCase
- Method names are in camelCase
- Method names must be verbs: `getFoo`, `isFoo`, `setFoo`, `run`, `update`
- Library classes, such as Creature must be passed as pointer like `const Creature *`, not as identifier like `CreatureID`

## Available globals

VCMI provides following global fields, that are accessible from any script:

- `LIBRARY` - constant game data access, such as creatures, heroes, factions. See (services)[#services].
- `ENUM` - container for all enumerations accessible for Lua scripting. See (enum)[#enum].
- `GAME` - Allows access to state of the ongoing game session. See (game)[#game].
- `print()` - emulates standard Lua function with the same name, available for use as logging. Printed text will be available in log file and in console output 
- `string` - standard Lua string library
- `math` - standard Lua math library

## Events Reference

### Spell Effect

#### applicable

Signature: `applicable = function(parameters, mechanics, problem)`

Parameters:

- `parameters` - contains all spell parameters provided in spell effect json config
- `mechanics` - contains settings at which spell is being cast, such as state of hero or creature that acts as caster of the spell. See [SpellMechanics](#spellmechanics).
- `problem` - storage for any "problems" with casting the spell. If spell can't be casted, reason for the failure must be added to the problem. See [SpellProblem](#spellproblem).

Return value: boolean

This function should return true if spell effect has at least one valid target on which it can be cast, or false if none of entities (such as units or hexes) can be used as target for the spell. 

#### transformTarget

Signature: `transformTarget = function(parameters, mechanics, aimPoint, spellTarget)`

Parameters:

- `parameters` - contains all spell parameters provided in spell effect json config
- `mechanics` - contains settings at which spell is being cast, such as state of hero or creature that acts as caster of the spell. See [SpellMechanics](#spellmechanics).
- `aimPoint` - TODO. See [SpellTarget](#spelltarget).
- `spellTarget` - TODO. See [SpellTarget](#spelltarget).

Return value: [SpellTarget](#spelltarget)

This function should examine `aimPoint` and `spellTarget` to generate list of targets that are affected by the spell

#### applicableTarget

Signature: `applicableTarget = function(parameters, mechanics, problem, target)`

Parameters:

- `parameters` - contains all spell parameters provided in spell effect json config
- `mechanics` - contains settings at which spell is being cast, such as state of hero or creature that acts as caster of the spell. See [SpellMechanics](#spellmechanics).
- `problem` - storage for any "problems" with casting the spell. If spell can't be casted, reason for the failure must be added to the problem. See [SpellProblem](#spellproblem).
- `target` - Target (such as unit or hex) on which this spell is being cast, after convertion by `transformTarget` See [SpellTarget](#spelltarget).

Return value: boolean

This function should return true if spell can be cast on a specified target(s).

#### apply

Signature: `apply = function(parameters, mechanics, server, target)`

Parameters:

- `parameters` - contains all spell parameters provided in spell effect json config
- `mechanics` - contains settings at which spell is being cast, such as state of hero or creature that acts as caster of the spell. See [SpellMechanics](#spellmechanics).
- `server` - This parameter can be used to apply actual changes in a battle state [Server](#server).
- `target` - Target (such as unit or hex) on which this spell is being cast, after convertion by `transformTarget` See [SpellTarget](#spelltarget).

Return value: nothing

This function performs actual cast of the spell and applies all effects caused by the spell to game via `server` parameter. It is guaranteed that `target` has been transformed via `transformTarget` and verified to be applicable via calls to `applicable` and `applicableTarget` 

## API Reference

## entity

These classes represent global, non-changeable entities

### Artifact

### Creature

### Faction

### HeroClass

### HeroType

### Skill

### Spell

## library

These classes represent global, non-changeable containers for entities

### services

Primary way to access all library entries. Available as `LIBRARY` global.

NOTE: all `get???ByName` methods require string identifier of an object, for example `grandElf` or `core:grandElf`, and not human-readable name such as `Grand Elf`

- `:getArtifactByName(string name)`
- `:getCreatureByName(string name)`
- `:getHeroClassByName(string name)`
- `:getHeroTypeByName(string name)`
- `:getSpellByName(string name)`
- `:getSecondarySkillByName(string name)`

# enumerations

## game

These classes represent global gamestate. Read-only by scripts, can be changed indirectly via network packs

### EventBus

### EventSubscription

### Battle

### Game

### Player

### Server

## adventure

These classes represent objects present on adventure map. Read-only by scripts, can be changed indirectly via network packs

### ObjectInstance

### HeroInstance

### StackInstance

## battle

These classes represent objects present during combat. Read-only by scripts, can be changed indirectly via network packs

### Unit

- `:getMinDamage()`
- `:getMaxDamage()`
- `:getAttack()`
- `:getDefense()`
- `:isAlive()`
- `:isClone()`
- `:isSummoned()`
- `:getOwner()`
- `:getSlot()`
- `:getCreature()`

### SpellMechanics

- `:isPositive()`
- `:isNegative()`
- `:getEffectLevel()`
- `:getRangeLevel()`
- `:getEffectPower()`
- `:getEffectDuration()`
- `:getEffectValue()`
- `:getCasterColor()`
- `:getBattle()`

## netpacks

These classes are used for making changes to a gamestate, and are the only way to make things happen either on adventure map or in combat. General workflow is to create a pack by calling `new`, fill it with data and apply on server

### BattleLogMessage

- `.new()`
- `:addText(text)`
- `:toNetpackLight()`

### BattleStackMoved

- `.new()`
- `:addTileToMove(integer)`
- `:setUnitId(integer)`
- `:setDistance(integer)`
- `:setTeleporting(bool)`
- `:toNetpackLight()`

### BattleUnitsChanged

- `.new()`
- `:add(integer id, Json data, integer healthDelta)`
- `:update(integer id, Json data, integer healthDelta)`
- `:resetState()` (not implemented)
- `:remove()` (not implemented)
- `:toNetpackLight()`

### EntitiesChanged

- `.new()`
- `:update(integer metaIndex, integer index, Json data)`
- `:toNetpackLight()`

### InfoWindow

FIXME: move this to metastring

- `.new()`
- `:addReplacement(string text)`
- `:addReplacement(integer number)`
- `:addText(string text)`
- `:addText(integer number)`
- `:setPlayer(integer index)`
- `:toNetpackLight()`

### SetResources

- `.new()`
- `:getPlayer() -> integer`
- `:setPlayer(integer playerIndex)`
- `:getAmount() -> integer`
- `:setAmount(integer which, integer amount)`
- `:clear()`
- `:toNetpackLight()`

## texts

Classes for managing text translations

### MetaString
