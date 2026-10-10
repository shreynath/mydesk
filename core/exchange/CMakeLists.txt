add_library(mydesk_exchange
  src/native_format.cpp
  src/dxf_writer.cpp
  src/dxf_reader.cpp
  src/plotter.cpp
)
add_library(mydesk::exchange ALIAS mydesk_exchange)

target_include_directories(mydesk_exchange
  PUBLIC
    $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
    $<INSTALL_INTERFACE:include>
)

target_link_libraries(mydesk_exchange
  PUBLIC
    mydesk::document
    mydesk::units
)

target_compile_features(mydesk_exchange PUBLIC cxx_std_20)
