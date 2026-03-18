cmake_minimum_required(VERSION 3.8)
project(CapePowerup)

set(CMAKE_CXX_STANDARD 20)

add_subdirectory(lib/BetterSunshineEngine)

file(GLOB_RECURSE SOURCES "src/*.cpp")
file(GLOB_RECURSE HEADERS "include/*.hxx" "include/*.hpp" "include/*.h")

add_executable(CapePowerup ${SOURCES} ${HEADERS})
target_include_directories(CapePowerup PRIVATE include)
target_link_libraries(CapePowerup PRIVATE BetterSunshineEngine)

# Post-build: convert ELF to KXE module
set(BSE_DIR ${CMAKE_CURRENT_SOURCE_DIR}/lib/BetterSunshineEngine)
add_custom_target(${PROJECT_NAME}.kxe ALL DEPENDS ${PROJECT_NAME})
add_custom_command(
    TARGET ${PROJECT_NAME}.kxe
    COMMAND ${BSE_DIR}/tools/KuriboConverter.exe ARGS ${PROJECT_NAME} ${PROJECT_NAME}.kxe ${BSE_DIR}/maps/${SMS_REGION}.map
)
