// Copyright (c) 2026 Breno Raiher.
// SPDX-License-Identifier: MIT

#include "TGSim/Forces/SolarRadiationPressureModel.h"

// Integrates SRP over converter-generated proxy triangles and estimates
// articulated component shadows with immutable component-local BVHs.

#include "TGSim/Environment/EnvironmentModels.h"

#include <algorithm>
#include <array>
#include <cmath>
#include <limits>
#include <numeric>
#include <utility>
#include <vector>

namespace tgsim
{
    namespace
    {
        constexpr std::size_t kBvhLeafTriangleCount = 4;
        constexpr double kMinimumTriangleDoubleAreaM2 = 1.0e-18;

        double Coordinate(const Vec3d& value, int axis)
        {
            if (axis == 0) return value.x;
            if (axis == 1) return value.y;
            return value.z;
        }

        struct AxisAlignedBounds
        {
            Vec3d minimum{
                std::numeric_limits<double>::infinity(),
                std::numeric_limits<double>::infinity(),
                std::numeric_limits<double>::infinity()};
            Vec3d maximum{
                -std::numeric_limits<double>::infinity(),
                -std::numeric_limits<double>::infinity(),
                -std::numeric_limits<double>::infinity()};

            void Expand(const Vec3d& point)
            {
                minimum.x = std::min(minimum.x, point.x);
                minimum.y = std::min(minimum.y, point.y);
                minimum.z = std::min(minimum.z, point.z);
                maximum.x = std::max(maximum.x, point.x);
                maximum.y = std::max(maximum.y, point.y);
                maximum.z = std::max(maximum.z, point.z);
            }

            void Expand(const AxisAlignedBounds& other)
            {
                Expand(other.minimum);
                Expand(other.maximum);
            }

            int LongestAxis() const
            {
                const Vec3d extent = maximum - minimum;
                if (extent.x >= extent.y && extent.x >= extent.z) return 0;
                return extent.y >= extent.z ? 1 : 2;
            }

            bool IntersectsRay(
                const Vec3d& origin,
                const Vec3d& direction,
                double minimum_distance) const
            {
                double near_distance = minimum_distance;
                double far_distance = std::numeric_limits<double>::infinity();
                for (int axis = 0; axis < 3; ++axis)
                {
                    const double origin_coordinate = Coordinate(origin, axis);
                    const double direction_coordinate = Coordinate(direction, axis);
                    const double minimum_coordinate = Coordinate(minimum, axis);
                    const double maximum_coordinate = Coordinate(maximum, axis);
                    if (std::abs(direction_coordinate) <= 1.0e-15)
                    {
                        if (origin_coordinate < minimum_coordinate ||
                            origin_coordinate > maximum_coordinate)
                        {
                            return false;
                        }
                        continue;
                    }

                    double entry_distance =
                        (minimum_coordinate - origin_coordinate) /
                        direction_coordinate;
                    double exit_distance =
                        (maximum_coordinate - origin_coordinate) /
                        direction_coordinate;
                    if (entry_distance > exit_distance)
                        std::swap(entry_distance, exit_distance);
                    near_distance = std::max(near_distance, entry_distance);
                    far_distance = std::min(far_distance, exit_distance);
                    if (far_distance < near_distance) return false;
                }
                return true;
            }
        };

        struct PreparedOpticalFacet
        {
            std::size_t component_index = 0;
            std::array<Vec3d, 3> vertices_component_m{};
            std::array<Vec3d, 3> shadow_samples_component_m{};
            Vec3d center_component_m;
            Vec3d outward_normal_component;
            double area_m2 = 0.0;
            double absorption = 1.0;
            double specular_reflection = 0.0;
            double diffuse_reflection = 0.0;
        };

        struct ShadowTriangle
        {
            std::size_t source_facet_index = 0;
            std::array<Vec3d, 3> vertices_component_m{};
            Vec3d center_component_m;
            AxisAlignedBounds bounds;
        };

        struct BvhNode
        {
            AxisAlignedBounds bounds;
            std::size_t first_triangle = 0;
            std::size_t triangle_count = 0;
            int left_child = -1;
            int right_child = -1;

