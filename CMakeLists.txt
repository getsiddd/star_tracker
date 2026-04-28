cmake_minimum_required(VERSION 3.28)
project(lost)

set(PROJECT_NAME_STR lost)

set(CMAKE_C_COMPILER /usr/bin/gcc) # Replace with your GCC path if different
set(CMAKE_CXX_COMPILER /usr/bin/g++) # Replace with your GCC path if different

set(CMAKE_CXX_STANDARD 20)
set(EXECUTABLE_OUTPUT_PATH ${PROJECT_SOURCE_DIR})
set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -Wno-deprecated-enum-enum-conversion")

include(${CMAKE_CURRENT_SOURCE_DIR}/cmake/protobuf.cmake)

file(GLOB_RECURSE INC_FILES include/*.h**)
file(GLOB_RECURSE SRC_FILES src/*.c* src/*.h)

file(GLOB_RECURSE PROTO_FILES proto/*.proto)

# Unit tests
# add_subdirectory(test)
add_subdirectory(tools)

include(FindProtobuf)
find_package(Protobuf REQUIRED)
include_directories(${Protobuf_INCLUDE_DIRS})
include_directories(${CMAKE_CURRENT_BINARY_DIR})
custom_protobuf_generate_cpp(src PROTO_SRC PROTO_HEADER ${PROTO_FILES})
add_library(proto ${PROTO_HEADER} ${PROTO_SRC})

find_package(absl REQUIRED)
target_link_libraries(proto PRIVATE absl::log_internal_message absl::log_internal_check_op absl::status)

find_package(TBB REQUIRED)
include_directories(${TBB_INCLUDE_DIRS})
link_directories(${TBB_LIBRARY_DIRS})
add_definitions(${TBB_DEFINITIONS})

find_package(Boost REQUIRED COMPONENTS log log_setup thread program_options)
include_directories(${Boost_INCLUDE_DIRS})

find_package(PkgConfig REQUIRED)
pkg_check_modules(CAIRO REQUIRED cairo)
include_directories(${CAIRO_INCLUDE_DIRS})
link_directories(${CAIRO_LIBRARY_DIRS})

# Assign the include directories
include_directories(include)
include_directories(vendor)
include_directories(documentation)

set(SOURCE_FILES ${SRC_FILES} main.cpp ${INC_FILES})
add_definitions(-DOATPP_SWAGGER_RES_PATH="${OATPP_BASE_DIR}/bin/oatpp-swagger/res")
add_definitions(-DWWW_FOLDER="${CMAKE_CURRENT_SOURCE_DIR}/res")

add_executable(${PROJECT_NAME_STR} ${SOURCE_FILES})
target_link_libraries(${PROJECT_NAME_STR} proto ${PROTOBUF_LIBRARY})
target_link_libraries(${PROJECT_NAME_STR} TBB::tbb)
target_link_libraries(${PROJECT_NAME_STR}
	Boost::log_setup
	Boost::program_options
	${CAIRO_LIBRARIES}
)

set_target_properties(${PROJECT_NAME_STR} PROPERTIES RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/bin)
