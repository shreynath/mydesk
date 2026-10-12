include_guard(GLOBAL)

# Packaging is intentionally configured for exactly one selected product. A build
# directory can therefore never produce a suite package or silently pull in a
# second application's executable.
set(_mydesk_product_ids
  draft draft-lt mechanical fusion buildings civil infra coordinate animate
  studio surface mill simulation vault)
set(_mydesk_product_names
  "MyDesk Draft" "MyDesk Draft LT" "MyDesk Mechanical" "MyDesk Fusion"
  "MyDesk Buildings" "MyDesk Civil" "MyDesk Infra" "MyDesk Coordinate"
  "MyDesk Animate" "MyDesk Studio" "MyDesk Surface" "MyDesk Mill"
  "MyDesk Simulation" "MyDesk Vault")
set(_mydesk_product_binaries
  MyDeskDraft MyDeskDraftLT MyDeskMechanical MyDeskFusion MyDeskBuildings
  MyDeskCivil MyDeskInfra MyDeskCoordinate MyDeskAnimate MyDeskStudio
  MyDeskSurface MyDeskMill MyDeskSimulation MyDeskVault)
set(_mydesk_product_summaries
  "Standalone professional 2D CAD application"
  "Standalone streamlined 2D drafting application"
  "Standalone parametric mechanical design application"
  "Standalone integrated CAD and CAM application"
  "Standalone building information modeling application"
  "Standalone civil engineering design application"
  "Standalone conceptual infrastructure design application"
  "Standalone multidisciplinary model coordination application"
  "Standalone 3D animation and digital content creation application"
  "Standalone modifier-stack 3D modeling application"
  "Standalone precision industrial surfacing application"
  "Standalone professional CAM application"
  "Standalone finite-element simulation application"
  "Standalone engineering product data management application")

list(FIND _mydesk_product_ids "${MYDESK_PRODUCT}" _mydesk_product_index)
if(_mydesk_product_index EQUAL -1)
  message(FATAL_ERROR "Cannot configure packaging for unknown product '${MYDESK_PRODUCT}'")
endif()

list(GET _mydesk_product_names ${_mydesk_product_index} MYDESK_PACKAGE_DISPLAY_NAME)
list(GET _mydesk_product_binaries ${_mydesk_product_index} MYDESK_PACKAGE_BINARY)
list(GET _mydesk_product_summaries ${_mydesk_product_index} MYDESK_PACKAGE_SUMMARY)

set(_mydesk_product_dir "${PROJECT_SOURCE_DIR}/products/${MYDESK_PRODUCT}")
set(_mydesk_version_file "${_mydesk_product_dir}/VERSION")
if(NOT EXISTS "${_mydesk_version_file}")
  message(FATAL_ERROR "${MYDESK_PRODUCT} has no VERSION file: ${_mydesk_version_file}")
endif()
file(STRINGS "${_mydesk_version_file}" MYDESK_PACKAGE_VERSION LIMIT_COUNT 1)
string(STRIP "${MYDESK_PACKAGE_VERSION}" MYDESK_PACKAGE_VERSION)
if(NOT MYDESK_PACKAGE_VERSION MATCHES "^[0-9]+\\.[0-9]+\\.[0-9]+([-.][0-9A-Za-z.]+)?$")
  message(FATAL_ERROR
    "${MYDESK_PRODUCT} VERSION must be a semantic version; got '${MYDESK_PACKAGE_VERSION}'")
endif()

set(MYDESK_PACKAGE_INSTALL_DIR "share/mydesk-${MYDESK_PRODUCT}")
configure_file(
  "${PROJECT_SOURCE_DIR}/packaging/PACKAGE_README.txt.in"
  "${PROJECT_BINARY_DIR}/packaging/${MYDESK_PRODUCT}/README.txt"
  @ONLY)

install(FILES
  "${PROJECT_BINARY_DIR}/packaging/${MYDESK_PRODUCT}/README.txt"
  "${_mydesk_version_file}"
  "${PROJECT_SOURCE_DIR}/LICENSE"
  "${PROJECT_SOURCE_DIR}/NOTICE"
  "${PROJECT_SOURCE_DIR}/ATTRIBUTION.md"
  "${PROJECT_SOURCE_DIR}/docs/legal/THIRD_PARTY.md"
  DESTINATION "${MYDESK_PACKAGE_INSTALL_DIR}")
