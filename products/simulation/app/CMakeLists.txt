# MyDesk Simulation app — FEA CLI driver
add_executable(MyDeskSimulation main.cpp)
target_link_libraries(MyDeskSimulation PRIVATE mydesk::simulation_engine)
install(TARGETS MyDeskSimulation RUNTIME DESTINATION bin)
