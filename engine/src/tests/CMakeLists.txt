set (CMAKE_RUNTIME_OUTPUT_DIRECTORY ${DIR_BIN}/tests)

add_executable(ResourceManagerTasks resource_manager_tasks.cpp)
target_link_libraries(ResourceManagerTasks engine_core)

add_executable(ECSTasks ecs_tasks.cpp)
target_link_libraries(ECSTasks engine_core)