include(FetchContent)

# Pin the structural JSON implementation used by the native repository format.
# The release archive hash prevents dependency substitution during configure.
FetchContent_Declare(
  nlohmann_json
  URL https://github.com/nlohmann/json/releases/download/v3.12.0/json.tar.xz
  URL_HASH SHA256=42f6e95cad6ec532fd372391373363b62a14af6d771056dbfc86160e6dfff7aa
  DOWNLOAD_EXTRACT_TIMESTAMP TRUE
)
set(JSON_BuildTests OFF CACHE INTERNAL "")
FetchContent_MakeAvailable(nlohmann_json)

add_library(mydesk_vault_engine
  src/vault_document.cpp
)
add_library(mydesk::vault_engine ALIAS mydesk_vault_engine)

target_include_directories(mydesk_vault_engine
  PUBLIC
    $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
    $<INSTALL_INTERFACE:include>
)
target_compile_features(mydesk_vault_engine PUBLIC cxx_std_20)
target_link_libraries(mydesk_vault_engine PRIVATE nlohmann_json::nlohmann_json)
