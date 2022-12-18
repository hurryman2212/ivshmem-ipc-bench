###########################################################
## TARGETS
###########################################################

add_executable(ivshmem-uio-client client.c)
add_executable(ivshmem-uio-server server.c)

###########################################################
## COMMON
###########################################################

target_link_libraries(ivshmem-uio-client ipc-bench-common)
target_link_libraries(ivshmem-uio-server ipc-bench-common)
