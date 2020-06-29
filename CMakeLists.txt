cmake_minimum_required(VERSION 3.15)

include(${CMAKE_ROOT}/Modules/ExternalProject.cmake)

# set build type
if (NOT CMAKE_BUILD_TYPE AND NOT CMAKE_CONFIGURATION_TYPES)
	message(STATUS "Setting build type to 'Release' as none was specified.")
	set(CMAKE_BUILD_TYPE Release CACHE STRING "Choose the type of build." FORCE)
	
	# Set the possible values of build type for cmake-gui
	set_property(CACHE CMAKE_BUILD_TYPE PROPERTY STRINGS "Debug" "Release" "RelWithDebInfo" "MinSizeRel")
endif ()

message("CMAKE_BUILD_TYPE is ${CMAKE_BUILD_TYPE}")

project(duckydd C)
set(CMAKE_C_STANDARD 99)

# set path variables for installation
set (INSTALL_DIR "/usr/bin")
set (DAEMON_CONF_DIR "/etc/duckydd")
set (SYSTEMD_UNIT_DIR "/usr/lib/systemd/system/")

# The version numbering
set (${PROJECT_NAME}_VERSION_MAJOR 0)
set (${PROJECT_NAME}_VERSION_MINOR 2)
set (${PROJECT_NAME}_PATCH_LEVEL 2)

set (${PROJECT_NAME}_VERSION
"${${PROJECT_NAME}_VERSION_MAJOR}.${${PROJECT_NAME}_VERSION_MINOR}")

# set source and include directories
include_directories (include)
file (GLOB SOURCES "src/*.c")

add_executable(duckydd ${SOURCES})


set (CMAKE_C_FLAGS_DEBUG "-g3 -lefence")
set (CMAKE_C_FLAGS_RELEASE "-O3")

string (APPEND CMAKE_C_FLAGS "-Wall -fstack-protector-strong")

ExternalProject_Add(sslib
	PREFIX ${CMAKE_SOURCE_DIR}/safestringlib
	GIT_REPOSITORY "https://github.com/intel/safestringlib"
	UPDATE_COMMAND ""
	SOURCE_DIR "${CMAKE_SOURCE_DIR}/safestringlib/safeclib"
	BINARY_DIR "${CMAKE_SOURCE_DIR}/safestringlib/build"
	BUILD_ALWAYS false
	BUILD_COMMAND make
	INSTALL_COMMAND ""
	LOG_INSTALL 0
	LOG_UPDATE 0
)

# find libs
find_library(UDEV_LIB udev)
find_library(XKBCOMMON_LIB xkbcommon)
find_library(XKBCOMMON_X11_LIB xkbcommon-x11)
find_library(XCB_LIB xcb)

add_library(safestringlib STATIC IMPORTED)
set_target_properties(safestringlib PROPERTIES
        IMPORTED_LOCATION "${CMAKE_SOURCE_DIR}/safestringlib/build/libsafestring_static.a"
        INTERFACE_INCLUDE_DIRECTORIES "${CMAKE_SOURCE_DIR}/safestringlib/include")

add_dependencies(duckydd safestringlib)
target_link_libraries(duckydd 
	PRIVATE
		${UDEV_LIB}
		${XKBCOMMON_LIB}
		${XKBCOMMON_X11_LIB}
		${XCB_LIB}
		safestringlib
)


# install the release build
install(TARGETS duckydd
        RUNTIME
        DESTINATION ${INSTALL_DIR}
        PERMISSIONS OWNER_READ OWNER_WRITE OWNER_EXECUTE GROUP_READ GROUP_EXECUTE WORLD_EXECUTE
        CONFIGURATIONS Release)

install(FILES ${CMAKE_SOURCE_DIR}/duckydd.conf
        DESTINATION ${DAEMON_CONF_DIR}
        PERMISSIONS OWNER_READ OWNER_WRITE OWNER_EXECUTE GROUP_READ GROUP_EXECUTE WORLD_EXECUTE
        CONFIGURATIONS Release
        )

install(FILES ${CMAKE_SOURCE_DIR}/duckydd.service
        DESTINATION ${SYSTEMD_UNIT_DIR}
        PERMISSIONS OWNER_READ OWNER_WRITE OWNER_EXECUTE GROUP_READ GROUP_EXECUTE WORLD_EXECUTE
        CONFIGURATIONS Release
        )