#include "loggers.hpp"
#include <boost/log/attributes.hpp>


BOOST_LOG_GLOBAL_LOGGER_INIT(logger, Logger)
{
   Logger log{boost::log::keywords::channel = "revermax.dll"};
   return log;
}

#include <boost/property_tree/ptree.hpp>
#include <boost/spirit/home/x3.hpp>
#include <boost/fusion/include/std_pair.hpp>
#include <boost/log/utility/setup/from_settings.hpp>
#include <boost/log/utility/setup/common_attributes.hpp>
#include <boost/program_options.hpp>

void init_log_from_unrecognized_program_options(boost::program_options::basic_parsed_options<char> const& parsed_options, boost::program_options::variables_map& options_map)
{
    auto tree = boost::property_tree::ptree{};
    //extension of boost::program_options::collect_unrecognized(parsed.options, boost::program_options::include_positional)
    for (auto const& option : parsed_options.options) {
        if(option.unregistered /* ||
            (mode == include_positional && options[i].position_key != -1)
            */){
            if (option.original_tokens.size() == 2) {
                auto begin = cbegin(option.original_tokens[0]);
                auto end = cend(option.original_tokens[0]);
                if (boost::spirit::x3::parse(begin, end, "Sinks." >> boost::spirit::x3::uint_ >> '.' >> +(boost::spirit::x3::char_ - '.') /*, value*/) || begin == end)
                   tree.put(option.original_tokens[0], option.original_tokens[1]);
                else
                    options_map.emplace(option.original_tokens[0], boost::program_options::variable_value{boost::any{option.original_tokens[1]}, false});
            }
        }
    }
    struct LogSectionWrapper : boost::log::basic_settings_section<char>
    {
        LogSectionWrapper(boost::property_tree::ptree* section) : boost::log::basic_settings_section<char>{section} {}
    };

    boost::log::init_from_settings(LogSectionWrapper{&tree});
    boost::log::add_common_attributes();
}