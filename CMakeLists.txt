cmake_minimum_required(VERSION 2.8 FATAL_ERROR)
project(synconv)

if(CMAKE_COMPILER_IS_GNUCXX)
    set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -Wall -Werror")
    set(CMAKE_CXX_FLAGS_DEBUG "${CMAKE_CXX_FLAGS} -ggdb -O0")
endif(CMAKE_COMPILER_IS_GNUCXX)

find_package(Boost 1.46.0 COMPONENTS filesystem system thread REQUIRED)

find_package(PkgConfig)
pkg_check_modules(LIBPIPELINE REQUIRED libpipeline)
pkg_check_modules(TAGLIB REQUIRED taglib)

set(SYNCONV_SOURCES
    src/main.cpp
    src/RenamingFilter.cpp
    src/Walker.cpp)

set(SYNCONV_HEADERS
    src/Codec.h
    src/config.h
    src/ConservativeRenamingFilter.h
    src/Decoder.h
    src/Encoder.h
    src/FlacCodec.h
    src/LameCodec.h
    src/RenamingFilter.h
    src/VorbisCodec.h
    src/Walker.h)

include_directories(${Boost_INCLUDE_DIRS} ${LIBPIPELINE_INCLUDE_DIRS} ${TAGLIB_INCLUDE_DIRS})

add_executable(synconv ${SYNCONV_SOURCES} ${SYNCONV_HEADERS})
target_link_libraries(synconv ${Boost_LIBRARIES} ${LIBPIPELINE_LIBRARIES} ${TAGLIB_LIBRARIES})

install(TARGETS synconv DESTINATION bin)

set(CPACK_PACKAGE_VERSION_MAJOR "1")
set(CPACK_PACKAGE_VERSION_MINOR "1")
set(CPACK_PACKAGE_VERSION_PATCH "1")
set(CPACK_SOURCE_GENERATOR "TGZ")
set(CPACK_SOURCE_PACKAGE_FILE_NAME "${CMAKE_PROJECT_NAME}-${CPACK_PACKAGE_VERSION_MAJOR}.${CPACK_PACKAGE_VERSION_MINOR}.${CPACK_PACKAGE_VERSION_PATCH}")
set(CPACK_SOURCE_IGNORE_FILES "/.git;/.gitignore;${CPACK_SOURCE_IGNORE_FILES}")
include(CPack)
add_custom_target(dist COMMAND ${CMAKE_MAKE_PROGRAM} package_source)
