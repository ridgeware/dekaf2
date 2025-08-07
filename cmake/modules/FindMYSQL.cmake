# - Try to find MySQL.
# Once done this will define:
# MYSQL_FOUND			- If false, do not try to use MySQL.
# MYSQL_INCLUDE_DIRS	- Where to find mysql.h, etc.
# MYSQL_LIBRARIES		- The libraries to link against.
# MYSQL_VERSION_STRING	- Version in a string of MySQL.
# MYSQL_IS_MARIADB      - true if we found mariadb, false otherwise
#
# Created by RenatoUtsch based on eAthena implementation.
#
# Please note that this module only supports Windows and Linux officially, but
# should work on all UNIX-like operational systems too.
#

#=============================================================================
# Copyright 2012 RenatoUtsch, changes 2019-2022 Ridgeware
#
# Distributed under the OSI-approved BSD License (the "License");
# see accompanying file Copyright.txt for details.
#
# This software is distributed WITHOUT ANY WARRANTY; without even the
# implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
# See the License for more information.
#=============================================================================
# (To distribute this file outside of CMake, substitute the full
#  License text for the above reference.)

if( WIN32 )
	set (P86 "PROGRAMFILES(x86)")
	find_path( MYSQL_INCLUDE_DIR
		NAMES "mysql.h"
		PATHS "$ENV{PROGRAMFILES}/MySQL/*/include"
		      "$ENV{${P86}}/MySQL/*/include"
			  "$ENV{SYSTEMDRIVE}/MySQL/*/include" )

	find_library( MYSQL_LIBRARY
		NAMES "mysqlclient" "mysqlclient_r"
		PATHS "$ENV{PROGRAMFILES}/MySQL/*/lib"
		      "$ENV{${P86}}/MySQL/*/lib"
			  "$ENV{SYSTEMDRIVE}/MySQL/*/lib" )
else()
	find_path( MYSQL_INCLUDE_DIR
		NAMES "mysql.h"
		PATHS "/usr/include/mariadb"
			"/usr/local/include/mariadb"
			"/usr/include/mysql"
		      "/usr/local/include/mysql"
			  "/usr/mysql/include/mysql"
			  "/opt/homebrew/include/mysql"
	)

	find_library( MYSQL_LIBRARY
		NAMES "mysqlclient" "mysqlclient_r" "mariadbclient" "mariadb"
		PATHS "/lib/mysql"
		      "/lib64/mysql"
			  "/usr/lib/mysql"
			  "/usr/lib64/mysql"
			  "/usr/local/lib/mysql"
			  "/usr/local/lib64/mysql"
			  "/usr/mysql/lib/mysql"
			  "/usr/mysql/lib64/mysql"
			  "/usr/local/lib"
			  "/opt/homebrew/lib"
	)
endif()

#message(STATUS "mysql include: ${MYSQL_INCLUDE_DIR}")
#message(STATUS "mysql library: ${MYSQL_LIBRARY}")

if( MYSQL_INCLUDE_DIR )

	if ( EXISTS "${MYSQL_INCLUDE_DIR}/mariadb_version.h" )

		set(MYSQL_IS_MARIADB ON)
		file( STRINGS "${MYSQL_INCLUDE_DIR}/mariadb_version.h"
			MYSQL_VERSION_H REGEX "^#define[ \t]+MYSQL_SERVER_VERSION[ \t]+\"[^\"]+\".*$" )
		string( REGEX REPLACE
			"^.*MYSQL_SERVER_VERSION[ \t]+\"([^\"]+)\".*$" "\\1" MYSQL_VERSION_STRING
			"${MYSQL_VERSION_H}" )

	elseif( EXISTS "${MYSQL_INCLUDE_DIR}/mysql_version.h" )

		# look in mysql_version.h
		file( STRINGS "${MYSQL_INCLUDE_DIR}/mysql_version.h"
			MYSQL_VERSION_H REGEX "^#define[ \t]+MYSQL_SERVER_VERSION[ \t]+\"[^\"]+\".*$" )
		string( REGEX REPLACE
			"^.*MYSQL_SERVER_VERSION[ \t]+\"([^\"]+)\".*$" "\\1" MYSQL_VERSION_STRING
			"${MYSQL_VERSION_H}" )

	else()

		# look in mysql.h
		file( STRINGS "${MYSQL_INCLUDE_DIR}/mysql.h"
			MYSQL_H REGEX "^#define[ \t]+MYSQL_SERVER_VERSION[ \t]+\"[^\"]+\".*$" )
		string( REGEX REPLACE
			"^.*MYSQL_SERVER_VERSION[ \t]+\"([^\"]+)\".*$" "\\1" MYSQL_VERSION_STRING
			"${MYSQL_H}" )

	endif()

	if (NOT MYSQL_IS_MARIADB)
		# old versions of mariadb do not have a mariadb_version.h file,
		# therefore we need to search in the mysql.h for signs of mariadb ..
		file(STRINGS "${MYSQL_INCLUDE_DIR}/mysql.h" TEST_FOR_MARIADB REGEX "typedef *struct *st_mysql")
		if (TEST_FOR_MARIADB STREQUAL "")
			set(MYSQL_IS_MARIADB OFF)
		else()
			set(MYSQL_IS_MARIADB ON)
		endif()
	endif()

endif()


# handle the QUIETLY and REQUIRED arguments and set MYSQL_FOUND to TRUE if
# all listed variables are TRUE
include( FindPackageHandleStandardArgs )
find_package_handle_standard_args( MYSQL
	REQUIRED_VARS	MYSQL_LIBRARY MYSQL_INCLUDE_DIR
	VERSION_VAR		MYSQL_VERSION_STRING )

set(MYSQL_VERSION "${MYSQL_VERSION_STRING}")

if(MYSQL_FOUND)
	set( MYSQL_INCLUDE_DIRS ${MYSQL_INCLUDE_DIR} )
	set( MYSQL_LIBRARIES ${MYSQL_LIBRARY} )
endif()

mark_as_advanced( MYSQL_INCLUDE_DIR MYSQL_LIBRARY )

