find_package( Boost COMPONENTS program_options log log_setup filesystem unit_test_framework REQUIRED )


add_executable (rio_sockperf_echo
				main.cpp
				sockperf_echo.cpp
				)

target_link_libraries(rio_sockperf_echo
				PRIVATE 
				mx-c::rio
				sockperf
				Boost::boost
				Boost::disable_autolinking
				Boost::log
				Boost::log_setup
				utils_program_options
				utils_program_options_log
				utils_nic_addresses
			)

set_property(TARGET rio_sockperf_echo PROPERTY CXX_STANDARD 20)

#target_compile_definitions(rio_sockperf_echo
#    PUBLIC
#      $<$<CXX_COMPILER_ID:MSVC>:WIN32_LEAN_AND_MEAN _WIN32_WINNT=0x0A00> # Don't include most of Windows.h
#  )
#