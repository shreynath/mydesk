# MyDesk Animate app placeholder (headless CLI driver)
# Full DCC viewport will follow once the engine is stable.
add_executable(MyDeskAnimate main.cpp)
target_link_libraries(MyDeskAnimate PRIVATE mydesk::animate_engine)
install(TARGETS MyDeskAnimate RUNTIME DESTINATION bin)
