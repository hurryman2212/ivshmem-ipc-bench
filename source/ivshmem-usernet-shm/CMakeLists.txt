###########################################################
## TARGETS
###########################################################

add_executable(ivshmem-usernet-shm-client client.c)
add_executable(ivshmem-usernet-shm-server server.c)

###########################################################
## COMMON
###########################################################

target_link_libraries(ivshmem-usernet-shm-client ipc-bench-common)
target_link_libraries(ivshmem-usernet-shm-server ipc-bench-common)
