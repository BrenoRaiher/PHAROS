// Copyright (c) 2026 Breno Raiher.
// SPDX-License-Identifier: MIT

#include "TGSim/Forces/AerodynamicCoefficientDatabase.h"

// Builds and queries the AVS-style scattered aerodynamic coefficient database.

#include <Eigen/LU>
#include <nanoflann/nanoflann.hpp>

#include <algorithm>
#include <cmath>
#include <numeric>
#include <utility>
#include <vector>

namespace tgsim
{
    namespace
    {
        constexpr double kCoordinateTolerance = 1.0e-12;

        bool IsFinite(double value)
        {
            return std::isfinite(value);
        }

        bool IsFinite(const Vec3d& value)
        {
            return IsFinite(value.x) && IsFinite(value.y) && IsFinite(value.z);
        }

        struct CoordinateSystem
        {
            std::size_t articulation_count = 0;
            std::vector<double> minimum;
            std::vector<double> maximum;
            std::vector<double> range;
            std::vector<std::vector<double>> normalized_samples;
        };

        std::vector<double> RawCoordinates(const AerodynamicCoefficientSample& sample)
        {
            const Vec3d direction = sample.incoming_flow_direction_body.Normalized();
            std::vector<double> coordinates = {
                sample.molecular_speed_ratio,
                std::log10(sample.knudsen_number),
                direction.x,
                direction.y,
                direction.z};
            coordinates.insert(
                coordinates.end(),
                sample.articulation_coordinates.begin(),
                sample.articulation_coordinates.end());
            return coordinates;
        }

        std::vector<double> RawCoordinates(const AerodynamicCoefficientQuery& query)
        {
            const Vec3d direction = query.incoming_flow_direction_body.Normalized();
            std::vector<double> coordinates = {
                query.molecular_speed_ratio,
                std::log10(query.knudsen_number),
                direction.x,
                direction.y,
                direction.z};
            if (query.articulation_coordinates)
            {
                coordinates.insert(
                    coordinates.end(),
                    query.articulation_coordinates->begin(),
                    query.articulation_coordinates->end());
            }
            return coordinates;
        }

        CoordinateSystem BuildCoordinateSystem(
            const AerodynamicCoefficientDatabase& database)
        {
            CoordinateSystem system;
            if (database.samples.empty()) return system;
            system.articulation_count =
                database.samples.front().articulation_coordinates.size();
            const std::size_t dimension = 5 + system.articulation_count;
            system.minimum.assign(dimension, std::numeric_limits<double>::infinity());
            system.maximum.assign(dimension, -std::numeric_limits<double>::infinity());

            std::vector<std::vector<double>> raw_samples;
            raw_samples.reserve(database.samples.size());
            for (const AerodynamicCoefficientSample& sample : database.samples)
            {
                raw_samples.push_back(RawCoordinates(sample));
                for (std::size_t axis = 0; axis < dimension; ++axis)
                {
                    system.minimum[axis] = std::min(
                        system.minimum[axis], raw_samples.back()[axis]);
                    system.maximum[axis] = std::max(
                        system.maximum[axis], raw_samples.back()[axis]);
                }
            }

            // Direction uses a common fixed scale so Euclidean distance in its
            // three coordinates is half the chord distance between unit vectors:
            // ||u-v||/2 = sin(theta/2). This does not distort angular proximity.
            for (std::size_t axis = 2; axis <= 4; ++axis)
            {
                system.minimum[axis] = -1.0;
                system.maximum[axis] = 1.0;
            }

            system.range.resize(dimension, 1.0);
            for (std::size_t axis = 0; axis < dimension; ++axis)
            {
                const double span = system.maximum[axis] - system.minimum[axis];
                system.range[axis] = span > kCoordinateTolerance ? span : 1.0;
            }

            system.normalized_samples.reserve(raw_samples.size());
            for (const std::vector<double>& raw : raw_samples)
            {
                std::vector<double> normalized(dimension, 0.0);
                for (std::size_t axis = 0; axis < dimension; ++axis)
                {
                    normalized[axis] =
                        (raw[axis] - system.minimum[axis]) / system.range[axis];
                }
                system.normalized_samples.push_back(std::move(normalized));
            }
            return system;
        }

        int MatrixRank(const Eigen::MatrixXd& matrix)
        {
            Eigen::FullPivLU<Eigen::MatrixXd> decomposition(matrix);
            decomposition.setThreshold(1.0e-10);
            return static_cast<int>(decomposition.rank());
        }

