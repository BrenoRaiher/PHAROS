#include "SpiceUsr.h"

#include <cmath>
#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <iomanip>
#include <iostream>
#include <sstream>
#include <stdexcept>
#include <string>
#include <vector>

namespace
{
    constexpr double kPi = 3.141592653589793238462643383279502884;
    constexpr double kNauticalMileToKilometer = 1.852;
    constexpr double kFootToKilometer = 0.0003048;

    struct Event
    {
        std::string id;
        std::string name;
        double get_seconds = 0.0;
        std::string body;
        double latitude_deg = 0.0;
        double longitude_east_deg = 0.0;
        double altitude_nmi = 0.0;
        double speed_ftps = 0.0;
        double flight_path_angle_deg = 0.0;
        double heading_deg = 0.0;
    };

    std::vector<std::string> Split(const std::string& line)
    {
        std::vector<std::string> fields;
        std::stringstream stream(line);
        std::string field;
        while (std::getline(stream, field, ',')) fields.push_back(field);
        return fields;
    }

    std::vector<Event> ReadEvents(const std::filesystem::path& path)
    {
        std::ifstream input(path);
        if (!input) throw std::runtime_error("Could not open event CSV");
        std::string line;
        std::getline(input, line);
        std::vector<Event> events;
        while (std::getline(input, line))
        {
            if (line.empty()) continue;
            const std::vector<std::string> fields = Split(line);
            if (fields.size() != 10)
                throw std::runtime_error("Expected ten fields in: " + line);
            Event event;
            event.id = fields[0];
            event.name = fields[1];
            event.get_seconds = std::stod(fields[2]);
            event.body = fields[3];
            event.latitude_deg = std::stod(fields[4]);
            event.longitude_east_deg = std::stod(fields[5]);
            event.altitude_nmi = std::stod(fields[6]);
            event.speed_ftps = std::stod(fields[7]);
            event.flight_path_angle_deg = std::stod(fields[8]);
            event.heading_deg = std::stod(fields[9]);
            events.push_back(event);
        }
        return events;
    }

    void ThrowIfSpiceFailed(const std::string& context)
    {
        if (!failed_c()) return;
        SpiceChar message[1841]{};
        getmsg_c("LONG", static_cast<SpiceInt>(sizeof(message)), message);
        reset_c();
        throw std::runtime_error(context + ": " + message);
    }

    std::string CsvText(std::string value)
    {
        std::string escaped;
        for (const char character : value)
        {
            if (character == '"') escaped += '"';
            escaped += character;
        }
        return '"' + escaped + '"';
    }
}

