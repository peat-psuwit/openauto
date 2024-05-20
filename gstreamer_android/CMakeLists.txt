# This is an adaption of gst-android's gstreamer-1.0.mk for CMake. Since the
# process involves building a new shared library for all included libraries and
# plugins, it can't be a CMake module or config file. I hope that this could be
# upstreamed at some point.
#
# The original copyright notice for the .mk file is as followed:
#
# cerbero - a multi-platform build system for Open Source software
# Copyright (C) 2012 Andoni Morales Alastruey <ylatuya@gmail.com>
#
# This library is free software; you can redistribute it and/or
# modify it under the terms of the GNU Library General Public
# License as published by the Free Software Foundation; either
# version 2 of the License, or (at your option) any later version.
#
# This library is distributed in the hope that it will be useful,
# but WITHOUT ANY WARRANTY; without even the implied warranty of
# MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
# Library General Public License for more details.
#
# You should have received a copy of the GNU Library General Public
# License along with this library; if not, write to the
# Free Software Foundation, Inc., 59 Temple Place - Suite 330,
# Boston, MA 02111-1307, USA.

if (NOT IS_DIRECTORY ${GSTREAMER_ROOT_ANDROID})
    message(FATAL_ERROR "GSTREAMER_ROOT_ANDROID is not defined or does not exist.")
endif()

#####################
#  Setup variables  #
#####################

if (NOT GSTREAMER_PLUGINS)
    message(WARNING "The list of GSTREAMER_PLUGINS is empty")
endif()

if (ANDROID_ABI STREQUAL "armeabi-v7a")
    set(gst_arch "armv7")
elseif (ANDROID_ABI STREQUAL "arm64-v8a")
    set(gst_arch "arm64")
else()
    set(gst_arch ${ANDROID_ABI}) # x86, x86_64
endif()

set(gst_root ${GSTREAMER_ROOT_ANDROID}/${gst_arch})

# Path for GStreamer static plugins
set(gst_static_plugins_path ${gst_root}/lib/gstreamer-1.0)

# Path for the NDK integration makefiles
set(gst_ndk_build_path ${gst_root}/share/gst-android/ndk-build)

if (NOT DEFINED GSTREAMER_INCLUDE_FONTS)
    set(GSTREAMER_INCLUDE_FONTS ON)
endif()

if (NOT DEFINED GSTREAMER_INCLUDE_CA_CERTIFICATES)
    set(GSTREAMER_INCLUDE_CA_CERTIFICATES ON)
endif()

if ((GSTREAMER_INCLUDE_FONTS OR GSTREAMER_INCLUDE_CA_CERTIFICATES)
        AND NOT DEFINED GSTREAMER_ASSETS_DIR)
    message(WARNING "GSTREAMER_ASSETS_DIR is not defined, cannot include font and/or CA certificates.")
    set(GSTREAMER_INCLUDE_FONTS OFF)
    set(GSTREAMER_INCLUDE_CA_CERTIFICATES OFF)
endif()

# Path for the static GIO modules
set(g_io_modules_path ${gst_root}/lib/gio/modules)

if (NOT GSTREAMER_ANDROID_MODULE_NAME)
    set(GSTREAMER_ANDROID_MODULE_NAME gstreamer_android)
endif()

set(gst_android_c ${CMAKE_CURRENT_BINARY_DIR}/${GSTREAMER_ANDROID_MODULE_NAME}.c)
set(gst_android_c_in ${gst_ndk_build_path}/gstreamer_android-1.0.c.in)
set(gst_deps ${GSTREAMER_EXTRA_DEPS} gstreamer-1.0)

# for setting the default GTlsDatabase
if (GSTREAMER_INCLUDE_CA_CERTIFICATES)
    list(APPEND gst_deps gio-2.0)
endif()

add_library(${GSTREAMER_ANDROID_MODULE_NAME} SHARED ${gst_android_c})

# Generate list of plugin deps
set(gst_plugins_deps)
foreach(plugin ${GSTREAMER_PLUGINS})
    list(APPEND gst_plugins_deps gst${plugin})
endforeach()

