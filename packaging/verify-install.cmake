foreach(_required
    MYDESK_BUILD_DIR MYDESK_VERIFY_ROOT MYDESK_PRODUCT_ID MYDESK_EXPECTED_BINARY)
  if(NOT DEFINED ${_required} OR "${${_required}}" STREQUAL "")
    message(FATAL_ERROR "verify-install.cmake requires -D${_required}=...")
  endif()
endforeach()

cmake_path(IS_PREFIX MYDESK_BUILD_DIR "${MYDESK_VERIFY_ROOT}" NORMALIZE _root_is_in_build)
if(NOT _root_is_in_build OR MYDESK_VERIFY_ROOT STREQUAL MYDESK_BUILD_DIR)
  message(FATAL_ERROR
    "Refusing to replace staging root outside the build directory: ${MYDESK_VERIFY_ROOT}")
endif()

file(REMOVE_RECURSE "${MYDESK_VERIFY_ROOT}")
execute_process(
  COMMAND "${CMAKE_COMMAND}" --install "${MYDESK_BUILD_DIR}"
    --prefix "${MYDESK_VERIFY_ROOT}"
  RESULT_VARIABLE _install_result
  OUTPUT_VARIABLE _install_stdout
  ERROR_VARIABLE _install_stderr)
if(NOT _install_result EQUAL 0)
  message(FATAL_ERROR
    "Standalone install failed (${_install_result})\n${_install_stdout}\n${_install_stderr}")
endif()

if(MYDESK_IS_APPLE)
  set(_expected_app
    "${MYDESK_VERIFY_ROOT}/${MYDESK_EXPECTED_BINARY}.app/Contents/MacOS/${MYDESK_EXPECTED_BINARY}")
endif()
set(_expected_cli
  "${MYDESK_VERIFY_ROOT}/bin/${MYDESK_EXPECTED_BINARY}${MYDESK_EXECUTABLE_SUFFIX}")
if(DEFINED _expected_app AND EXISTS "${_expected_app}")
  set(_installed_binary "${_expected_app}")
elseif(EXISTS "${_expected_cli}")
  set(_installed_binary "${_expected_cli}")
else()
  message(FATAL_ERROR
    "Expected standalone executable was not installed. Checked:\n"
    "  ${_expected_app}\n  ${_expected_cli}")
endif()

set(_known_binaries
  MyDeskDraft MyDeskDraftLT MyDeskMechanical MyDeskFusion MyDeskBuildings
  MyDeskCivil MyDeskInfra MyDeskCoordinate MyDeskAnimate MyDeskStudio
  MyDeskSurface MyDeskMill MyDeskSimulation MyDeskVault)
file(GLOB_RECURSE _installed_files LIST_DIRECTORIES FALSE
  "${MYDESK_VERIFY_ROOT}/*")
foreach(_installed_file IN LISTS _installed_files)
  get_filename_component(_installed_name "${_installed_file}" NAME)
  string(REGEX REPLACE "\\.exe$" "" _installed_stem "${_installed_name}")
  if(_installed_stem IN_LIST _known_binaries AND
      NOT _installed_stem STREQUAL MYDESK_EXPECTED_BINARY)
    message(FATAL_ERROR
      "${MYDESK_PRODUCT_ID} install leaked another product executable: ${_installed_file}")
  endif()
endforeach()

set(_metadata_root "${MYDESK_VERIFY_ROOT}/share/mydesk-${MYDESK_PRODUCT_ID}")
foreach(_metadata README.txt VERSION LICENSE NOTICE ATTRIBUTION.md THIRD_PARTY.md)
  if(NOT EXISTS "${_metadata_root}/${_metadata}")
    message(FATAL_ERROR
      "${MYDESK_PRODUCT_ID} install is missing package metadata: ${_metadata}")
  endif()
endforeach()

message(STATUS
  "Verified isolated ${MYDESK_PRODUCT_ID} install: ${_installed_binary}")
