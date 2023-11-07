cmake_minimum_required (VERSION 3.8)

set(Boost_USE_STATIC_LIBS ON)
find_package( Boost COMPONENTS program_options log log_setup REQUIRED )

add_executable (echo
                echo.cpp
                main.cpp
                )

target_link_libraries(echo
        sockperf
        utils::program_options
        utils::program_options::log
        Boost::boost
        Boost::program_options
        Boost::log
        Boost::log_setup
)

set_property(TARGET echo PROPERTY CXX_STANDARD 23)
