###########################################################
## TARGETS
###########################################################

add_executable(socket-udp-shm-client client.c)
add_executable(socket-udp-shm-server server.c)

###########################################################
## COMMON
###########################################################

target_link_libraries(socket-udp-shm-client ipc-bench-common)
target_link_libraries(socket-udp-shm-server ipc-bench-common)
