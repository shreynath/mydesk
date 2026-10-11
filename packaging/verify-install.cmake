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

# Products that advertise a deployed macOS runtime must not accidentally keep
# dependencies on a developer's Homebrew, MacPorts, or home-directory paths.
if(MYDESK_IS_APPLE AND MYDESK_REQUIRE_RELOCATABLE_BUNDLE)
  set(_frameworks_root
    "${MYDESK_VERIFY_ROOT}/${MYDESK_EXPECTED_BINARY}.app/Contents/Frameworks")
  if(NOT EXISTS "${_frameworks_root}/QtCore.framework")
    message(FATAL_ERROR
      "${MYDESK_PRODUCT_ID} claims a deployed runtime but QtCore.framework is absent")
  endif()

  find_program(_mydesk_otool otool REQUIRED)
  foreach(_installed_file IN LISTS _installed_files)
    execute_process(
      COMMAND "${_mydesk_otool}" -l "${_installed_file}"
      RESULT_VARIABLE _otool_result
      OUTPUT_VARIABLE _otool_output
      ERROR_QUIET)
    if(_otool_result EQUAL 0)
      string(REPLACE "\n" ";" _otool_lines "${_otool_output}")
      set(_expect_load_name FALSE)
      foreach(_otool_line IN LISTS _otool_lines)
        if(_otool_line MATCHES
            "^[ \t]*cmd LC_(LOAD|LOAD_WEAK|LAZY_LOAD|LOAD_UPWARD|REEXPORT)_DYLIB")
          set(_expect_load_name TRUE)
        elseif(_expect_load_name AND _otool_line MATCHES "^[ \t]*name ([^ ]+)")
          set(_load_path "${CMAKE_MATCH_1}")
          set(_expect_load_name FALSE)
          if(_load_path MATCHES "^/(opt/homebrew|opt/local|usr/local|Users)/")
            message(FATAL_ERROR
              "${MYDESK_PRODUCT_ID} installed Mach-O retains a host dependency:\n"
              "${_installed_file}\n${_load_path}")
          endif()
        endif()
      endforeach()
    endif()
  endforeach()
endif()

if(MYDESK_REQUIRE_RELOCATABLE_BUNDLE)
  if(MYDESK_IS_APPLE)
    execute_process(
      COMMAND "${_installed_binary}" --packaging-smoke
      RESULT_VARIABLE _launch_result
      OUTPUT_VARIABLE _launch_stdout
      ERROR_VARIABLE _launch_stderr
      TIMEOUT 15)
  else()
    execute_process(
      COMMAND "${CMAKE_COMMAND}" -E env QT_QPA_PLATFORM=offscreen
        "${_installed_binary}" --packaging-smoke
      RESULT_VARIABLE _launch_result
      OUTPUT_VARIABLE _launch_stdout
      ERROR_VARIABLE _launch_stderr
      TIMEOUT 15)
  endif()
  if(NOT _launch_result EQUAL 0)
    message(FATAL_ERROR
      "Installed ${MYDESK_PRODUCT_ID} application failed its UI construction smoke "
      "(${_launch_result})\n${_launch_stdout}\n${_launch_stderr}")
  endif()
endif()

set(_metadata_root "${MYDESK_VERIFY_ROOT}/share/mydesk-${MYDESK_PRODUCT_ID}")
foreach(_metadata README.txt VERSION LICENSE NOTICE ATTRIBUTION.md THIRD_PARTY.md)
  if(NOT EXISTS "${_metadata_root}/${_metadata}")
    message(FATAL_ERROR
      "${MYDESK_PRODUCT_ID} install is missing package metadata: ${_metadata}")
  endif()
endforeach()

message(STATUS
  "Verified isolated ${MYDESK_PRODUCT_ID} install: ${_installed_binary}")
