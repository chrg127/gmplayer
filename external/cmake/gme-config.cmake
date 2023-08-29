cmake_minimum_required(VERSION 3.5)

include(FeatureSummary)
set_package_properties(GME PROPERTIES
	DESCRIPTION "gme library"
)

set(GME_FOUND TRUE)
set(GME_INCLUDE_DIRS "${CMAKE_CURRENT_LIST_DIR}/../include")
set(GME_LIBRARY_DIRS "${CMAKE_CURRENT_LIST_DIR}/../lib")
set(GME_LIBRARIES gme)
