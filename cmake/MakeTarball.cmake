# Build the plugin manager tarball and its catalog metadata.
#
# Run by the `tarball` target as:
#   cmake -DBUILD_DIR=... -DCONFIG=... -DDISPLAY_NAME=... -DTAR_NAME=...
#         -DAPPLE_FIX_LIBS=0|1 -P MakeTarball.cmake
#
# Produces, in BUILD_DIR:
#   <TAR_NAME>.tar.gz   install tree + metadata.xml under DISPLAY_NAME/
#   <TAR_NAME>.xml      metadata with tarball checksum, for a catalog

cmake_minimum_required(VERSION 3.16)

foreach (_var BUILD_DIR CONFIG DISPLAY_NAME TAR_NAME)
  if (NOT DEFINED ${_var})
    message(FATAL_ERROR "MakeTarball.cmake: ${_var} is not set")
  endif ()
endforeach ()

set(_stage_root "${BUILD_DIR}/pkg")
set(_stage "${_stage_root}/${DISPLAY_NAME}")
file(REMOVE_RECURSE "${_stage_root}")
file(MAKE_DIRECTORY "${_stage}")

set(_strip "")
if (CONFIG MATCHES "Release|MinSizeRel")
  set(_strip "--strip")
endif ()
execute_process(
  COMMAND ${CMAKE_COMMAND} --install "${BUILD_DIR}" --prefix "${_stage}"
          --config "${CONFIG}" ${_strip}
  RESULT_VARIABLE _rc
)
if (NOT _rc EQUAL 0)
  message(FATAL_ERROR "Install into staging directory failed")
endif ()

# On macOS the plugin is linked against a build-machine wxWidgets; point
# it at the copy bundled in OpenCPN.app instead.
if (APPLE_FIX_LIBS)
  file(GLOB_RECURSE _dylibs "${_stage}/*.dylib")
  foreach (_lib IN LISTS _dylibs)
    execute_process(COMMAND otool -L "${_lib}" OUTPUT_VARIABLE _deps)
    string(REPLACE "\n" ";" _deps "${_deps}")
    foreach (_line IN LISTS _deps)
      if (_line MATCHES "^[ \t]+([^ ]*libwx[^ ]*\\.dylib)")
        set(_dep "${CMAKE_MATCH_1}")
        get_filename_component(_name "${_dep}" NAME)
        if (NOT _dep MATCHES "^@")
          execute_process(
            COMMAND install_name_tool -change "${_dep}"
                    "@executable_path/../Frameworks/${_name}" "${_lib}"
          )
        endif ()
      endif ()
    endforeach ()
    # Editing load commands voids the signature; Apple silicon refuses to
    # load unsigned code, so sign again (ad hoc).
    execute_process(COMMAND codesign --force --sign - "${_lib}"
                    RESULT_VARIABLE _rc)
    if (NOT _rc EQUAL 0)
      message(FATAL_ERROR "codesign failed for ${_lib}")
    endif ()
    execute_process(COMMAND otool -L "${_lib}")
  endforeach ()
endif ()

# metadata.xml inside the tarball carries no checksum: it cannot know it.
set(checksum "")
configure_file("${BUILD_DIR}/metadata.xml.in" "${_stage}/metadata.xml" @ONLY)

set(_tarball "${BUILD_DIR}/${TAR_NAME}.tar.gz")
file(REMOVE "${_tarball}")
execute_process(
  COMMAND ${CMAKE_COMMAND} -E tar czf "${_tarball}" --format=gnutar
          "${DISPLAY_NAME}"
  WORKING_DIRECTORY "${_stage_root}"
  RESULT_VARIABLE _rc
)
if (NOT _rc EQUAL 0)
  message(FATAL_ERROR "Creating ${_tarball} failed")
endif ()

file(SHA256 "${_tarball}" checksum)
configure_file("${BUILD_DIR}/metadata.xml.in" "${BUILD_DIR}/${TAR_NAME}.xml"
               @ONLY)
message(STATUS "Created ${TAR_NAME}.tar.gz (sha256 ${checksum})")
