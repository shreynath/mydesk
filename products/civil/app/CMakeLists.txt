add_executable(MyDeskCivil
  src/main.cpp
)

target_link_libraries(MyDeskCivil
  PRIVATE
    mydesk::civil_engine
)

target_compile_features(MyDeskCivil PUBLIC cxx_std_20)

install(TARGETS MyDeskCivil RUNTIME DESTINATION bin)
