find_package(Rivermax REQUIRED)
find_package( Boost COMPONENTS program_options log log_setup filesystem unit_test_framework REQUIRED )

# Add source to this project's executable.
add_library (rio
			 socket.cpp
			 channels.cpp
			 buffers.cpp
			)

add_library (mx-c::rio ALIAS rio)

target_sources(rio PUBLIC FILE_SET HEADERS
				FILES
                options.hpp
                socket.hpp
                channels.hpp
                buffers.hpp
			   )

target_include_directories(rio INTERFACE
		$<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/..>
		$<INSTALL_INTERFACE:include/rio>
		)

target_link_libraries(rio
				PUBLIC
				Boost::boost
				PRIVATE 
				Boost::disable_autolinking
				Boost::log
				Boost::log_setup
		#        Boost::program_options
				utils::utils
			)


set_property(TARGET rio PROPERTY CXX_STANDARD 20)
target_compile_options(rio PRIVATE
     $<$<CXX_COMPILER_ID:MSVC>:/W4 /WX /EHsc>
     $<$<NOT:$<CXX_COMPILER_ID:MSVC>>:-Wall -Wextra -Wpedantic -Werror -Wno-c++98-compat -Wno-unsafe-buffer-usage>
)


add_executable (rio_test
        test.cpp
        )

target_link_libraries(rio_test
		PRIVATE
        mx-c::rio
		utils
		Boost::unit_test_framework
)

set_property(TARGET rio_test PROPERTY CXX_STANDARD 20)


include(GNUInstallDirs)
include(CMakePackageConfigHelpers)
set(rio_VERSION 0.0.1)


install(TARGETS rio
        FILE_SET HEADERS DESTINATION include/rio
        ARCHIVE DESTINATION ${CMAKE_INSTALL_LIBDIR}
        LIBRARY DESTINATION ${CMAKE_INSTALL_LIBDIR}
        RUNTIME DESTINATION ${CMAKE_INSTALL_LIBDIR}
        )