            bool IsLeaf() const { return left_child < 0; }
        };

        bool RayIntersectsTriangle(
            const Vec3d& origin,
            const Vec3d& direction,
            const ShadowTriangle& triangle,
            double minimum_distance)
        {
            // Two-sided Moller-Trumbore test. Proxy surfaces are opaque shadow
            // casters regardless of which side is facing the ray.
            const Vec3d edge_ab =
                triangle.vertices_component_m[1] -
                triangle.vertices_component_m[0];
            const Vec3d edge_ac =
                triangle.vertices_component_m[2] -
                triangle.vertices_component_m[0];
            const Vec3d p = Cross(direction, edge_ac);
            const double determinant = Dot(edge_ab, p);
            if (std::abs(determinant) <= 1.0e-15) return false;

            const double inverse_determinant = 1.0 / determinant;
            const Vec3d from_a = origin - triangle.vertices_component_m[0];
            const double barycentric_b = Dot(from_a, p) * inverse_determinant;
            if (barycentric_b < -1.0e-12 || barycentric_b > 1.0 + 1.0e-12)
                return false;

            const Vec3d q = Cross(from_a, edge_ab);
            const double barycentric_c = Dot(direction, q) * inverse_determinant;
            if (barycentric_c < -1.0e-12 ||
                barycentric_b + barycentric_c > 1.0 + 1.0e-12)
            {
                return false;
            }

            const double distance = Dot(edge_ac, q) * inverse_determinant;
            return distance > minimum_distance;
        }

        class ComponentBvh
        {
        public:
            void AddTriangle(
                std::size_t source_facet_index,
                const std::array<Vec3d, 3>& vertices_component_m)
            {
                ShadowTriangle triangle;
                triangle.source_facet_index = source_facet_index;
                triangle.vertices_component_m = vertices_component_m;
                triangle.center_component_m =
                    (vertices_component_m[0] + vertices_component_m[1] +
                     vertices_component_m[2]) / 3.0;
                for (const Vec3d& vertex : vertices_component_m)
                    triangle.bounds.Expand(vertex);
                triangles_.push_back(triangle);
            }

            void Build()
            {
                root_node_ = -1;
                ordered_triangle_indices_.resize(triangles_.size());
                std::iota(
                    ordered_triangle_indices_.begin(),
                    ordered_triangle_indices_.end(),
                    std::size_t{0});
                nodes_.clear();
                if (!triangles_.empty())
                    root_node_ = BuildNode(0, triangles_.size());
            }

            bool Empty() const { return root_node_ < 0; }

            bool IntersectsRay(
                const Vec3d& origin_component_m,
                const Vec3d& direction_component,
                std::size_t excluded_source_facet_index,
                double minimum_distance_m) const
            {
                if (root_node_ < 0) return false;
                return IntersectsNode(
                    root_node_,
                    origin_component_m,
                    direction_component,
                    excluded_source_facet_index,
                    minimum_distance_m);
            }

        private:
            int BuildNode(std::size_t begin, std::size_t end)
            {
                const int node_index = static_cast<int>(nodes_.size());
                nodes_.push_back({});

                AxisAlignedBounds bounds;
                AxisAlignedBounds center_bounds;
                for (std::size_t index = begin; index < end; ++index)
                {
                    const ShadowTriangle& triangle =
                        triangles_[ordered_triangle_indices_[index]];
                    bounds.Expand(triangle.bounds);
                    center_bounds.Expand(triangle.center_component_m);
                }
                nodes_[node_index].bounds = bounds;

                const std::size_t count = end - begin;
                if (count <= kBvhLeafTriangleCount)
                {
                    nodes_[node_index].first_triangle = begin;
                    nodes_[node_index].triangle_count = count;
                    return node_index;
                }

                const int split_axis = center_bounds.LongestAxis();
                const std::size_t middle = begin + count / 2;
                std::nth_element(
                    ordered_triangle_indices_.begin() +
                        static_cast<std::ptrdiff_t>(begin),
                    ordered_triangle_indices_.begin() +
                        static_cast<std::ptrdiff_t>(middle),
                    ordered_triangle_indices_.begin() +
                        static_cast<std::ptrdiff_t>(end),
                    [&](std::size_t lhs, std::size_t rhs)
                    {
                        return Coordinate(
                            triangles_[lhs].center_component_m,
                            split_axis) <
                            Coordinate(
                                triangles_[rhs].center_component_m,
                                split_axis);
                    });
                const int left_child = BuildNode(begin, middle);
                const int right_child = BuildNode(middle, end);
                nodes_[node_index].left_child = left_child;
                nodes_[node_index].right_child = right_child;
                return node_index;
            }

