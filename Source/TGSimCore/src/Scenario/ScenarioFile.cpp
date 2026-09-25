// Copyright (c) 2026 Breno Raiher.
// SPDX-License-Identifier: MIT

#include "TGSim/Scenario/ScenarioFile.h"

#include <toml.hpp>

#include <cmath>
#include <fstream>
#include <initializer_list>
#include <sstream>
#include <string_view>
#include <utility>

namespace tgsim::scenario
{
    namespace
    {
        constexpr double kRadiansPerDegree =
            3.141592653589793238462643383279502884 / 180.0;

        double RotationalValueFromFileUnits(const double value)
        {
            return value * kRadiansPerDegree;
        }

        double RotationalValueToFileUnits(const double value)
        {
            return value / kRadiansPerDegree;
        }

        void AddTypeError(
            Diagnostics& diagnostics,
            const std::string& path,
            const std::string& expected)
        {
            diagnostics.push_back({
                DiagnosticSeverity::Error,
                "TGSCN-TYPE",
                path,
                "Expected " + expected + "."});
        }

        void CheckKnownKeys(
            const toml::table& table,
            const std::string& table_path,
            Diagnostics& diagnostics,
            const std::initializer_list<std::string_view> known_keys)
        {
            for (const auto& [key, node] : table)
            {
                (void)node;
                const std::string_view key_text = key.str();
                bool known = false;
                for (const std::string_view candidate : known_keys)
                {
                    if (key_text == candidate)
                    {
                        known = true;
                        break;
                    }
                }
                if (known) continue;

                const std::string field_path = table_path.empty()
                    ? std::string(key_text)
                    : table_path + "." + std::string(key_text);
                diagnostics.push_back({
                    DiagnosticSeverity::Error,
                    "TGSCN-UNKNOWN-KEY",
                    field_path,
                    "Unknown PHAROS scenario key. Check the spelling."});
            }
        }

        const toml::table* ReadTable(
            const toml::table& parent,
            const std::string_view key,
            const std::string& path,
            Diagnostics& diagnostics,
            const bool required = false)
        {
            const toml::node_view<const toml::node> node = parent[key];
            if (!node)
            {
                if (required)
                {
                    diagnostics.push_back({
                        DiagnosticSeverity::Error,
                        "TGSCN-MISSING",
                        path,
                        "Required table is missing."});
                }
                return nullptr;
            }
            const toml::table* result = node.as_table();
            if (result == nullptr) AddTypeError(diagnostics, path, "a table");
            return result;
        }

        const toml::array* ReadArray(
            const toml::table& parent,
            const std::string_view key,
            const std::string& path,
            Diagnostics& diagnostics)
        {
            const toml::node_view<const toml::node> node = parent[key];
            if (!node) return nullptr;
            const toml::array* result = node.as_array();
            if (result == nullptr) AddTypeError(diagnostics, path, "an array");
            return result;
        }

        std::string ReadString(
            const toml::table& table,
            const std::string_view key,
            const std::string& fallback,
            const std::string& path,
            Diagnostics& diagnostics,
            const bool required = false)
        {
            const auto node = table[key];
            if (!node)
            {
                if (required)
                {
                    diagnostics.push_back({
                        DiagnosticSeverity::Error,
                        "TGSCN-MISSING",
                        path,
                        "Required string is missing."});
                }
                return fallback;
            }
            if (const auto value = node.value<std::string>()) return *value;
            AddTypeError(diagnostics, path, "a string");
            return fallback;
        }

        bool ReadBool(
            const toml::table& table,
            const std::string_view key,
            const bool fallback,
            const std::string& path,
            Diagnostics& diagnostics)
        {
            const auto node = table[key];
            if (!node) return fallback;
            if (const auto value = node.value<bool>()) return *value;
            AddTypeError(diagnostics, path, "a Boolean");
            return fallback;
        }

        double ReadNumberNode(
            const toml::node& node,
            const double fallback,
            const std::string& path,
            Diagnostics& diagnostics)
        {
            if (const auto value = node.value<double>()) return *value;
            if (const auto value = node.value<std::int64_t>())
                return static_cast<double>(*value);
            AddTypeError(diagnostics, path, "a number");
            return fallback;
        }

        double ReadNumber(
            const toml::table& table,
            const std::string_view key,
            const double fallback,
            const std::string& path,
            Diagnostics& diagnostics)
        {
            const auto node = table[key];
            return node ? ReadNumberNode(*node.node(), fallback, path, diagnostics)
                        : fallback;
        }

        std::int64_t ReadInteger(
            const toml::table& table,
            const std::string_view key,
            const std::int64_t fallback,
            const std::string& path,
            Diagnostics& diagnostics)
        {
            const auto node = table[key];
            if (!node) return fallback;
            if (const auto value = node.value<std::int64_t>()) return *value;
            AddTypeError(diagnostics, path, "an integer");
            return fallback;
        }

        std::size_t ReadSize(
            const toml::table& table,
            const std::string_view key,
            const std::size_t fallback,
            const std::string& path,
            Diagnostics& diagnostics)
        {
            const std::int64_t value = ReadInteger(
                table, key, static_cast<std::int64_t>(fallback), path, diagnostics);
            if (value < 0)
            {
                diagnostics.push_back({DiagnosticSeverity::Error, "TGSCN-RANGE",
                    path, "Value cannot be negative."});
                return fallback;
            }
            return static_cast<std::size_t>(value);
        }

        std::optional<double> ReadOptionalNumber(
            const toml::table& table,
            const std::string_view key,
            const std::string& path,
            Diagnostics& diagnostics)
        {
            const auto node = table[key];
            if (!node) return std::nullopt;
            return ReadNumberNode(*node.node(), 0.0, path, diagnostics);
        }

        Vec3d ReadVec3Node(
            const toml::node& node,
            const Vec3d& fallback,
            const std::string& path,
            Diagnostics& diagnostics)
        {
            const toml::array* array = node.as_array();
            if (array == nullptr || array->size() != 3)
            {
                AddTypeError(diagnostics, path, "a three-number array [x, y, z]");
                return fallback;
            }
            return {
                ReadNumberNode(*array->get(0), fallback.x, path + "[0]", diagnostics),
                ReadNumberNode(*array->get(1), fallback.y, path + "[1]", diagnostics),
                ReadNumberNode(*array->get(2), fallback.z, path + "[2]", diagnostics)};
        }

        Vec3d ReadVec3(
            const toml::table& table,
            const std::string_view key,
            const Vec3d& fallback,
            const std::string& path,
            Diagnostics& diagnostics)
        {
            const auto node = table[key];
            return node ? ReadVec3Node(*node.node(), fallback, path, diagnostics)
                        : fallback;
        }

        Quatd ReadQuat(
            const toml::table& table,
            const std::string_view key,
            const Quatd& fallback,
            const std::string& path,
            Diagnostics& diagnostics)
        {
            const auto node = table[key];
            if (!node) return fallback;
            const toml::array* array = node.as_array();
            if (array == nullptr || array->size() != 4)
            {
                AddTypeError(
                    diagnostics, path,
                    "a four-number quaternion array [w, x, y, z]");
                return fallback;
            }
            return {
                ReadNumberNode(*array->get(0), fallback.w, path + "[0]", diagnostics),
                ReadNumberNode(*array->get(1), fallback.x, path + "[1]", diagnostics),
                ReadNumberNode(*array->get(2), fallback.y, path + "[2]", diagnostics),
                ReadNumberNode(*array->get(3), fallback.z, path + "[3]", diagnostics)};
        }

        Color ReadColor(
            const toml::table& table,
            const std::string_view key,
            const Color& fallback,
            const std::string& path,
            Diagnostics& diagnostics)
        {
            const auto node = table[key];
            if (!node) return fallback;
            const toml::array* array = node.as_array();
            if (array == nullptr || array->size() != 4)
            {
                AddTypeError(diagnostics, path, "a four-number color [r, g, b, a]");
                return fallback;
            }
            return {
                ReadNumberNode(*array->get(0), fallback.r, path + "[0]", diagnostics),
                ReadNumberNode(*array->get(1), fallback.g, path + "[1]", diagnostics),
                ReadNumberNode(*array->get(2), fallback.b, path + "[2]", diagnostics),
                ReadNumberNode(*array->get(3), fallback.a, path + "[3]", diagnostics)};
        }

        std::vector<double> ReadNumberVector(
            const toml::table& table,
            const std::string_view key,
            const std::string& path,
            Diagnostics& diagnostics)
        {
            std::vector<double> result;
            const toml::array* array = ReadArray(table, key, path, diagnostics);
            if (array == nullptr) return result;
            result.reserve(array->size());
            for (std::size_t index = 0; index < array->size(); ++index)
            {
                result.push_back(ReadNumberNode(
                    *array->get(index), 0.0,
                    path + "[" + std::to_string(index) + "]", diagnostics));
            }
            return result;
        }

        std::vector<std::string> ReadStringVector(
            const toml::table& table,
            const std::string_view key,
            const std::string& path,
            Diagnostics& diagnostics)
        {
            std::vector<std::string> result;
            const toml::array* array = ReadArray(table, key, path, diagnostics);
            if (array == nullptr) return result;
            result.reserve(array->size());
            for (std::size_t index = 0; index < array->size(); ++index)
            {
                const toml::node* node = array->get(index);
                const auto value = node != nullptr
                    ? node->value<std::string>()
                    : std::optional<std::string>{};
                if (value)
                    result.push_back(*value);
                else
                    AddTypeError(
                        diagnostics,
                        path + "[" + std::to_string(index) + "]", "a string");
            }
            return result;
        }

        template <typename Enum>
        Enum ReadEnum(
            const toml::table& table,
            const std::string_view key,
            const Enum fallback,
            const std::string& path,
            Diagnostics& diagnostics,
            const std::initializer_list<std::pair<std::string_view, Enum>> values)
        {
            const auto node = table[key];
            if (!node) return fallback;
            const auto text = node.value<std::string>();
            if (!text)
            {
                AddTypeError(diagnostics, path, "an enum string");
                return fallback;
            }
            for (const auto& value : values)
            {
                if (*text == value.first) return value.second;
            }
            diagnostics.push_back({DiagnosticSeverity::Error, "TGSCN-ENUM", path,
                "Unknown value '" + *text + "'."});
            return fallback;
        }

