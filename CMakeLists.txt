cmake_minimum_required(VERSION 3.5.1)
project(openauto C CXX)

set(CMAKE_AUTOMOC ON)
set(CMAKE_AUTOUIC ON)
set(CMAKE_AUTORCC ON)
set(base_directory ${CMAKE_CURRENT_SOURCE_DIR})
set(resources_directory ${base_directory}/assets)
set(sources_directory ${base_directory}/src)
set(include_directory ${base_directory}/include)

if(ANDROID)
    option(USE_GSTREAMER "Uses Gstreamer for video output" ON)
else()
    option(USE_GSTREAMER "Uses Gstreamer for video output" OFF)
endif()

# androiddeployqt expects library to be at a certain place
if(NOT ANDROID)
    set(CMAKE_ARCHIVE_OUTPUT_DIRECTORY ${CMAKE_CURRENT_BINARY_DIR}/lib)
    set(CMAKE_LIBRARY_OUTPUT_DIRECTORY ${CMAKE_CURRENT_BINARY_DIR}/lib)

    set(CMAKE_RUNTIME_OUTPUT_DIRECTORY ${CMAKE_CURRENT_BINARY_DIR}/bin)
    set(EXECUTABLE_OUTPUT_PATH ${CMAKE_CURRENT_BINARY_DIR}/bin)
endif()

set(Boost_USE_STATIC_LIBS OFF)
set(Boost_USE_MULTITHREADED ON)
set(Boost_USE_STATIC_RUNTIME OFF)

set(CMAKE_MODULE_PATH ${CMAKE_MODULE_PATH} "${CMAKE_SOURCE_DIR}/cmake_modules/")
SET(CMAKE_CXX_STANDARD 14)
set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS_INIT} -Wall -pedantic -fPIC")
set(CMAKE_CXX_FLAGS_DEBUG "-g -O0")
set(CMAKE_CXX_FLAGS_RELEASE "-g -O3")

add_definitions(-DBOOST_ALL_DYN_LINK)

find_package(Boost REQUIRED COMPONENTS system log OPTIONAL_COMPONENTS unit_test_framework)
find_package(libusb-1.0 REQUIRED)
find_package(Qt5 COMPONENTS Multimedia MultimediaWidgets Bluetooth Network)

if(ANDROID)
    find_package(Qt5 REQUIRED COMPONENTS AndroidExtras)

    set(ANDROID_PACKAGE_SOURCE_DIR ${base_directory}/android)

    # TODO: will go away when we migrate to qmlglsink
    find_package(Qt5 REQUIRED COMPONENTS MultimediaGstTools)
endif()

# Newer Protobuf starts requiring Abseil, and the old CMake's FindProtobuf
# module is not updated to reflect that. Thus, prefer upstream's config module
# if available.
set(CMAKE_FIND_PACKAGE_PREFER_CONFIG ON)
find_package(Protobuf REQUIRED)
unset(CMAKE_FIND_PACKAGE_PREFER_CONFIG)

find_package(OpenSSL REQUIRED)
find_package(rtaudio REQUIRED)
find_package(taglib REQUIRED)
find_package(gps REQUIRED)

if(WIN32)
    set(WINSOCK2_LIBRARIES "ws2_32")
endif(WIN32)

if(RPI3_BUILD)
    add_definitions(-DUSE_OMX -DOMX_SKIP64BIT -DRASPBERRYPI3)
    set(BCM_HOST_LIBRARIES "/opt/vc/lib/libbcm_host.so")
    set(BCM_HOST_INCLUDE_DIRS "/opt/vc/include")
    set(ILCLIENT_INCLUDE_DIRS "/opt/vc/src/hello_pi/libs/ilclient")
    set(ILCLIENT_LIBRARIES "/opt/vc/src/hello_pi/libs/ilclient/libilclient.a;/opt/vc/lib/libvcos.so;/opt/vc/lib/libvcilcs.a;/opt/vc/lib/libvchiq_arm.so")
endif(RPI3_BUILD)

if(USE_GSTREAMER)
    add_definitions(-DUSE_GSTREAMER)

    if(NOT ANDROID)
        find_package(PkgConfig REQUIRED)
        pkg_check_modules(Gst REQUIRED IMPORTED_TARGET
            gstreamer-1.0 gstreamer-app-1.0)
        set(gst_target PkgConfig::Gst)
    else()
        set(GSTREAMER_ANDROID_MODULE_NAME gstreamer_android)
        set(GSTREAMER_EXTRA_DEPS gstreamer-app-1.0)
        set(GSTREAMER_PLUGINS
            coreelements playback videotestsrc audioconvert
            videoconvertscale autodetect app isomp4
            audioparsers typefindfunctions androidmedia
            opengl videoparsersbad)
        set(GSTREAMER_JAVA_SRC_DIR ${CMAKE_CURRENT_BINARY_DIR}/android-build/java)
        set(GSTREAMER_ASSETS_DIR ${CMAKE_CURRENT_BINARY_DIR}/android-build/assets)

        add_subdirectory(gstreamer_android)
        set(gst_target ${GSTREAMER_ANDROID_MODULE_NAME})
    endif()
endif()

