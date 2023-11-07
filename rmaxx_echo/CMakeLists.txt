find_package (Boost COMPONENTS program_options log log_setup unit_test_framework REQUIRED )

# Add source to this project's executable.
add_executable (rmaxx_echo
				stats.h
				stats.cpp
				main.cpp
				echo.cpp
				)

target_link_libraries(rmaxx_echo 
				PRIVATE 
		        mx-c::rmaxx
				Boost::boost
				Boost::disable_autolinking
				Boost::log
				Boost::log_setup
				utils::program_options
				utils::program_options::log
				utils::nic_addresses
			)

set_property(TARGET rmaxx_echo PROPERTY CXX_STANDARD 23)


