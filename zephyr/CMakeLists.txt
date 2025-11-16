cmake_minimum_required(VERSION 3.13.1)
include($ENV{ZEPHYR_BASE}/cmake/app/boilerplate.cmake NO_POLICY_SCOPE)
project(Schachroboter)

# Collect all application sources recursively and include cJSON
file(GLOB_RECURSE app_sources
	 ../src/*.c
	 ../src/*.cc
	 ../src/*.cpp)

set(third_party_sources
	../lib/cJSON/src/cJSON.c)

target_sources(app PRIVATE ${app_sources} ${third_party_sources})

target_include_directories(app PRIVATE
	${CMAKE_CURRENT_LIST_DIR}/../include
	${CMAKE_CURRENT_LIST_DIR}/../lib/cJSON/include)