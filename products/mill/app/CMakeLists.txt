# MyDesk Mill app — CAM CLI driver
add_executable(MyDeskMill main.cpp)
target_link_libraries(MyDeskMill PRIVATE mydesk::mill_engine)
install(TARGETS MyDeskMill RUNTIME DESTINATION bin)
