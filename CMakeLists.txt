cmake_minimum_required(VERSION 3.20)

project(ollama-kde-importer VERSION 1.0.0)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_AUTOMOC ON)   # Q_OBJECT (wizard); harmless for the core

find_package(ECM 6.0 REQUIRED NO_MODULE)
set(CMAKE_MODULE_PATH ${ECM_MODULE_PATH})

find_package(Qt6 COMPONENTS Core Widgets REQUIRED)
find_package(KF6 COMPONENTS CoreAddons I18n REQUIRED)

add_library(oli_core STATIC core.cpp)
target_include_directories(oli_core PUBLIC ${CMAKE_CURRENT_SOURCE_DIR})
target_link_libraries(oli_core PUBLIC Qt6::Core)

option(BUILD_TESTING "Build helper tests" ON)
if(BUILD_TESTING)
  enable_testing()
  add_executable(test_oli_core tests/test_core.cpp)
  target_link_libraries(test_oli_core PRIVATE oli_core)
  add_test(NAME test_core COMMAND test_oli_core)
endif()

# The wizard lands in Task 2; guard so this CMake stays green until then.
if(EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/main.cpp")
  add_executable(ollama-kde-importer main.cpp importwizard.cpp)
  target_link_libraries(ollama-kde-importer PRIVATE oli_core Qt6::Widgets KF6::CoreAddons KF6::I18n)
  install(TARGETS ollama-kde-importer RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR})
endif()