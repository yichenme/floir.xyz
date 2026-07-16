set(CMAKE_SYSTEM_NAME Generic)

cmake_minimum_required(VERSION 3.16)
project(floir-server)

include_directories(..)

set(SOURCES
    EntityFunctions/Damage.cc
    EntityFunctions/Death.cc
    EntityFunctions/Detection.cc
    EntityFunctions/Misc.cc
    Process/Ai.cc
    Process/Camera.cc
    Process/Collision.cc
    Process/Culling.cc
    Process/Curse.cc
    Process/Flower.cc
    Process/FlowerAi.cc
    Process/Health.cc
    Process/Motion.cc
    Process/Petal.cc
    Process/Score.cc
    Process/Segment.cc
    Client.cc
    Game.cc
    Main.cc
    PetalTracker.cc
    Server.cc
    Simulation.cc
    Spawn.cc
    TeamManager.cc
    ../Helpers/Math.cc
    ../Helpers/UTF8.cc
    ../Helpers/Vector.cc
    ../Shared/Arena.cc
    ../Shared/Binary.cc
    ../Shared/Config.cc
    ../Shared/Entity.cc
    ../Shared/EntityDef.cc
    ../Shared/Map.cc
    ../Shared/Simulation.cc
    ../Shared/StaticData.cc
    ../Shared/StackFormat.cc
    ../Shared/AccountValidation.cc
)

set(SOURCES ${SOURCES} Account/Auth.cc EntityFunctions/InventoryOps.cc)
if(WASM_SERVER)
    set(SOURCES ${SOURCES} Wasm.cc Account/DatabaseWasm.cc)
else()
    set(SOURCES ${SOURCES} Native.cc Account/DatabaseNative.cc)
endif()
if(GENERAL_SPATIAL_HASH)
    set(SOURCES ${SOURCES} SpatialHashCanonical.cc)
else()
    set(SOURCES ${SOURCES} SpatialHashUniform.cc)
endif()
set(CMAKE_CXX_FLAGS "-std=c++20 -DSERVERSIDE=1")

if (TDM)
    set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -DGAMEMODE_TDM=1")
endif()
if (USE_CODEPOINT_LEN)
    set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -DUSE_CODEPOINT_LEN=1")
endif()
if(DEBUG)
    set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -gdwarf-4 -DDEBUG=1")
else()
    set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -O3 -ffast-math")
endif()

if(WASM_SERVER)
    set(CMAKE_CXX_COMPILER "em++")
    set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -DWASM_SERVER=1")
    add_link_options(-sEXIT_RUNTIME=0 -sEXPORTED_FUNCTIONS=_main,_on_connect,_on_disconnect,_tick,_on_message,_malloc,_free)
    if (NOT DEBUG) 
        set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} --closure=1")
    endif()
    add_executable(floir-server ${SOURCES})
    set(CMAKE_EXECUTABLE_SUFFIX ".js")
else()
    set(CMAKE_CXX_COMPILER "g++")
    add_executable(floir-server ${SOURCES})
    target_include_directories(floir-server PRIVATE ${CMAKE_SOURCE_DIR}/uWebSockets/src)
    target_include_directories(floir-server PRIVATE ${CMAKE_SOURCE_DIR}/uWebSockets/uSockets/src)
    target_link_directories(floir-server PRIVATE ${CMAKE_SOURCE_DIR}/uWebSockets/uSockets)
    target_link_libraries(floir-server uv z)
    target_link_libraries(floir-server -l:uSockets.a)
    if(CMAKE_HOST_WIN32)
        target_link_libraries(floir-server ws2_32)
    endif()
endif()