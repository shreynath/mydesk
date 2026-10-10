add_executable(mydesk_mill_tests test_mill.cpp)
target_link_libraries(mydesk_mill_tests PRIVATE mydesk::mill_engine)
add_test(NAME mydesk_mill_tests COMMAND mydesk_mill_tests)
