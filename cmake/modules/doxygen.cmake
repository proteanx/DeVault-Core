# Helper macro to add a "doc" target with CMake build system.
# and configure doxy.config.in to doxy.config
#
# Please note, that the tools, e.g.:
# doxygen, dot, latex, dvips, makeindex, gswin32, etc.
# must be in path.
#
# adapted from work of Jan Woetzel 2004-2006
# www.mip.informatik.uni-kiel.de/~jw

find_package(Doxygen)

if (DOXYGEN_FOUND)
  if (OPSYS STREQUAL "MACOSX")
    set(GENERATE_DOCSET "YES")
  else (OPSYS STREQUAL "MACOSX")
    set(GENERATE_DOCSET "NO")
  endif (OPSYS STREQUAL "MACOSX")
	if (EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/doc/tessa.doxygen.in")
		message(STATUS "Configured ${CMAKE_CURRENT_SOURCE_DIR}/doc/tessa.doxygen.in")
		configure_file(${CMAKE_CURRENT_SOURCE_DIR}/doc/tessa.doxygen.in
			${CMAKE_CURRENT_BINARY_DIR}/doc/tessa.doxygen @ONLY )
		# use config from BUILD tree
		set(DOXY_CONFIG "${CMAKE_CURRENT_BINARY_DIR}/doc/tessa.doxygen")
	else (EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/doc/tessa.doxygen.in")
		# use config from SOURCE tree
		if (EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/doc/tessa.doxygen")
			message(STATUS "Using existing ${CMAKE_CURRENT_SOURCE_DIR}/doc/tessa.doxygen")
			set(DOXY_CONFIG "${CMAKE_CURRENT_SOURCE_DIR}/doc/tessa.doxygen")
		else (exists "${CMAKE_CURRENT_SOURCE_DIR}/doc/tessa.doxygen")
			# failed completely...
			message(SEND_ERROR "Please create ${CMAKE_CURRENT_SOURCE_DIR}/doxy.config.in (or doxy.config as fallback)")
		endif(EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/doc/tessa.doxygen")

	endif(EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/doc/tessa.doxygen.in")

	add_custom_target(docs ${DOXYGEN_EXECUTABLE} ${DOXY_CONFIG})

endif(DOXYGEN_FOUND)
