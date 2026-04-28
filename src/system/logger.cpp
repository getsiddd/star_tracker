#include "system/logger.hpp"

namespace lost {

  Logger* _pDefaultLogger = NULL;

  Logger* Logger::getDefaultLogger()
  {
    if(!_pDefaultLogger)
    {
      // _pDefaultLogger = new TempLogger();
      _pDefaultLogger = new lost::Logger();
    }
    return _pDefaultLogger;
  }

  Logger::Logger()
  {

  }

  void Logger::init(){
    logging::add_common_attributes();
    logging::add_file_log(
        keywords::target = "logs/", keywords::file_name = "logs/%Y%m%d_%H%M%S.log",
        keywords::rotation_size = 1*1024*1024,
        keywords::scan_method = sinks::file::scan_matching,
        keywords::time_based_rotation = sinks::file::rotation_at_time_point(23,59,59),
        keywords::format = "[%TimeStamp%] [%Severity%]: %Message%",
        keywords::auto_flush = true
        );

    logging::add_console_log(std::cout,
                             boost::log::keywords::format = ">>[%TimeStamp%] [%Severity%]: %Message% ");

    logging::core::get()->set_filter(logging::trivial::severity >= logging::trivial::info);
  }

  void Logger::operator<<(auto msg){
    BOOST_LOG_TRIVIAL(info) << msg;
  }
}