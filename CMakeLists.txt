cmake_minimum_required(VERSION 3.10)

project(pawncmd)

list(APPEND CMAKE_MODULE_PATH ${CMAKE_SOURCE_DIR}/cmake/modules)

include(AddSAMPPlugin)

add_subdirectory(lib/omp-sdk)

add_samp_plugin(${PROJECT_NAME}
  plugin.def

  src/Pawn.CMD.inc
  src/main.h
  src/main.cc
  src/plugin.h
  src/plugin.cc
  src/native_param.h
  src/script.h
  src/script.cc
  src/command.h

  lib/samp-ptl/ptl.h
)

target_include_directories(${PROJECT_NAME} PRIVATE lib)

target_link_libraries(${PROJECT_NAME} PRIVATE OMP-SDK)
