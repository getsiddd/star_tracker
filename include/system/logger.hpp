/**
 * LOST starting point
 *
 * Reads in CLI arguments/flags and starts the appropriate pipelines
 */

#ifndef LOGGER_H
#define LOGGER_H

#include <assert.h>
#include <sys/types.h>
#include <unistd.h>
#include <getopt.h>

#include <bitset>
#include <string>
#include <iostream>
#include <fstream>
#include <chrono>
#include <cstring>
#include <map>

#include <tbb/parallel_for.h>

#include <boost/log/core.hpp>
#include <boost/log/expressions.hpp>
#include <boost/log/sinks/text_file_backend.hpp>
#include <boost/log/sources/record_ostream.hpp>
#include <boost/log/sources/severity_logger.hpp>
#include <boost/log/trivial.hpp>
#include <boost/log/utility/setup/common_attributes.hpp>
#include <boost/log/utility/setup/console.hpp>
#include <boost/log/utility/setup/file.hpp>
#include <boost/thread.hpp>
using boost::thread;
using boost::mutex;

namespace logging = boost::log;
namespace src = boost::log::sources;
namespace sinks = boost::log::sinks;
namespace keywords = boost::log::keywords;


namespace lost {
    class Logger {
        public:
        Logger();
        void init();
        void operator<<(auto msg);
        static Logger* getDefaultLogger();
    };

    
}

#endif