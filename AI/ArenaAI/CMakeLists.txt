set(arenaAI_SRCS
		CArenaAI.cpp
)

set(arenaAI_HEADERS
		StdInc.h

		CArenaAI.h
)

assign_source_group(${arenaAI_SRCS} ${arenaAI_HEADERS})

add_library(ArenaAI OBJECT ${arenaAI_SRCS} ${arenaAI_HEADERS})
target_compile_definitions(ArenaAI PRIVATE VCMI_DLL=1)

target_link_libraries(ArenaAI PRIVATE vcmiMain)
target_include_directories(ArenaAI PUBLIC ${CMAKE_CURRENT_SOURCE_DIR})

vcmi_set_output_dir(ArenaAI "AI")
enable_pch(ArenaAI)
