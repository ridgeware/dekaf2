cmake_minimum_required(VERSION 3.8)

# this is a minimal cmake recipe for our stripped down
# version of facebook's folly
project(minifolly)

set(CMAKE_CXX_STANDARD 17)
# we do not force C++17 if not available
# set(CMAKE_CXX_STANDARD_REQUIRED ON)

# make sure we have our version of folly installed
#execute_process(COMMAND "${CMAKE_CURRENT_SOURCE_DIR}/folly-gen.sh" installifmissing "${CMAKE_CURRENT_SOURCE_DIR}/folly" "${CMAKE_CURRENT_SOURCE_DIR}/folly-patch.diff")

set(CMAKE_THREAD_PREFER_PTHREAD TRUE)
set(THREADS_PREFER_PTHREAD_FLAG TRUE)
find_package(Threads REQUIRED)

if(BOOST_INCLUDE_DIR STREQUAL "")
	message(STATIC "searching boost for minifolly (should normally be done by dekaf2 before)")
	set(Boost_USE_STATIC_LIBS ON)
	# we prefer the boost lib that we installed ourselves in /opt/lib ..
	set(BOOST_ROOT "/opt/boost")
	find_package(Boost QUIET COMPONENTS system REQUIRED)
endif()

set (HEADERS
    glog/logging.h
    folly/portability/Asm.h
    folly/portability/BitsFunctexcept.h
    folly/portability/Builtins.h
    folly/portability/Config.h
    folly/portability/Constexpr.h
    folly/portability/IOVec.h
    folly/portability/Malloc.h
    folly/portability/Memory.h
    folly/portability/PThread.h
    folly/portability/String.h
    folly/portability/SysResource.h
    folly/portability/SysSyscall.h
    folly/portability/SysTime.h
    folly/portability/SysTypes.h
    folly/portability/Unistd.h
    folly/portability/Windows.h
    folly/detail/BitIteratorDetail.h
    folly/detail/Futex.h
    folly/detail/MallocImpl.h
    folly/detail/RangeCommon.h
    folly/detail/RangeSse42.h
    folly/concurrency/CacheLocality.h
    folly/hash/SpookyHashV1.h
    folly/hash/SpookyHashV2.h
    folly/ApplyTuple.h
    folly/Assume.h
    folly/Bits.h
    folly/CPortability.h
    folly/CppAttributes.h
    folly/CpuId.h
    folly/FBString.h
    folly/FBVector.h
    folly/folly-config.h
    folly/FormatTraits.h
    folly/Hash.h
    folly/Indestructible.h
    folly/Likely.h
    folly/Malloc.h
    folly/Memory.h
    folly/Portability.h
    folly/Range.h
    folly/SharedMutex.h
    folly/SparseByteSet.h
    folly/ThreadId.h
    folly/Traits.h
    folly/Utility.h
)

set (SOURCES
    folly/portability/BitsFunctexcept.cpp
    folly/portability/Builtins.cpp
    folly/portability/Malloc.cpp
    folly/portability/Memory.cpp
    folly/portability/PThread.cpp
    folly/portability/SysResource.cpp
    folly/portability/SysTime.cpp
    folly/portability/Unistd.cpp
    folly/concurrency/CacheLocality.cpp
    folly/hash/SpookyHashV1.cpp
    folly/hash/SpookyHashV2.cpp
    folly/detail/Futex.cpp
    folly/detail/MallocImpl.cpp
    folly/detail/RangeCommon.cpp
    folly/detail/RangeSse42.cpp
    folly/Assume.cpp
)

if (NOT CMAKE_SYSTEM_PROCESSOR MATCHES "arm")
	# the shared mutex implementation does not compile on arm
	set (SOURCES "${SOURCES}"
		folly/SharedMutex.cpp
)
endif()

set (MINIFOLLY_INCLUDE_DIR "${CMAKE_CURRENT_SOURCE_DIR}")

# we add the HEADERS as well, as otherwise qtcreator would not see them
# as belonging to the project. CMake would not need them here.
add_library(minifolly ${HEADERS} ${SOURCES})

target_include_directories(minifolly
	PUBLIC "${MINIFOLLY_INCLUDE_DIR}" "${Boost_INCLUDE_DIR}"
)

target_link_libraries(minifolly
    ${Boost_LIBRARIES}
    Threads::Threads
)
