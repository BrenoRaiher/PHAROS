// Copyright (c) 2026 Breno Raiher.
// SPDX-License-Identifier: MIT

#include "StandaloneSpiceProvider.h"

#include <SpiceUsr.h>

#include <array>

namespace
{
    // Later-loaded SPKs win wherever segments overlap. Load system-specific
    // satellite kernels first and DE442 last so documented barycentric states
    // always use DE442 while unique physical planets/moons remain available.
    constexpr std::array<const char*, 13> kKernelFiles = {
        "naif0012.tls",
        "pck00011.tpc",
        "gm_de440.tpc",
        "codes_300ast_20100725.tf",
        "mar099s.bsp",
        "jup230-short.bsp",
        "jup348.bsp",
        "sat252s.bsp",
        "ura111.bsp",
        "nep076.bsp",
        "plu058.bsp",
        "codes_300ast_20100725.bsp",
        "de442.bsp"};

    bool IsSystemBarycenter(const std::string& body_name)
    {
        SpiceInt body_code = 0;
        SpiceBoolean found = SPICEFALSE;
        bods2c_c(body_name.c_str(), &body_code, &found);
        if (failed_c())
        {
            reset_c();
            return false;
        }
        return found && body_code >= 1 && body_code <= 9;
    }
}

bool StandaloneSpiceProvider::CheckSucceeded(
    const std::string& operation,
    std::string& error) const
{
    if (!failed_c()) return true;
    SpiceChar message[1841] = {};
    getmsg_c("LONG", static_cast<SpiceInt>(sizeof(message)), message);
    error = "CSPICE " + operation + " failed: " + message;
    reset_c();
    return false;
}

bool StandaloneSpiceProvider::TryResolveBodyFixedFrameName(
    const std::string& body_name,
    std::string& frame_name) const
{
    SpiceInt body_code = 0;
    SpiceBoolean body_found = SPICEFALSE;
    bods2c_c(body_name.c_str(), &body_code, &body_found);
    std::string error;
    if (!CheckSucceeded("body-code lookup for " + body_name, error)
        || !body_found)
    {
        frame_name.clear();
        return false;
    }

    SpiceInt frame_code = 0;
    SpiceChar frame_buffer[128] = {};
    SpiceBoolean frame_found = SPICEFALSE;
    cidfrm_c(
        body_code,
        static_cast<SpiceInt>(sizeof(frame_buffer)),
        &frame_code,
        frame_buffer,
        &frame_found);
    if (!CheckSucceeded("frame lookup for " + body_name, error)
        || !frame_found)
    {
        frame_name.clear();
        return false;
    }

    frame_name = frame_buffer;
    return true;
}

bool StandaloneSpiceProvider::LoadKernels(
    const std::filesystem::path& kernel_directory,
    std::string& error)
{
    error.clear();
    if (loaded_) return true;
    SpiceChar return_action[] = "RETURN";
    erract_c("SET", static_cast<SpiceInt>(sizeof(return_action)), return_action);
    // All failures are captured through CheckSucceeded. Prevent CSPICE from also
    // writing raw "severe error" blocks to stderr before the caller handles them.
    SpiceChar no_direct_output[] = "NONE";
    errprt_c(
        "SET", static_cast<SpiceInt>(sizeof(no_direct_output)),
        no_direct_output);

    for (const char* file_name : kKernelFiles)
    {
        const std::filesystem::path path = kernel_directory / file_name;
        if (!std::filesystem::is_regular_file(path))
        {
            error = "Missing SPICE kernel: " + path.string();
            return false;
        }
        furnsh_c(path.string().c_str());
        if (!CheckSucceeded("loading " + path.string(), error)) return false;
    }
    loaded_ = true;
    return true;
}

bool StandaloneSpiceProvider::ConvertUtcToEphemerisTime(
    const std::string& utc,
    double& ephemeris_time_tdb_seconds,
    std::string& error) const
{
    if (!loaded_)
    {
        error = "SPICE kernels have not been loaded.";
        return false;
    }
    SpiceDouble value = 0.0;
    str2et_c(utc.c_str(), &value);
    if (!CheckSucceeded("UTC-to-ET conversion", error)) return false;
    ephemeris_time_tdb_seconds = value;
    return true;
}

