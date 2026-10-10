add_library(mydesk_buildings_engine src/project_document.cpp)
add_library(mydesk::buildings_engine ALIAS mydesk_buildings_engine)
target_include_directories(mydesk_buildings_engine
  PUBLIC
    $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
    $<INSTALL_INTERFACE:include>
)
target_compile_features(mydesk_buildings_engine PUBLIC cxx_std_20)