        OpticalProperties ReadOptics(
            const toml::table& table,
            const std::string& path,
            Diagnostics& diagnostics,
            const OpticalProperties& fallback = {})
        {
            CheckKnownKeys(
                table, path, diagnostics,
                {"absorption", "specular_reflection", "diffuse_reflection"});
            OpticalProperties value = fallback;
            value.absorption = ReadNumber(
                table, "absorption", value.absorption,
                path + ".absorption", diagnostics);
            value.specular_reflection = ReadNumber(
                table, "specular_reflection", value.specular_reflection,
                path + ".specular_reflection", diagnostics);
            value.diffuse_reflection = ReadNumber(
                table, "diffuse_reflection", value.diffuse_reflection,
                path + ".diffuse_reflection", diagnostics);
            return value;
        }

        ScalarProfile ReadScalarProfile(
            const toml::table& table,
            const std::string& path,
            Diagnostics& diagnostics)
        {
            CheckKnownKeys(
                table, path, diagnostics,
                {"source", "constant_value", "csv_file"});
            ScalarProfile profile;
            profile.source = ReadEnum(
                table, "source", profile.source, path + ".source", diagnostics,
                {{"constant", ScalarProfileSource::Constant},
                 {"csv", ScalarProfileSource::Csv}});
            profile.constant_value = ReadNumber(
                table, "constant_value", profile.constant_value,
                path + ".constant_value", diagnostics);
            profile.csv_file_path = ReadString(
                table, "csv_file", profile.csv_file_path,
                path + ".csv_file", diagnostics);
            return profile;
        }

        SrpLogicalRegion ReadSrpRegion(
            const toml::table& table,
            const std::string_view key,
            const SrpLogicalRegion fallback,
            const std::string& path,
            Diagnostics& diagnostics)
        {
            return ReadEnum(
                table, key, fallback, path, diagnostics,
                {{"none", SrpLogicalRegion::None},
                 {"positive_x", SrpLogicalRegion::PositiveX},
                 {"negative_x", SrpLogicalRegion::NegativeX},
                 {"positive_y", SrpLogicalRegion::PositiveY},
                 {"negative_y", SrpLogicalRegion::NegativeY},
                 {"positive_z", SrpLogicalRegion::PositiveZ},
                 {"negative_z", SrpLogicalRegion::NegativeZ},
                 {"cylinder_side", SrpLogicalRegion::CylinderSide},
                 {"cylinder_positive_cap", SrpLogicalRegion::CylinderPositiveCap},
                 {"cylinder_negative_cap", SrpLogicalRegion::CylinderNegativeCap}});
        }

        toml::array Vec3Array(const Vec3d& value)
        {
            return toml::array{value.x, value.y, value.z};
        }

        toml::array QuatArray(const Quatd& value)
        {
            return toml::array{value.w, value.x, value.y, value.z};
        }

        toml::array ColorArray(const Color& value)
        {
            return toml::array{value.r, value.g, value.b, value.a};
        }

        toml::array NumberArray(const std::vector<double>& values)
        {
            toml::array result;
            for (const double value : values) result.push_back(value);
            return result;
        }

        toml::array StringArray(const std::vector<std::string>& values)
        {
            toml::array result;
            for (const std::string& value : values) result.push_back(value);
            return result;
        }

        toml::table OpticsTable(const OpticalProperties& value)
        {
            toml::table table;
            table.insert("absorption", value.absorption);
            table.insert("specular_reflection", value.specular_reflection);
            table.insert("diffuse_reflection", value.diffuse_reflection);
            return table;
        }

        const char* ToString(const EndMode value)
        {
            return value == EndMode::FinalUtc ? "final_utc" : "duration";
        }

        const char* ToString(const IntegratorKind value)
        {
            return value == IntegratorKind::AdaptiveDormandPrince54
                ? "adaptive_dormand_prince_54"
                : "fixed_step_rk4";
        }

        const char* ToString(const OutputMode value)
        {
            return value == OutputMode::FixedInterval
                ? "fixed_interval"
                : "every_integrator_step";
        }

        const char* ToString(const MassFlowConvention value)
        {
            return value == MassFlowConvention::MomentumDerivative
                ? "momentum_derivative"
                : "thrust_includes_exhaust_momentum";
        }

        const char* ToString(const JointMotion value)
        {
            return value == JointMotion::Translation ? "translation" : "rotation";
        }

        const char* ToString(const GeometrySource value)
        {
            switch (value)
            {
                case GeometrySource::None: return "none";
                case GeometrySource::CustomStl: return "custom_stl";
                case GeometrySource::Primitive:
                default: return "primitive";
            }
        }

        const char* ToString(const PrimitiveGeometry value)
        {
            switch (value)
            {
                case PrimitiveGeometry::Sphere: return "sphere";
                case PrimitiveGeometry::Cylinder: return "cylinder";
                case PrimitiveGeometry::Box:
                default: return "box";
            }
        }

        const char* ToString(const StlLengthUnit value)
        {
            switch (value)
            {
                case StlLengthUnit::Centimeters: return "centimeters";
                case StlLengthUnit::Meters: return "meters";
                case StlLengthUnit::Millimeters:
                default: return "millimeters";
            }
        }

        const char* ToString(const StlRecenterMode value)
        {
            switch (value)
            {
                case StlRecenterMode::CenterOnBounds: return "center_on_bounds";
                case StlRecenterMode::PlaceBaseAtOrigin: return "place_base_at_origin";
                case StlRecenterMode::KeepImportedOrigin:
                default: return "keep_imported_origin";
            }
        }

        const char* ToString(const SurfaceAppearance value)
        {
            return value == SurfaceAppearance::Textured ? "textured" : "solid_color";
        }

        const char* ToString(const SrpProxyResolution value)
        {
            switch (value)
            {
                case SrpProxyResolution::Custom: return "custom";
                case SrpProxyResolution::Automatic:
                default: return "automatic";
            }
        }

        const char* ToString(const SrpLogicalRegion value)
        {
            switch (value)
            {
                case SrpLogicalRegion::PositiveX: return "positive_x";
                case SrpLogicalRegion::NegativeX: return "negative_x";
                case SrpLogicalRegion::PositiveY: return "positive_y";
                case SrpLogicalRegion::NegativeY: return "negative_y";
                case SrpLogicalRegion::PositiveZ: return "positive_z";
                case SrpLogicalRegion::NegativeZ: return "negative_z";
                case SrpLogicalRegion::CylinderSide: return "cylinder_side";
                case SrpLogicalRegion::CylinderPositiveCap:
                    return "cylinder_positive_cap";
                case SrpLogicalRegion::CylinderNegativeCap:
                    return "cylinder_negative_cap";
                case SrpLogicalRegion::None:
                default: return "none";
            }
        }

        const char* ToString(const ScalarProfileSource value)
        {
            return value == ScalarProfileSource::Csv ? "csv" : "constant";
        }

        const char* ToString(const ThrusterMode value)
        {
            return value == ThrusterMode::Commanded ? "commanded" : "prescribed_profile";
        }

        const char* ToString(const ThrusterTimeMode value)
        {
            return value == ThrusterTimeMode::Utc ? "utc" : "elapsed";
        }

        const char* ToString(const ControlMode value)
        {
            return value == ControlMode::CompiledUserController
                ? "compiled_user_controller"
                : "none";
        }

        const char* ToString(const AtmosphereModel value)
        {
            return value == AtmosphereModel::CubicHarrisPriesterEarth
                ? "cubic_harris_priester_earth"
                : "uploaded_profile";
        }

        const char* ToString(const AerodynamicInterpolation value)
        {
            return value == AerodynamicInterpolation::NearestRow
                ? "nearest_row"
                : "inverse_distance";
        }

        const char* ToString(const AerodynamicExtrapolation value)
        {
            return value == AerodynamicExtrapolation::NearestRow
                ? "nearest_row"
                : "constant_drag_fallback";
        }

        toml::table ScalarProfileTable(const ScalarProfile& profile)
        {
            toml::table table;
            table.insert("source", ToString(profile.source));
            table.insert("constant_value", profile.constant_value);
            table.insert("csv_file", profile.csv_file_path);
            return table;
        }
    }

