#ifndef LOST_H
#define LOST_H


#include "databases.hpp"
#include "io.hpp"
#include "stream.hpp"
#include "blind-solve.hpp"

// OS Specfic Libraries

#if defined(__linux__) // Or #if __linux__
#include <linux/videodev2.h>
#include <fcntl.h>
#endif

/// Convert string to boolean
bool atobool(const char *cstr);

namespace lost {

    #define LOST_OPTIONAL_OPTARG()                                   \
        ((optarg == NULL && optind < argc && argv[optind][0] != '-') \
         ? (bool) (optarg = argv[optind++])                          \
         : (optarg != NULL))

    class Lost {
    public:
        void init();
        static Lost* getInstance();
        static void DatabaseBuild(const DatabaseOptions &values);
        static void PipelineRun(const PipelineOptions &values);
        static void StreamRun(const StreamOptions &values);
        static void BlindSolveRun(const BlindSolveOptions &values);
        static void Run(int argc, char **argv);
    };
}

#endif