        std::size_t DirectionIntrinsicDimension(
            const AerodynamicCoefficientDatabase& database)
        {
            if (database.samples.size() < 2) return 0;
            const Vec3d first =
                database.samples.front().incoming_flow_direction_body.Normalized();
            bool varies = false;
            Vec3d tangent_reference;
            for (const AerodynamicCoefficientSample& sample : database.samples)
            {
                const Vec3d direction = sample.incoming_flow_direction_body.Normalized();
                if ((direction - first).Norm() > 1.0e-8) varies = true;
                tangent_reference += direction;
            }
            if (!varies) return 0;
            if (tangent_reference.Norm() <= 1.0e-12) tangent_reference = first;
            tangent_reference = tangent_reference.Normalized();

            Eigen::MatrixXd tangent_samples(
                static_cast<Eigen::Index>(database.samples.size()), 3);
            for (std::size_t row = 0; row < database.samples.size(); ++row)
            {
                const Vec3d direction =
                    database.samples[row].incoming_flow_direction_body.Normalized();
                const Vec3d tangent = direction -
                    Dot(direction, tangent_reference) * tangent_reference;
                tangent_samples(static_cast<Eigen::Index>(row), 0) = tangent.x;
                tangent_samples(static_cast<Eigen::Index>(row), 1) = tangent.y;
                tangent_samples(static_cast<Eigen::Index>(row), 2) = tangent.z;
            }
            // A unit direction has two intrinsic coordinates. The minimum of one
            // handles a database sampled only along one angular sweep.
            return static_cast<std::size_t>(
                std::max(1, std::min(2, MatrixRank(tangent_samples))));
        }

        bool NearlySamePoint(
            const std::vector<double>& a,
            const std::vector<double>& b)
        {
            if (a.size() != b.size()) return false;
            double squared_distance = 0.0;
            for (std::size_t axis = 0; axis < a.size(); ++axis)
            {
                const double difference = a[axis] - b[axis];
                squared_distance += difference * difference;
            }
            return squared_distance <= 1.0e-20;
        }
    }

    std::string ValidateAerodynamicCoefficientDatabase(
        const AerodynamicCoefficientDatabase& database,
        std::size_t articulation_coordinate_count)
    {
        if (!database.enabled) return {};
        if (!IsFinite(database.moment_reference_point_body_m))
            return "Aerodynamic database moment-reference coordinates must be finite.";
        if (database.samples.empty())
            return "The enabled aerodynamic coefficient database has no sample rows.";
        if (database.inverse_distance_power <= 0.0 ||
            !IsFinite(database.inverse_distance_power))
        {
            return "Aerodynamic inverse-distance power must be finite and positive.";
        }
        if (database.maximum_normalized_neighbor_distance <= 0.0 ||
            std::isnan(database.maximum_normalized_neighbor_distance))
        {
            return "Aerodynamic maximum normalized neighbor distance must be positive.";
        }

        for (const AerodynamicCoefficientSample& sample : database.samples)
        {
            if (!IsFinite(sample.molecular_speed_ratio) ||
                sample.molecular_speed_ratio < 0.0)
            {
                return "Aerodynamic database molecular speed ratios must be finite and nonnegative.";
            }
            if (!IsFinite(sample.knudsen_number) || sample.knudsen_number <= 0.0)
                return "Aerodynamic database Knudsen numbers must be finite and positive.";
            if (!IsFinite(sample.incoming_flow_direction_body) ||
                std::abs(sample.incoming_flow_direction_body.Norm() - 1.0) > 1.0e-6)
            {
                return "Every aerodynamic incoming-flow direction must be a normalized B-frame vector.";
            }
            if (sample.articulation_coordinates.size() != articulation_coordinate_count)
                return "Every aerodynamic row must contain the complete articulation-coordinate vector eta.";
            for (double coordinate : sample.articulation_coordinates)
                if (!IsFinite(coordinate))
                    return "Aerodynamic articulation coordinates must be finite.";
            if (!IsFinite(sample.force_coefficients_body) ||
                !IsFinite(sample.moment_coefficients_body_about_reference))
            {
                return "Aerodynamic force and moment coefficients must be finite.";
            }
        }

        const CoordinateSystem coordinates = BuildCoordinateSystem(database);
        std::vector<std::size_t> order(database.samples.size());
        std::iota(order.begin(), order.end(), 0);
        std::sort(order.begin(), order.end(), [&](std::size_t left, std::size_t right)
        {
            return coordinates.normalized_samples[left] <
                coordinates.normalized_samples[right];
        });
        for (std::size_t index = 1; index < order.size(); ++index)
        {
            if (NearlySamePoint(
                    coordinates.normalized_samples[order[index - 1]],
                    coordinates.normalized_samples[order[index]]))
            {
                return "Aerodynamic database contains duplicate independent-variable rows.";
            }
        }

        std::size_t expected_dimension = DirectionIntrinsicDimension(database);
        // Speed ratio, log10(Kn), and each eta coordinate contribute one domain
        // dimension only when that coordinate actually varies in the supplied rows.
        for (std::size_t axis : {std::size_t{0}, std::size_t{1}})
        {
            if (coordinates.maximum[axis] - coordinates.minimum[axis] > 1.0e-10)
                ++expected_dimension;
        }
        for (std::size_t axis = 5; axis < coordinates.minimum.size(); ++axis)
        {
            if (coordinates.maximum[axis] - coordinates.minimum[axis] > 1.0e-10)
                ++expected_dimension;
        }

        const std::size_t minimum_samples = expected_dimension + 1;
        if (database.samples.size() < minimum_samples)
        {
            return "Aerodynamic database needs at least " +
                std::to_string(minimum_samples) +
                " independent rows for its varying coordinates.";
        }

        if (expected_dimension > 0)
        {
            const std::size_t full_dimension = coordinates.minimum.size();
            Eigen::MatrixXd differences(
                static_cast<Eigen::Index>(database.samples.size() - 1),
                static_cast<Eigen::Index>(full_dimension));
            for (std::size_t row = 1; row < database.samples.size(); ++row)
            {
                for (std::size_t axis = 0; axis < full_dimension; ++axis)
                {
                    differences(
                        static_cast<Eigen::Index>(row - 1),
                        static_cast<Eigen::Index>(axis)) =
                        coordinates.normalized_samples[row][axis] -
                        coordinates.normalized_samples[0][axis];
                }
            }
            if (static_cast<std::size_t>(MatrixRank(differences)) < expected_dimension)
            {
                return "Aerodynamic database rows do not independently span all varying speed-ratio, Kn, direction, and eta coordinates.";
            }
        }
        return {};
    }

