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