            bool IntersectsNode(
                int node_index,
                const Vec3d& origin,
                const Vec3d& direction,
                std::size_t excluded_source_facet_index,
                double minimum_distance) const
            {
                const BvhNode& node = nodes_[node_index];
                if (!node.bounds.IntersectsRay(
                        origin, direction, minimum_distance))
                {
                    return false;
                }

                if (!node.IsLeaf())
                {
                    return IntersectsNode(
                               node.left_child,
                               origin,
                               direction,
                               excluded_source_facet_index,
                               minimum_distance) ||
                        IntersectsNode(
                            node.right_child,
                            origin,
                            direction,
                            excluded_source_facet_index,
                            minimum_distance);
                }

                for (std::size_t offset = 0;
                     offset < node.triangle_count;
                     ++offset)
                {
                    const ShadowTriangle& triangle = triangles_[
                        ordered_triangle_indices_[node.first_triangle + offset]];
                    if (triangle.source_facet_index ==
                        excluded_source_facet_index)
                    {
                        continue;
                    }
                    if (RayIntersectsTriangle(
                            origin,
                            direction,
                            triangle,
                            minimum_distance))
                    {
                        return true;
                    }
                }
                return false;
            }

            std::vector<ShadowTriangle> triangles_;
            std::vector<std::size_t> ordered_triangle_indices_;
            std::vector<BvhNode> nodes_;
            int root_node_ = -1;
        };

        bool IsConfiguredOcculter(
            const SolarRadiationSettings& settings,
            const std::string& body_name)
        {
            if (body_name == settings.sun_body_name) return false;
            if (settings.occulting_body_names.empty()) return true;
            return std::find(
                settings.occulting_body_names.begin(),
                settings.occulting_body_names.end(),
                body_name) != settings.occulting_body_names.end();
        }

        double ApparentDiskOverlapArea(
            double radius_a,
            double radius_b,
            double separation)
        {
            // Plane-of-sky circle intersection area. Angular radii and angular
            // separation are all in radians, so the returned area is in rad^2.
            if (radius_a <= 0.0 || radius_b <= 0.0 ||
                separation >= radius_a + radius_b)
            {
                return 0.0;
            }
            if (separation <= std::abs(radius_a - radius_b))
            {
                const double smaller_radius = std::min(radius_a, radius_b);
                return kPi * smaller_radius * smaller_radius;
            }

            const double separation_squared = separation * separation;
            const double radius_a_squared = radius_a * radius_a;
            const double radius_b_squared = radius_b * radius_b;
            const double angle_a = std::acos(std::clamp(
                (separation_squared + radius_a_squared - radius_b_squared) /
                    (2.0 * separation * radius_a),
                -1.0,
                1.0));
            const double angle_b = std::acos(std::clamp(
                (separation_squared + radius_b_squared - radius_a_squared) /
                    (2.0 * separation * radius_b),
                -1.0,
                1.0));
            const double radical = std::max(
                0.0,
                (-separation + radius_a + radius_b) *
                    (separation + radius_a - radius_b) *
                    (separation - radius_a + radius_b) *
                    (separation + radius_a + radius_b));
            return radius_a_squared * angle_a + radius_b_squared * angle_b -
                0.5 * std::sqrt(radical);
        }

