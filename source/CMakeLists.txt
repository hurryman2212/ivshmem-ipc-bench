###########################################################
## CONFIGURATION
###########################################################

include_directories(${CMAKE_CURRENT_SOURCE_DIR})

add_compile_options(-g -O3 -Wall -Werror)

add_compile_options(-std=gnu11)
link_libraries(m)

link_libraries(pthread)

###########################################################
## DEPENDENCIES
###########################################################

find_package(PkgConfig REQUIRED)

###########################################################
## SUBDIRECTORIES
###########################################################

add_subdirectory(common)

add_subdirectory(shm)
add_subdirectory(ivshmem-shm)

add_subdirectory(ivshmem-uio)

add_subdirectory(socket-tcp-shm)
add_subdirectory(ivshmem-usernet)

add_subdirectory(socket-tcp)

