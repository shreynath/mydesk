add_library(mydesk_fusion_engine
  src/fusion_document.cpp
)
add_library(mydesk::fusion_engine ALIAS mydesk_fusion_engine)

list(APPEND CMAKE_PREFIX_PATH "/opt/homebrew/opt/opencascade" "/usr/local/opt/opencascade")
find_package(OpenCASCADE REQUIRED)

target_include_directories(mydesk_fusion_engine
  PUBLIC
    $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
    $<INSTALL_INTERFACE:include>
    ${OpenCASCADE_INCLUDE_DIR}
)

target_link_directories(mydesk_fusion_engine PUBLIC ${OpenCASCADE_LIBRARY_DIR})
target_link_libraries(mydesk_fusion_engine
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
)
target_compile_features(mydesk_fusion_engine PUBLIC cxx_std_20)
