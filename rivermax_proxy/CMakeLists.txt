cmake_minimum_required (VERSION 3.8)
#find_package(Rivermax REQUIRED)
find_package( Boost COMPONENTS program_options log log_setup filesystem iostreams unit_test_framework REQUIRED )


add_library(rivermax_proxy
			SHARED
			dllmain.cpp
			jump_table.hpp
			diversion.hpp
			rivermax_proxy.cpp
			proxy_call_logger.cpp
			loggers.hpp
			loggers.cpp
			utility.cpp
			utility.hpp
			dummy.cpp
			rio.cpp
#			extentions.cpp
			hacks.cpp
#			jmp.asm
			rivermax.def
			)

set_property(TARGET rivermax_proxy PROPERTY CXX_STANDARD 23)

target_link_libraries(rivermax_proxy
#        utils_program_options
#        Ws2_32.lib
#		Rivermax::Rivermax
        mx-c::rmaxx
		mx-c::rio
		utils::utils
        Boost::boost
		Boost::disable_autolinking
        Boost::program_options
		Boost::log
		Boost::log_setup
)

configure_file(   
    ${CMAKE_CURRENT_SOURCE_DIR}/rivermax.dll.config
    ${CMAKE_CURRENT_BINARY_DIR}/rivermax.dll.config
    COPYONLY
)


target_compile_options(rivermax_proxy PRIVATE
     $<$<CXX_COMPILER_ID:MSVC>:-W4 -WX -DNOMINMAX>
#     $<$<NOT:$<CXX_COMPILER_ID:MSVC>>:-Wall -Wextra -Wpedantic -Werror -Wno-c++98-compat -Wno-exit-time-destructors -Wno-missing-prototypes>
)

set_target_properties(rivermax_proxy PROPERTIES OUTPUT_NAME "rivermax")


add_executable (rivermax_proxy_test
        test.cpp
		rio_test.cpp
        )

target_link_libraries(rivermax_proxy_test
		PRIVATE
	    mx-c::rmaxx
		utils::utils
		utils::nic_addresses
		Boost::unit_test_framework
)

set_property(TARGET rivermax_proxy_test PROPERTY CXX_STANDARD 20)
