cmake_minimum_required(VERSION 3.14)
project(ALifeSim VERSION 0.2.0 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

# Enable warnings
if(MSVC)
    add_compile_options(/W4)
else()
    add_compile_options(-Wall -Wextra -Wpedantic)
endif()

# ---------------------------------------------------------------------------
# Dependencies
# ---------------------------------------------------------------------------
find_package(SDL2 REQUIRED)
find_package(glm  REQUIRED)

enable_testing()

set(ALIFE_TEST_ENV "PATH=C:/msys64/ucrt64/bin\\;$ENV{PATH}")

# ---------------------------------------------------------------------------
# Phase 1: Biochemistry static library
# ---------------------------------------------------------------------------
add_library(biochem STATIC
    src/biochem/ChemicalPool.cpp
    src/biochem/Reaction.cpp
    src/biochem/Emitter.cpp
    src/biochem/Receptor.cpp
    src/biochem/BiochemEngine.cpp
)
target_include_directories(biochem PUBLIC include)

# ---------------------------------------------------------------------------
# Phase 2: Neural brain static library
# ---------------------------------------------------------------------------
add_library(brain STATIC
    src/brain/Brain.cpp
)
target_include_directories(brain PUBLIC include)
target_link_libraries(brain PUBLIC biochem)

# ---------------------------------------------------------------------------
# v0.2: World static library
# ---------------------------------------------------------------------------
add_library(world STATIC
    src/world/WorldGrid.cpp
)
target_include_directories(world PUBLIC include)
target_link_libraries(world PUBLIC biochem glm::glm)

# ---------------------------------------------------------------------------
# v0.2: Render static library  (SDL2 + GLM)
# ---------------------------------------------------------------------------
add_library(render STATIC
    src/render/Renderer.cpp
    src/render/DebugOverlay.cpp
    src/render/TileRenderer.cpp
)
target_include_directories(render PUBLIC include)
target_link_libraries(render PUBLIC world SDL2::SDL2 glm::glm)

# ---------------------------------------------------------------------------
# Unit tests
# ---------------------------------------------------------------------------
add_executable(test_biochem tests/test_biochem.cpp)
target_link_libraries(test_biochem biochem)
add_test(NAME test_biochem COMMAND test_biochem)
set_tests_properties(test_biochem PROPERTIES ENVIRONMENT "${ALIFE_TEST_ENV}")

add_executable(test_brain tests/test_brain.cpp)
target_link_libraries(test_brain brain biochem)
add_test(NAME test_brain COMMAND test_brain)
set_tests_properties(test_brain PROPERTIES ENVIRONMENT "${ALIFE_TEST_ENV}")

add_executable(test_world tests/test_world.cpp)
target_link_libraries(test_world world glm::glm)
add_test(NAME test_world COMMAND test_world)
set_tests_properties(test_world PROPERTIES ENVIRONMENT "${ALIFE_TEST_ENV}")

# ---------------------------------------------------------------------------
# v0.3: Sim static library (Genome, GenomePhenotype, Species)
# ---------------------------------------------------------------------------
add_library(sim STATIC
    src/sim/Genome.cpp
    src/sim/GenomePhenotype.cpp
    src/sim/Species.cpp
    src/sim/Creature.cpp
    src/sim/SimSystem.cpp
)
target_include_directories(sim PUBLIC include)
target_link_libraries(sim PUBLIC world brain biochem glm::glm)

add_executable(test_genome tests/test_genome.cpp)
target_link_libraries(test_genome sim world brain biochem)
add_test(NAME test_genome COMMAND test_genome)
set_tests_properties(test_genome PROPERTIES ENVIRONMENT "${ALIFE_TEST_ENV}")

add_executable(test_sim tests/test_sim.cpp)
target_link_libraries(test_sim sim world brain biochem glm::glm)
add_test(NAME test_sim COMMAND test_sim)
set_tests_properties(test_sim PROPERTIES ENVIRONMENT "${ALIFE_TEST_ENV}")

# ---------------------------------------------------------------------------
# Main simulation executable
# ---------------------------------------------------------------------------
add_executable(alife src/main.cpp)
# SDL2::SDL2main must come before SDL2::SDL2 — it provides WinMain on Windows
target_link_libraries(alife render sim world brain biochem SDL2::SDL2main SDL2::SDL2 glm::glm)

# On Windows with MinGW, SDL2 needs its DLL next to the executable
if(WIN32 AND MINGW)
    get_target_property(_sdl2_loc SDL2::SDL2 LOCATION)
    if(_sdl2_loc)
        add_custom_command(TARGET alife POST_BUILD
            COMMAND ${CMAKE_COMMAND} -E copy_if_different
                "${_sdl2_loc}" $<TARGET_FILE_DIR:alife>
            COMMENT "Copying SDL2.dll")
    endif()
endif()
