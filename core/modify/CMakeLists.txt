add_library(mydesk_modify
  src/modify_ops.cpp
)
add_library(mydesk::modify ALIAS mydesk_modify)

target_include_directories(mydesk_modify
  PUBLIC
    $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
    $<INSTALL_INTERFACE:include>
)

target_link_libraries(mydesk_modify
  PUBLIC
    mydesk::document
    mydesk::geometry
)

target_compile_features(mydesk_modify PUBLIC cxx_std_20)
