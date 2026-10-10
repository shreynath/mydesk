add_library(mydesk_commands
  src/registry.cpp
  src/draft_commands.cpp
  src/modify_commands.cpp
  src/session.cpp
)
add_library(mydesk::commands ALIAS mydesk_commands)

target_include_directories(mydesk_commands
  PUBLIC
    $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
    $<INSTALL_INTERFACE:include>
)

target_link_libraries(mydesk_commands
  PUBLIC
    mydesk::document
    mydesk::geometry
    mydesk::modify
)

target_compile_features(mydesk_commands PUBLIC cxx_std_20)
