#find_package(Rivermax MODULE REQUIRED)
find_package(Rivermax 1.51.6 REQUIRED)
find_package(Boost COMPONENTS log log_setup unit_test_framework REQUIRED )



add_library (rmaxx 
				device.cpp
				tx_channel.cpp
				rx_channel.cpp
				rmaxx_utils.cpp
				circular_memory.cpp
				)

add_library (mx-c::rmaxx ALIAS rmaxx)

target_sources(rmaxx PUBLIC FILE_SET HEADERS
				FILES
				device.hpp
				tx_channel.hpp
				rx_channel.hpp
				rmaxx_utils.hpp
				circular_memory.hpp
			   )

target_include_directories(rmaxx INTERFACE
		$<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/..>
		$<INSTALL_INTERFACE:include/rmaxx>
		)

target_link_libraries(rmaxx
				PUBLIC
				Rivermax::Rivermax
				Boost::boost
				PRIVATE 
				Boost::disable_autolinking
				Boost::log
				Boost::log_setup
				fb::fb_utils
			)

#target_include_directories(rmaxx INTERFACE ..)

set_property(TARGET rmaxx PROPERTY CXX_STANDARD 20)


add_executable (rmaxx_utils_test
        test.cpp
        )

target_link_libraries(rmaxx_utils_test
		PRIVATE
        mx-c::rmaxx
		fb::fb_utils
		Boost::unit_test_framework
		Boost::disable_autolinking
)

set_property(TARGET rmaxx_utils_test PROPERTY CXX_STANDARD 20)
