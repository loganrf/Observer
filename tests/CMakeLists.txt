add_executable(
  observer_tests
  test_main.cpp
  test_csv.cpp
  test_export.cpp
  test_geo.cpp
  test_nmea.cpp
  test_observation.cpp
  test_store.cpp
  test_time.cpp
  testing.h
)
target_link_libraries(observer_tests PRIVATE observer_core)
add_test(NAME observer_tests COMMAND observer_tests)

# A stand-in OpenCPN that loads the built plugin (Linux only, needs X).
if (UNIX AND NOT APPLE)
  add_executable(stub_host stub_host.cpp)
  set_target_properties(stub_host PROPERTIES ENABLE_EXPORTS ON)
  target_link_libraries(stub_host PRIVATE ocpn::api ${wxWidgets_LIBRARIES}
                                          ${CMAKE_DL_LIBS})
  add_dependencies(stub_host ${PACKAGE_NAME})

  # GetPluginDataDir() in the stub resolves to <dir>/observer_pi.
  set(_host_data ${CMAKE_BINARY_DIR}/host-data)
  add_custom_command(
    TARGET stub_host POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E copy_directory ${PROJECT_SOURCE_DIR}/data
            ${_host_data}/${PACKAGE_NAME}/data)

  find_program(XVFB_RUN xvfb-run)
  set(_host_cmd $<TARGET_FILE:stub_host> $<TARGET_FILE:${PACKAGE_NAME}>
                ${_host_data})
  if (XVFB_RUN)
    set(_x ${XVFB_RUN} -a)
  endif ()
  if (XVFB_RUN OR DEFINED ENV{DISPLAY})
    add_test(NAME plugin_loads
             COMMAND ${_x} ${_host_cmd} ${CMAKE_BINARY_DIR}/host-smoke smoke)
    add_test(NAME plugin_logs_sighting
             COMMAND ${_x} ${_host_cmd} ${CMAKE_BINARY_DIR}/host-save save)
    # Each run starts from an empty log.
    set_tests_properties(plugin_logs_sighting PROPERTIES FIXTURES_REQUIRED
                                                         clean_host)
    add_test(NAME clean_host_dirs
             COMMAND ${CMAKE_COMMAND} -E rm -rf ${CMAKE_BINARY_DIR}/host-save)
    set_tests_properties(clean_host_dirs PROPERTIES FIXTURES_SETUP clean_host)
  else ()
    message(STATUS "No X display or xvfb-run: plugin_loads test skipped")
  endif ()
endif ()
