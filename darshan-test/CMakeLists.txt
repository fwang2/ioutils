cmake_minimum_required(VERSION 2.8)
project(darshan-test)


if ( ${CMAKE_SOURCE_DIR} STREQUAL ${CMAKE_BINARY_DIR}  )
        file(REMOVE test.txt)
        message( FATAL_ERROR "In-source builds not allowed. Please make a new
        directory (called a build directory) and run CMake from there. You may
        need to remove CMakeCache.txt."  )
endif()


# set c99
set(CMAKE_C_FLAGS  "${CMAKE_C_FLAGS} -Wall -std=c99")
set(CMAKE_C_FLAGS_DEBUG  "${CMAKE_C_FLAGS_DEBUG} -Wall -std=c99")

# set c++11 support
set(CMAKE_CXX_FLAGS  "${CMAKE_CXX_CFLAGS} -Wall -std=c++11")
set(CMAKE_CXX_FLAGS_DEBUG   "${CMAKE_CXX_FLAGS_DEBUG} -Wall")
set(CMAKE_CXX_FLAGS_RELEASE "${CMAKE_CXX_FLAGS_RELEASE} -Wall")

# Compiler-specific C++11 activation.
if ("${CMAKE_CXX_COMPILER_ID}" MATCHES "GNU")
    execute_process(
        COMMAND ${CMAKE_CXX_COMPILER} -dumpversion OUTPUT_VARIABLE GCC_VERSION)
    if (NOT (GCC_VERSION VERSION_GREATER 4.7 OR GCC_VERSION VERSION_EQUAL 4.7))
        message(FATAL_ERROR "${PROJECT_NAME} requires g++ 4.7 or greater.")
    endif ()
elseif ("${CMAKE_CXX_COMPILER_ID}" MATCHES "Clang")
    set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -stdlib=libc++")
else ()
    message(FATAL_ERROR "Your C++ compiler does not support C++11.")
endif ()

set(CMAKE_MODULE_PATH ${PROJECT_SOURCE_DIR}/CMakeScripts)


## MPI
find_package(MPI REQUIRED)
include_directories(SYSTEM ${MPI_INCLUDE_PATH})

## Build
add_executable(fprintf fprintf.cc)
add_executable(fwrite fwrite.cc)
add_executable(write write.cc)

add_executable(readn readn.cc)
target_link_libraries(readn ${MPI_LIBRARIES})

add_executable(fprintf-mpi fprintf-mpi.cc)
target_link_libraries(fprintf-mpi ${MPI_LIBRARIES})

add_executable(fwrite-mpi fwrite-mpi.cc)
target_link_libraries(fwrite-mpi ${MPI_LIBRARIES})

add_executable(mpiio-single-file mpiio-single-file.cc)
target_link_libraries(mpiio-single-file ${MPI_LIBRARIES})

#add_executable(read-after-write read-after-write.cc)
#target_link_libraries(read-after-write ${MPI_LIBRARIES})

add_executable(mpiio-shared-write mpiio-shared-write.cc)
target_link_libraries(mpiio-shared-write ${MPI_LIBRARIES})

add_executable(mpiio-shared-read mpiio-shared-read.cc)
target_link_libraries(mpiio-shared-read ${MPI_LIBRARIES})

add_executable(mpiio-shared-raw mpiio-shared-raw.cc)
target_link_libraries(mpiio-shared-raw ${MPI_LIBRARIES})






