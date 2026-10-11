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