    bool ParseScenarioText(
        const std::string& text,
        ScenarioDocument& document,
        Diagnostics& diagnostics,
        const std::string& source_name)
    {
        diagnostics.clear();
        document = ScenarioDocument{};

        toml::table root;
#if TOML_EXCEPTIONS
        try
        {
            root = toml::parse(text, source_name);
        }
        catch (const toml::parse_error& error)
        {
            diagnostics.push_back({
                DiagnosticSeverity::Error,
                "TGSCN-SYNTAX",
                {},
                std::string(error.description()),
                static_cast<std::size_t>(error.source().begin.line),
                static_cast<std::size_t>(error.source().begin.column)});
            return false;
        }
#else
        toml::parse_result parsed = toml::parse(text, source_name);
        if (!parsed)
        {
            const toml::parse_error& error = parsed.error();
            diagnostics.push_back({
                DiagnosticSeverity::Error,
                "TGSCN-SYNTAX",
                {},
                std::string(error.description()),
                static_cast<std::size_t>(error.source().begin.line),
                static_cast<std::size_t>(error.source().begin.column)});
            return false;
        }
        root = std::move(parsed).table();
#endif

        CheckKnownKeys(
            root, {}, diagnostics,
            {"format", "generator", "scenario",
             "initial_state", "components", "thrusters", "reaction_wheels",
             "control", "gravity", "srp", "atmosphere", "aerodynamics"});

        const std::string format = ReadString(
            root, "format", {}, "format", diagnostics, true);
        if (!format.empty() && format != "TGSCN")
        {
            diagnostics.push_back({DiagnosticSeverity::Error, "TGSCN-FORMAT",
                "format", "Expected format = \"TGSCN\"."});
        }
        document.generator = ReadString(
            root, "generator", {}, "generator", diagnostics);

        if (const toml::table* table = ReadTable(
                root, "scenario", "scenario", diagnostics, true))
        {
            CheckKnownKeys(
                *table, "scenario", diagnostics,
                {"name", "start_utc", "end_mode", "final_utc",
                 "duration_seconds", "integrator",
                 "maximum_integrator_step_seconds",
                 "initial_integrator_step_seconds", "absolute_tolerance",
                 "relative_tolerance", "output_mode", "output_step_seconds",
                 "maximum_integration_steps", "maximum_output_samples",
                 "maximum_wall_clock_runtime_seconds",
                 "mass_flow_convention"});
            ScenarioSolver& value = document.scenario;
            value.name = ReadString(*table, "name", value.name,
                "scenario.name", diagnostics, true);
            value.start_utc = ReadString(*table, "start_utc", value.start_utc,
                "scenario.start_utc", diagnostics, true);
            value.end_mode = ReadEnum(*table, "end_mode", value.end_mode,
                "scenario.end_mode", diagnostics,
                {{"final_utc", EndMode::FinalUtc}, {"duration", EndMode::Duration}});
            value.final_utc = ReadString(*table, "final_utc", value.final_utc,
                "scenario.final_utc", diagnostics);
            value.duration_seconds = ReadNumber(*table, "duration_seconds",
                value.duration_seconds, "scenario.duration_seconds", diagnostics);
            value.integrator = ReadEnum(*table, "integrator", value.integrator,
                "scenario.integrator", diagnostics,
                {{"fixed_step_rk4", IntegratorKind::FixedStepRK4},
                 {"adaptive_dormand_prince_54",
                  IntegratorKind::AdaptiveDormandPrince54}});
            value.maximum_integrator_step_seconds = ReadNumber(
                *table, "maximum_integrator_step_seconds",
                value.maximum_integrator_step_seconds,
                "scenario.maximum_integrator_step_seconds", diagnostics);
            value.initial_integrator_step_seconds = ReadNumber(
                *table, "initial_integrator_step_seconds",
                value.initial_integrator_step_seconds,
                "scenario.initial_integrator_step_seconds", diagnostics);
            value.absolute_tolerance = ReadNumber(*table, "absolute_tolerance",
                value.absolute_tolerance, "scenario.absolute_tolerance", diagnostics);
            value.relative_tolerance = ReadNumber(*table, "relative_tolerance",
                value.relative_tolerance, "scenario.relative_tolerance", diagnostics);
            value.output_mode = ReadEnum(*table, "output_mode", value.output_mode,
                "scenario.output_mode", diagnostics,
                {{"every_integrator_step", OutputMode::EveryIntegratorStep},
                 {"fixed_interval", OutputMode::FixedInterval}});
            value.output_step_seconds = ReadNumber(*table, "output_step_seconds",
                value.output_step_seconds, "scenario.output_step_seconds", diagnostics);
            value.maximum_integration_steps = ReadSize(
                *table, "maximum_integration_steps", value.maximum_integration_steps,
                "scenario.maximum_integration_steps", diagnostics);
            value.maximum_output_samples = ReadSize(
                *table, "maximum_output_samples", value.maximum_output_samples,
                "scenario.maximum_output_samples", diagnostics);
            value.maximum_wall_clock_runtime_seconds = ReadNumber(
                *table, "maximum_wall_clock_runtime_seconds",
                value.maximum_wall_clock_runtime_seconds,
                "scenario.maximum_wall_clock_runtime_seconds", diagnostics);
            value.mass_flow_convention = ReadEnum(
                *table, "mass_flow_convention", value.mass_flow_convention,
                "scenario.mass_flow_convention", diagnostics,
                {{"thrust_includes_exhaust_momentum",
                  MassFlowConvention::ThrustIncludesExhaustMomentum},
                 {"momentum_derivative", MassFlowConvention::MomentumDerivative}});
        }

        if (const toml::table* table = ReadTable(
                root, "initial_state", "initial_state", diagnostics, true))
        {
            CheckKnownKeys(
                *table, "initial_state", diagnostics,
                {"position_icrf_m", "velocity_icrf_mps",
                 "attitude_body_to_icrf", "angular_velocity_body_radps",
                 "authoring_frame"});
            InitialState& value = document.initial_state;
            value.position_icrf_m = ReadVec3(*table, "position_icrf_m",
                value.position_icrf_m, "initial_state.position_icrf_m", diagnostics);
            value.velocity_icrf_mps = ReadVec3(*table, "velocity_icrf_mps",
                value.velocity_icrf_mps, "initial_state.velocity_icrf_mps", diagnostics);
            value.attitude_body_to_icrf = ReadQuat(*table, "attitude_body_to_icrf",
                value.attitude_body_to_icrf,
                "initial_state.attitude_body_to_icrf", diagnostics);
            value.angular_velocity_body_radps = ReadVec3(
                *table, "angular_velocity_body_radps",
                value.angular_velocity_body_radps,
                "initial_state.angular_velocity_body_radps", diagnostics);
            value.authoring_frame = ReadString(
                *table, "authoring_frame", value.authoring_frame,
                "initial_state.authoring_frame", diagnostics);
        }

        if (const toml::array* array = ReadArray(
                root, "components", "components", diagnostics))
        {
            document.components.reserve(array->size());
            for (std::size_t index = 0; index < array->size(); ++index)
            {
                const std::string path = "components[" + std::to_string(index) + "]";
                const toml::table* table = array->get(index)->as_table();
                if (table == nullptr)
                {
                    AddTypeError(diagnostics, path, "a table");
                    continue;
                }
                CheckKnownKeys(
                    *table, path, diagnostics,
                    {"id", "name", "initial_mass_kg", "minimum_mass_kg",
                     "variable_mass",
                     "local_center_of_mass_m", "origin_body_m",
                     "component_to_body", "parent_component",
                     "parent_anchor_m", "child_anchor_m",
                     "child_to_parent_zero_orientation", "inertia", "dofs",
                     "visual", "srp"});
                Component value;
                value.id = ReadString(*table, "id", {}, path + ".id", diagnostics);
                value.name = ReadString(*table, "name", {}, path + ".name", diagnostics, true);
                value.initial_mass_kg = ReadNumber(*table, "initial_mass_kg", 0.0,
                    path + ".initial_mass_kg", diagnostics);
                value.minimum_mass_kg = ReadNumber(*table, "minimum_mass_kg", 0.0,
                    path + ".minimum_mass_kg", diagnostics);
                value.variable_mass = ReadBool(*table, "variable_mass", false,
                    path + ".variable_mass", diagnostics);
                value.local_center_of_mass_m = ReadVec3(
                    *table, "local_center_of_mass_m", {},
                    path + ".local_center_of_mass_m", diagnostics);
                value.origin_body_m = ReadVec3(*table, "origin_body_m", {},
                    path + ".origin_body_m", diagnostics);
                value.component_to_body = ReadQuat(*table, "component_to_body", {},
                    path + ".component_to_body", diagnostics);
                value.parent_component_name = ReadString(
                    *table, "parent_component", {}, path + ".parent_component", diagnostics);
                value.parent_anchor_m = ReadVec3(*table, "parent_anchor_m", {},
                    path + ".parent_anchor_m", diagnostics);
                value.child_anchor_m = ReadVec3(*table, "child_anchor_m", {},
                    path + ".child_anchor_m", diagnostics);
                value.child_to_parent_zero_orientation = ReadQuat(
                    *table, "child_to_parent_zero_orientation", {},
                    path + ".child_to_parent_zero_orientation", diagnostics);

                if (const toml::table* inertia = ReadTable(
                        *table, "inertia", path + ".inertia", diagnostics))
                {
                    CheckKnownKeys(
                        *inertia, path + ".inertia", diagnostics,
                        {"ixx_kgm2", "iyy_kgm2", "izz_kgm2", "ixy_kgm2",
                         "ixz_kgm2", "iyz_kgm2"});
                    value.centroidal_inertia.ixx_kgm2 = ReadNumber(
                        *inertia, "ixx_kgm2", 0.0, path + ".inertia.ixx_kgm2", diagnostics);
                    value.centroidal_inertia.iyy_kgm2 = ReadNumber(
                        *inertia, "iyy_kgm2", 0.0, path + ".inertia.iyy_kgm2", diagnostics);
                    value.centroidal_inertia.izz_kgm2 = ReadNumber(
                        *inertia, "izz_kgm2", 0.0, path + ".inertia.izz_kgm2", diagnostics);
                    value.centroidal_inertia.ixy_kgm2 = ReadNumber(
                        *inertia, "ixy_kgm2", 0.0, path + ".inertia.ixy_kgm2", diagnostics);
                    value.centroidal_inertia.ixz_kgm2 = ReadNumber(
                        *inertia, "ixz_kgm2", 0.0, path + ".inertia.ixz_kgm2", diagnostics);
                    value.centroidal_inertia.iyz_kgm2 = ReadNumber(
                        *inertia, "iyz_kgm2", 0.0, path + ".inertia.iyz_kgm2", diagnostics);
                }

                if (const toml::array* dofs = ReadArray(
                        *table, "dofs", path + ".dofs", diagnostics))
                {
                    for (std::size_t dof_index = 0; dof_index < dofs->size(); ++dof_index)
                    {
                        const std::string dof_path = path + ".dofs[" +
                            std::to_string(dof_index) + "]";
                        const toml::table* dof_table = dofs->get(dof_index)->as_table();
                        if (dof_table == nullptr)
                        {
                            AddTypeError(diagnostics, dof_path, "a table");
                            continue;
                        }
                        CheckKnownKeys(
                            *dof_table, dof_path, diagnostics,
                            {"id", "name", "motion", "axis",
                             "initial_coordinate", "initial_rate",
                             "minimum_coordinate", "maximum_coordinate",
                             "maximum_absolute_rate",
                             "maximum_absolute_effort"});
                        JointDof dof;
                        dof.id = ReadString(*dof_table, "id", {}, dof_path + ".id", diagnostics);
                        dof.name = ReadString(*dof_table, "name", {}, dof_path + ".name", diagnostics);
                        dof.motion = ReadEnum(
                            *dof_table, "motion", dof.motion, dof_path + ".motion", diagnostics,
                            {{"rotation", JointMotion::Rotation},
                             {"translation", JointMotion::Translation}});
                        dof.axis = ReadVec3(*dof_table, "axis", dof.axis,
                            dof_path + ".axis", diagnostics);
                        dof.initial_coordinate = ReadNumber(
                            *dof_table, "initial_coordinate", 0.0,
                            dof_path + ".initial_coordinate", diagnostics);
                        dof.initial_rate = ReadNumber(*dof_table, "initial_rate", 0.0,
                            dof_path + ".initial_rate", diagnostics);
                        dof.minimum_coordinate = ReadOptionalNumber(
                            *dof_table, "minimum_coordinate",
                            dof_path + ".minimum_coordinate", diagnostics);
                        dof.maximum_coordinate = ReadOptionalNumber(
                            *dof_table, "maximum_coordinate",
                            dof_path + ".maximum_coordinate", diagnostics);
                        dof.maximum_absolute_rate = ReadNumber(
                            *dof_table, "maximum_absolute_rate", dof.maximum_absolute_rate,
                            dof_path + ".maximum_absolute_rate", diagnostics);
                        dof.maximum_absolute_effort = ReadNumber(
                            *dof_table, "maximum_absolute_effort", dof.maximum_absolute_effort,
                            dof_path + ".maximum_absolute_effort", diagnostics);

                        if (dof.motion == JointMotion::Rotation)
                        {
                            dof.initial_coordinate =
                                RotationalValueFromFileUnits(
                                    dof.initial_coordinate);
                            dof.initial_rate =
                                RotationalValueFromFileUnits(
                                    dof.initial_rate);
                            if (dof.minimum_coordinate)
                            {
                                *dof.minimum_coordinate =
                                    RotationalValueFromFileUnits(
                                        *dof.minimum_coordinate);
                            }
                            if (dof.maximum_coordinate)
                            {
                                *dof.maximum_coordinate =
                                    RotationalValueFromFileUnits(
                                        *dof.maximum_coordinate);
                            }
                            dof.maximum_absolute_rate =
                                RotationalValueFromFileUnits(
                                    dof.maximum_absolute_rate);
                        }

                        value.degrees_of_freedom.push_back(std::move(dof));
                    }
                }

                if (const toml::table* visual = ReadTable(
                        *table, "visual", path + ".visual", diagnostics))
                {
                    CheckKnownKeys(
                        *visual, path + ".visual", diagnostics,
                        {"geometry_source", "primitive_type",
                         "box_dimensions_m", "sphere_radius_m",
                         "cylinder_radius_m", "cylinder_length_m", "stl_file",
                         "stl_length_unit", "stl_recenter_mode",
                         "visual_offset_m", "visual_orientation", "visual_scale",
                         "surface_appearance", "display_color", "base_color_tint",
                         "base_color_texture_file", "normal_texture_file",
                         "roughness_texture_file", "metallic_texture_file",
                         "visible"});
                    ComponentVisual& v = value.visual;
                    v.geometry_source = ReadEnum(
                        *visual, "geometry_source", v.geometry_source,
                        path + ".visual.geometry_source", diagnostics,
                        {{"none", GeometrySource::None},
                         {"primitive", GeometrySource::Primitive},
                         {"custom_stl", GeometrySource::CustomStl}});
                    v.primitive_type = ReadEnum(
                        *visual, "primitive_type", v.primitive_type,
                        path + ".visual.primitive_type", diagnostics,
                        {{"box", PrimitiveGeometry::Box},
                         {"sphere", PrimitiveGeometry::Sphere},
                         {"cylinder", PrimitiveGeometry::Cylinder}});
                    v.box_dimensions_m = ReadVec3(*visual, "box_dimensions_m",
                        v.box_dimensions_m, path + ".visual.box_dimensions_m", diagnostics);
                    v.sphere_radius_m = ReadNumber(*visual, "sphere_radius_m",
                        v.sphere_radius_m, path + ".visual.sphere_radius_m", diagnostics);
                    v.cylinder_radius_m = ReadNumber(*visual, "cylinder_radius_m",
                        v.cylinder_radius_m, path + ".visual.cylinder_radius_m", diagnostics);
                    v.cylinder_length_m = ReadNumber(*visual, "cylinder_length_m",
                        v.cylinder_length_m, path + ".visual.cylinder_length_m", diagnostics);
                    v.stl_file_path = ReadString(*visual, "stl_file", {},
                        path + ".visual.stl_file", diagnostics);
                    v.stl_length_unit = ReadEnum(
                        *visual, "stl_length_unit", v.stl_length_unit,
                        path + ".visual.stl_length_unit", diagnostics,
                        {{"millimeters", StlLengthUnit::Millimeters},
                         {"centimeters", StlLengthUnit::Centimeters},
                         {"meters", StlLengthUnit::Meters}});
                    v.stl_recenter_mode = ReadEnum(
                        *visual, "stl_recenter_mode", v.stl_recenter_mode,
                        path + ".visual.stl_recenter_mode", diagnostics,
                        {{"keep_imported_origin", StlRecenterMode::KeepImportedOrigin},
                         {"center_on_bounds", StlRecenterMode::CenterOnBounds},
                         {"place_base_at_origin", StlRecenterMode::PlaceBaseAtOrigin}});
                    v.visual_offset_m = ReadVec3(*visual, "visual_offset_m", {},
                        path + ".visual.visual_offset_m", diagnostics);
                    v.visual_orientation = ReadQuat(*visual, "visual_orientation", {},
                        path + ".visual.visual_orientation", diagnostics);
                    v.visual_scale = ReadVec3(*visual, "visual_scale", v.visual_scale,
                        path + ".visual.visual_scale", diagnostics);
                    v.surface_appearance = ReadEnum(
                        *visual, "surface_appearance", v.surface_appearance,
                        path + ".visual.surface_appearance", diagnostics,
                        {{"solid_color", SurfaceAppearance::SolidColor},
                         {"textured", SurfaceAppearance::Textured}});
                    v.display_color = ReadColor(*visual, "display_color", v.display_color,
                        path + ".visual.display_color", diagnostics);
                    v.base_color_tint = ReadColor(*visual, "base_color_tint", v.base_color_tint,
                        path + ".visual.base_color_tint", diagnostics);
                    v.base_color_texture_file_path = ReadString(
                        *visual, "base_color_texture_file", {},
                        path + ".visual.base_color_texture_file", diagnostics);
                    v.normal_texture_file_path = ReadString(
                        *visual, "normal_texture_file", {},
                        path + ".visual.normal_texture_file", diagnostics);
                    v.roughness_texture_file_path = ReadString(
                        *visual, "roughness_texture_file", {},
                        path + ".visual.roughness_texture_file", diagnostics);
                    v.metallic_texture_file_path = ReadString(
                        *visual, "metallic_texture_file", {},
                        path + ".visual.metallic_texture_file", diagnostics);
                    v.visible = ReadBool(*visual, "visible", v.visible,
                        path + ".visual.visible", diagnostics);
                }

                if (const toml::table* srp = ReadTable(
                        *table, "srp", path + ".srp", diagnostics))
                {
                    CheckKnownKeys(
                        *srp, path + ".srp", diagnostics,
                        {"included_in_proxy", "proxy_resolution",
                         "custom_target_triangle_count",
                         "use_global_fallback_optics",
                         "one_optical_configuration", "optics",
                         "generated_geometry_signature",
                         "generated_triangle_count", "proxy_generation_required",
                         "last_proxy_generation_message",
                         "logical_region_overrides", "triangle_overrides"});
                    ComponentSrpAuthoring& s = value.srp;
                    s.included_in_proxy = ReadBool(*srp, "included_in_proxy",
                        s.included_in_proxy, path + ".srp.included_in_proxy", diagnostics);
                    s.proxy_resolution = ReadEnum(
                        *srp, "proxy_resolution", s.proxy_resolution,
                        path + ".srp.proxy_resolution", diagnostics,
                        {{"automatic", SrpProxyResolution::Automatic},
                         {"custom", SrpProxyResolution::Custom}});
                    s.custom_target_triangle_count = static_cast<int>(ReadInteger(
                        *srp, "custom_target_triangle_count", s.custom_target_triangle_count,
                        path + ".srp.custom_target_triangle_count", diagnostics));
                    s.use_global_fallback_optical_properties = ReadBool(
                        *srp, "use_global_fallback_optics",
                        s.use_global_fallback_optical_properties,
                        path + ".srp.use_global_fallback_optics", diagnostics);
                    s.apply_one_optical_configuration_to_entire_component = ReadBool(
                        *srp, "one_optical_configuration",
                        s.apply_one_optical_configuration_to_entire_component,
                        path + ".srp.one_optical_configuration", diagnostics);
                    if (const toml::table* optics = ReadTable(
                            *srp, "optics", path + ".srp.optics", diagnostics))
                        s.component_optical_properties = ReadOptics(
                            *optics, path + ".srp.optics", diagnostics,
                            s.component_optical_properties);
                    s.generated_geometry_signature = ReadString(
                        *srp, "generated_geometry_signature", {},
                        path + ".srp.generated_geometry_signature", diagnostics);
                    s.generated_triangle_count = static_cast<int>(ReadInteger(
                        *srp, "generated_triangle_count", 0,
                        path + ".srp.generated_triangle_count", diagnostics));
                    s.proxy_generation_required = ReadBool(
                        *srp, "proxy_generation_required", true,
                        path + ".srp.proxy_generation_required", diagnostics);
                    s.last_proxy_generation_message = ReadString(
                        *srp, "last_proxy_generation_message", {},
                        path + ".srp.last_proxy_generation_message", diagnostics);

                    if (const toml::array* overrides = ReadArray(
                            *srp, "logical_region_overrides",
                            path + ".srp.logical_region_overrides", diagnostics))
                    {
                        for (std::size_t override_index = 0;
                             override_index < overrides->size(); ++override_index)
                        {
                            const toml::table* override_table =
                                overrides->get(override_index)->as_table();
                            const std::string override_path =
                                path + ".srp.logical_region_overrides[" +
                                std::to_string(override_index) + "]";
                            if (override_table == nullptr)
                            {
                                AddTypeError(diagnostics, override_path, "a table");
                                continue;
                            }
                            CheckKnownKeys(
                                *override_table, override_path, diagnostics,
                                {"region", "optics"});
                            SrpLogicalRegionOverride override_value;
                            override_value.region = ReadSrpRegion(
                                *override_table, "region", override_value.region,
                                override_path + ".region", diagnostics);
                            if (const toml::table* optics = ReadTable(
                                    *override_table, "optics",
                                    override_path + ".optics", diagnostics))
                                override_value.optical_properties = ReadOptics(
                                    *optics, override_path + ".optics", diagnostics);
                            s.logical_region_overrides.push_back(std::move(override_value));
                        }
                    }
                    if (const toml::array* overrides = ReadArray(
                            *srp, "triangle_overrides",
                            path + ".srp.triangle_overrides", diagnostics))
                    {
                        for (std::size_t override_index = 0;
                             override_index < overrides->size(); ++override_index)
                        {
                            const toml::table* override_table =
                                overrides->get(override_index)->as_table();
                            const std::string override_path =
                                path + ".srp.triangle_overrides[" +
                                std::to_string(override_index) + "]";
                            if (override_table == nullptr)
                            {
                                AddTypeError(diagnostics, override_path, "a table");
                                continue;
                            }
                            CheckKnownKeys(
                                *override_table, override_path, diagnostics,
                                {"triangle_index", "optics"});
                            SrpTriangleOverride override_value;
                            override_value.proxy_triangle_index = static_cast<int>(ReadInteger(
                                *override_table, "triangle_index", -1,
                                override_path + ".triangle_index", diagnostics));
                            if (const toml::table* optics = ReadTable(
                                    *override_table, "optics",
                                    override_path + ".optics", diagnostics))
                                override_value.optical_properties = ReadOptics(
                                    *optics, override_path + ".optics", diagnostics);
                            s.triangle_overrides.push_back(std::move(override_value));
                        }
                    }
                }
                document.components.push_back(std::move(value));
            }
        }

        if (const toml::array* array = ReadArray(root, "thrusters", "thrusters", diagnostics))
        {
            for (std::size_t index = 0; index < array->size(); ++index)
            {
                const std::string path = "thrusters[" + std::to_string(index) + "]";
                const toml::table* table = array->get(index)->as_table();
                if (table == nullptr)
                {
                    AddTypeError(diagnostics, path, "a table");
                    continue;
                }
                CheckKnownKeys(
                    *table, path, diagnostics,
                    {"name", "mode", "mount_component", "propellant_component",
                     "application_point_component_m", "direction_component",
                     "ignition_time_mode", "ignition_utc",
                     "ignition_elapsed_seconds", "never_shuts_down",
                     "shutdown_time_mode", "shutdown_utc",
                     "shutdown_elapsed_seconds", "maximum_thrust_n",
                     "prescribed_thrust", "prescribed_specific_impulse"});
                Thruster value;
                value.name = ReadString(*table, "name", {}, path + ".name", diagnostics, true);
                value.mode = ReadEnum(
                    *table, "mode", value.mode, path + ".mode", diagnostics,
                    {{"prescribed_profile", ThrusterMode::PrescribedProfile},
                     {"commanded", ThrusterMode::Commanded}});
                value.mount_component_name = ReadString(
                    *table, "mount_component", {}, path + ".mount_component", diagnostics);
                value.propellant_component_name = ReadString(
                    *table, "propellant_component", {},
                    path + ".propellant_component", diagnostics);
                value.application_point_component_m = ReadVec3(
                    *table, "application_point_component_m", {},
                    path + ".application_point_component_m", diagnostics);
                value.direction_component = ReadVec3(
                    *table, "direction_component", value.direction_component,
                    path + ".direction_component", diagnostics);
                value.ignition_time_mode = ReadEnum(
                    *table, "ignition_time_mode", value.ignition_time_mode,
                    path + ".ignition_time_mode", diagnostics,
                    {{"utc", ThrusterTimeMode::Utc},
                     {"elapsed", ThrusterTimeMode::Elapsed}});
                value.ignition_utc = ReadString(
                    *table, "ignition_utc", {}, path + ".ignition_utc", diagnostics);
                value.ignition_elapsed_seconds = ReadNumber(
                    *table, "ignition_elapsed_seconds", 0.0,
                    path + ".ignition_elapsed_seconds", diagnostics);
                value.never_shuts_down = ReadBool(
                    *table, "never_shuts_down", false,
                    path + ".never_shuts_down", diagnostics);
                value.shutdown_time_mode = ReadEnum(
                    *table, "shutdown_time_mode", value.shutdown_time_mode,
                    path + ".shutdown_time_mode", diagnostics,
                    {{"utc", ThrusterTimeMode::Utc},
                     {"elapsed", ThrusterTimeMode::Elapsed}});
                value.shutdown_utc = ReadString(
                    *table, "shutdown_utc", {}, path + ".shutdown_utc", diagnostics);
                value.shutdown_elapsed_seconds = ReadNumber(
                    *table, "shutdown_elapsed_seconds", 0.0,
                    path + ".shutdown_elapsed_seconds", diagnostics);
                value.maximum_thrust_n = ReadNumber(
                    *table, "maximum_thrust_n", 0.0,
                    path + ".maximum_thrust_n", diagnostics);
                if (const toml::table* profile = ReadTable(
                        *table, "prescribed_thrust", path + ".prescribed_thrust", diagnostics))
                    value.prescribed_thrust = ReadScalarProfile(
                        *profile, path + ".prescribed_thrust", diagnostics);
                if (const toml::table* profile = ReadTable(
                        *table, "prescribed_specific_impulse",
                        path + ".prescribed_specific_impulse", diagnostics))
                    value.prescribed_specific_impulse = ReadScalarProfile(
                        *profile, path + ".prescribed_specific_impulse", diagnostics);
                document.thrusters.push_back(std::move(value));
            }
        }

        if (const toml::array* array = ReadArray(
                root, "reaction_wheels", "reaction_wheels", diagnostics))
        {
            for (std::size_t index = 0; index < array->size(); ++index)
            {
                const std::string path =
                    "reaction_wheels[" + std::to_string(index) + "]";
                const toml::table* table = array->get(index)->as_table();
                if (table == nullptr)
                {
                    AddTypeError(diagnostics, path, "a table");
                    continue;
                }
                CheckKnownKeys(
                    *table, path, diagnostics,
                    {"name", "mount_component", "axis_component",
                     "initial_momentum_nms", "maximum_absolute_momentum_nms"});
                ReactionWheel value;
                value.name = ReadString(*table, "name", {}, path + ".name", diagnostics);
                value.mount_component_name = ReadString(
                    *table, "mount_component", {}, path + ".mount_component", diagnostics);
                value.axis_component = ReadVec3(
                    *table, "axis_component", value.axis_component,
                    path + ".axis_component", diagnostics);
                value.initial_momentum_nms = ReadNumber(
                    *table, "initial_momentum_nms", 0.0,
                    path + ".initial_momentum_nms", diagnostics);
                value.maximum_absolute_momentum_nms = ReadNumber(
                    *table, "maximum_absolute_momentum_nms", 0.0,
                    path + ".maximum_absolute_momentum_nms", diagnostics);
                document.reaction_wheels.push_back(std::move(value));
            }
        }

        if (const toml::table* table = ReadTable(
                root, "control", "control", diagnostics))
        {
            CheckKnownKeys(
                *table, "control", diagnostics,
                {"mode", "unreal_controller_id", "controller_dll_file"});
            document.control.mode = ReadEnum(
                *table, "mode", document.control.mode, "control.mode", diagnostics,
                {{"none", ControlMode::None},
                 {"compiled_user_controller", ControlMode::CompiledUserController}});
            document.control.unreal_controller_id = ReadString(
                *table, "unreal_controller_id", {},
                "control.unreal_controller_id", diagnostics);
            document.control.controller_dll_path = ReadString(
                *table, "controller_dll_file", {},
                "control.controller_dll_file", diagnostics);
        }

        if (const toml::table* table = ReadTable(
                root, "gravity", "gravity", diagnostics))
        {
            CheckKnownKeys(
                *table, "gravity", diagnostics,
                {"include_first_post_newtonian_correction", "bodies"});
            document.include_first_post_newtonian_correction = ReadBool(
                *table, "include_first_post_newtonian_correction", false,
                "gravity.include_first_post_newtonian_correction", diagnostics);
            if (const toml::array* bodies = ReadArray(
                    *table, "bodies", "gravity.bodies", diagnostics))
            {
                for (std::size_t index = 0; index < bodies->size(); ++index)
                {
                    const std::string path =
                        "gravity.bodies[" + std::to_string(index) + "]";
                    const toml::table* body_table = bodies->get(index)->as_table();
                    if (body_table == nullptr)
                    {
                        AddTypeError(diagnostics, path, "a table");
                        continue;
                    }
                    CheckKnownKeys(
                        *body_table, path, diagnostics,
                        {"catalog_key", "gravity_enabled",
                         "automatic_activation_radius_m",
                         "barycenter_resolution_radius_m",
                         "harmonic_model_csv_file", "maximum_harmonic_degree"});
                    CelestialBody value;
                    value.catalog_key = ReadString(
                        *body_table, "catalog_key", {}, path + ".catalog_key", diagnostics);
                    value.gravity_enabled = ReadBool(
                        *body_table, "gravity_enabled", false,
                        path + ".gravity_enabled", diagnostics);
                    value.automatic_activation_radius_m = ReadNumber(
                        *body_table, "automatic_activation_radius_m", 0.0,
                        path + ".automatic_activation_radius_m", diagnostics);
                    value.barycenter_resolution_radius_m = ReadNumber(
                        *body_table, "barycenter_resolution_radius_m", 0.0,
                        path + ".barycenter_resolution_radius_m", diagnostics);
                    value.harmonic_model_csv_file_path = ReadString(
                        *body_table, "harmonic_model_csv_file", {},
                        path + ".harmonic_model_csv_file", diagnostics);
                    value.maximum_harmonic_degree = static_cast<int>(ReadInteger(
                        *body_table, "maximum_harmonic_degree", 0,
                        path + ".maximum_harmonic_degree", diagnostics));
                    document.celestial_bodies.push_back(std::move(value));
                }
            }
        }

        if (const toml::table* table = ReadTable(root, "srp", "srp", diagnostics))
        {
            CheckKnownKeys(
                *table, "srp", diagnostics,
                {"enabled", "sun_body", "pressure_at_one_au_pa",
                 "compute_eclipse", "occulting_bodies",
                 "compute_component_shadows", "global_fallback_optics",
                 "facets"});
            SolarRadiationPressure& value = document.solar_radiation_pressure;
            value.enabled = ReadBool(*table, "enabled", value.enabled,
                "srp.enabled", diagnostics);
            value.sun_body_name = ReadString(*table, "sun_body", value.sun_body_name,
                "srp.sun_body", diagnostics);
            value.pressure_at_one_au_pa = ReadNumber(
                *table, "pressure_at_one_au_pa", value.pressure_at_one_au_pa,
                "srp.pressure_at_one_au_pa", diagnostics);
            value.compute_eclipse = ReadBool(*table, "compute_eclipse",
                value.compute_eclipse, "srp.compute_eclipse", diagnostics);
            value.occulting_body_names = ReadStringVector(
                *table, "occulting_bodies", "srp.occulting_bodies", diagnostics);
            value.compute_component_shadows = ReadBool(
                *table, "compute_component_shadows", value.compute_component_shadows,
                "srp.compute_component_shadows", diagnostics);
            if (const toml::table* optics = ReadTable(
                    *table, "global_fallback_optics",
                    "srp.global_fallback_optics", diagnostics))
                value.global_fallback_optical_properties = ReadOptics(
                    *optics, "srp.global_fallback_optics", diagnostics,
                    value.global_fallback_optical_properties);
            if (const toml::array* facets = ReadArray(
                    *table, "facets", "srp.facets", diagnostics))
            {
                for (std::size_t index = 0; index < facets->size(); ++index)
                {
                    const std::string path = "srp.facets[" + std::to_string(index) + "]";
                    const toml::table* facet_table = facets->get(index)->as_table();
                    if (facet_table == nullptr)
                    {
                        AddTypeError(diagnostics, path, "a table");
                        continue;
                    }
                    CheckKnownKeys(
                        *facet_table, path, diagnostics,
                        {"name", "component_id", "component_name",
                         "stable_triangle_index", "logical_region",
                         "vertex0_component_m", "vertex1_component_m",
                         "vertex2_component_m", "optics"});
                    OpticalFacet facet;
                    facet.name = ReadString(*facet_table, "name", {}, path + ".name", diagnostics);
                    facet.component_id = ReadString(
                        *facet_table, "component_id", {}, path + ".component_id", diagnostics);
                    facet.component_name = ReadString(
                        *facet_table, "component_name", {}, path + ".component_name", diagnostics);
                    facet.stable_triangle_index = static_cast<int>(ReadInteger(
                        *facet_table, "stable_triangle_index", -1,
                        path + ".stable_triangle_index", diagnostics));
                    facet.logical_region = ReadSrpRegion(
                        *facet_table, "logical_region", facet.logical_region,
                        path + ".logical_region", diagnostics);
                    facet.vertex0_component_m = ReadVec3(
                        *facet_table, "vertex0_component_m", {},
                        path + ".vertex0_component_m", diagnostics);
                    facet.vertex1_component_m = ReadVec3(
                        *facet_table, "vertex1_component_m", {},
                        path + ".vertex1_component_m", diagnostics);
                    facet.vertex2_component_m = ReadVec3(
                        *facet_table, "vertex2_component_m", {},
                        path + ".vertex2_component_m", diagnostics);
                    if (const toml::table* optics = ReadTable(
                            *facet_table, "optics", path + ".optics", diagnostics))
                        facet.optical_properties = ReadOptics(
                            *optics, path + ".optics", diagnostics);
                    value.optical_facets.push_back(std::move(facet));
                }
            }
        }

        if (const toml::table* table = ReadTable(
                root, "atmosphere", "atmosphere", diagnostics))
        {
            CheckKnownKeys(
                *table, "atmosphere", diagnostics,
                {"enabled", "central_body", "model",
                 "general_profile_csv_file", "centered_average_f107_sfu",
                 "chp_coefficient_csv_file",
                 "chp_molecular_profile_csv_file"});
            Atmosphere& value = document.atmosphere;
            value.enabled = ReadBool(*table, "enabled", value.enabled,
                "atmosphere.enabled", diagnostics);
            value.central_body_name = ReadString(
                *table, "central_body", value.central_body_name,
                "atmosphere.central_body", diagnostics);
            value.model = ReadEnum(
                *table, "model", value.model, "atmosphere.model", diagnostics,
                {{"uploaded_profile", AtmosphereModel::UploadedProfile},
                 {"cubic_harris_priester_earth",
                  AtmosphereModel::CubicHarrisPriesterEarth}});
            value.general_profile_csv_path = ReadString(
                *table, "general_profile_csv_file", {},
                "atmosphere.general_profile_csv_file", diagnostics);
            value.centered_average_f107_sfu = ReadNumber(
                *table, "centered_average_f107_sfu", 0.0,
                "atmosphere.centered_average_f107_sfu", diagnostics);
            value.chp_coefficient_csv_path = ReadString(
                *table, "chp_coefficient_csv_file", {},
                "atmosphere.chp_coefficient_csv_file", diagnostics);
            value.chp_molecular_profile_csv_path = ReadString(
                *table, "chp_molecular_profile_csv_file", {},
                "atmosphere.chp_molecular_profile_csv_file", diagnostics);
        }

        if (const toml::table* table = ReadTable(
                root, "aerodynamics", "aerodynamics", diagnostics))
        {
            CheckKnownKeys(
                *table, "aerodynamics", diagnostics,
                {"enabled", "reference_area_m2", "reference_length_m",
                 "minimum_dynamic_pressure_pa",
                 "maximum_valid_dynamic_pressure_pa",
                 "constant_drag_fallback_enabled", "fallback_drag_coefficient",
                 "database"});
            Aerodynamics& value = document.aerodynamics;
            value.enabled = ReadBool(*table, "enabled", value.enabled,
                "aerodynamics.enabled", diagnostics);
            value.reference_area_m2 = ReadNumber(
                *table, "reference_area_m2", value.reference_area_m2,
                "aerodynamics.reference_area_m2", diagnostics);
            value.reference_length_m = ReadNumber(
                *table, "reference_length_m", value.reference_length_m,
                "aerodynamics.reference_length_m", diagnostics);
            value.minimum_dynamic_pressure_pa = ReadNumber(
                *table, "minimum_dynamic_pressure_pa",
                value.minimum_dynamic_pressure_pa,
                "aerodynamics.minimum_dynamic_pressure_pa", diagnostics);
            value.maximum_valid_dynamic_pressure_pa = ReadNumber(
                *table, "maximum_valid_dynamic_pressure_pa",
                value.maximum_valid_dynamic_pressure_pa,
                "aerodynamics.maximum_valid_dynamic_pressure_pa", diagnostics);
            value.constant_drag_fallback_enabled = ReadBool(
                *table, "constant_drag_fallback_enabled",
                value.constant_drag_fallback_enabled,
                "aerodynamics.constant_drag_fallback_enabled", diagnostics);
            value.fallback_drag_coefficient = ReadNumber(
                *table, "fallback_drag_coefficient", value.fallback_drag_coefficient,
                "aerodynamics.fallback_drag_coefficient", diagnostics);
            if (const toml::table* database = ReadTable(
                    *table, "database", "aerodynamics.database", diagnostics))
            {
                CheckKnownKeys(
                    *database, "aerodynamics.database", diagnostics,
                    {"enabled", "csv_file", "interpolation", "extrapolation",
                     "neighbor_count", "inverse_distance_power",
                     "maximum_normalized_neighbor_distance",
                     "moment_reference_center_body_m", "rows"});
                AerodynamicDatabase& db = value.database;
                db.enabled = ReadBool(*database, "enabled", db.enabled,
                    "aerodynamics.database.enabled", diagnostics);
                db.csv_file_path = ReadString(
                    *database, "csv_file", {},
                    "aerodynamics.database.csv_file", diagnostics);
                db.interpolation = ReadEnum(
                    *database, "interpolation", db.interpolation,
                    "aerodynamics.database.interpolation", diagnostics,
                    {{"inverse_distance", AerodynamicInterpolation::InverseDistance},
                     {"nearest_row", AerodynamicInterpolation::NearestRow}});
                db.extrapolation = ReadEnum(
                    *database, "extrapolation", db.extrapolation,
                    "aerodynamics.database.extrapolation", diagnostics,
                    {{"constant_drag_fallback",
                      AerodynamicExtrapolation::ConstantDragFallback},
                     {"nearest_row", AerodynamicExtrapolation::NearestRow}});
                db.neighbor_count = ReadSize(
                    *database, "neighbor_count", db.neighbor_count,
                    "aerodynamics.database.neighbor_count", diagnostics);
                db.inverse_distance_power = ReadNumber(
                    *database, "inverse_distance_power", db.inverse_distance_power,
                    "aerodynamics.database.inverse_distance_power", diagnostics);
                db.maximum_normalized_neighbor_distance = ReadOptionalNumber(
                    *database, "maximum_normalized_neighbor_distance",
                    "aerodynamics.database.maximum_normalized_neighbor_distance",
                    diagnostics);
                db.moment_reference_center_body_m = ReadVec3(
                    *database, "moment_reference_center_body_m", {},
                    "aerodynamics.database.moment_reference_center_body_m", diagnostics);
                if (const toml::array* rows = ReadArray(
                        *database, "rows", "aerodynamics.database.rows", diagnostics))
                {
                    for (std::size_t index = 0; index < rows->size(); ++index)
                    {
                        const std::string path = "aerodynamics.database.rows[" +
                            std::to_string(index) + "]";
                        const toml::table* row_table = rows->get(index)->as_table();
                        if (row_table == nullptr)
                        {
                            AddTypeError(diagnostics, path, "a table");
                            continue;
                        }
                        CheckKnownKeys(
                            *row_table, path, diagnostics,
                            {"speed_ratio", "knudsen_number",
                             "gas_flow_direction_body",
                             "articulation_coordinates",
                             "force_coefficients_body",
                             "moment_coefficients_body"});
                        AerodynamicDatabaseRow row;
                        row.speed_ratio = ReadNumber(
                            *row_table, "speed_ratio", 0.0,
                            path + ".speed_ratio", diagnostics);
                        row.knudsen_number = ReadNumber(
                            *row_table, "knudsen_number", 0.0,
                            path + ".knudsen_number", diagnostics);
                        row.gas_flow_direction_body = ReadVec3(
                            *row_table, "gas_flow_direction_body",
                            row.gas_flow_direction_body,
                            path + ".gas_flow_direction_body", diagnostics);
                        row.articulation_coordinates = ReadNumberVector(
                            *row_table, "articulation_coordinates",
                            path + ".articulation_coordinates", diagnostics);
                        row.force_coefficients_body = ReadVec3(
                            *row_table, "force_coefficients_body", {},
                            path + ".force_coefficients_body", diagnostics);
                        row.moment_coefficients_body = ReadVec3(
                            *row_table, "moment_coefficients_body", {},
                            path + ".moment_coefficients_body", diagnostics);
                        db.rows.push_back(std::move(row));
                    }
                }
            }
        }

        return !HasErrors(diagnostics);
    }

