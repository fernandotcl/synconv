cmake_minimum_required(VERSION 2.8 FATAL_ERROR)
project(synconv)

find_library(FOUNDATION_LIBRARY Foundation)

find_package(PkgConfig)
pkg_check_modules(TAGLIB REQUIRED taglib)

add_definitions(-DPROGRAM_NAME=\"${PROJECT_NAME}\")

set(SYNCONV_SOURCES
    src/SCVConsole.m
    src/SCVPlugin.m
    src/SCVPluginManager.m
    src/SCVTagger.mm
    src/SCVWalker.m
    src/main.m
    src/plugins/SCVLamePlugin.m)

set(SYNCONV_HEADERS
    src/SCVConsole.h
    src/SCVDecoder.h
    src/SCVEncoder.h
    src/SCVPlugin.h
    src/SCVPluginManager.h
    src/SCVTagger.h
    src/SCVWalker.h
    src/plugins/SCVLamePlugin.h)

include_directories(
    ${PROJECT_SOURCE_DIR}/src
    ${PROJECT_SOURCE_DIR}/src/plugins
    ${TAGLIB_INCLUDE_DIRS})

add_executable(synconv
    ${SYNCONV_SOURCES}
    ${SYNCONV_HEADERS})

set_target_properties(synconv PROPERTIES
    COMPILE_FLAGS "-fobjc-arc")

target_link_libraries(synconv
    ${FOUNDATION_LIBRARY}
    ${TAGLIB_LIBRARIES})

install(TARGETS synconv DESTINATION bin)
