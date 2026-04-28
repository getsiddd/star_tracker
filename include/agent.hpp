
#ifndef AGENT_HPP
#define AGENT_HPP

#include "image.hpp"
#include "star-utils.hpp"
#include "attitude-utils.hpp"

#include <string>
#include <vector>

namespace lost {
    class Agent {
        public:
            Agent();
            ~Agent();
            static Agent* getDefaultAgent();
            void printDeviceList();

            void init(std::string networkDevice, unsigned int port);

            //inputs
            void setRawImagesInput();
            void setStarCatalogInput();
            void setAttitudesInput();

            //outputs
            void setRawImagesOutput(std::vector<Image> rawImages);
            void setAttitudesOutput(std::vector<Attitude> attitudes);
            void setProcessedImagesOutput(std::vector<Image> processedImages);
            void setStarCatalogOutput(Catalog catlog);
            void setHeartBeatOutput();
            void setStatusOutput(std::string msg);

            //services
            void ComputeAttitudes();
            void HeartBeat();

            void Run();

        private:
            std::string networkDevice = "";
            unsigned int port = 5670;
            std::string agentName = "Agent Name";
            bool interactiveloop = false;
            //inputs

            //outputs

    };
}

#endif /* LostAgent_h */
