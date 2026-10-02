# Install layout and the `tarball` target.
#
# OpenCPN's plugin manager installs a tarball whose single top-level
# directory holds metadata.xml and the files below, laid out per platform
# (see the *_entry_set_install_path() functions in OpenCPN's
# model/src/plugin_handler.cpp).

set(_data_dir "${CMAKE_CURRENT_SOURCE_DIR}/data")

if (APPLE)
  install(TARGETS ${PACKAGE_NAME}
          LIBRARY DESTINATION OpenCPN.app/Contents/PlugIns
          RUNTIME DESTINATION OpenCPN.app/Contents/PlugIns)
  install(DIRECTORY ${_data_dir}/ DESTINATION
          OpenCPN.app/Contents/SharedSupport/plugins/${PACKAGE_NAME}/data)
elseif (WIN32)
  # Only the DLL: OpenCPN rejects a tarball holding anything else outside
  # plugins/, such as the import library.
  install(FILES $<TARGET_FILE:${PACKAGE_NAME}> DESTINATION plugins)
  install(DIRECTORY ${_data_dir}/ DESTINATION plugins/${PACKAGE_NAME}/data)
else ()
  install(TARGETS ${PACKAGE_NAME} LIBRARY DESTINATION lib/opencpn)
  install(DIRECTORY ${_data_dir}/
          DESTINATION share/opencpn/plugins/${PACKAGE_NAME}/data)
endif ()

# -------- Metadata --------
set(OBSERVER_TARBALL_BASE_URL "" CACHE STRING
    "URL the published tarball will live under, without the file name")

string(REPLACE "+" "." _file_version "${OBSERVER_VERSION}")
set(pkg_displayname
    "${PLUGIN_API_NAME}-${_file_version}-${plugin_target}-${plugin_target_version}"
)
set(pkg_tarname
    "${PLUGIN_API_NAME}-${_file_version}_${plugin_target}-${plugin_target_version}-${plugin_arch_label}"
)
if (OBSERVER_TARBALL_BASE_URL)
  set(pkg_tarball_url "${OBSERVER_TARBALL_BASE_URL}/${pkg_tarname}.tar.gz")
else ()
  set(pkg_tarball_url "${pkg_tarname}.tar.gz")
endif ()

# API_VERSION comes from opencpn-libs/<api>/CMakeLists.txt.
set(checksum "@checksum@")  # Filled in once the tarball exists
configure_file(${CMAKE_CURRENT_SOURCE_DIR}/cmake/metadata.xml.in
               ${CMAKE_BINARY_DIR}/metadata.xml.in @ONLY)

# -------- tarball target --------
add_custom_target(
  tarball
  COMMAND
    ${CMAKE_COMMAND} -DBUILD_DIR=${CMAKE_BINARY_DIR} -DCONFIG=$<CONFIG>
    -DDISPLAY_NAME=${pkg_displayname} -DTAR_NAME=${pkg_tarname}
    -DAPPLE_FIX_LIBS=$<BOOL:${APPLE}> -P
    ${CMAKE_CURRENT_SOURCE_DIR}/cmake/MakeTarball.cmake
  DEPENDS ${PACKAGE_NAME}
  WORKING_DIRECTORY ${CMAKE_BINARY_DIR}
  COMMENT "Packaging ${pkg_tarname}.tar.gz"
  VERBATIM
)
