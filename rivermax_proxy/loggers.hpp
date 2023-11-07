#pragma once
#include <boost/log/sources/logger.hpp>
#include <boost/log/trivial.hpp>
#include <boost/log/common.hpp>
//#include <boost/log/expressions.hpp>
#include <string>

using Logger = boost::log::sources::severity_channel_logger_mt<boost::log::trivial::severity_level, std::string>;
BOOST_LOG_GLOBAL_LOGGER(logger, Logger)


//BOOST_LOG_ATTRIBUTE_KEYWORD(tag_attr, "Tag", std::string)
