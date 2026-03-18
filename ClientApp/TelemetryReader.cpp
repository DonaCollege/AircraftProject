#include "TelemetryReader.h"
#include <sstream>

bool TelemetryReader::open(const std::string& filename)
{
    file.open(filename);
    return file.is_open();
}

bool TelemetryReader::readNext(double& time, double& fuel)
{
    std::string line;

    if (!std::getline(file, line))
        return false;

    std::stringstream ss(line);
    std::string token;

    std::getline(ss, token, ',');
    time = std::stod(token);

    std::getline(ss, token, ',');
    fuel = std::stod(token);

    return true;
}