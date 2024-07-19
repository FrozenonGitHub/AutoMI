cmake_minimum_required(VERSION 3.10)
project(Quegel)

set(CMAKE_CXX_STANDARD 14)

# CMAKE_BUILD_TYPE default to RELEASE if not specified
if(NOT CMAKE_BUILD_TYPE)
    set(CMAKE_BUILD_TYPE "RELEASE")
endif()
# Set debug flags if build type is Debug
if(CMAKE_BUILD_TYPE STREQUAL "DEBUG")
    set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -g")
endif()

set(CMAKE_CXX_COMPILER mpic++)
set(PLATFORM Linux-amd64-64)

# Add JAVA / Hadoop related directories
if(DEFINED ENV{HADOOP_HOME})
    set(HADOOP_HOME $ENV{HADOOP_HOME})
else()
    set(HADOOP_HOME /code/hadoop-3.3.6)
endif()

if(DEFINED ENV{JAVA_HOME})
    set(JAVA_HOME $ENV{JAVA_HOME})
else()
    set(JAVA_HOME /usr/lib/jvm/java-11-openjdk-amd64)
endif()

include_directories(
    ${HADOOP_HOME}/src/c++/libhdfs
    ${JAVA_HOME}/include
    ${JAVA_HOME}/include/linux
    ${HADOOP_HOME}/include
    system
)

link_directories(${HADOOP_HOME}/lib/native ${JAVA_HOME}/lib/server)

# Add compile options
set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -Wno-deprecated -O2")
# add_compile_options(-Wno-deprecated -O2)

# Specify the executable target and link the hdfs library
add_executable(bbfs app/bbfs/run.cpp)
target_link_libraries(bbfs ${HADOOP_HOME}/lib/native/libhdfs.so)

add_executable(ibfs app/ibfs/run.cpp)
target_link_libraries(ibfs ${HADOOP_HOME}/lib/native/libhdfs.so)

add_executable(sssp app/sssp/run.cpp)
target_link_libraries(sssp ${HADOOP_HOME}/lib/native/libhdfs.so)

add_executable(original_bfs original_app/bfs/run.cpp)
target_link_libraries(original_bfs ${HADOOP_HOME}/lib/native/libhdfs.so)

# Build targets in subdirectories
# add_subdirectory(app)
# add_subdirectory(client)
# add_subdirectory(tools)
# add_subdirectory(dag_scc)
