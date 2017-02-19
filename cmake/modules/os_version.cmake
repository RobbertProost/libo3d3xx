function(get_os_version OS_NAME_STRING VERSION_STRING)
  set(OS_NAME ${OS_NAME_STRING})
  set(OS_VERSION ${VERSION_STRING})
  set(OS_MAJOR "0")
  set(OS_MINOR "0")
  set(OS_PATCH "0")

  if (OS_VERSION VERSION_EQUAL "0.0.0")
      execute_process(COMMAND "/usr/bin/lsb_release" "-is"
                      TIMEOUT 4
                      OUTPUT_VARIABLE OS_NAME
                      ERROR_QUIET
                      OUTPUT_STRIP_TRAILING_WHITESPACE)
  
      message(STATUS "Linux distro is: ${OS_NAME}")

      execute_process(COMMAND "/usr/bin/lsb_release" "-rs"
                      TIMEOUT 4
                      OUTPUT_VARIABLE OS_VERSION
                      ERROR_QUIET
                      OUTPUT_STRIP_TRAILING_WHITESPACE)
  
      message(STATUS "Linux version is: ${OS_VERSION}")
  endif()

  string(REPLACE "." ";" OS_VERSION_LIST ${OS_VERSION})
  list(LENGTH OS_VERSION_LIST vlen)
  if(NOT (vlen LESS 3))
    list(GET OS_VERSION_LIST 0 OS_MAJOR)
    list(GET OS_VERSION_LIST 1 OS_MINOR)
    list(GET OS_VERSION_LIST 2 OS_PATCH)
  elseif(NOT (vlen LESS 2))
    list(GET OS_VERSION_LIST 0 OS_MAJOR)
    list(GET OS_VERSION_LIST 1 OS_MINOR)
  elseif(NOT (vlen LESS 1))
    list(GET OS_VERSION_LIST 0 OS_MAJOR)
  endif()

  set(OS_NAME ${OS_NAME} PARENT_SCOPE)
  set(OS_VERSION ${OS_VERSION} PARENT_SCOPE)
  set(OS_MAJOR ${OS_MAJOR} PARENT_SCOPE)
  set(OS_MINOR ${OS_MINOR} PARENT_SCOPE)
  set(OS_PATCH ${OS_PATCH} PARENT_SCOPE)

  MESSAGE(STATUS "OS_VERSION: " ${OS_VERSION})
  MESSAGE(STATUS "OS_MAJOR: " ${OS_MAJOR})
  MESSAGE(STATUS "OS_MINOR: " ${OS_MINOR})
  MESSAGE(STATUS "OS_PATCH: " ${OS_PATCH})
endfunction()

# To explicitly set your OS version:
# cmake -DOS_VERSION:STRING=16.04 ..
set(OS_VERSION "0.0.0" CACHE STRING
    "User-overridable detected version of OS")

set(OS_NAME "Debian" CACHE STRING
    "User-overridable detected name of OS")
