###########################################################
## TARGETS
###########################################################

add_executable(ivshmem-shm-client client.c)
add_executable(ivshmem-shm-server server.c)

###########################################################
## COMMON
###########################################################

target_link_libraries(ivshmem-shm-client ipc-bench-common)
target_link_libraries(ivshmem-shm-server ipc-bench-common)
