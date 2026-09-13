cmake_minimum_required(VERSION 3.16)

project(denso_d2_decision LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

if(NOT CMAKE_BUILD_TYPE AND NOT CMAKE_CONFIGURATION_TYPES)
    set(CMAKE_BUILD_TYPE Release CACHE STRING "Build type" FORCE)
endif()

add_library(denso_d2_decision STATIC
    src/baselines.cpp
    src/catalog.cpp
    src/evaluation.cpp
    src/gate.cpp
    src/post_tour.cpp
)
target_include_directories(denso_d2_decision PUBLIC ${CMAKE_CURRENT_SOURCE_DIR}/include)

if(CMAKE_CXX_COMPILER_ID MATCHES "GNU|Clang")
    target_compile_options(denso_d2_decision PRIVATE -Wall -Wextra -Wpedantic)
endif()

add_executable(denso_d2_decision_tests tests/decision_tests.cpp)
target_link_libraries(denso_d2_decision_tests PRIVATE denso_d2_decision)
if(CMAKE_CXX_COMPILER_ID MATCHES "GNU|Clang")
    target_compile_options(denso_d2_decision_tests PRIVATE -Wall -Wextra -Wpedantic)
endif()

add_executable(denso_d2_baseline_tests tests/baseline_tests.cpp)
target_link_libraries(denso_d2_baseline_tests PRIVATE denso_d2_decision)
if(CMAKE_CXX_COMPILER_ID MATCHES "GNU|Clang")
    target_compile_options(denso_d2_baseline_tests PRIVATE -Wall -Wextra -Wpedantic)
endif()

add_executable(denso_d2_post_tour_tests tests/post_tour_tests.cpp)
target_link_libraries(denso_d2_post_tour_tests PRIVATE denso_d2_decision)
if(CMAKE_CXX_COMPILER_ID MATCHES "GNU|Clang")
    target_compile_options(denso_d2_post_tour_tests PRIVATE -Wall -Wextra -Wpedantic)
endif()

enable_testing()
add_test(NAME decision_tests COMMAND denso_d2_decision_tests)
add_test(NAME baseline_tests COMMAND denso_d2_baseline_tests)
add_test(NAME post_tour_tests COMMAND denso_d2_post_tour_tests)
