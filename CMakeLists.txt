cmake_minimum_required(VERSION 3.16)
project( terminalraycast )

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

#if(NOT CMAKE_BUILD_TYPE)
#  set(CMAKE_BUILD_TYPE Release)
#endif()

#set(CMAKE_CXX_FLAGS "-Wall -Wextra")
#set(CMAKE_CXX_FLAGS_DEBUG "-g")
#set(CMAKE_CXX_FLAGS_RELEASE "-O3")
set(CMAKE_BUILD_TYPE Debug)

#string(REGEX REPLACE "([\\/\\-]O)3" "\\12"
#  CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS}")
#string(REGEX REPLACE "([\\/\\-]O)3" "\\12"
#  CMAKE_CXX_FLAGS_RELEASE "${CMAKE_CXX_FLAGS_RELEASE}")
#string(REGEX REPLACE "([\\/\\-]O)3" "\\12"
#  CMAKE_CXX_FLAGS_MINSIZEREL "${CMAKE_CXX_FLAGS_MINSIZEREL}")
#string(REGEX REPLACE "([\\/\\-]O)3" "\\12"
#  CMAKE_CXX_FLAGS_RELWITHDEBINFO "${CMAKE_CXX_FLAGS_RELWITHDEBINFO}")

find_package(Curses REQUIRED)

if(NOT CURSES_HAVE_CURSES_H)
  foreach(d IN LISTS CURSES_INCLUDE_DIRS)
    find_path(curses_inc
    NAMES curses.h
    HINTS ${d}/ncurses ${d}/pdcurses
    NO_DEFAULT_PATH
    )
    if(curses_inc)
      list(APPEND CURSES_INCLUDE_DIRS ${curses_inc})
      break()
    endif()
  endforeach()
endif()

if(NOT TARGET Curses::Curses)
  add_library(Curses::Curses INTERFACE IMPORTED)
  target_link_libraries(Curses::Curses INTERFACE ${CURSES_LIBRARIES})
  target_include_directories(Curses::Curses INTERFACE ${CURSES_INCLUDE_DIRS})
  if(CURSES_HAVE_NCURSES_H OR CURSES_HAVE_NCURSES_NCURSES_H)
    # https://stackoverflow.com/a/67964252/4236751
    # avoids MinGW link failure
    target_compile_definitions(Curses::Curses INTERFACE $<$<BOOL:${MINGW}>:NCURSES_STATIC>)
  endif()
endif()

add_executable( terminalraycast main.cpp )
target_link_libraries(terminalraycast PRIVATE Curses::Curses)
include_directories( "./" )