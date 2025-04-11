# CMakeList.txt : Top-level CMake project file, do global configuration
# and include sub-projects here.
#
cmake_minimum_required (VERSION 3.8)

# Enable Hot Reload for MSVC compilers if supported.
if (POLICY CMP0141)
  cmake_policy(SET CMP0141 NEW)
  set(CMAKE_MSVC_DEBUG_INFORMATION_FORMAT "$<IF:$<AND:$<C_COMPILER_ID:MSVC>,$<CXX_COMPILER_ID:MSVC>>,$<$<CONFIG:Debug,RelWithDebInfo>:EditAndContinue>,$<$<CONFIG:Debug,RelWithDebInfo>:ProgramDatabase>>")
endif()

#cmake_policy(SET CMP0074 NEW)

project ("mx-c" 
	      LANGUAGES
			CXX #ASM_MASM
		)

list(APPEND CMAKE_MODULE_PATH ${CMAKE_CURRENT_SOURCE_DIR}/cmake)

include(FetchContent)

FetchContent_Declare(
		fb_utils
		GIT_REPOSITORY https://github.com/serge-klim/fb_utils.git
		GIT_TAG        7b4381a69bcbcb0f98f8c1f22076dbfba272e79d
	)

FetchContent_MakeAvailable(fb_utils)

 add_subdirectory ("rmaxx")
 add_subdirectory ("rmaxx_echo")
 add_subdirectory ("rivermax_proxy")
 add_subdirectory ("rio_echo")
 add_subdirectory ("rio")
 add_subdirectory ("sockperf")
 add_subdirectory ("asio")