set(gst_plugins_with_classes)
foreach(plugin ${GSTREAMER_PLUGINS})
    file(GLOB plugin_classes ${gst_ndk_build_path}/${plugin}/*.java)
    if (plugin_classes)
        list(APPEND gst_plugins_with_classes ${plugin})
    endif()
endforeach()

# Generate the plugins' declaration and registration strings
set(gst_plugins_declare "")
set(gst_plugins_register "")
foreach(plugin ${GSTREAMER_PLUGINS})
    string(APPEND gst_plugins_declare "GST_PLUGIN_STATIC_DECLARE(${plugin});\n")
    string(APPEND gst_plugins_register "GST_PLUGIN_STATIC_REGISTER(${plugin});\n")
endforeach()

# Generate list of gio modules
set(g_io_modules_libs)
set(g_io_modules_declare "")
set(g_io_modules_load "")
foreach(module ${G_IO_MODULES})
    list(APPEND g_io_modules_libs ${g_io_modules_path}/libgio${module}.a)
    string(APPEND g_io_modules_declare "GST_G_IO_MODULE_DECLARE(${module});\n")
    string(APPEND g_io_modules_declare "GST_G_IO_MODULE_LOAD(${module});")
endforeach()

# Get the full list of libraries using pkg-config.
# This gets a bit tricky, since PkgConfig module doesn't seem to have an
# official to set search path explicitly.
find_package(PkgConfig)

if (DEFINED ENV{PKG_CONFIG_PATH})
    set(gst_orig_pkg_config_path "$ENV{PKG_CONFIG_PATH}")
endif()
set(ENV{PKG_CONFIG_PATH} "${gst_root}/lib/pkgconfig:${gst_static_plugins_path}/pkgconfig")

if (GSTREAMER_PLUGIN_QT_ROOT_ANDROID)
    set(ENV{PKG_CONFIG_PATH} "$ENV{PKG_CONFIG_PATH}:${GSTREAMER_PLUGIN_QT_ROOT_ANDROID}/${gst_arch}/lib/pkgconfig")
endif()
message("$ENV{PKG_CONFIG_PATH}")

pkg_check_modules(gst_deps REQUIRED ${gst_deps})
pkg_check_modules(gst_plugins_deps REQUIRED ${gst_plugins_deps})

if (DEFINED gst_orig_pkg_config_path)
    set(ENV{PKG_CONFIG_PATH} ${gst_orig_pkg_config_path})
else()
    unset(ENV{PKG_CONFIG_PATH})
endif()

# XXX: on a custom build of Qt for Android that includes Gstreamer plugin,
# Qt5MultimediaGstTools will contain a copy of Gstreamer, so it MUST not be
# linked whole-archive. Instead, to ensure consistent behavior,
# Qt5MultimediaGstTools must be linked.
# DO NOT upstream this feature.
if (ANDROID AND Qt5MultimediaGstTools_LIBRARIES)
    # List comes from gsttools.pro
    set(gsttools_included_libs
        gstapp-1.0 gstreamer-1.0 gstaudio-1.0 gsttag-1.0 gstvideo-1.0 gstbase-1.0 gstpbutils-1.0
        gobject-2.0 gmodule-2.0 glib-2.0 ffi intl iconv orc-0.4)
    list(REMOVE_ITEM gst_deps_LIBRARIES ${gsttools_included_libs})
endif()

# Set target's compile and link flags.
target_include_directories(
    ${GSTREAMER_ANDROID_MODULE_NAME} PUBLIC
    ${gst_deps_INCLUDE_DIRS}
    ${gst_root}/include)
target_compile_options(${GSTREAMER_ANDROID_MODULE_NAME} PUBLIC
    ${gst_deps_CFLAGS_OTHER})

target_link_directories(${GSTREAMER_ANDROID_MODULE_NAME} PRIVATE
    ${gst_deps_LIBRARY_DIRS}
    ${gst_plugins_deps_LIBRARY_DIRS})

if (ANDROID AND Qt5MultimediaGstTools_LIBRARIES)
    target_link_libraries(${GSTREAMER_ANDROID_MODULE_NAME} PUBLIC
        ${Qt5MultimediaGstTools_LIBRARIES})
endif()

target_link_libraries(${GSTREAMER_ANDROID_MODULE_NAME} PRIVATE
    # TODO: figure out how we can use $<LINK_LIBRARY:WHOLE_ARCHIVE,...> while
    # retaining compatibility with older CMake (it got introduced in 3.24).
    # https://cmake.org/cmake/help/latest/manual/cmake-generator-expressions.7.html#link-features
    # FIXME: this doesn't actually replicate the functionallity of
    # "pkg-config-get-libs-no-deps" fully, so this will still include some
    # direct dependencies. However, it could be expected that users will call
    # directly into e.g. glib-2.0 as well.
    -Wl,--whole-archive
    ${gst_deps_LIBRARIES}
    ${GSTREAMER_EXTRA_LIBS}
    -Wl,--no-whole-archive

    ${gst_deps_STATIC_LIBRARIES}
    ${gst_plugins_deps_STATIC_LIBRARIES}
    ${g_io_modules_libs}
    ${GSTREAMER_EXTRA_LIBS}
    log
    z
)

# Generates a source file that declares and registers all the required plugins
set(PLUGINS_DECLARATION ${gst_plugins_declare})
set(PLUGINS_REGISTRATION ${gst_plugins_register})
set(G_IO_MODULES_LOAD ${g_io_modules_load})
set(G_IO_MODULES_DECLARE ${g_io_modules_declare})

configure_file(${gst_android_c_in} ${gst_android_c} @ONLY)

if (DEFINED GSTREAMER_JAVA_SRC_DIR)
    file(READ ${gst_ndk_build_path}/GStreamer.java GStreamer_java)

    if (GSTREAMER_INCLUDE_FONTS)
        string(REPLACE "//copyFonts" "copyFonts" GStreamer_java "${GStreamer_java}")
    endif()

    if (GSTREAMER_INCLUDE_CA_CERTIFICATES)
        string(REPLACE "//copyCaCertificates" "copyCaCertificates" GStreamer_java "${GStreamer_java}")
    endif()

    file(WRITE ${CMAKE_CURRENT_BINARY_DIR}/GStreamer.java "${GStreamer_java}")
    
    add_custom_command(TARGET ${GSTREAMER_ANDROID_MODULE_NAME} POST_BUILD
        COMMAND ${CMAKE_COMMAND} -E make_directory ${GSTREAMER_JAVA_SRC_DIR}/org/freedesktop/gstreamer
        COMMAND ${CMAKE_COMMAND} -E copy_if_different
            ${CMAKE_CURRENT_BINARY_DIR}/GStreamer.java
            ${GSTREAMER_JAVA_SRC_DIR}/org/freedesktop/gstreamer)

        foreach(plugin ${gst_plugins_with_classes})
            add_custom_command(TARGET ${GSTREAMER_ANDROID_MODULE_NAME} POST_BUILD
            COMMAND ${CMAKE_COMMAND} -E copy_directory
                ${gst_ndk_build_path}/${plugin}
                ${GSTREAMER_JAVA_SRC_DIR}/org/freedesktop/gstreamer/${plugin})
        endforeach()
    
endif()

if (GSTREAMER_INCLUDE_FONTS)
    add_custom_command(TARGET ${GSTREAMER_ANDROID_MODULE_NAME} POST_BUILD
        COMMAND ${CMAKE_COMMAND} -E make_directory ${GSTREAMER_ASSETS_DIR}/fontconfig/fonts/truetype
        COMMAND ${CMAKE_COMMAND} -E copy_if_different
            ${gst_ndk_build_path}/fontconfig/fonts.conf ${GSTREAMER_ASSETS_DIR}/fontconfig
        COMMAND ${CMAKE_COMMAND} -E copy_if_different
            ${gst_ndk_build_path}/fontconfig/fonts/Ubuntu-R.ttf ${GSTREAMER_ASSETS_DIR}/fontconfig/fonts/truetype)
endif()

if (GSTREAMER_INCLUDE_CA_CERTIFICATES)
    add_custom_command(TARGET ${GSTREAMER_ANDROID_MODULE_NAME} POST_BUILD
        COMMAND ${CMAKE_COMMAND} -E make_directory ${GSTREAMER_ASSETS_DIR}/ssl/certs
        COMMAND ${CMAKE_COMMAND} -E copy_if_different
        ${gst_root}/etc/ssl/certs/ca-certificates.crt ${GSTREAMER_ASSETS_DIR}/ssl/certs)
endif()
