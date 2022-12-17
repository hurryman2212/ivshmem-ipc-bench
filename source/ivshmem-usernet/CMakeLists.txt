###########################################################
## TARGETS
###########################################################

add_executable(ivshmem-usernet-client client.c)
add_executable(ivshmem-usernet-server server.c)
add_executable(ivshmem-usernet ivshmem-usernet.c)

###########################################################
## COMMON
###########################################################

target_link_libraries(ivshmem-usernet-client ipc-bench-common)
target_link_libraries(ivshmem-usernet-server ipc-bench-common)
target_link_libraries(ivshmem-usernet ipc-bench-common)
