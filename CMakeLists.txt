cmake_minimum_required(VERSION 3.26)

project(audioreroute CXX)

if (${CMAKE_CXX_COMPILER_ARCHITECTURE_ID} STREQUAL "x64")
	set(ARCH 64)
elseif (${CMAKE_CXX_COMPILER_ARCHITECTURE_ID} STREQUAL "X86")
	set(ARCH 32)
else ()
	message(FATAL_ERROR "could not detect arch")
endif ()

set(DLL_TARGET arrdll${ARCH})
set(EXE_TARGET arrexe${ARCH})

add_compile_definitions(UNICODE)
add_compile_definitions(EXE_FILENAME=\"${EXE_TARGET}.exe\")
add_compile_definitions(DLL_FILENAME=\"${DLL_TARGET}.dll\")

find_path(DETOURS_INCLUDE_DIRS "detours/detours.h" REQUIRED)
find_library(DETOURS_LIBRARY detours REQUIRED)
add_library(detours INTERFACE)
target_include_directories(detours INTERFACE ${DETOURS_INCLUDE_DIRS})
target_link_libraries(detours INTERFACE ${DETOURS_LIBRARY})

add_library(${DLL_TARGET} SHARED src/dll.cpp)
set_target_properties(${DLL_TARGET} PROPERTIES WINDOWS_EXPORT_ALL_SYMBOLS ON)
target_link_libraries(${DLL_TARGET} PRIVATE detours)
target_compile_features(${DLL_TARGET} PUBLIC cxx_std_17)
target_include_directories(${DLL_TARGET} PUBLIC inc)

add_executable(${EXE_TARGET} WIN32 src/exe.cpp)
target_link_libraries(${EXE_TARGET} PRIVATE ${DLL_TARGET} detours)
target_compile_features(${EXE_TARGET} PRIVATE cxx_std_17)
target_include_directories(${EXE_TARGET} PRIVATE inc)
