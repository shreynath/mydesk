include(FetchContent)
find_package(nlohmann_json 3.12.0 CONFIG QUIET)
if(NOT nlohmann_json_FOUND)
  FetchContent_Declare(nlohmann_json
    URL https://github.com/nlohmann/json/releases/download/v3.12.0/json.tar.xz
    URL_HASH SHA256=42f6e95cad6ec532fd372391373363b62a14af6d771056dbfc86160e6dfff7aa
    DOWNLOAD_EXTRACT_TIMESTAMP TRUE)
  FetchContent_MakeAvailable(nlohmann_json)
endif()

add_library(mydesk_mill_engine
  src/mill_document.cpp
)
add_library(mydesk::mill_engine ALIAS mydesk_mill_engine)

target_include_directories(mydesk_mill_engine
  PUBLIC
    $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
    $<INSTALL_INTERFACE:include>
)
target_compile_features(mydesk_mill_engine PUBLIC cxx_std_20)
target_link_libraries(mydesk_mill_engine PUBLIC nlohmann_json::nlohmann_json)
