#include <iostream>
#include <fstream>
#include <stdexcept>

#include "../src/config.pb.h"

using namespace std;


void saveConfiguration(const char* fname, const systemConfiguration& hero) {
    fstream out(fname, ios::out | ios::trunc | ios::binary);
    if(!hero.SerializeToOstream(&out))
        throw runtime_error("saveHero() failed");
}

void loadConfiguration(const char* fname, systemConfiguration& hero) {        
    fstream in(fname, ios::in | ios::binary);
    if(!hero.ParseFromIstream(&in))
        throw runtime_error("loadHero() failed");
}

void printConfiguration(const systemConfiguration& hero) {
    for (int i=0; i < hero.cameras_size();i++){
        cout << "Name: " << hero.cameras(i).deviceid() << endl;
    }

    cout << endl;
}

int main() {
    // Verify that the version of the library that we linked against is
    // compatible with the version of the headers we compiled against.

    systemConfiguration system;

    cameraConfiguration camera1;
    camera1.set_deviceid("/dev/video0");
    camera1.set_index(0);
    camera1.set_focallength(50);
    camera1.set_pixelsize(1);
    camera1.set_fov(50);
    camera1.set_xresolution(664);
    camera1.set_yresolution(480);
    camera1.set_pixelrate(50);
    system.add_cameras()->CopyFrom(camera1);

    cameraConfiguration camera2;
    camera2.set_deviceid("/dev/video1");
    camera2.set_index(1);
    camera2.set_focallength(50);
    camera2.set_pixelsize(1);
    camera2.set_fov(50);
    camera2.set_xresolution(640);
    camera2.set_yresolution(480);
    camera2.set_pixelrate(50);
    system.add_cameras()->CopyFrom(camera2);

    cout << "Saving Configuration..." << endl;
    saveConfiguration("config.dat", system);

    cout << "Loading heroes..." << endl;
    systemConfiguration system1;
    loadConfiguration("config.dat", system1);

    cout << endl;
    printConfiguration(system1);
    return 0;
}