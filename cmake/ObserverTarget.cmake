# Work out the OpenCPN plugin ABI this build is for.
#
# OpenCPN's plugin manager only offers a plugin whose metadata names a
# target the host accepts (see PluginHandler::IsCompatible in OpenCPN).
# CI passes the target explicitly as OCPN_TARGET_TUPLE, for example
# "debian-x86_64;12;x86_64" or "flatpak-aarch64;24.08;aarch64". Local
# builds detect it.
#
# Sets plugin_target, plugin_target_version and plugin_arch.

set(OCPN_TARGET_TUPLE "" CACHE STRING
    "OpenCPN plugin target as \"target;version;arch\"")

# Architecture name as OpenCPN spells it.
if (APPLE AND CMAKE_OSX_ARCHITECTURES)
  set(_arch "${CMAKE_OSX_ARCHITECTURES}")
elseif (WIN32)
  if (CMAKE_SIZEOF_VOID_P EQUAL 8)
    set(_arch "x86_64")
  else ()
    set(_arch "x86")
  endif ()
elseif (CMAKE_SYSTEM_PROCESSOR MATCHES "^(aarch64|arm64|ARM64)$")
  set(_arch "arm64")
elseif (CMAKE_SYSTEM_PROCESSOR MATCHES "^arm")
  set(_arch "armhf")
else ()
  set(_arch "${CMAKE_SYSTEM_PROCESSOR}")
endif ()

if (NOT OCPN_TARGET_TUPLE STREQUAL "")
  list(GET OCPN_TARGET_TUPLE 0 plugin_target)
  list(GET OCPN_TARGET_TUPLE 1 plugin_target_version)
  list(GET OCPN_TARGET_TUPLE 2 plugin_arch)
  if (plugin_arch STREQUAL "universal")
    set(plugin_arch "x86_64;arm64")
  endif ()
elseif (DEFINED ENV{FLATPAK_ID})
  if (_arch STREQUAL "arm64")
    set(_arch "aarch64")
  endif ()
  set(plugin_target "flatpak-${_arch}")
  set(plugin_target_version "$ENV{OBSERVER_FLATPAK_SDK}")
  set(plugin_arch "${_arch}")
elseif (MSVC)
  if (CMAKE_SIZEOF_VOID_P EQUAL 8)
    set(plugin_target "msvc-64")
  else ()
    set(plugin_target "msvc-wx32")
  endif ()
  set(plugin_target_version "10")
  set(plugin_arch "${_arch}")
elseif (APPLE)
  set(plugin_target "darwin-wx32")
  set(plugin_target_version "11")
  set(plugin_arch "${_arch}")
  if (NOT plugin_arch)
    set(plugin_arch "${CMAKE_HOST_SYSTEM_PROCESSOR}")
  endif ()
elseif (UNIX)
  find_program(LSB_RELEASE lsb_release)
  if (LSB_RELEASE)
    execute_process(COMMAND ${LSB_RELEASE} -is OUTPUT_VARIABLE _id
                    OUTPUT_STRIP_TRAILING_WHITESPACE)
    execute_process(COMMAND ${LSB_RELEASE} -rs OUTPUT_VARIABLE _rel
                    OUTPUT_STRIP_TRAILING_WHITESPACE)
  else ()
    set(_id "linux")
    set(_rel "1")
  endif ()
  string(TOLOWER "${_id}" _id)
  set(plugin_target "${_id}-${_arch}")
  set(plugin_target_version "${_rel}")
  set(plugin_arch "${_arch}")
else ()
  set(plugin_target "unknown")
  set(plugin_target_version "1")
  set(plugin_arch "${_arch}")
endif ()

string(TOLOWER "${plugin_target}" plugin_target)
string(TOLOWER "${plugin_target_version}" plugin_target_version)

# Arch label for file names; "x86_64;arm64" reads as "universal".
string(REPLACE ";" "-" plugin_arch_label "${plugin_arch}")
if (plugin_arch MATCHES "x86_64" AND plugin_arch MATCHES "arm64")
  set(plugin_arch_label "universal")
endif ()

message(STATUS
  "Plugin target: ${plugin_target} ${plugin_target_version} (${plugin_arch_label})"
)
