cmake_minimum_required(VERSION 3.10.0)
project(pyorekt-c VERSION 0.1.0 LANGUAGES C CXX)

set(CMAKE_CXX_STANDARD 23)
set(CMAKE_CXX_STANDARD_REQUIRED True)

# Libpq and libgit2

find_package(PostgreSQL REQUIRED)
find_package(PkgConfig REQUIRED)
pkg_check_modules(LIBGIT2 REQUIRED libgit2)

add_executable(pyorekt-c main.cpp server.cpp)

if(PostgreSQL_FOUND)
	message(STATUS "Found PostgreSQL: ${PostgreSQL_VERSION_STRING}")
	target_include_directories(pyorekt-c PRIVATE ${PostgreSQL_INCLUDE_DIRS})
	target_link_libraries(pyorekt-c PRIVATE ${PostgreSQL_LIBRARIES})
else()
	message(FATAL_ERROR "PostgreSQL (libpq) not found. Install the development package (e.g. libpq-dev or postgresql-devel) and try again.")
endif()

if(LIBGIT2_FOUND)
	message(STATUS "Found libgit2: ${LIBGIT2_VERSION}")
	target_include_directories(pyorekt-c PRIVATE ${LIBGIT2_INCLUDE_DIRS})
	target_link_libraries(pyorekt-c PRIVATE ${LIBGIT2_LIBRARIES})
	target_compile_options(pyorekt-c PRIVATE ${LIBGIT2_CFLAGS_OTHER})
else()
	message(FATAL_ERROR "libgit2 not found. Install the development package (e.g. libgit2-dev) and try again.")
endif()

