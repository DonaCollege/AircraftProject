#pragma once
#include <string>
#include <fstream>

class TelemetryReader
{

    std::ifstream file;

public:
    bool open(const std::string& filename);
    bool readNext(double& time, double& fuel); // check for the format in given csv
};