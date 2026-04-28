
#include <string>
#include <iostream>

#include "lost.hpp"
#include "agent.hpp"
#include "system/logger.hpp"

#include <boost/log/trivial.hpp>

static int s_sig_num;
static void signal_handler(int sig_num) {
  signal(sig_num, signal_handler);
  s_sig_num = sig_num;
}

namespace lost {

    void exitHandler(int iSignum){
        BOOST_LOG_TRIVIAL(info) << "Terminate Lost Server";
        exit(iSignum);
    }
}

int main(int argc, char **argv) {

    // Registering signal SIGINT and signal handler
	signal(SIGINT, lost::exitHandler); // for Ctrl + C keyboard interrupt
	signal(SIGTERM, lost::exitHandler); // for Terminate signal
    signal(SIGPIPE, SIG_IGN);
    
    lost::Logger *logger = lost::Logger::getDefaultLogger();
    logger->init();
    
    lost::Agent *agent = lost::Agent::getDefaultAgent();
    agent->init("en0", 8000);
    agent->Run();

    lost::Lost::Run(argc, argv);
    /* Print how much objects were created during app running, and what have left-probably leaked */
    /* Disable object counting for release builds using '-D OATPP_DISABLE_ENV_OBJECT_COUNTERS' flag for better performance */
    BOOST_LOG_TRIVIAL(info) << "Environment:";
    return 0;
//    return lost::LostMain(argc, argv);
}
