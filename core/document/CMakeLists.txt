add_library(mydesk_document
  src/document.cpp
  src/history.cpp
  src/entity.cpp
)
add_library(mydesk::document ALIAS mydesk_document)

target_include_directories(mydesk_document
  PUBLIC
    $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
    $<INSTALL_INTERFACE:include>
)

target_link_libraries(mydesk_document
  PUBLIC
    mydesk::geometry
    mydesk::units
)

target_compile_features(mydesk_document PUBLIC cxx_std_20)