include_directories(${CMAKE_CURRENT_BINARY_DIR}
                    ${Qt5Core_PRIVATE_INCLUDE_DIRS}
                    ${Qt5Multimedia_INCLUDE_DIRS}
                    ${Qt5MultimediaWidgets_INCLUDE_DIRS}
                    ${Qt5Widgets_INCLUDE_DIRS}
                    ${Qt5Bluetooth_INCLUDE_DIRS}
                    ${Qt5Network_INCLUDE_DIRS}
                    ${Qt5AndroidExtras_INCLUDE_DIRS}
                    ${Qt5MultimediaGstTools_INCLUDE_DIRS}
                    ${Boost_INCLUDE_DIRS}
                    ${OPENSSL_INCLUDE_DIR}
                    ${RTAUDIO_INCLUDE_DIRS}
                    ${TAGLIB_INCLUDE_DIRS}
                    ${BLKID_INCLUDE_DIRS}
                    ${GPS_INCLUDE_DIRS}
                    ${AASDK_PROTO_INCLUDE_DIRS}
                    ${AASDK_INCLUDE_DIRS}
                    ${BCM_HOST_INCLUDE_DIRS}
                    ${ILCLIENT_INCLUDE_DIRS}
                    ${include_directory})
								
link_directories(${CMAKE_LIBRARY_OUTPUT_DIRECTORY})

set(common_include_directory ${include_directory}/f1x/openauto/Common)

set(aasdk_android_sources_directory ${sources_directory}/aasdk_android)
set(aasdk_android_include_directory ${include_directory}/f1x/aasdk_android)
set(autoapp_sources_directory ${sources_directory}/autoapp)
set(autoapp_include_directory ${include_directory}/f1x/openauto/autoapp)
file(GLOB_RECURSE autoapp_source_files
    ${aasdk_android_sources_directory}/*.cpp
    ${aasdk_android_include_directory}/*.hpp
    ${autoapp_sources_directory}/*.ui
    ${autoapp_sources_directory}/*.cpp
    ${autoapp_include_directory}/*.hpp
    ${common_include_directory}/*.hpp
    ${resources_directory}/*.qrc)

if(ANDROID)
    # Android does not exec a new binary. Instead, the entry point
    # is a Java code which will have to dlopen() this library.
    add_library(autoapp SHARED ${autoapp_source_files})
    # androiddeployqt expects library to match project name by default.
    set_target_properties(autoapp PROPERTIES OUTPUT_NAME openauto)
else()
    add_executable(autoapp ${autoapp_source_files})
endif()

target_link_libraries(autoapp libusb
                        ${Boost_LIBRARIES}
                        ${Qt5Multimedia_LIBRARIES}
                        ${Qt5MultimediaWidgets_LIBRARIES}
                        ${Qt5Bluetooth_LIBRARIES}
                        ${Qt5Network_LIBRARIES}
                        ${Qt5AndroidExtras_LIBRARIES}
                        ${Qt5MultimediaGstTools_LIBRARIES}
                        ${gst_target}
                        protobuf::libprotobuf
                        ${BCM_HOST_LIBRARIES}
                        ${ILCLIENT_LIBRARIES}
                        ${WINSOCK2_LIBRARIES}
                        ${RTAUDIO_LIBRARIES}
                        ${TAGLIB_LIBRARIES}
                        ${BLKID_LIBRARIES}
                        ${GPS_LIBRARIES}
                        ${AASDK_PROTO_LIBRARIES}
                        ${AASDK_LIBRARIES})

if(ANDROID)
    # A hackish way to bundle all libraries needed into APK. The proper way is
    # probably QT_ANDROID_EXTRA_LIBS, but that's Qt 6.
    file(GENERATE OUTPUT ${CMAKE_CURRENT_BINARY_DIR}/copy_libs.cmake CONTENT "
        # Pull in indirect Abseil dependencies.
        file(GET_RUNTIME_DEPENDENCIES
            RESOLVED_DEPENDENCIES_VAR PROTOBUF_LIBRARIES
            UNRESOLVED_DEPENDENCIES_VAR PROTOBUF_LIBRARIES_SYSTEM_LIBS
            LIBRARIES $<TARGET_LINKER_FILE:protobuf::libprotobuf>
            DIRECTORIES $<TARGET_FILE_DIR:protobuf::libprotobuf>
        )

        set(LIBFILES
            ${Boost_LIBRARIES}
            ${LIBUSB_1_LIBRARIES}
            $<TARGET_LINKER_FILE:protobuf::libprotobuf>
            \${PROTOBUF_LIBRARIES}
            ${RTAUDIO_LIBRARIES}
            ${TAGLIB_LIBRARIES}
            ${OPENSSL_LIBRARIES}
            ${AASDK_PROTO_LIBRARIES}
            ${AASDK_LIBRARIES}
        )

        # Filter out e.g. '-pthread'
        list(FILTER LIBFILES INCLUDE REGEX \\\\.so)

        file(COPY \${LIBFILES}
            DESTINATION ${CMAKE_LIBRARY_OUTPUT_DIRECTORY}
            FOLLOW_SYMLINK_CHAIN
        )
    ")

    add_custom_command(TARGET autoapp POST_BUILD
        COMMAND cmake -P ${CMAKE_CURRENT_BINARY_DIR}/copy_libs.cmake)
endif()

set(btservice_sources_directory ${sources_directory}/btservice)
set(btservice_include_directory ${include_directory}/f1x/openauto/btservice)
file(GLOB_RECURSE btservice_source_files ${btservice_sources_directory}/*.cpp ${btservice_include_directory}/*.hpp ${autoapp_sources_directory}/Configuration/*.cpp ${autoapp_includes_directory}/Configuration/*.hpp ${common_include_directory}/*.hpp)

add_executable(btservice ${btservice_source_files})

target_link_libraries(btservice
                        ${Boost_LIBRARIES}
                        ${Qt5Bluetooth_LIBRARIES}
                        ${Qt5Network_LIBRARIES}
                        ${Qt5MultimediaWidgets_LIBRARIES}
                        protobuf::libprotobuf
                        ${AASDK_PROTO_LIBRARIES})
