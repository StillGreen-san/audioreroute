cmake_minimum_required(VERSION 3.26)

project(audioreroute CXX)

add_compile_definitions(UNICODE)

find_path(DETOURS_INCLUDE_DIRS "detours/detours.h" REQUIRED)
find_library(DETOURS_LIBRARY detours REQUIRED)
add_library(detours INTERFACE)
target_include_directories(detours INTERFACE ${DETOURS_INCLUDE_DIRS})
target_link_libraries(detours INTERFACE ${DETOURS_LIBRARY})

add_library(arrdll SHARED src/dll.cpp)
set_target_properties(arrdll PROPERTIES WINDOWS_EXPORT_ALL_SYMBOLS ON)
target_link_libraries(arrdll PRIVATE detours)
target_compile_features(arrdll PUBLIC cxx_std_17)
target_include_directories(arrdll PUBLIC inc)

add_executable(arrexe WIN32 src/exe.cpp)
target_link_libraries(arrexe PRIVATE arrdll detours)
target_compile_features(arrexe PRIVATE cxx_std_17)
target_include_directories(arrexe PRIVATE inc)
