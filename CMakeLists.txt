cmake_minimum_required(VERSION 3.25)

set(CMAKE_C_FLAGS_RELEASE "${CMAKE_C_FLAGS_RELEASE} -s -O3")
set(CMAKE_CXX_FLAGS_RELEASE "${CMAKE_CXX_FLAGS_RELEASE} -s -O3")

set(CMAKE_C_FLAGS_DEBUG "${CMAKE_C_FLAGS_DEBUG} -g -O0")

project(Arch-Installer VERSION 1.0.1)

# Set the output directory for the binary file
set(CMAKE_RUNTIME_OUTPUT_DIRECTORY ${PROJECT_BINARY_DIR}/${PROJECT_NAME}/)

# configure a header file to pass some of the CMake settings
# to the source code
configure_file(CMakeProjectConfig.h.in CMakeProjectConfig.h)

# Set up source directories and files
file(GLOB SOURCES "src/*.cpp")
add_executable(${PROJECT_NAME} ${SOURCES})

target_include_directories(${PROJECT_NAME} PRIVATE ${CMAKE_BINARY_DIR})

# Include ncurses library
set(NCURSES_LIBRARY ncurses)
set(MENU_LIBRARY menu)
find_library(MENU_LIBRARY NAMES menu)
find_library(NCURSES_LIBRARY NAMES ncurses)
target_link_libraries(${PROJECT_NAME} ${MENU_LIBRARY})
target_link_libraries(${PROJECT_NAME} ${NCURSES_LIBRARY})

target_compile_definitions(${PROJECT_NAME} PRIVATE DEBUG)

install(TARGETS ${PROJECT_NAME} DESTINATION bin)
install(FILES "${PROJECT_BINARY_DIR}/CMakeProjectConfig.h" DESTINATION include)