    std::string SerializeScenarioText(const ScenarioDocument& document)
    {
        toml::table root;
        root.insert("format", "TGSCN");
        root.insert("generator", document.generator);

        toml::table scenario;
        scenario.insert("name", document.scenario.name);
        scenario.insert("start_utc", document.scenario.start_utc);
        scenario.insert("end_mode", ToString(document.scenario.end_mode));
        scenario.insert("final_utc", document.scenario.final_utc);
        scenario.insert("duration_seconds", document.scenario.duration_seconds);
        scenario.insert("integrator", ToString(document.scenario.integrator));
        scenario.insert("maximum_integrator_step_seconds",
            document.scenario.maximum_integrator_step_seconds);
        scenario.insert("initial_integrator_step_seconds",
            document.scenario.initial_integrator_step_seconds);
        scenario.insert("absolute_tolerance", document.scenario.absolute_tolerance);
        scenario.insert("relative_tolerance", document.scenario.relative_tolerance);
        scenario.insert("output_mode", ToString(document.scenario.output_mode));
        scenario.insert("output_step_seconds", document.scenario.output_step_seconds);
        scenario.insert("maximum_integration_steps",
            static_cast<std::int64_t>(document.scenario.maximum_integration_steps));
        scenario.insert("maximum_output_samples",
            static_cast<std::int64_t>(document.scenario.maximum_output_samples));
        scenario.insert("maximum_wall_clock_runtime_seconds",
            document.scenario.maximum_wall_clock_runtime_seconds);
        scenario.insert("mass_flow_convention",
            ToString(document.scenario.mass_flow_convention));
        root.insert("scenario", std::move(scenario));

        toml::table initial;
        initial.insert("position_icrf_m", Vec3Array(document.initial_state.position_icrf_m));
        initial.insert("velocity_icrf_mps", Vec3Array(document.initial_state.velocity_icrf_mps));
        initial.insert("attitude_body_to_icrf",
            QuatArray(document.initial_state.attitude_body_to_icrf));
        initial.insert("angular_velocity_body_radps",
            Vec3Array(document.initial_state.angular_velocity_body_radps));
        if (!document.initial_state.authoring_frame.empty())
        {
            initial.insert(
                "authoring_frame", document.initial_state.authoring_frame);
        }
        root.insert("initial_state", std::move(initial));

        toml::array components;
        for (const Component& value : document.components)
        {
            toml::table table;
            table.insert("id", value.id);
            table.insert("name", value.name);
            table.insert("initial_mass_kg", value.initial_mass_kg);
            table.insert("minimum_mass_kg", value.minimum_mass_kg);
            table.insert("variable_mass", value.variable_mass);
            table.insert("local_center_of_mass_m", Vec3Array(value.local_center_of_mass_m));
            table.insert("origin_body_m", Vec3Array(value.origin_body_m));
            table.insert("component_to_body", QuatArray(value.component_to_body));
            table.insert("parent_component", value.parent_component_name);
            table.insert("parent_anchor_m", Vec3Array(value.parent_anchor_m));
            table.insert("child_anchor_m", Vec3Array(value.child_anchor_m));
            table.insert("child_to_parent_zero_orientation",
                QuatArray(value.child_to_parent_zero_orientation));

            toml::table inertia;
            inertia.insert("ixx_kgm2", value.centroidal_inertia.ixx_kgm2);
            inertia.insert("iyy_kgm2", value.centroidal_inertia.iyy_kgm2);
            inertia.insert("izz_kgm2", value.centroidal_inertia.izz_kgm2);
            inertia.insert("ixy_kgm2", value.centroidal_inertia.ixy_kgm2);
            inertia.insert("ixz_kgm2", value.centroidal_inertia.ixz_kgm2);
            inertia.insert("iyz_kgm2", value.centroidal_inertia.iyz_kgm2);
            table.insert("inertia", std::move(inertia));

            toml::array dofs;
            for (const JointDof& dof : value.degrees_of_freedom)
            {
                toml::table dof_table;
                dof_table.insert("id", dof.id);
                dof_table.insert("name", dof.name);
                dof_table.insert("motion", ToString(dof.motion));
                dof_table.insert("axis", Vec3Array(dof.axis));
                const bool rotational =
                    dof.motion == JointMotion::Rotation;
                const auto to_file_units =
                    [&](const double canonical_value)
                    {
                        return rotational
                            ? RotationalValueToFileUnits(
                                canonical_value)
                            : canonical_value;
                    };
                dof_table.insert(
                    "initial_coordinate",
                    to_file_units(dof.initial_coordinate));
                dof_table.insert(
                    "initial_rate",
                    to_file_units(dof.initial_rate));
                if (dof.minimum_coordinate)
                    dof_table.insert(
                        "minimum_coordinate",
                        to_file_units(*dof.minimum_coordinate));
                if (dof.maximum_coordinate)
                    dof_table.insert(
                        "maximum_coordinate",
                        to_file_units(*dof.maximum_coordinate));
                if (std::isfinite(dof.maximum_absolute_rate))
                    dof_table.insert(
                        "maximum_absolute_rate",
                        to_file_units(dof.maximum_absolute_rate));
                if (std::isfinite(dof.maximum_absolute_effort))
                    dof_table.insert("maximum_absolute_effort", dof.maximum_absolute_effort);
                dofs.push_back(std::move(dof_table));
            }
            table.insert("dofs", std::move(dofs));

            const ComponentVisual& v = value.visual;
            toml::table visual;
            visual.insert("geometry_source", ToString(v.geometry_source));
            visual.insert("primitive_type", ToString(v.primitive_type));
            visual.insert("box_dimensions_m", Vec3Array(v.box_dimensions_m));
            visual.insert("sphere_radius_m", v.sphere_radius_m);
            visual.insert("cylinder_radius_m", v.cylinder_radius_m);
            visual.insert("cylinder_length_m", v.cylinder_length_m);
            visual.insert("stl_file", v.stl_file_path);
            visual.insert("stl_length_unit", ToString(v.stl_length_unit));
            visual.insert("stl_recenter_mode", ToString(v.stl_recenter_mode));
            visual.insert("visual_offset_m", Vec3Array(v.visual_offset_m));
            visual.insert("visual_orientation", QuatArray(v.visual_orientation));
            visual.insert("visual_scale", Vec3Array(v.visual_scale));
            visual.insert("surface_appearance", ToString(v.surface_appearance));
            visual.insert("display_color", ColorArray(v.display_color));
            visual.insert("base_color_tint", ColorArray(v.base_color_tint));
            visual.insert("base_color_texture_file", v.base_color_texture_file_path);
            visual.insert("normal_texture_file", v.normal_texture_file_path);
            visual.insert("roughness_texture_file", v.roughness_texture_file_path);
            visual.insert("metallic_texture_file", v.metallic_texture_file_path);
            visual.insert("visible", v.visible);
            table.insert("visual", std::move(visual));

            const ComponentSrpAuthoring& s = value.srp;
            toml::table srp;
            srp.insert("included_in_proxy", s.included_in_proxy);
            srp.insert("proxy_resolution", ToString(s.proxy_resolution));
            srp.insert("custom_target_triangle_count", s.custom_target_triangle_count);
            srp.insert("use_global_fallback_optics",
                s.use_global_fallback_optical_properties);
            srp.insert("one_optical_configuration",
                s.apply_one_optical_configuration_to_entire_component);
            srp.insert("optics", OpticsTable(s.component_optical_properties));
            srp.insert("generated_geometry_signature", s.generated_geometry_signature);
            srp.insert("generated_triangle_count", s.generated_triangle_count);
            srp.insert("proxy_generation_required", s.proxy_generation_required);
            srp.insert("last_proxy_generation_message", s.last_proxy_generation_message);
            toml::array region_overrides;
            for (const SrpLogicalRegionOverride& override_value : s.logical_region_overrides)
            {
                toml::table override_table;
                override_table.insert("region", ToString(override_value.region));
                override_table.insert("optics", OpticsTable(override_value.optical_properties));
                region_overrides.push_back(std::move(override_table));
            }
            srp.insert("logical_region_overrides", std::move(region_overrides));
            toml::array triangle_overrides;
            for (const SrpTriangleOverride& override_value : s.triangle_overrides)
            {
                toml::table override_table;
                override_table.insert("triangle_index", override_value.proxy_triangle_index);
                override_table.insert("optics", OpticsTable(override_value.optical_properties));
                triangle_overrides.push_back(std::move(override_table));
            }
            srp.insert("triangle_overrides", std::move(triangle_overrides));
            table.insert("srp", std::move(srp));
            components.push_back(std::move(table));
        }
        root.insert("components", std::move(components));

        toml::array thrusters;
        for (const Thruster& value : document.thrusters)
        {
            toml::table table;
            table.insert("name", value.name);
            table.insert("mode", ToString(value.mode));
            table.insert("mount_component", value.mount_component_name);
            table.insert("propellant_component", value.propellant_component_name);
            table.insert("application_point_component_m",
                Vec3Array(value.application_point_component_m));
            table.insert("direction_component", Vec3Array(value.direction_component));
            table.insert("ignition_time_mode", ToString(value.ignition_time_mode));
            table.insert("ignition_utc", value.ignition_utc);
            table.insert("ignition_elapsed_seconds", value.ignition_elapsed_seconds);
            table.insert("never_shuts_down", value.never_shuts_down);
            table.insert("shutdown_time_mode", ToString(value.shutdown_time_mode));
            table.insert("shutdown_utc", value.shutdown_utc);
            table.insert("shutdown_elapsed_seconds", value.shutdown_elapsed_seconds);
            table.insert("maximum_thrust_n", value.maximum_thrust_n);
            table.insert("prescribed_thrust", ScalarProfileTable(value.prescribed_thrust));
            table.insert("prescribed_specific_impulse",
                ScalarProfileTable(value.prescribed_specific_impulse));
            thrusters.push_back(std::move(table));
        }
        root.insert("thrusters", std::move(thrusters));

        toml::array wheels;
        for (const ReactionWheel& value : document.reaction_wheels)
        {
            toml::table table;
            table.insert("name", value.name);
            table.insert("mount_component", value.mount_component_name);
            table.insert("axis_component", Vec3Array(value.axis_component));
            table.insert("initial_momentum_nms", value.initial_momentum_nms);
            table.insert("maximum_absolute_momentum_nms",
                value.maximum_absolute_momentum_nms);
            wheels.push_back(std::move(table));
        }
        root.insert("reaction_wheels", std::move(wheels));

        toml::table control;
        control.insert("mode", ToString(document.control.mode));
        control.insert("unreal_controller_id", document.control.unreal_controller_id);
        control.insert("controller_dll_file", document.control.controller_dll_path);
        root.insert("control", std::move(control));

        toml::table gravity;
        gravity.insert("include_first_post_newtonian_correction",
            document.include_first_post_newtonian_correction);
        toml::array bodies;
        for (const CelestialBody& value : document.celestial_bodies)
        {
            toml::table table;
            table.insert("catalog_key", value.catalog_key);
            table.insert("gravity_enabled", value.gravity_enabled);
            table.insert("automatic_activation_radius_m",
                value.automatic_activation_radius_m);
            table.insert("barycenter_resolution_radius_m",
                value.barycenter_resolution_radius_m);
            table.insert("harmonic_model_csv_file",
                value.harmonic_model_csv_file_path);
            table.insert("maximum_harmonic_degree", value.maximum_harmonic_degree);
            bodies.push_back(std::move(table));
        }
        gravity.insert("bodies", std::move(bodies));
        root.insert("gravity", std::move(gravity));

        const SolarRadiationPressure& srp_value = document.solar_radiation_pressure;
        toml::table srp;
        srp.insert("enabled", srp_value.enabled);
        srp.insert("sun_body", srp_value.sun_body_name);
        srp.insert("pressure_at_one_au_pa", srp_value.pressure_at_one_au_pa);
        srp.insert("compute_eclipse", srp_value.compute_eclipse);
        srp.insert("occulting_bodies", StringArray(srp_value.occulting_body_names));
        srp.insert("compute_component_shadows", srp_value.compute_component_shadows);
        srp.insert("global_fallback_optics",
            OpticsTable(srp_value.global_fallback_optical_properties));
        toml::array facets;
        for (const OpticalFacet& value : srp_value.optical_facets)
        {
            toml::table table;
            table.insert("name", value.name);
            table.insert("component_id", value.component_id);
            table.insert("component_name", value.component_name);
            table.insert("stable_triangle_index", value.stable_triangle_index);
            table.insert("logical_region", ToString(value.logical_region));
            table.insert("vertex0_component_m", Vec3Array(value.vertex0_component_m));
            table.insert("vertex1_component_m", Vec3Array(value.vertex1_component_m));
            table.insert("vertex2_component_m", Vec3Array(value.vertex2_component_m));
            table.insert("optics", OpticsTable(value.optical_properties));
            facets.push_back(std::move(table));
        }
        srp.insert("facets", std::move(facets));
        root.insert("srp", std::move(srp));

        const Atmosphere& atmosphere_value = document.atmosphere;
        toml::table atmosphere;
        atmosphere.insert("enabled", atmosphere_value.enabled);
        atmosphere.insert("central_body", atmosphere_value.central_body_name);
        atmosphere.insert("model", ToString(atmosphere_value.model));
        atmosphere.insert("general_profile_csv_file",
            atmosphere_value.general_profile_csv_path);
        atmosphere.insert("centered_average_f107_sfu",
            atmosphere_value.centered_average_f107_sfu);
        atmosphere.insert("chp_coefficient_csv_file",
            atmosphere_value.chp_coefficient_csv_path);
        atmosphere.insert("chp_molecular_profile_csv_file",
            atmosphere_value.chp_molecular_profile_csv_path);
        root.insert("atmosphere", std::move(atmosphere));

        const Aerodynamics& aerodynamic_value = document.aerodynamics;
        toml::table aerodynamics;
        aerodynamics.insert("enabled", aerodynamic_value.enabled);
        aerodynamics.insert("reference_area_m2", aerodynamic_value.reference_area_m2);
        aerodynamics.insert("reference_length_m", aerodynamic_value.reference_length_m);
        aerodynamics.insert("minimum_dynamic_pressure_pa",
            aerodynamic_value.minimum_dynamic_pressure_pa);
        aerodynamics.insert("maximum_valid_dynamic_pressure_pa",
            aerodynamic_value.maximum_valid_dynamic_pressure_pa);
        aerodynamics.insert("constant_drag_fallback_enabled",
            aerodynamic_value.constant_drag_fallback_enabled);
        aerodynamics.insert("fallback_drag_coefficient",
            aerodynamic_value.fallback_drag_coefficient);
        const AerodynamicDatabase& db_value = aerodynamic_value.database;
        toml::table database;
        database.insert("enabled", db_value.enabled);
        database.insert("csv_file", db_value.csv_file_path);
        database.insert("interpolation", ToString(db_value.interpolation));
        database.insert("extrapolation", ToString(db_value.extrapolation));
        database.insert("neighbor_count", static_cast<std::int64_t>(db_value.neighbor_count));
        database.insert("inverse_distance_power", db_value.inverse_distance_power);
        if (db_value.maximum_normalized_neighbor_distance)
            database.insert("maximum_normalized_neighbor_distance",
                *db_value.maximum_normalized_neighbor_distance);
        database.insert("moment_reference_center_body_m",
            Vec3Array(db_value.moment_reference_center_body_m));
        toml::array rows;
        for (const AerodynamicDatabaseRow& value : db_value.rows)
        {
            toml::table table;
            table.insert("speed_ratio", value.speed_ratio);
            table.insert("knudsen_number", value.knudsen_number);
            table.insert("gas_flow_direction_body",
                Vec3Array(value.gas_flow_direction_body));
            table.insert("articulation_coordinates",
                NumberArray(value.articulation_coordinates));
            table.insert("force_coefficients_body",
                Vec3Array(value.force_coefficients_body));
            table.insert("moment_coefficients_body",
                Vec3Array(value.moment_coefficients_body));
            rows.push_back(std::move(table));
        }
        database.insert("rows", std::move(rows));
        aerodynamics.insert("database", std::move(database));
        root.insert("aerodynamics", std::move(aerodynamics));

        std::ostringstream output;
        output << toml::toml_formatter{root};
        return output.str() + "\n";
    }