int main(const int argc, char** argv)
{
    if (argc < 5)
    {
        std::cerr
            << "Usage: apollo_event_converter_frames <events.csv> "
            << "<output.csv> <kernel1> <kernel2> [...]\n";
        return 2;
    }

    try
    {
        SpiceChar return_action[] = "RETURN";
        SpiceChar no_error_printing[] = "NONE";
        erract_c("SET", 0, return_action);
        errprt_c("SET", 0, no_error_printing);
        for (int index = 3; index < argc; ++index)
        {
            furnsh_c(argv[index]);
            ThrowIfSpiceFailed("Could not load kernel");
        }

        SpiceDouble range_zero_et = 0.0;
        str2et_c("1968-12-21T12:51:00Z", &range_zero_et);
        ThrowIfSpiceFailed("Could not convert Apollo range-zero epoch");

        const std::vector<Event> events = ReadEvents(argv[1]);
        const std::filesystem::path output_path = argv[2];
        std::filesystem::create_directories(output_path.parent_path());
        std::ofstream output(output_path, std::ios::binary | std::ios::trunc);
        if (!output) throw std::runtime_error("Could not create output CSV");
        output
            << "event_id,event_name,utc,get_seconds,body,body_fixed_frame,"
            << "latitude_deg,"
            << "longitude_east_deg,altitude_nmi,speed_ftps,"
            << "flight_path_angle_deg,heading_east_of_north_deg,"
            << "position_icrf_x_m,position_icrf_y_m,position_icrf_z_m,"
            << "velocity_icrf_x_mps,velocity_icrf_y_mps,velocity_icrf_z_mps,"
            << "relative_position_icrf_x_m,relative_position_icrf_y_m,"
            << "relative_position_icrf_z_m,relative_velocity_icrf_x_mps,"
            << "relative_velocity_icrf_y_mps,relative_velocity_icrf_z_mps\n"
            << std::setprecision(17);

        for (const Event& event : events)
        {
            const SpiceDouble et = range_zero_et + event.get_seconds;
            const std::string frame = event.body == "Earth"
                ? "ITRF93" : "MOON_ME_DE421";

            SpiceInt radii_count = 0;
            SpiceDouble radii[3]{};
            bodvrd_c(event.body.c_str(), "RADII", 3, &radii_count, radii);
            ThrowIfSpiceFailed("Could not obtain body radii");
            const double flattening = (radii[0] - radii[2]) / radii[0];
            const double latitude = event.latitude_deg * kPi / 180.0;
            const double longitude =
                event.longitude_east_deg * kPi / 180.0;
            const double altitude_km =
                event.altitude_nmi * kNauticalMileToKilometer;

            SpiceDouble position_fixed[3]{};
            georec_c(
                longitude, latitude, altitude_km,
                radii[0], flattening, position_fixed);
            ThrowIfSpiceFailed("Could not convert event position");

            const double sin_latitude = std::sin(latitude);
            const double cos_latitude = std::cos(latitude);
            const double sin_longitude = std::sin(longitude);
            const double cos_longitude = std::cos(longitude);
            const double north[3] = {
                -sin_latitude * cos_longitude,
                -sin_latitude * sin_longitude,
                cos_latitude};
            const double east[3] = {
                -sin_longitude, cos_longitude, 0.0};
            const double up[3] = {
                cos_latitude * cos_longitude,
                cos_latitude * sin_longitude,
                sin_latitude};
            const double gamma =
                event.flight_path_angle_deg * kPi / 180.0;
            const double heading = event.heading_deg * kPi / 180.0;
            const double speed_kmps =
                event.speed_ftps * kFootToKilometer;
            SpiceDouble velocity_fixed_basis[3]{};
            for (int axis = 0; axis < 3; ++axis)
            {
                velocity_fixed_basis[axis] = speed_kmps * (
                    std::cos(gamma) * std::cos(heading) * north[axis] +
                    std::cos(gamma) * std::sin(heading) * east[axis] +
                    std::sin(gamma) * up[axis]);
            }

            SpiceDouble fixed_to_j2000[3][3]{};
            pxform_c(frame.c_str(), "J2000", et, fixed_to_j2000);
            ThrowIfSpiceFailed("Could not obtain body-fixed rotation");
            SpiceDouble relative_position[3]{};
            SpiceDouble relative_velocity[3]{};
            mxv_c(fixed_to_j2000, position_fixed, relative_position);
            mxv_c(fixed_to_j2000, velocity_fixed_basis, relative_velocity);

            SpiceDouble body_state[6]{};
            SpiceDouble light_time = 0.0;
            spkezr_c(
                event.body.c_str(), et, "J2000", "NONE",
                "SOLAR SYSTEM BARYCENTER", body_state, &light_time);
            ThrowIfSpiceFailed("Could not obtain body state");

            SpiceChar utc[64]{};
            et2utc_c(et, "ISOC", 3, static_cast<SpiceInt>(sizeof(utc)), utc);
            ThrowIfSpiceFailed("Could not format event UTC");

            output << event.id << ',' << CsvText(event.name) << ',' << utc
                   << ',' << event.get_seconds << ',' << event.body << ','
                   << frame << ','
                   << event.latitude_deg << ',' << event.longitude_east_deg
                   << ',' << event.altitude_nmi << ',' << event.speed_ftps
                   << ',' << event.flight_path_angle_deg << ','
                   << event.heading_deg;
            for (int axis = 0; axis < 3; ++axis)
                output << ',' << 1000.0 *
                    (body_state[axis] + relative_position[axis]);
            for (int axis = 0; axis < 3; ++axis)
                output << ',' << 1000.0 *
                    (body_state[axis + 3] + relative_velocity[axis]);
            for (const double value : relative_position)
                output << ',' << 1000.0 * value;
            for (const double value : relative_velocity)
                output << ',' << 1000.0 * value;
            output << '\n';
        }

        if (!output) throw std::runtime_error("Could not finish output CSV");
        kclear_c();
        std::cout << "Converted " << events.size()
                  << " Apollo event anchors.\n";
        return 0;
    }
    catch (const std::exception& exception)
    {
        std::cerr << exception.what() << '\n';
        kclear_c();
        return 1;
    }
}
