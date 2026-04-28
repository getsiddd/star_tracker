include(${CMAKE_CURRENT_SOURCE_DIR}/../cmake/protobuf.cmake)

file(GLOB_RECURSE PROTO_FILES ../proto/*.proto)

# Unit tests
# add_subdirectory(test)

include_directories(../include)

include(FindProtobuf)
find_package(Protobuf REQUIRED)
include_directories(${Protobuf_INCLUDE_DIRS})
include_directories(${CMAKE_CURRENT_BINARY_DIR})
custom_protobuf_generate_cpp(../src PROTO_SRC PROTO_HEADER ${PROTO_FILES})


add_executable(systemConfigurationGenerator systemConfigurationGenerator.cpp ${PROTO_HEADER} ${PROTO_SRC})
target_link_libraries(systemConfigurationGenerator proto ${Protobuf_LIBRARIES})
set_target_properties(systemConfigurationGenerator PROPERTIES RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/bin)

add_executable(functionRegister functionRegister.cpp)
target_link_libraries(functionRegister proto ${Protobuf_LIBRARIES})
set_target_properties(functionRegister PROPERTIES RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/bin)