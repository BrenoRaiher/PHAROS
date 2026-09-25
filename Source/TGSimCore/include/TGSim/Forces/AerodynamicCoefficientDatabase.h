// Copyright (c) 2026 Breno Raiher.
// SPDX-License-Identifier: MIT

#pragma once

#include "TGSim/Core/ModelTypes.h"

#include <memory>
#include <string>

namespace tgsim
{
    struct AerodynamicCoefficientQuery
    {
        double molecular_speed_ratio = 0.0;
        double knudsen_number = 0.0;
        Vec3d incoming_flow_direction_body = Vec3d::UnitX();
        const std::vector<double>* articulation_coordinates = nullptr;
    };

    struct AerodynamicCoefficientResult
    {
        Vec3d force_coefficients_body;
        Vec3d moment_coefficients_body_about_reference;
        // True only when nearest-row extrapolation accepted a query outside
        // the sampled speed-ratio, Kn, or eta intervals.
        bool outside_sampled_scalar_bounds = false;
    };

    /// Validates row dimensions, finite values, duplicate inputs, and whether the
    /// scattered points span every independent coordinate that varies in the data.
    std::string ValidateAerodynamicCoefficientDatabase(
        const AerodynamicCoefficientDatabase& database,
        std::size_t articulation_coordinate_count);

    /// Immutable normalized scattered-data interpolator built once per simulation.
    /// nanoflann performs exact k-nearest searches; the coefficient interpolation
    /// is exact Shepard inverse-distance weighting or nearest-neighbor selection.
    class AerodynamicCoefficientInterpolator
    {
    public:
        explicit AerodynamicCoefficientInterpolator(
            const AerodynamicCoefficientDatabase& database);
        ~AerodynamicCoefficientInterpolator();

        AerodynamicCoefficientInterpolator(const AerodynamicCoefficientInterpolator&) = delete;
        AerodynamicCoefficientInterpolator& operator=(const AerodynamicCoefficientInterpolator&) = delete;
        AerodynamicCoefficientInterpolator(AerodynamicCoefficientInterpolator&&) noexcept;
        AerodynamicCoefficientInterpolator& operator=(AerodynamicCoefficientInterpolator&&) noexcept;

        bool IsReady() const;
        bool Interpolate(
            const AerodynamicCoefficientQuery& query,
            AerodynamicCoefficientResult& result) const;

    private:
        struct Impl;
        std::unique_ptr<Impl> impl_;
    };
}