        double ComputeVisibleSunFraction(
            double ephemeris_time_tdb_seconds,
            const SpacecraftState& spacecraft,
            const SimulationConfig& config,
            const GravityBody& sun,
            const BodyState& sun_state)
        {
            const Vec3d spacecraft_to_sun =
                sun_state.position_icrf_m - spacecraft.position_icrf_m;
            const double sun_distance = spacecraft_to_sun.Norm();
            if (!config.solar_radiation.compute_eclipse_shadow ||
                sun.reference_radius_m <= 0.0 ||
                sun_distance <= sun.reference_radius_m)
            {
                return 1.0;
            }

            const Vec3d sun_direction = spacecraft_to_sun / sun_distance;
            const double sun_angular_radius = std::asin(std::clamp(
                sun.reference_radius_m / sun_distance, 0.0, 1.0));
            const double apparent_sun_area =
                kPi * sun_angular_radius * sun_angular_radius;
            double maximum_occulted_fraction = 0.0;

            for (const GravityBody& body : config.gravity.bodies)
            {
                if (!IsConfiguredOcculter(
                        config.solar_radiation, body.name) ||
                    body.reference_radius_m <= 0.0)
                {
                    continue;
                }

                const BodyState body_state = ResolveBodyState(
                    body, config.gravity, ephemeris_time_tdb_seconds);
                const Vec3d spacecraft_to_body =
                    body_state.position_icrf_m - spacecraft.position_icrf_m;
                const double body_distance = spacecraft_to_body.Norm();
                // Only a body between the spacecraft and the Sun can eclipse it.
                if (body_distance <= body.reference_radius_m ||
                    body_distance >= sun_distance)
                {
                    continue;
                }

                const double body_angular_radius = std::asin(std::clamp(
                    body.reference_radius_m / body_distance, 0.0, 1.0));
                const double center_separation = std::acos(std::clamp(
                    Dot(
                        sun_direction,
                        spacecraft_to_body / body_distance),
                    -1.0,
                    1.0));
                const double occulted_area = ApparentDiskOverlapArea(
                    sun_angular_radius,
                    body_angular_radius,
                    center_separation);
                maximum_occulted_fraction = std::max(
                    maximum_occulted_fraction,
                    occulted_area / apparent_sun_area);
            }

            // The largest single occulting disk is exact for the usual
            // one-occulter case and avoids double counting overlapping disks.
            return std::clamp(
                1.0 - maximum_occulted_fraction, 0.0, 1.0);
        }
    }

    struct SolarRadiationPressureModel::Impl
    {
        explicit Impl(const SolarRadiationSettings& settings)
        {
            facets.reserve(settings.facets.size());
            std::size_t largest_component_index = 0;
            double largest_absolute_coordinate_m = 1.0;
            for (std::size_t source_index = 0;
                 source_index < settings.facets.size();
                 ++source_index)
            {
                const OpticalFacet& source = settings.facets[source_index];
                const Vec3d edge_01 =
                    source.vertices_component_m[1] -
                    source.vertices_component_m[0];
                const Vec3d edge_02 =
                    source.vertices_component_m[2] -
                    source.vertices_component_m[0];
                const Vec3d double_area_normal = Cross(edge_01, edge_02);
                const double double_area_m2 = double_area_normal.Norm();
                if (double_area_m2 <= kMinimumTriangleDoubleAreaM2) continue;

                PreparedOpticalFacet facet;
                facet.component_index = source.component_index;
                facet.vertices_component_m = source.vertices_component_m;
                facet.center_component_m =
                    (source.vertices_component_m[0] +
                     source.vertices_component_m[1] +
                     source.vertices_component_m[2]) / 3.0;
                facet.outward_normal_component =
                    double_area_normal / double_area_m2;
                facet.area_m2 = 0.5 * double_area_m2;
                facet.absorption = source.absorption;
                facet.specular_reflection = source.specular_reflection;
                facet.diffuse_reflection = source.diffuse_reflection;

                // Symmetric degree-two triangle quadrature locations. Each
                // sample owns exactly one third of the facet area.
                const Vec3d& v0 = source.vertices_component_m[0];
                const Vec3d& v1 = source.vertices_component_m[1];
                const Vec3d& v2 = source.vertices_component_m[2];
                facet.shadow_samples_component_m = {
                    (4.0 * v0 + v1 + v2) / 6.0,
                    (v0 + 4.0 * v1 + v2) / 6.0,
                    (v0 + v1 + 4.0 * v2) / 6.0};

                largest_component_index = std::max(
                    largest_component_index, source.component_index);
                for (const Vec3d& vertex : source.vertices_component_m)
                {
                    largest_absolute_coordinate_m = std::max({
                        largest_absolute_coordinate_m,
                        std::abs(vertex.x),
                        std::abs(vertex.y),
                        std::abs(vertex.z)});
                }
                facets.push_back(facet);
            }

            if (facets.empty()) return;
            component_bvhs.resize(largest_component_index + 1);
            for (std::size_t prepared_index = 0;
                 prepared_index < facets.size();
                 ++prepared_index)
            {
                const PreparedOpticalFacet& facet = facets[prepared_index];
                component_bvhs[facet.component_index].AddTriangle(
                    prepared_index, facet.vertices_component_m);
            }
            for (ComponentBvh& bvh : component_bvhs) bvh.Build();

            // Offset rays only far enough to leave their source plane. Scaling
            // with local geometry keeps the tolerance useful across spacecraft sizes.
            ray_origin_offset_m = std::max(
                1.0e-9, 1.0e-9 * largest_absolute_coordinate_m);
        }

