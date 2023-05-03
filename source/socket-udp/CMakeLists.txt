###########################################################
## TARGETS
###########################################################

add_executable(socket-udp-client client.c)
add_executable(socket-udp-server server.c)

###########################################################
## COMMON
###########################################################

target_link_libraries(socket-udp-client ipc-bench-common)
target_link_libraries(socket-udp-server ipc-bench-common)
