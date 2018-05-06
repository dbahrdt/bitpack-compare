cmake_minimum_required(VERSION 3.5)
project(fastpfor-compare)

set(CMAKE_MODULE_PATH
	${CMAKE_SOURCE_DIR}/cmake
)

#include(add_buildtype_ultra)

add_subdirectory(vendor/fastpfor fastpfor)
add_subdirectory(vendor/sserialize)

include_directories(
	${SSERIALIZE_INCLUDE_DIR}
)

set(SOURCES_CPP
	src/main.cpp
)

SET(LINK_LIBS
	${SSERIALIZE_LIBRARIES}
)

add_executable(${PROJECT_NAME} ${SOURCES_CPP})
target_link_libraries(${PROJECT_NAME} ${LINK_LIBS})
add_dependencies(${PROJECT_NAME} sserialize FastPFor)
