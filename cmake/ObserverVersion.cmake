# Calendar versioning: YYYY.M.N
#
#   YYYY  release year
#   M     release month, no leading zero (semver forbids one)
#   N     release number within that month, starting at 0
#
# Resolution order:
#   1. -DOBSERVER_VERSION=2026.10.0 (what CI passes)
#   2. the OBSERVER_VERSION environment variable
#   3. a vYYYY.M.N tag on HEAD
#   4. the next free number for the current month, marked "-dev" with the
#      commit hash as build metadata (local builds)
#
# Sets OBSERVER_VERSION (full semver), OBSERVER_VERSION_CORE (YYYY.M.N),
# OBSERVER_VERSION_PRE (pre-release, may be empty) and
# OBSERVER_VERSION_BUILD (build metadata, may be empty).

set(_calver_re "^([0-9][0-9][0-9][0-9])\\.([1-9]|1[0-2])\\.([0-9]+)")

if (NOT OBSERVER_VERSION AND DEFINED ENV{OBSERVER_VERSION})
  set(OBSERVER_VERSION "$ENV{OBSERVER_VERSION}")
endif ()

find_package(Git QUIET)

function (_observer_git out)
  if (NOT GIT_FOUND)
    set(${out} "" PARENT_SCOPE)
    return ()
  endif ()
  execute_process(
    COMMAND ${GIT_EXECUTABLE} ${ARGN}
    WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}
    OUTPUT_VARIABLE _out
    ERROR_QUIET
    RESULT_VARIABLE _rc
    OUTPUT_STRIP_TRAILING_WHITESPACE
  )
  if (_rc EQUAL 0)
    set(${out} "${_out}" PARENT_SCOPE)
  else ()
    set(${out} "" PARENT_SCOPE)
  endif ()
endfunction ()

if (NOT OBSERVER_VERSION)
  _observer_git(_head_tags tag --points-at HEAD)
  string(REPLACE "\n" ";" _head_tags "${_head_tags}")
  foreach (_tag IN LISTS _head_tags)
    if (_tag MATCHES "^v([0-9]+\\.[0-9]+\\.[0-9]+)$")
      set(OBSERVER_VERSION "${CMAKE_MATCH_1}")
      break ()
    endif ()
  endforeach ()
endif ()

if (NOT OBSERVER_VERSION)
  string(TIMESTAMP _year "%Y" UTC)
  string(TIMESTAMP _month "%m" UTC)
  math(EXPR _month "${_month}")  # drop the leading zero
  _observer_git(_month_tags tag --list "v${_year}.${_month}.*")
  string(REPLACE "\n" ";" _month_tags "${_month_tags}")
  set(_next 0)
  foreach (_tag IN LISTS _month_tags)
    if (_tag MATCHES "^v${_year}\\.${_month}\\.([0-9]+)$")
      if (CMAKE_MATCH_1 GREATER_EQUAL _next)
        math(EXPR _next "${CMAKE_MATCH_1} + 1")
      endif ()
    endif ()
  endforeach ()
  _observer_git(_hash rev-parse --short=8 HEAD)
  if (_hash)
    set(OBSERVER_VERSION "${_year}.${_month}.${_next}-dev+g${_hash}")
  else ()
    set(OBSERVER_VERSION "${_year}.${_month}.${_next}-dev")
  endif ()
endif ()

string(REGEX MATCH "${_calver_re}" _m "${OBSERVER_VERSION}")
if (NOT _m)
  message(FATAL_ERROR
    "OBSERVER_VERSION '${OBSERVER_VERSION}' is not a YYYY.M.N calendar version"
  )
endif ()
set(OBSERVER_VERSION_YEAR ${CMAKE_MATCH_1})
set(OBSERVER_VERSION_MONTH ${CMAKE_MATCH_2})
set(OBSERVER_VERSION_NUMBER ${CMAKE_MATCH_3})
set(OBSERVER_VERSION_CORE
    "${OBSERVER_VERSION_YEAR}.${OBSERVER_VERSION_MONTH}.${OBSERVER_VERSION_NUMBER}"
)

set(OBSERVER_VERSION_PRE "")
set(OBSERVER_VERSION_BUILD "")
string(REGEX MATCH "\\+(.*)$" _m "${OBSERVER_VERSION}")
if (_m)
  set(OBSERVER_VERSION_BUILD "${CMAKE_MATCH_1}")
endif ()
string(REGEX MATCH "^[0-9.]+-([^+]*)" _m "${OBSERVER_VERSION}")
if (_m)
  set(OBSERVER_VERSION_PRE "${CMAKE_MATCH_1}")
endif ()

message(STATUS "Observer version: ${OBSERVER_VERSION}")
