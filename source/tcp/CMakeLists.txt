###########################################################
## TARGETS
###########################################################

add_executable(tcp-client client.c)
add_executable(tcp-server server.c)

###########################################################
## COMMON
###########################################################

target_link_libraries(tcp-client ipc-bench-common)
target_link_libraries(tcp-server ipc-bench-common)
