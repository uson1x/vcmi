set(battlellm_SRCS
        BattleLLM.cpp
)

set(battlellm_HEADERS
        StdInc.h
        BattleLLM.h
)

if(NOT ENABLE_STATIC_LIBS)
        list(APPEND battlellm_SRCS main.cpp StdInc.cpp)
endif()
assign_source_group(${battlellm_SRCS} ${battlellm_HEADERS})

if(ENABLE_STATIC_LIBS)
        add_library(BattleLLM STATIC ${battlellm_SRCS} ${battlellm_HEADERS})
else()
        add_library(BattleLLM SHARED ${battlellm_SRCS} ${battlellm_HEADERS})
        install(TARGETS BattleLLM RUNTIME DESTINATION ${AI_LIB_DIR} LIBRARY DESTINATION ${AI_LIB_DIR})
endif()

target_include_directories(BattleLLM PUBLIC ${CMAKE_CURRENT_SOURCE_DIR})
target_link_libraries(BattleLLM PRIVATE vcmi)

vcmi_set_output_dir(BattleLLM "AI")
enable_pch(BattleLLM)