    bool LoadScenarioFile(
        const std::filesystem::path& file_path,
        ScenarioDocument& document,
        Diagnostics& diagnostics)
    {
        diagnostics.clear();
        std::ifstream input(file_path, std::ios::binary);
        if (!input)
        {
            diagnostics.push_back({DiagnosticSeverity::Error, "TGSCN-OPEN", {},
                "Could not open scenario file '" + file_path.string() + "'."});
            return false;
        }
        std::ostringstream contents;
        contents << input.rdbuf();
        if (!input.good() && !input.eof())
        {
            diagnostics.push_back({DiagnosticSeverity::Error, "TGSCN-READ", {},
                "Could not read scenario file '" + file_path.string() + "'."});
            return false;
        }
        return ParseScenarioText(
            contents.str(), document, diagnostics, file_path.string());
    }

    bool SaveScenarioFile(
        const std::filesystem::path& file_path,
        const ScenarioDocument& document,
        Diagnostics& diagnostics)
    {
        diagnostics.clear();
        std::ofstream output(file_path, std::ios::binary | std::ios::trunc);
        if (!output)
        {
            diagnostics.push_back({DiagnosticSeverity::Error, "TGSCN-WRITE", {},
                "Could not create scenario file '" + file_path.string() + "'."});
            return false;
        }
        output << SerializeScenarioText(document);
        if (!output)
        {
            diagnostics.push_back({DiagnosticSeverity::Error, "TGSCN-WRITE", {},
                "Could not finish writing scenario file '" +
                    file_path.string() + "'."});
            return false;
        }
        return true;
    }

    std::filesystem::path ResolveReferencedPath(
        const std::filesystem::path& scenario_file_path,
        const std::string& authored_path)
    {
        if (authored_path.empty()) return {};
        std::filesystem::path path(authored_path);
        if (path.is_relative()) path = scenario_file_path.parent_path() / path;
        return path.lexically_normal();
    }
}
