set(arenaAI_SRCS
		CArenaAI.cpp
)

set(arenaAI_HEADERS
		StdInc.h
		CArenaAI.h
)

if(NOT ENABLE_STATIC_LIBS)
	list(APPEND arenaAI_SRCS main.cpp StdInc.cpp)
endif()
assign_source_group(${arenaAI_SRCS} ${arenaAI_HEADERS})

if(ENABLE_STATIC_LIBS)
	add_library(ArenaAI STATIC ${arenaAI_SRCS} ${arenaAI_HEADERS})
else()
	add_library(ArenaAI SHARED ${arenaAI_SRCS} ${arenaAI_HEADERS})
	install(TARGETS ArenaAI RUNTIME DESTINATION ${AI_LIB_DIR} LIBRARY DESTINATION ${AI_LIB_DIR})
endif()

target_include_directories(ArenaAI PUBLIC ${CMAKE_CURRENT_SOURCE_DIR})
target_link_libraries(ArenaAI PRIVATE vcmi)

vcmi_set_output_dir(ArenaAI "AI")
enable_pch(ArenaAI)
