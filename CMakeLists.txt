cmake_minimum_required(VERSION 3.5)
project(bitpack-compare)

set(CMAKE_MODULE_PATH
	${CMAKE_SOURCE_DIR}/cmake
)

include(add_buildtype_ultra)

add_subdirectory(vendor/fastpfor fastpfor)
add_subdirectory(vendor/sserialize)

set(SOURCES_CPP
	src/main.cpp
)

SET(LINK_LIBS
	sserialize
	FastPFor
)

add_executable(${PROJECT_NAME} ${SOURCES_CPP})
target_link_libraries(${PROJECT_NAME} ${LINK_LIBS})
target_include_directories(${PROJECT_NAME} PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/vendor/fastpfor/headers)
