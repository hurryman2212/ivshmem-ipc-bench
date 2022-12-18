###########################################################
## TARGETS
###########################################################

add_executable(socket-tcp-client client.c)
add_executable(socket-tcp-server server.c)

###########################################################
## COMMON
###########################################################

target_link_libraries(socket-tcp-client ipc-bench-common)
target_link_libraries(socket-tcp-server ipc-bench-common)
