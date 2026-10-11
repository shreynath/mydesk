include(FetchContent)
find_package(nlohmann_json 3.12.0 CONFIG QUIET)
if(NOT nlohmann_json_FOUND)
  FetchContent_Declare(nlohmann_json
    URL https://github.com/nlohmann/json/releases/download/v3.12.0/json.tar.xz
    DOWNLOAD_EXTRACT_TIMESTAMP TRUE)
  FetchContent_MakeAvailable(nlohmann_json)
endif()

add_library(mydesk_animate_engine
  src/animate_document.cpp
)
add_library(mydesk::animate_engine ALIAS mydesk_animate_engine)

target_include_directories(mydesk_animate_engine
  PUBLIC
    $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
    $<INSTALL_INTERFACE:include>
)
target_compile_features(mydesk_animate_engine PUBLIC cxx_std_20)
target_link_libraries(mydesk_animate_engine PUBLIC nlohmann_json::nlohmann_json)
