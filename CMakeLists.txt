# CMakeList.txt : Top-level CMake project file, do global configuration
# and include sub-projects here.
#
cmake_minimum_required (VERSION 3.8)

# Enable Hot Reload for MSVC compilers if supported.
if (POLICY CMP0141)
  cmake_policy(SET CMP0141 NEW)
  set(CMAKE_MSVC_DEBUG_INFORMATION_FORMAT "$<IF:$<AND:$<C_COMPILER_ID:MSVC>,$<CXX_COMPILER_ID:MSVC>>,$<$<CONFIG:Debug,RelWithDebInfo>:EditAndContinue>,$<$<CONFIG:Debug,RelWithDebInfo>:ProgramDatabase>>")
endif()

project ("mx-c" 
	      LANGUAGES
			CXX #ASM_MASM
		)

list(APPEND CMAKE_MODULE_PATH ${CMAKE_CURRENT_SOURCE_DIR}/cmake)

 add_subdirectory ("rmaxx")
 add_subdirectory ("rmaxx_echo")
 add_subdirectory ("rivermax_proxy")
 add_subdirectory ("rio_echo")
 add_subdirectory ("rio")
 add_subdirectory ("sockperf")
 add_subdirectory ("asio")
 add_subdirectory(${toolz_DIR}/utils ${CMAKE_CURRENT_BINARY_DIR}/utils)
 add_subdirectory(${toolz_DIR}/utils/program_options ${CMAKE_CURRENT_BINARY_DIR}/utils/program_options)
