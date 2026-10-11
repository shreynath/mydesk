add_library(mydesk_fusion_engine
  src/fusion_document.cpp
)
add_library(mydesk::fusion_engine ALIAS mydesk_fusion_engine)

target_include_directories(mydesk_fusion_engine
  PUBLIC
    $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
    $<INSTALL_INTERFACE:include>
)
target_compile_features(mydesk_fusion_engine PUBLIC cxx_std_20)
