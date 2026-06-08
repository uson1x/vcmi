include_directories(${CMAKE_HOME_DIRECTORY} ${CMAKE_HOME_DIRECTORY}/include ${CMAKE_CURRENT_SOURCE_DIR} ${CMAKE_HOME_DIRECTORY}/lib)

set(lualib_SRCS
		StdInc.cpp
		LuaReference.cpp
		LuaModule.cpp
		LuaScriptInstance.cpp
		LuaContext.cpp
		LuaScriptPool.cpp
		LuaSpellEffect.cpp
		LuaStack.cpp

		api/adventure/HeroInstance.cpp
		api/adventure/StackInstance.cpp

		api/battle/Unit.cpp
		api/battle/UnitState.cpp
		api/battle/BattleHexArray.cpp
		api/battle/BattleHex.cpp
		api/battle/Obstacle.cpp
		api/battle/SpellObstacleDescriptor.cpp

		api/events/BattleEvents.cpp
		api/events/EventBus.cpp
		api/events/GenericEvents.cpp
		api/events/SubscriptionRegistry.cpp

		api/spells/Mechanics.cpp
		api/spells/Problem.cpp

		api/library/Artifact.cpp
		api/library/Bonus.cpp
		api/library/BonusDescriptor.cpp
		api/library/Creature.cpp
		api/library/Faction.cpp
		api/library/HeroClass.cpp
		api/library/HeroType.cpp
		api/library/Services.cpp
		api/library/Skill.cpp
		api/library/Spell.cpp

		api/callback/IBattleInfoCallback.cpp
		api/Enums.cpp
		api/LuaMetaString.cpp
		api/callback/IGameInfoCallback.cpp
		api/Registry.cpp
		api/callback/ServerCallback.cpp
)

set(lualib_HDRS
		StdInc.h
		LuaCallWrapper.h
		LuaReference.h
		LuaModule.h
		LuaScriptInstance.h
		LuaContext.h
		LuaSpellEffect.h
		LuaScriptPool.h
		LuaStack.h
		LuaWrapper.h

		api/adventure/HeroInstance.h
		api/adventure/StackInstance.h

		api/battle/Unit.h
		api/battle/UnitState.h
		api/battle/BattleHexArray.h
		api/battle/BattleHex.h
		api/battle/Obstacle.h
		api/battle/SpellObstacleDescriptor.h

		api/events/BattleEvents.h
		api/events/EventBus.h
		api/events/GenericEvents.h
		api/events/SubscriptionRegistry.h

		api/library/Artifact.h
		api/library/Bonus.h
		api/library/BonusDescriptor.h
		api/library/Creature.h
		api/library/Faction.h
		api/library/HeroClass.h
		api/library/HeroType.h
		api/library/Services.h
		api/library/Skill.h
		api/library/Spell.h

		api/spells/Mechanics.h
		api/spells/Problem.h

		api/callback/IBattleInfoCallback.h
		api/Enums.h
		api/LuaMetaString.h
		api/callback/IGameInfoCallback.h
		api/Registry.h
		api/callback/ServerCallback.h
)

assign_source_group(${lualib_SRCS} ${lualib_HDRS})

add_library(vcmiLua OBJECT ${lualib_SRCS} ${lualib_HDRS})
target_compile_definitions(vcmiLua PRIVATE VCMI_DLL=1)

if(TARGET luajit::luajit)
	target_link_libraries(vcmiLua Boost::boost luajit::luajit vcmiMain)
elseif(TARGET lua::lua)
	target_link_libraries(vcmiLua Boost::boost lua::lua vcmiMain)
else()
	target_link_libraries(vcmiLua Boost::boost Lua::Lua vcmiMain)
endif()

vcmi_set_output_dir(vcmiLua "scripting")
enable_pch(vcmiLua)