        bool IsOccluded(
            const Vec3d& sample_point_body_m,
            const Vec3d& direction_to_sun_body,
            std::size_t source_facet_index,
            const std::vector<ComponentPose>& component_poses) const
        {
            const Vec3d ray_origin_body_m = sample_point_body_m +
                ray_origin_offset_m * direction_to_sun_body;
            const double minimum_distance_m =
                std::max(1.0e-12, 0.5 * ray_origin_offset_m);

            for (std::size_t component_index = 0;
                 component_index < component_bvhs.size() &&
                 component_index < component_poses.size();
                 ++component_index)
            {
                const ComponentBvh& bvh = component_bvhs[component_index];
                if (bvh.Empty()) continue;

                const ComponentPose& pose = component_poses[component_index];
                const Mat3d body_to_component =
                    pose.component_to_body.Transposed();
                const Vec3d ray_origin_component_m = body_to_component *
                    (ray_origin_body_m - pose.origin_body_m);
                const Vec3d ray_direction_component = body_to_component *
                    direction_to_sun_body;
                if (bvh.IntersectsRay(
                        ray_origin_component_m,
                        ray_direction_component,
                        source_facet_index,
                        minimum_distance_m))
                {
                    return true;
                }
            }
            return false;
        }

        std::vector<PreparedOpticalFacet> facets;
        std::vector<ComponentBvh> component_bvhs;
        double ray_origin_offset_m = 1.0e-9;
    };

    SolarRadiationPressureModel::SolarRadiationPressureModel(
        const SolarRadiationSettings& settings)
        : impl_(std::make_shared<Impl>(settings))
    {
    }