    struct AerodynamicCoefficientInterpolator::Impl
    {
        struct PointCloud
        {
            std::vector<std::vector<double>> points;

            std::size_t kdtree_get_point_count() const { return points.size(); }
            double kdtree_get_pt(std::size_t point, std::size_t axis) const
            {
                return points[point][axis];
            }
            template <class BoundingBox>
            bool kdtree_get_bbox(BoundingBox&) const { return false; }
        };

        using Metric = nanoflann::L2_Simple_Adaptor<double, PointCloud>;
        using Index = nanoflann::KDTreeSingleIndexAdaptor<
            Metric, PointCloud, -1, std::size_t>;

        AerodynamicCoefficientDatabase database;
        CoordinateSystem coordinates;
        PointCloud point_cloud;
        std::unique_ptr<Index> index;

        explicit Impl(const AerodynamicCoefficientDatabase& source)
            : database(source), coordinates(BuildCoordinateSystem(source))
        {
            point_cloud.points = coordinates.normalized_samples;
            if (!point_cloud.points.empty())
            {
                index = std::make_unique<Index>(
                    static_cast<int>(point_cloud.points.front().size()),
                    point_cloud,
                    nanoflann::KDTreeSingleIndexAdaptorParams(10));
            }
        }

        bool NormalizeQuery(
            const AerodynamicCoefficientQuery& query,
            std::vector<double>& normalized,
            bool& outside_scalar_bounds) const
        {
            if (!IsFinite(query.molecular_speed_ratio) ||
                query.molecular_speed_ratio < 0.0 ||
                !IsFinite(query.knudsen_number) || query.knudsen_number <= 0.0 ||
                !IsFinite(query.incoming_flow_direction_body) ||
                query.incoming_flow_direction_body.Norm() <= 1.0e-12 ||
                !query.articulation_coordinates ||
                query.articulation_coordinates->size() != coordinates.articulation_count)
            {
                return false;
            }

            const std::vector<double> raw = RawCoordinates(query);
            normalized.resize(raw.size(), 0.0);
            outside_scalar_bounds = false;
            for (std::size_t axis = 0; axis < raw.size(); ++axis)
            {
                normalized[axis] =
                    (raw[axis] - coordinates.minimum[axis]) /
                    coordinates.range[axis];
                // Direction lives on S^2, so componentwise bounds do not define
                // directional coverage. Only speed ratio, Kn, and eta use interval checks.
                if ((axis < 2 || axis >= 5) &&
                    (raw[axis] < coordinates.minimum[axis] - 1.0e-12 ||
                     raw[axis] > coordinates.maximum[axis] + 1.0e-12))
                {
                    outside_scalar_bounds = true;
                }
            }
            return true;
        }
    };

