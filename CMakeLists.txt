cmake_minimum_required(VERSION 3.24)
project(MyDesk VERSION 0.1.0 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

# Standalone product selection (ADR-0001). Default builds MyDesk Draft only.
set(MYDESK_PRODUCT "draft" CACHE STRING "Which standalone product to build (draft|...)")
set_property(CACHE MYDESK_PRODUCT PROPERTY STRINGS draft)

option(MYDESK_BUILD_DESKTOP "Build the selected product's Qt desktop app" ON)
option(MYDESK_BUILD_TESTS "Build unit and integration tests" ON)
option(MYDESK_ENABLE_WARNINGS "Enable strict warnings" ON)

if(MYDESK_ENABLE_WARNINGS)
  if(MSVC)
    add_compile_options(/W4 /permissive-)
  else()
    add_compile_options(-Wall -Wextra -Wpedantic -Wconversion -Wshadow)
  endif()
endif()

list(APPEND CMAKE_MODULE_PATH "${CMAKE_CURRENT_SOURCE_DIR}/cmake")

# Shared infrastructure (migrating toward libs/ — keep core/ paths for now).
add_subdirectory(core/geometry)
add_subdirectory(core/units)
add_subdirectory(core/document)
add_subdirectory(core/modify)
add_subdirectory(core/commands)
add_subdirectory(core/exchange)
add_subdirectory(modules/draft)

if(MYDESK_PRODUCT STREQUAL "draft")
  if(MYDESK_BUILD_DESKTOP)
    find_package(Qt6 COMPONENTS Widgets Gui REQUIRED)
    qt_standard_project_setup()
    add_subdirectory(products/draft/app)
  endif()
else()
  message(FATAL_ERROR "Product '${MYDESK_PRODUCT}' is catalog-stub only. Implemented product: draft")
endif()

if(MYDESK_BUILD_TESTS)
  enable_testing()
  add_subdirectory(tests)
endif()

install(DIRECTORY samples/ DESTINATION share/mydesk-draft/samples)