    SolarRadiationEvaluation SolarRadiationPressureModel::ComputeLoads(
        double ephemeris_time_tdb_seconds,
        const SpacecraftState& state,
        const SimulationConfig& config,
        const Vec3d& center_of_mass_body_m,
        const std::vector<ComponentPose>& component_poses) const
    {
        // General6DofDynamics uses the totals for CM translation/telemetry and
        // forwards the per-component loads to the corresponding ABA nodes.
        SolarRadiationEvaluation result;
        const SolarRadiationSettings& settings = config.solar_radiation;
        result.component_loads.resize(config.vehicle.components.size());

        const GravityBody* sun = FindGravityBody(
            config.gravity, settings.sun_body_name);
        if (!sun) return result;
        const BodyState sun_state = ResolveBodyState(
            *sun, config.gravity, ephemeris_time_tdb_seconds);
        // e_sun points from spacecraft to Sun. Radiation travels oppositely,
        // represented by the leading minus sign in the facet force law.
        const Vec3d to_sun =
            sun_state.position_icrf_m - state.position_icrf_m;
        const double sun_distance = to_sun.Norm();
        if (sun_distance <= 1.0e-9) return result;

        // Eclipse visibility is useful simulation telemetry in its own right:
        // playback uses it to extinguish the visual Sun during an occultation.
        // Compute it even when SRP forces are disabled or no optical facets exist.
        result.visible_sun_fraction = ComputeVisibleSunFraction(
            ephemeris_time_tdb_seconds, state, config, *sun, sun_state);

        // The remaining calculation applies physical SRP loads and therefore
        // requires both an enabled model and at least one prepared load facet.
        if (!settings.enabled || !impl_ || impl_->facets.empty()) return result;

        const Vec3d sun_direction_icrf = to_sun / sun_distance;
        // Celestial eclipse and inverse-square distance scale the pressure:
        // P = nu_celestial P_1AU (AU/r_sun)^2.
        const double pressure = settings.pressure_at_one_au_pa *
            std::pow(kAstronomicalUnitM / sun_distance, 2.0) *
            result.visible_sun_fraction;
        if (pressure <= 0.0) return result;

        const Mat3d body_to_icrf =
            state.attitude_body_to_icrf.ToRotationMatrix();
        const Mat3d icrf_to_body = body_to_icrf.Transposed();
        const Vec3d sun_direction_body =
            icrf_to_body * sun_direction_icrf;

        for (std::size_t prepared_index = 0;
             prepared_index < impl_->facets.size();
             ++prepared_index)
        {
            const PreparedOpticalFacet& facet = impl_->facets[prepared_index];
            if (facet.component_index >= component_poses.size()) continue;
            const ComponentPose& pose = component_poses[facet.component_index];

            // n^I = R_IB R_BC(eta) n^C. Only the outward side with
            // c = n^I dot e_sun^I > 0 receives radiation pressure.
            const Vec3d normal_body =
                pose.component_to_body * facet.outward_normal_component;
            const Vec3d normal_icrf = body_to_icrf * normal_body;
            const double incidence = Dot(
                normal_icrf, sun_direction_icrf);
            if (incidence <= 0.0) continue;

            // Force per illuminated area, from TG-1 Eqs. (58)-(62):
            // dF/dA = -P c [(alpha+rho_d)e_sun
            //          + 2(rho_s c+rho_d/3)n].
            const Vec3d force_per_area_icrf = -pressure * incidence * (
                (facet.absorption + facet.diffuse_reflection) *
                    sun_direction_icrf +
                2.0 *
                    (facet.specular_reflection * incidence +
                     facet.diffuse_reflection / 3.0) *
                    normal_icrf);

            const auto AccumulateSample =
                [&](const Vec3d& sample_component_m, double sample_area_m2)
                {
                    const Vec3d application_point_body =
                        ComponentKinematicsModel::PointToBody(
                            pose, sample_component_m);
                    if (settings.compute_component_shadows &&
                        impl_->IsOccluded(
                            application_point_body,
                            sun_direction_body,
                            prepared_index,
                            component_poses))
                    {
                        return;
                    }

                    const Vec3d sample_force_icrf =
                        sample_area_m2 * force_per_area_icrf;
                    const Vec3d sample_force_body =
                        icrf_to_body * sample_force_icrf;
                    result.force_icrf_n += sample_force_icrf;
                    // dM_CM^B = (r_sample^B-r_CM^B) x dF^B.
                    result.torque_body_nm += Cross(
                        application_point_body - center_of_mass_body_m,
                        sample_force_body);

                    ComponentLoad& component_load =
                        result.component_loads[facet.component_index];
                    component_load.force_icrf_n += sample_force_icrf;
                    const Vec3d sample_force_component =
                        pose.component_to_body.Transposed() * sample_force_body;
                    // ABA node moment: dM_OC^C = r_sample/OC^C x dF^C.
                    component_load.
                        torque_about_component_origin_component_nm += Cross(
                            sample_component_m,
                            sample_force_component);
                };

            if (!settings.compute_component_shadows)
            {
                AccumulateSample(facet.center_component_m, facet.area_m2);
                continue;
            }

            const double sample_area_m2 = facet.area_m2 / 3.0;
            for (const Vec3d& sample : facet.shadow_samples_component_m)
                AccumulateSample(sample, sample_area_m2);
        }
        return result;
    }
}