bool StandaloneSpiceProvider::TryGetBodyState(
    const std::string& body_name,
    const double ephemeris_time_tdb_seconds,
    tgsim::BodyState& body_state) const
{
    SpiceDouble state[6] = {};
    SpiceDouble light_time = 0.0;
    spkezr_c(
        body_name.c_str(), ephemeris_time_tdb_seconds, "J2000", "NONE",
        "SOLAR SYSTEM BARYCENTER", state, &light_time);
    std::string error;
    if (!CheckSucceeded("state lookup for " + body_name, error)) return false;
    constexpr double km_to_m = 1.0e3;
    body_state.position_icrf_m = {
        state[0] * km_to_m, state[1] * km_to_m, state[2] * km_to_m};
    body_state.velocity_icrf_mps = {
        state[3] * km_to_m, state[4] * km_to_m, state[5] * km_to_m};
    return true;
}

bool StandaloneSpiceProvider::TryGetBodyFixedToIcrf(
    const std::string& body_name,
    const double ephemeris_time_tdb_seconds,
    tgsim::Mat3d& body_fixed_to_icrf) const
{
    std::string frame_name;
    if (!TryResolveBodyFixedFrameName(body_name, frame_name))
        return false;
    SpiceDouble rotation[3][3] = {};
    pxform_c(
        frame_name.c_str(),
        "J2000",
        ephemeris_time_tdb_seconds,
        rotation);
    std::string error;
    if (!CheckSucceeded("frame transform for " + body_name, error)) return false;
    for (int row = 0; row < 3; ++row)
        for (int column = 0; column < 3; ++column)
            body_fixed_to_icrf.m[row][column] = rotation[row][column];
    return true;
}

bool StandaloneSpiceProvider::TryGetBodyAngularVelocityIcrf(
    const std::string& body_name,
    const double ephemeris_time_tdb_seconds,
    tgsim::Vec3d& angular_velocity_icrf_radps) const
{
    std::string frame_name;
    if (!TryResolveBodyFixedFrameName(body_name, frame_name))
        return false;
    SpiceDouble transform[6][6] = {};
    SpiceDouble rotation[3][3] = {};
    SpiceDouble angular_velocity[3] = {};
    sxform_c(
        "J2000",
        frame_name.c_str(),
        ephemeris_time_tdb_seconds,
        transform);
    std::string error;
    if (!CheckSucceeded("state transform for " + body_name, error)) return false;
    xf2rav_c(transform, rotation, angular_velocity);
    angular_velocity_icrf_radps = {
        angular_velocity[0], angular_velocity[1], angular_velocity[2]};
    return true;
}

bool StandaloneSpiceProvider::TryGetBodyGravityMetadata(
    const std::string& body_name,
    tgsim::BodyGravityMetadata& metadata) const
{
    SpiceInt dimension = 0;
    SpiceDouble gm[1] = {};
    bodvrd_c(body_name.c_str(), "GM", 1, &dimension, gm);
    std::string error;
    if (!CheckSucceeded("GM lookup for " + body_name, error) || dimension < 1)
        return false;

    SpiceDouble radii[3] = {};
    bool has_radius = false;
    // NAIF IDs 1..9 are system barycenters, not physical surfaces. Avoid
    // deliberately asking the kernel pool for nonexistent BODYn_RADII values.
    if (!IsSystemBarycenter(body_name))
    {
        bodvrd_c(body_name.c_str(), "RADII", 3, &dimension, radii);
        has_radius = !failed_c() && dimension >= 1;
        if (failed_c()) reset_c();
    }
    metadata.gravitational_parameter_m3ps2 = gm[0] * 1.0e9;
    metadata.reference_radius_m = has_radius ? radii[0] * 1.0e3 : 0.0;
    return true;
}
