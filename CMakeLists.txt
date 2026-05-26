cmake_minimum_required(VERSION 3.20)
project(TribesAscendServerLagCompensation CXX)

# Set C++20 standard
set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_MSVC_DEBUG_INFORMATION_FORMAT ProgramDatabase)

# Print configuration details
message(STATUS "CMAKE_BUILD_TYPE: ${CMAKE_BUILD_TYPE}")
message(STATUS "CMAKE_CXX_FLAGS: ${CMAKE_CXX_FLAGS}")
message(STATUS "CMAKE_CXX_FLAGS_DEBUG: ${CMAKE_CXX_FLAGS_DEBUG}")
message(STATUS "CMAKE_CXX_FLAGS_RELEASE: ${CMAKE_CXX_FLAGS_RELEASE}")

# Project type (Dynamic Library)
add_library(${PROJECT_NAME} SHARED dllmain.cpp)

# Include directories
target_include_directories(${PROJECT_NAME} PRIVATE 
    "plog/include"
    "Tribes-Ascend-SDK"
    "Unreal-UFunction-Hook"
)

# Source files
target_sources(${PROJECT_NAME} PRIVATE
    dllmain.cpp
    helper.cpp
    lag_compensation.cpp
    native_hooks.cpp
    processinternal_hooks.cpp

    Unreal-UFunction-Hook/uhook.cpp
    
    # SDK Sources
    Tribes-Ascend-SDK/SDK_SOURCES/Core_functions.cpp
    Tribes-Ascend-SDK/SDK_SOURCES/Engine_functions.cpp
    # Tribes-Ascend-SDK/SDK_SOURCES/GameFramework_functions.cpp
    # Tribes-Ascend-SDK/SDK_SOURCES/GFxUI_functions.cpp
    # Tribes-Ascend-SDK/SDK_SOURCES/IpDrv_functions.cpp
    # Tribes-Ascend-SDK/SDK_SOURCES/OnlineSubsystemMcts_functions.cpp
    # Tribes-Ascend-SDK/SDK_SOURCES/PlatformCommon_functions.cpp
    # Tribes-Ascend-SDK/SDK_SOURCES/TribesGameContent_functions.cpp
    Tribes-Ascend-SDK/SDK_SOURCES/TribesGame_functions.cpp
    # Tribes-Ascend-SDK/SDK_SOURCES/UDKBase_functions.cpp
    Tribes-Ascend-SDK/SDK_SOURCES/UTGame_functions.cpp
    # Tribes-Ascend-SDK/SDK_SOURCES/WinDrv_functions.cpp
    # Tribes-Ascend-SDK/SDK_SOURCES/XAudio2_functions.cpp
)

# Linker settings
target_link_libraries(${PROJECT_NAME} PUBLIC
    "${CMAKE_SOURCE_DIR}/Detours/lib.X86/detours.lib"
)