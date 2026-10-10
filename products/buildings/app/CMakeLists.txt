add_executable(mydesk_buildings_app src/main.cpp)
target_link_libraries(mydesk_buildings_app PRIVATE mydesk::buildings_engine)
set_target_properties(mydesk_buildings_app PROPERTIES OUTPUT_NAME "MyDeskBuildings")
install(TARGETS mydesk_buildings_app RUNTIME DESTINATION bin)
