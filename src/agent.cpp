#include "agent.hpp"

#include <iostream>

#include <boost/log/trivial.hpp>

namespace lost {

Agent* _pDefaultAgent = NULL;

Agent::Agent() = default;

Agent::~Agent() = default;

Agent* Agent::getDefaultAgent() {
    if (!_pDefaultAgent) {
        _pDefaultAgent = new Agent();
    }
    return _pDefaultAgent;
}

void Agent::printDeviceList() {
    BOOST_LOG_TRIVIAL(info) << "Ingescape removed: network device enumeration is disabled.";
}

void Agent::init(std::string networkDevice, unsigned int port) {
    this->networkDevice = networkDevice;
    this->port = port;
    BOOST_LOG_TRIVIAL(info) << "Agent initialized (local mode, no Ingescape). device="
                            << this->networkDevice << " port=" << this->port;
}

void Agent::setRawImagesInput() {
    BOOST_LOG_TRIVIAL(info) << "setRawImagesInput called (no-op in local mode).";
}

void Agent::setStarCatalogInput() {
    BOOST_LOG_TRIVIAL(info) << "setStarCatalogInput called (no-op in local mode).";
}

void Agent::setAttitudesInput() {
    BOOST_LOG_TRIVIAL(info) << "setAttitudesInput called (no-op in local mode).";
}

void Agent::setRawImagesOutput(std::vector<Image> rawImages) {
    BOOST_LOG_TRIVIAL(info) << "RawImages produced: " << rawImages.size();
}

void Agent::setAttitudesOutput(std::vector<Attitude> attitudes) {
    BOOST_LOG_TRIVIAL(info) << "Attitudes produced: " << attitudes.size();
}

void Agent::setProcessedImagesOutput(std::vector<Image> processedImages) {
    BOOST_LOG_TRIVIAL(info) << "ProcessedImages produced: " << processedImages.size();
}

void Agent::setStarCatalogOutput(Catalog catlog) {
    BOOST_LOG_TRIVIAL(info) << "Catalog produced: " << catlog.size() << " entries";
}

void Agent::setHeartBeatOutput() {
    BOOST_LOG_TRIVIAL(info) << "Heartbeat emitted (no-op in local mode).";
}

void Agent::setStatusOutput(std::string msg) {
    BOOST_LOG_TRIVIAL(info) << "Status: " << msg;
}

void Agent::ComputeAttitudes() {
    BOOST_LOG_TRIVIAL(info) << "ComputeAttitudes requested (no-op in local mode).";
}

void Agent::HeartBeat() {
    BOOST_LOG_TRIVIAL(info) << "HeartBeat requested (no-op in local mode).";
}

void Agent::Run() {
    BOOST_LOG_TRIVIAL(info) << "Agent running (local mode, no Ingescape).";
}

}
