#I am genually sorry if this doesn't work but it is the best I managed to get, suggestions are allowed though

cmake_minimum_required(VERSION 3.23.1)
set (CMAKE_CXX_STANDARD 23)
set(CMAKE_MODULE_PATH ${CMAKE_MODULE_PATH} "${CMAKE_SOURCE_DIR}/cmake/")

project(EditBMP)

add_library(
filesToAdd
src/engineInternals.cpp
src/engineInternals.hpp
src/options.cpp
src/options.hpp
src/paintingTools.cpp
src/paintingTools.hpp
src/logger.cpp
src/logger.hpp
src/renderLib.cpp
src/renderLib.hpp
#Add here your extra code files 
)

SET(SDL2_DIR SDL2/cmake)
find_package(SDL2 REQUIRED)

SET(SDL2_image_DIR SDL2_image/cmake)
find_package(SDL2_image REQUIRED)

SET(SDL2_ttf_DIR SDL2_ttf/cmake)
find_package(SDL2_ttf REQUIRED)

include_directories(${SDL2_INCLUDE_DIRS} ${SDL2_IMAGE_INCLUDE_DIRS} ${SDL2_TTF_INCLUDE_DIRS})

add_executable(${PROJECT_NAME} src/main.cpp)

target_link_libraries(filesToAdd ${SDL2_LIBRARIES} ${SDL2_IMAGE_LIBRARIES} ${SDL2_TTF_LIBRARIES})

target_link_libraries(${PROJECT_NAME} PRIVATE filesToAdd)