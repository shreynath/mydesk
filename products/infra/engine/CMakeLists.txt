add_library(mydesk_infra_engine
  src/infra_document.cpp
)
add_library(mydesk::infra_engine ALIAS mydesk_infra_engine)

target_include_directories(mydesk_infra_engine
  PUBLIC
    $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
    $<INSTALL_INTERFACE:include>
)
target_compile_features(mydesk_infra_engine PUBLIC cxx_std_20)
