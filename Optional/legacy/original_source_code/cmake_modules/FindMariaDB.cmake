# - Find mysqlclient
# Find the native MYSQL includes and library
#
#  MYSQL_INCLUDE_DIR - where to find mysql.h, etc.
#  MYSQL_LIBRARIES   - List of libraries when using MYSQL.
#  MYSQL_FOUND       - True if MYSQL found.

if (CMAKE_CL_64)
	SET(MYSQL_WINPATH "C:/Program Files/mariadb-connector-c")
else()
	SET(MYSQL_WINPATH "C:/Program Files (x86)/mariadb-connector-c")
endif()

FIND_PATH(MYSQL_INCLUDE_DIR NAMES mysql.h
  PATHS
  /usr/include/mariadb
  /usr/local/include/mariadb
  /usr/local/mariadb/include
  ${MYSQL_WINPATH}/include/mariadb
)

FIND_PATH(MYSQL_LIB_DIR NAMES mariadbclient.lib
  PATHS
  /usr/lib
  /usr/lib/i386-linux-gnu
  /usr/local/lib
  /usr/local/mysql/lib
	${MYSQL_WINPATH}/lib/mariadb
)

SET(MYSQL_NAMES mariadbclient)
FIND_LIBRARY(MYSQL_LIBRARY
  NAMES ${MYSQL_NAMES}
  PATHS /usr/lib /usr/local/lib /usr/local/mariadb/lib ${MYSQL_WINPATH}/lib
  PATH_SUFFIXES mariadb
)

IF (MYSQL_INCLUDE_DIR AND MYSQL_LIBRARY)
  SET(MYSQL_FOUND TRUE)
  SET(MYSQL_LIBRARIES ${MYSQL_LIBRARY} )
ELSE (MYSQL_INCLUDE_DIR AND MYSQL_LIBRARY)
  SET(MYSQL_FOUND FALSE)
  SET( MYSQL_LIBRARIES )
ENDIF (MYSQL_INCLUDE_DIR AND MYSQL_LIBRARY)

IF (MYSQL_FOUND)
  IF (NOT MYSQL_FIND_QUIETLY)
    MESSAGE(STATUS "Found MYSQL: ${MYSQL_LIBRARY}")
  ENDIF (NOT MYSQL_FIND_QUIETLY)
ELSE (MYSQL_FOUND)
  IF (MYSQL_FIND_REQUIRED)
    MESSAGE(STATUS "Looked for MYSQL libraries named ${MYSQL_NAMES}.")
    MESSAGE(FATAL_ERROR "Could NOT find MYSQL library")
  ENDIF (MYSQL_FIND_REQUIRED)
ENDIF (MYSQL_FOUND)

MARK_AS_ADVANCED(
  MYSQL_LIBRARY
  MYSQL_LIB_DIR
  MYSQL_INCLUDE_DIR
)
