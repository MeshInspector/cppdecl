
cmake_minimum_required(VERSION 3.20)
project(cppdecl)

set(CMAKE_CXX_STANDARD 20)


if(CMAKE_CXX_COMPILER MATCHES "/em\\+\\+(-[a-zA-Z0-9.])?$")
  message(" * C++ compiler: Emscripten")
  set(CMAKE_CXX_COMPILER_ID "Emscripten")
  if ("${EMSCRIPTEN_ENV}" STREQUAL "web")
  set(CMAKE_EXECUTABLE_SUFFIX ".html")
	else()
  set(CMAKE_EXECUTABLE_SUFFIX ".js")
  endif()
  set(CMAKE_CXX_BYTE_ORDER "LITTLE_ENDIAN")
else()
  message(" * C++ compiler: ${CMAKE_CXX_COMPILER_ID}")
endif()

if ("${EMSCRIPTEN_ENV}" STREQUAL "web")

	add_executable(index cppdecl-shell.cpp )
	target_include_directories(index PUBLIC ../include)
	target_link_options(index PUBLIC  --shell-file ${CMAKE_SOURCE_DIR}/cppdecl-shell.html)
	target_link_options(index PUBLIC -sEXPORTED_FUNCTIONS=_main,_malloc,_free -sEXPORTED_RUNTIME_METHODS=UTF8ToString,stringToUTF8,lengthBytesUTF8,ccall,cwrap)

else()
	add_executable(cppdecl repl.cpp)
	target_include_directories(cppdecl PUBLIC ../include)

	add_executable(tests tests.cpp)
	target_include_directories(tests PUBLIC ../include)
endif()
