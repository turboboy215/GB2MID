cmake_minimum_required(VERSION 3.10)
project(GB2MID C)

set(CMAKE_C_STANDARD 99)

find_package(ZLIB REQUIRED)

file(GLOB GBMLIB_SRC "GBMLIB/*.C" "GBMLIB/*.c")

# Tell CMake these are C files
set_source_files_properties(GB2MID.C ${GBMLIB_SRC} PROPERTIES LANGUAGE C)

# GCC still reads .C (capital) as cpp so we need to force it to treat it as normal C
set_source_files_properties(GB2MID.C ${GBMLIB_SRC} PROPERTIES COMPILE_FLAGS "-x c -fcommon")

add_executable(gb2mid
    GB2MID.C
    ${GBMLIB_SRC}
)

target_include_directories(gb2mid PRIVATE GBMLIB)

# Link against the system's zlib and math library (m)
target_link_libraries(gb2mid PRIVATE ZLIB::ZLIB m)

# Strip final binary to make it a true "Release" build
set_target_properties(gb2mid PROPERTIES LINK_FLAGS "-s")
