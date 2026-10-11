add_library(mydesk_studio_engine
  src/studio_document.cpp
)
add_library(mydesk::studio_engine ALIAS mydesk_studio_engine)

target_include_directories(mydesk_studio_engine
  PUBLIC
    $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
    $<INSTALL_INTERFACE:include>
)
target_compile_features(mydesk_studio_engine PUBLIC cxx_std_20)
