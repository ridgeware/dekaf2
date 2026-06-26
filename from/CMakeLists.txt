cmake_minimum_required(VERSION 3.8...3.31)

# project(from)

if (DEKAF2_USE_VENDORED_NLOHMANN)
	add_subdirectory(nlohmann)
endif()
