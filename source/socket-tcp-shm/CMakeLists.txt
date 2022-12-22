###########################################################
## TARGETS
###########################################################

add_executable(socket-tcp-shm-client client.c)
add_executable(socket-tcp-shm-server server.c)

###########################################################
## COMMON
###########################################################

target_link_libraries(socket-tcp-shm-client ipc-bench-common)
target_link_libraries(socket-tcp-shm-server ipc-bench-common)
