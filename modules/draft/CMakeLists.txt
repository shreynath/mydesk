add_library(mydesk_draft
  src/keymap.cpp
  src/snaps.cpp
)
add_library(mydesk::draft ALIAS mydesk_draft)

target_include_directories(mydesk_draft
  PUBLIC
    $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
    $<INSTALL_INTERFACE:include>
)

target_link_libraries(mydesk_draft
  PUBLIC
    mydesk::commands
    mydesk::document
    mydesk::geometry
)

target_compile_features(mydesk_draft PUBLIC cxx_std_20)
