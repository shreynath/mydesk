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
