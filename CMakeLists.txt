cmake_minimum_required(VERSION 3.8)

project(CapePowerup VERSION 1.0)

set(CMAKE_CXX_STANDARD 20)

file(GLOB CAPE_SRC
    "src/*.cpp"
    "include/*.hxx"
)

add_executable(CapePowerup ${CAPE_SRC})
add_subdirectory(lib/BetterSunshineEngine)

target_link_libraries(CapePowerup PUBLIC BetterSunshineEngine)

target_include_directories(CapePowerup PUBLIC "include")

target_compile_options(CapePowerup PUBLIC ${SMS_COMPILE_FLAGS})
target_link_options(CapePowerup PUBLIC ${SMS_LINK_FLAGS})

list(APPEND CAPE_CONFIG_DEFINES "KURIBO_NO_TYPES")
target_compile_definitions(CapePowerup PUBLIC ${CAPE_CONFIG_DEFINES})

add_custom_target(${PROJECT_NAME}.kxe ALL DEPENDS ${PROJECT_NAME})
add_custom_command(
    TARGET ${PROJECT_NAME}.kxe
    COMMAND ${PROJECT_SOURCE_DIR}/lib/BetterSunshineEngine/tools/KuriboConverter.exe ARGS ${PROJECT_NAME} ${PROJECT_NAME}.kxe ${PROJECT_SOURCE_DIR}/lib/BetterSunshineEngine/maps/${SMS_REGION}.map
)
