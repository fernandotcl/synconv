cmake_minimum_required(VERSION 3.0 FATAL_ERROR)
project(synconv)

find_library(FOUNDATION_LIBRARY Foundation)

find_package(PkgConfig)
pkg_check_modules(TAGLIB REQUIRED taglib)

# We don't want to -I the whole taglib directory, that's just stupid.
# Detect that case and instead prepare to -I its parent directory
foreach(dir IN LISTS TAGLIB_INCLUDE_DIRS)
    set(fn ${dir}/tag.h)
    if (EXISTS ${fn})
        get_filename_component(fn ${fn} ABSOLUTE)
        get_filename_component(dir ${fn} DIRECTORY)
        get_filename_component(dir ${dir} DIRECTORY)
    endif()
    set(TAGLIB_INCLUDE_DIRS_FINAL ${dir})
endforeach(dir)
set(TAGLIB_INCLUDE_DIRS ${TAGLIB_INCLUDE_DIRS_FINAL})

add_definitions(-DPROGRAM_NAME=\"${PROJECT_NAME}\")

set(SYNCONV_SOURCES
    src/SCVConservativeRenamingFilter.m
    src/SCVConsole.m
    src/SCVPathReference.m
    src/SCVPlugin.m
    src/SCVPluginManager.m
    src/SCVProgressMonitor.m
    src/SCVRenamingFilter.m
    src/SCVTagger.mm
    src/SCVWalker.m
    src/main.m
    src/plugins/SCVFlacPlugin.m
    src/plugins/SCVLamePlugin.m
    src/plugins/SCVVorbisPlugin.m)

set(SYNCONV_HEADERS
    src/SCVConservativeRenamingFilter.h
    src/SCVConsole.h
    src/SCVDecoder.h
    src/SCVEncoder.h
    src/SCVPathReference.h
    src/SCVPlugin.h
    src/SCVPluginManager.h
    src/SCVProgressMonitor.h
    src/SCVRenamingFilter.h
    src/SCVTagger.h
    src/SCVWalker.h
    src/plugins/SCVFlacPlugin.h
    src/plugins/SCVLamePlugin.h
    src/plugins/SCVVorbisPlugin.h)

# Note that this only applies to targets created after we set it. Also note
# how, as of 3.0, there's no way to specify this per-target. Ideally, we would
# specify full paths to the libraries, but that's not how pkg-config works...
link_directories(${TAGLIB_LIBRARY_DIRS})

add_executable(synconv
    ${SYNCONV_SOURCES}
    ${SYNCONV_HEADERS})

target_compile_options(synconv
    PRIVATE "-fobjc-arc")

target_include_directories(synconv
    PRIVATE ${PROJECT_SOURCE_DIR}/src
    PRIVATE ${PROJECT_SOURCE_DIR}/src/plugins
    SYSTEM ${TAGLIB_INCLUDE_DIRS})

target_link_libraries(synconv
    ${FOUNDATION_LIBRARY}
    ${TAGLIB_LIBRARIES})

install(TARGETS synconv DESTINATION bin)
