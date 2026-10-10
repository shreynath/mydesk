add_executable(MyDeskCoordinate
  src/main.cpp
)

target_link_libraries(MyDeskCoordinate
  PRIVATE
    mydesk::coordinate_engine
)

target_compile_features(MyDeskCoordinate PUBLIC cxx_std_20)

install(TARGETS MyDeskCoordinate RUNTIME DESTINATION bin)