    AerodynamicCoefficientInterpolator::AerodynamicCoefficientInterpolator(
        const AerodynamicCoefficientDatabase& database)
        : impl_(std::make_unique<Impl>(database))
    {
    }

    AerodynamicCoefficientInterpolator::~AerodynamicCoefficientInterpolator() = default;
    AerodynamicCoefficientInterpolator::AerodynamicCoefficientInterpolator(
        AerodynamicCoefficientInterpolator&&) noexcept = default;
    AerodynamicCoefficientInterpolator&
    AerodynamicCoefficientInterpolator::operator=(
        AerodynamicCoefficientInterpolator&&) noexcept = default;

    bool AerodynamicCoefficientInterpolator::IsReady() const
    {
        return impl_ && impl_->index && impl_->database.enabled;
    }

    bool AerodynamicCoefficientInterpolator::Interpolate(
        const AerodynamicCoefficientQuery& query,
        AerodynamicCoefficientResult& result) const
    {
        if (!IsReady()) return false;

        std::vector<double> normalized_query;
        bool outside_scalar_bounds = false;
        if (!impl_->NormalizeQuery(
                query, normalized_query, outside_scalar_bounds)) return false;
        if (outside_scalar_bounds &&
            impl_->database.extrapolation ==
                AerodynamicDatabaseExtrapolationMethod::UseConstantDragFallback)
        {
            return false;
        }
        result.outside_sampled_scalar_bounds = outside_scalar_bounds;

        const std::size_t dimension = normalized_query.size();
        const std::size_t automatic_neighbors = std::max<std::size_t>(
            8, 2 * (dimension + 1));
        const std::size_t requested_neighbors =
            impl_->database.nearest_neighbor_count > 0
                ? impl_->database.nearest_neighbor_count
                : automatic_neighbors;
        const std::size_t neighbor_count = std::min(
            requested_neighbors, impl_->database.samples.size());

        std::vector<std::size_t> indices(neighbor_count);
        std::vector<double> squared_distances(neighbor_count);
        nanoflann::KNNResultSet<double, std::size_t> result_set(neighbor_count);
        result_set.init(indices.data(), squared_distances.data());
        impl_->index->findNeighbors(
            result_set,
            normalized_query.data(),
            nanoflann::SearchParams(32));
        if (result_set.size() == 0) return false;

        const double nearest_distance = std::sqrt(squared_distances.front());
        if (nearest_distance >
            impl_->database.maximum_normalized_neighbor_distance)
        {
            return false;
        }

        const auto assign_sample = [&](std::size_t sample_index)
        {
            const AerodynamicCoefficientSample& sample =
                impl_->database.samples[sample_index];
            result.force_coefficients_body = sample.force_coefficients_body;
            result.moment_coefficients_body_about_reference =
                sample.moment_coefficients_body_about_reference;
        };

        // Exact-row query or selected nearest extrapolation: return the row exactly.
        if (squared_distances.front() <= 1.0e-24 ||
            impl_->database.interpolation ==
                AerodynamicDatabaseInterpolationMethod::NearestNeighbor ||
            (outside_scalar_bounds &&
             impl_->database.extrapolation ==
                AerodynamicDatabaseExtrapolationMethod::NearestNeighbor))
        {
            assign_sample(indices.front());
            return true;
        }

        // Shepard interpolation for each of the six outputs:
        // C(x) = sum_i w_i C_i / sum_i w_i,
        // w_i = 1 / ||x-x_i||^p.
        // See Franke and Nielson (1980), DOI 10.1002/nme.1620151110.
        double weight_sum = 0.0;
        Vec3d force_sum;
        Vec3d moment_sum;
        const double half_power = 0.5 * impl_->database.inverse_distance_power;
        for (std::size_t neighbor = 0; neighbor < result_set.size(); ++neighbor)
        {
            const double weight = 1.0 /
                std::pow(std::max(squared_distances[neighbor], 1.0e-24), half_power);
            const AerodynamicCoefficientSample& sample =
                impl_->database.samples[indices[neighbor]];
            weight_sum += weight;
            force_sum += weight * sample.force_coefficients_body;
            moment_sum += weight *
                sample.moment_coefficients_body_about_reference;
        }
        if (!std::isfinite(weight_sum) || weight_sum <= 0.0) return false;
        result.force_coefficients_body = force_sum / weight_sum;
        result.moment_coefficients_body_about_reference = moment_sum / weight_sum;
        return true;
    }
}