if(EXISTS "${_mydesk_product_dir}/CHANGELOG.md")
  install(FILES "${_mydesk_product_dir}/CHANGELOG.md"
    DESTINATION "${MYDESK_PACKAGE_INSTALL_DIR}")
endif()
if(EXISTS "${_mydesk_product_dir}/docs")
  install(DIRECTORY "${_mydesk_product_dir}/docs/"
    DESTINATION "${MYDESK_PACKAGE_INSTALL_DIR}/docs")
endif()

if(MYDESK_BUILD_TESTS AND MYDESK_BUILD_DESKTOP)
  set(_mydesk_relocatable_bundle_products
    draft draft-lt mechanical fusion buildings civil infra coordinate animate studio surface mill simulation vault)
  if(MYDESK_PRODUCT IN_LIST _mydesk_relocatable_bundle_products)
    set(_mydesk_require_relocatable_bundle TRUE)
  else()
    set(_mydesk_require_relocatable_bundle FALSE)
  endif()
  add_test(NAME packaging.install-contract
    COMMAND "${CMAKE_COMMAND}"
      "-DMYDESK_BUILD_DIR=${PROJECT_BINARY_DIR}"
      "-DMYDESK_VERIFY_ROOT=${PROJECT_BINARY_DIR}/_package-install-contract"
      "-DMYDESK_PRODUCT_ID=${MYDESK_PRODUCT}"
      "-DMYDESK_EXPECTED_BINARY=${MYDESK_PACKAGE_BINARY}"
      "-DMYDESK_EXECUTABLE_SUFFIX=${CMAKE_EXECUTABLE_SUFFIX}"
      "-DMYDESK_IS_APPLE=${APPLE}"
      "-DMYDESK_REQUIRE_RELOCATABLE_BUNDLE=${_mydesk_require_relocatable_bundle}"
      -P "${PROJECT_SOURCE_DIR}/packaging/verify-install.cmake")
  set_tests_properties(packaging.install-contract PROPERTIES
    LABELS "packaging")
endif()

set(CPACK_PACKAGE_NAME "${MYDESK_PACKAGE_BINARY}")
set(CPACK_PACKAGE_VENDOR "MyDesk contributors")
set(CPACK_PACKAGE_CONTACT "MyDesk contributors")
set(CPACK_PACKAGE_DESCRIPTION_SUMMARY "${MYDESK_PACKAGE_SUMMARY}")
set(CPACK_PACKAGE_HOMEPAGE_URL "https://github.com/shreynath/mydesk")
set(CPACK_PACKAGE_VERSION "${MYDESK_PACKAGE_VERSION}")
set(CPACK_PACKAGE_INSTALL_DIRECTORY "${MYDESK_PACKAGE_BINARY}")
set(CPACK_RESOURCE_FILE_LICENSE "${PROJECT_SOURCE_DIR}/LICENSE")
set(CPACK_RESOURCE_FILE_README
  "${PROJECT_BINARY_DIR}/packaging/${MYDESK_PRODUCT}/README.txt")
set(CPACK_PACKAGE_CHECKSUM SHA256)
set(CPACK_VERBATIM_VARIABLES YES)

string(TOLOWER "${CMAKE_SYSTEM_NAME}" _mydesk_package_system)
string(TOLOWER "${CMAKE_SYSTEM_PROCESSOR}" _mydesk_package_arch)
if(_mydesk_package_arch STREQUAL "")
  set(_mydesk_package_arch "unknown")
endif()
set(CPACK_PACKAGE_FILE_NAME
  "${MYDESK_PACKAGE_BINARY}-${MYDESK_PACKAGE_VERSION}-${_mydesk_package_system}-${_mydesk_package_arch}")

# These generators create per-product development artifacts. Product install
# rules may bundle their linked runtimes, but this layer intentionally makes no
# blanket claim about runtime completeness, signing, notarization, or
# clean-machine readiness; release-ready native installers remain a separate
# product-and-platform gate.
if(APPLE)
  set(_mydesk_default_generators "DragNDrop;TGZ")
elseif(WIN32)
  set(_mydesk_default_generators "ZIP")
else()
  set(_mydesk_default_generators "TGZ")
endif()
set(MYDESK_CPACK_GENERATORS "${_mydesk_default_generators}" CACHE STRING
  "Semicolon-separated CPack generators for this standalone product")
set(CPACK_GENERATOR "${MYDESK_CPACK_GENERATORS}")

include(CPack)
