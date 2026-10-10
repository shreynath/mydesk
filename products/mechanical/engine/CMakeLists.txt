add_library(mydesk_mechanical_engine
  src/part_document.cpp
  src/assembly_document.cpp
)
add_library(mydesk::mechanical_engine ALIAS mydesk_mechanical_engine)

find_package(OpenCASCADE REQUIRED)

target_include_directories(mydesk_mechanical_engine
  PUBLIC
    $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
    $<INSTALL_INTERFACE:include>
    ${OpenCASCADE_INCLUDE_DIR}
)

target_link_libraries(mydesk_mechanical_engine
  PUBLIC
    TKernel
    TKMath
    TKG2d
    TKG3d
    TKGeomBase
    TKBRep
    TKGeomAlgo
    TKTopAlgo
    TKPrim
    TKBO
    TKShHealing
    TKMesh
    TKDE
    TKXSBase
    TKDESTEP
)

target_link_directories(mydesk_mechanical_engine PUBLIC ${OpenCASCADE_LIBRARY_DIR})
target_compile_features(mydesk_mechanical_engine PUBLIC cxx_std_20)
