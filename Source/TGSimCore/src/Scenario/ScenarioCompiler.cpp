// Copyright (c) 2026 Breno Raiher.
// SPDX-License-Identifier: MIT

#include "TGSim/Scenario/ScenarioCompiler.h"

#include "TGSim/Scenario/CelestialCatalog.h"
#include "TGSim/Scenario/ScenarioFile.h"

#include <algorithm>
#include <array>
#include <cerrno>
#include <cctype>
#include <cmath>
#include <cstdlib>
#include <fstream>
#include <limits>
#include <set>
#include <sstream>
#include <unordered_map>
#include <unordered_set>

namespace tgsim::scenario
{
    namespace
    {
        struct CsvRow
        {
            std::size_t line = 0;
            std::vector<double> values;
        };

        void AddError(
            Diagnostics& diagnostics,
            const std::string& code,
            const std::string& path,
            const std::string& message,
            const std::size_t line = 0)
        {
            diagnostics.push_back({
                DiagnosticSeverity::Error, code, path, message, line, 0});
        }

        void AddWarning(
            Diagnostics& diagnostics,
            const std::string& code,
            const std::string& path,
            const std::string& message)
        {
            diagnostics.push_back({
                DiagnosticSeverity::Warning, code, path, message});
        }

        std::string Lower(std::string value)
        {
            std::transform(
                value.begin(), value.end(), value.begin(),
                [](const unsigned char character)
                {
                    return static_cast<char>(std::tolower(character));
                });
            return value;
        }

        std::string Trim(std::string value)
        {
            const auto first = std::find_if_not(
                value.begin(), value.end(),
                [](const unsigned char c) { return std::isspace(c) != 0; });
            const auto last = std::find_if_not(
                value.rbegin(), value.rend(),
                [](const unsigned char c) { return std::isspace(c) != 0; }).base();
            return first < last ? std::string(first, last) : std::string{};
        }

        bool ParseFiniteDouble(const std::string& text, double& value)
        {
            const std::string trimmed = Trim(text);
            if (trimmed.empty()) return false;
            char* end = nullptr;
            errno = 0;
            value = std::strtod(trimmed.c_str(), &end);
            return end == trimmed.c_str() + trimmed.size() && errno != ERANGE &&
                std::isfinite(value);
        }

        bool ReadNumericCsv(
            const std::filesystem::path& file_path,
            const std::string& field_path,
            Diagnostics& diagnostics,
            std::vector<CsvRow>& rows)
        {
            rows.clear();
            bool succeeded = true;
            std::ifstream input(file_path, std::ios::binary);
            if (!input)
            {
                AddError(
                    diagnostics, "SCN-CSV-OPEN", field_path,
                    "Could not open CSV file '" + file_path.string() + "'.");
                return false;
            }

            std::string line;
            std::size_t line_number = 0;
            while (std::getline(input, line))
            {
                ++line_number;
                if (line_number == 1 && line.size() >= 3 &&
                    static_cast<unsigned char>(line[0]) == 0xef &&
                    static_cast<unsigned char>(line[1]) == 0xbb &&
                    static_cast<unsigned char>(line[2]) == 0xbf)
                {
                    line.erase(0, 3);
                }
                line = Trim(line);
                if (line.empty()) continue;

                CsvRow row;
                row.line = line_number;
                std::istringstream cells(line);
                std::string cell;
                bool valid = true;
                while (std::getline(cells, cell, ','))
                {
                    double value = 0.0;
                    if (!ParseFiniteDouble(cell, value))
                    {
                        AddError(
                            diagnostics, "SCN-CSV-NUMBER", field_path,
                            "Every CSV cell must be a finite number.", line_number);
                        succeeded = false;
                        valid = false;
                        break;
                    }
                    row.values.push_back(value);
                }
                if (valid) rows.push_back(std::move(row));
            }

            if (!input.good() && !input.eof())
            {
                AddError(
                    diagnostics, "SCN-CSV-READ", field_path,
                    "The CSV file could not be read completely.");
                succeeded = false;
            }
            if (rows.empty())
            {
                AddError(
                    diagnostics, "SCN-CSV-EMPTY", field_path,
                    "The CSV file contains no numeric rows.");
                succeeded = false;
            }
            return succeeded;
        }

        bool ConvertUtc(
            const std::string& utc,
            const std::string& field_path,
            const ScenarioCompilerServices& services,
            Diagnostics& diagnostics,
            double& ephemeris_time)
        {
            if (utc.empty())
            {
                AddError(
                    diagnostics, "SCN-TIME-MISSING", field_path,
                    "A UTC epoch is required.");
                return false;
            }
            if (!services.utc_to_ephemeris_time)
            {
                AddError(
                    diagnostics, "SCN-TIME-SERVICE", field_path,
                    "No CSPICE UTC-to-ET converter was supplied.");
                return false;
            }
            std::string error;
            if (!services.utc_to_ephemeris_time(utc, ephemeris_time, error))
            {
                AddError(
                    diagnostics, "SCN-TIME-INVALID", field_path,
                    error.empty() ? "CSPICE rejected the UTC epoch." : error);
                return false;
            }
            return true;
        }

        Mat3d InertiaMatrix(const SymmetricInertia& inertia)
        {
            Mat3d matrix = Mat3d::Zero();
            matrix.m[0][0] = inertia.ixx_kgm2;
            matrix.m[1][1] = inertia.iyy_kgm2;
            matrix.m[2][2] = inertia.izz_kgm2;
            matrix.m[0][1] = matrix.m[1][0] = inertia.ixy_kgm2;
            matrix.m[0][2] = matrix.m[2][0] = inertia.ixz_kgm2;
            matrix.m[1][2] = matrix.m[2][1] = inertia.iyz_kgm2;
            return matrix;
        }

        bool IsUnitDirection(
            const Vec3d& vector,
            const std::string& path,
            Diagnostics& diagnostics,
            Vec3d& normalized)
        {
            const double norm = vector.Norm();
            if (!std::isfinite(norm) || norm <= 1.0e-15)
            {
                AddError(
                    diagnostics, "SCN-VECTOR-ZERO", path,
                    "A finite nonzero direction vector is required.");
                normalized = Vec3d::Zero();
                return false;
            }
            normalized = vector / norm;
            if (std::abs(norm - 1.0) > 1.0e-9)
            {
                AddWarning(
                    diagnostics, "SCN-VECTOR-NORMALIZED", path,
                    "The vector was normalized before entering the backend.");
            }
            return true;
        }

        bool BuildComponentOrder(
            const ScenarioDocument& document,
            Diagnostics& diagnostics,
            std::vector<std::size_t>& ordered_source_indices,
            std::unordered_map<std::string, std::size_t>& source_by_name)
        {
            ordered_source_indices.clear();
            source_by_name.clear();
            std::size_t root_count = 0;
            for (std::size_t index = 0; index < document.components.size(); ++index)
            {
                const Component& component = document.components[index];
                const std::string key = Lower(component.name);
                if (key.empty())
                {
                    AddError(
                        diagnostics, "SCN-COMPONENT-NAME",
                        "components[" + std::to_string(index) + "].name",
                        "Every component needs a nonempty name.");
                    continue;
                }
                if (!source_by_name.emplace(key, index).second)
                {
                    AddError(
                        diagnostics, "SCN-COMPONENT-DUPLICATE",
                        "components[" + std::to_string(index) + "].name",
                        "Component names must be unique, ignoring case.");
                }
                if (component.parent_component_name.empty()) ++root_count;
            }
            if (document.components.empty())
                AddError(diagnostics, "SCN-COMPONENT-EMPTY", "components",
                    "At least one physical component is required.");
            if (root_count != 1)
                AddError(diagnostics, "SCN-COMPONENT-ROOT", "components",
                    "The component tree must contain exactly one root component.");

            std::vector<bool> inserted(document.components.size(), false);
            bool progress = true;
            while (ordered_source_indices.size() < document.components.size() && progress)
            {
                progress = false;
                for (std::size_t index = 0; index < document.components.size(); ++index)
                {
                    if (inserted[index]) continue;
                    const Component& component = document.components[index];
                    if (component.parent_component_name.empty())
                    {
                        inserted[index] = true;
                        ordered_source_indices.push_back(index);
                        progress = true;
                        continue;
                    }
                    const auto parent = source_by_name.find(
                        Lower(component.parent_component_name));
                    if (parent == source_by_name.end()) continue;
                    if (inserted[parent->second])
                    {
                        inserted[index] = true;
                        ordered_source_indices.push_back(index);
                        progress = true;
                    }
                }
            }

            for (std::size_t index = 0; index < document.components.size(); ++index)
            {
                if (inserted[index]) continue;
                const Component& component = document.components[index];
                const auto parent = source_by_name.find(
                    Lower(component.parent_component_name));
                AddError(
                    diagnostics,
                    parent == source_by_name.end()
                        ? "SCN-COMPONENT-PARENT"
                        : "SCN-COMPONENT-CYCLE",
                    "components[" + std::to_string(index) + "].parent_component",
                    parent == source_by_name.end()
                        ? "The named parent component does not exist."
                        : "The component hierarchy contains a cycle.");
            }
            return !HasErrors(diagnostics);
        }

        bool LoadScalarCurve(
            const ScalarProfile& source,
            const bool thrust,
            const std::filesystem::path& scenario_file_path,
            const std::string& field_path,
            Diagnostics& diagnostics,
            double& constant_value,
            ScalarCurve& curve)
        {
            constant_value = source.constant_value;
            curve = ScalarCurve{};
            curve.default_value = source.constant_value;
            curve.extrapolation = ScalarExtrapolationMethod::ClampToEndpoint;

            if (source.source == ScalarProfileSource::Constant)
            {
                const bool valid = thrust
                    ? source.constant_value >= 0.0
                    : source.constant_value > 0.0;
                if (!std::isfinite(source.constant_value) || !valid)
                {
                    AddError(
                        diagnostics, "SCN-PROFILE-CONSTANT",
                        field_path + ".constant_value",
                        thrust
                            ? "Constant thrust must be finite and nonnegative."
                            : "Constant specific impulse must be finite and positive.");
                }
                return valid;
            }

            const std::filesystem::path path = ResolveReferencedPath(
                scenario_file_path, source.csv_file_path);
            std::vector<CsvRow> rows;
            if (!ReadNumericCsv(path, field_path + ".csv_file", diagnostics, rows))
                return false;

            double previous_time = -std::numeric_limits<double>::infinity();
            for (const CsvRow& row : rows)
            {
                if (row.values.size() != 2)
                {
                    AddError(
                        diagnostics, "SCN-PROFILE-COLUMNS", field_path + ".csv_file",
                        "Expected exactly two columns: time_seconds,value.", row.line);
                    continue;
                }
                const double time = row.values[0];
                const double value = row.values[1];
                if (time <= previous_time)
                {
                    AddError(
                        diagnostics, "SCN-PROFILE-TIME", field_path + ".csv_file",
                        "Profile times must be strictly increasing.", row.line);
                }
                if ((thrust && value < 0.0) || (!thrust && value <= 0.0))
                {
                    AddError(
                        diagnostics, "SCN-PROFILE-VALUE", field_path + ".csv_file",
                        thrust
                            ? "Thrust samples must be nonnegative."
                            : "Specific-impulse samples must be positive.",
                        row.line);
                }
                curve.samples.push_back({time, value});
                previous_time = time;
            }
            if (thrust)
            {
                if (curve.samples.size() < 2)
                    AddError(diagnostics, "SCN-PROFILE-SAMPLES", field_path + ".csv_file",
                        "A thrust curve requires at least two samples.");
                else if (curve.samples.front().value != 0.0 ||
                         curve.samples.back().value != 0.0)
                    AddError(diagnostics, "SCN-PROFILE-ENDPOINT", field_path + ".csv_file",
                        "A thrust curve must start and finish at zero thrust.");
            }
            return !HasErrors(diagnostics);
        }

        bool LoadHarmonicModel(
            const CelestialBody& source,
            const CelestialCatalogEntry& catalog,
            const std::filesystem::path& scenario_file_path,
            const std::string& field_path,
            Diagnostics& diagnostics,
            GravityBody& body)
        {
            if (source.maximum_harmonic_degree <= 0) return true;
            if (!catalog.supports_harmonic_gravity)
            {
                AddError(
                    diagnostics, "SCN-GRAVITY-HARMONICS", field_path,
                    "This catalog source does not support harmonic gravity.");
                return false;
            }
            if (source.harmonic_model_csv_file_path.empty())
            {
                AddError(
                    diagnostics, "SCN-GRAVITY-HARMONICS", field_path,
                    "A harmonic CSV is required when maximum degree is positive.");
                return false;
            }
            std::vector<CsvRow> rows;
            const std::filesystem::path path = ResolveReferencedPath(
                scenario_file_path, source.harmonic_model_csv_file_path);
            if (!ReadNumericCsv(path, field_path, diagnostics, rows)) return false;
            if (rows.front().values.size() != 2)
            {
                AddError(
                    diagnostics, "SCN-GRAVITY-CONSTANTS", field_path,
                    "The first row must contain model GM and model reference radius.",
                    rows.front().line);
                return false;
            }
            body.harmonic_model_gravitational_parameter_m3ps2 = rows.front().values[0];
            body.harmonic_model_reference_radius_m = rows.front().values[1];
            if (body.harmonic_model_gravitational_parameter_m3ps2 <= 0.0 ||
                body.harmonic_model_reference_radius_m <= 0.0)
                AddError(diagnostics, "SCN-GRAVITY-CONSTANTS", field_path,
                    "Harmonic-model GM and reference radius must both be positive.",
                    rows.front().line);

            int available_degree = 0;
            std::set<std::pair<int, int>> seen;
            for (std::size_t index = 1; index < rows.size(); ++index)
            {
                const CsvRow& row = rows[index];
                if (row.values.size() != 4)
                {
                    AddError(
                        diagnostics, "SCN-GRAVITY-COLUMNS", field_path,
                        "Coefficient rows must contain n,m,Cbar_nm,Sbar_nm.", row.line);
                    continue;
                }
                const int degree = static_cast<int>(row.values[0]);
                const int order = static_cast<int>(row.values[1]);
                if (row.values[0] != degree || row.values[1] != order ||
                    degree < 0 || order < 0 || order > degree ||
                    (degree == 0 && order == 0))
                {
                    AddError(
                        diagnostics, "SCN-GRAVITY-INDEX", field_path,
                        "Each coefficient needs integer indices with n >= m >= 0; "
                        "the implicit (0,0) term must not be supplied.", row.line);
                    continue;
                }
                if (!seen.emplace(degree, order).second)
                {
                    AddError(
                        diagnostics, "SCN-GRAVITY-DUPLICATE", field_path,
                        "Duplicate harmonic coefficient pair.", row.line);
                    continue;
                }
                available_degree = std::max(available_degree, degree);
                if (degree <= source.maximum_harmonic_degree)
                {
                    body.harmonics.push_back(
                        {degree, order, row.values[2], row.values[3]});
                }
            }
            if (available_degree < source.maximum_harmonic_degree)
                AddError(diagnostics, "SCN-GRAVITY-DEGREE", field_path,
                    "Requested maximum degree exceeds the CSV's available degree.");
            body.maximum_harmonic_degree = source.maximum_harmonic_degree;
            return !HasErrors(diagnostics);
        }

        const CelestialCatalogEntry* ResolveBodyReference(
            const std::string& reference)
        {
            if (const CelestialCatalogEntry* direct =
                    FindCelestialCatalogEntry(reference))
                return direct;
            for (const CelestialCatalogEntry& entry : GetCelestialCatalog())
            {
                if (Lower(entry.display_name) == Lower(reference) ||
                    Lower(entry.spice_target) == Lower(reference))
                    return &entry;
            }
            return nullptr;
        }

        std::string ResolveSpiceName(
            const std::string& reference,
            const std::string& field_path,
            Diagnostics& diagnostics)
        {
            const CelestialCatalogEntry* entry = ResolveBodyReference(reference);
            if (entry == nullptr)
            {
                AddError(diagnostics, "SCN-BODY-REFERENCE", field_path,
                    "The celestial-body reference is not in the fixed catalog.");
                return reference;
            }
            return entry->spice_target;
        }

        void LoadAtmosphereProfile(
            const Atmosphere& source,
            const std::filesystem::path& scenario_file_path,
            Diagnostics& diagnostics,
            AtmosphereSettings& target)
        {
            const std::filesystem::path path = ResolveReferencedPath(
                scenario_file_path, source.general_profile_csv_path);
            std::vector<CsvRow> rows;
            if (!ReadNumericCsv(
                    path, "atmosphere.general_profile_csv_file", diagnostics, rows))
                return;
            double previous_altitude = -std::numeric_limits<double>::infinity();
            for (const CsvRow& row : rows)
            {
                if (row.values.size() != 5)
                {
                    AddError(
                        diagnostics, "SCN-ATMOSPHERE-COLUMNS",
                        "atmosphere.general_profile_csv_file",
                        "Expected altitude,density,temperature,mean_particle_mass,"
                        "collision_cross_section.", row.line);
                    continue;
                }
                if (row.values[0] <= previous_altitude || row.values[1] < 0.0 ||
                    row.values[2] <= 0.0 || row.values[3] <= 0.0 ||
                    row.values[4] <= 0.0)
                    AddError(diagnostics, "SCN-ATMOSPHERE-VALUE",
                        "atmosphere.general_profile_csv_file",
                        "Altitudes must increase; density must be nonnegative; "
                        "temperature, particle mass, and cross section must be positive.",
                        row.line);
                target.density_profile.push_back({row.values[0], row.values[1]});
                target.thermodynamic_profile.push_back(
                    {row.values[0], row.values[2], row.values[3], row.values[4]});
                previous_altitude = row.values[0];
            }
        }

        void LoadCubicHarrisPriester(
            const Atmosphere& source,
            const std::filesystem::path& scenario_file_path,
            Diagnostics& diagnostics,
            AtmosphereSettings& target)
        {
            std::vector<CsvRow> coefficient_rows;
            const std::filesystem::path coefficient_path = ResolveReferencedPath(
                scenario_file_path, source.chp_coefficient_csv_path);
            if (ReadNumericCsv(
                    coefficient_path, "atmosphere.chp_coefficient_csv_file",
                    diagnostics, coefficient_rows))
            {
                static constexpr std::array<double, 50> PublicationAltitudesMeters = {
                    100000.0, 120000.0, 130000.0, 140000.0, 150000.0,
                    160000.0, 170000.0, 180000.0, 190000.0, 200000.0,
                    210000.0, 220000.0, 230000.0, 240000.0, 250000.0,
                    260000.0, 270000.0, 280000.0, 290000.0, 300000.0,
                    320000.0, 340000.0, 360000.0, 380000.0, 400000.0,
                    420000.0, 440000.0, 460000.0, 480000.0, 500000.0,
                    520000.0, 540000.0, 560000.0, 580000.0, 600000.0,
                    620000.0, 640000.0, 660000.0, 680000.0, 700000.0,
                    720000.0, 740000.0, 760000.0, 780000.0, 800000.0,
                    840000.0, 880000.0, 920000.0, 960000.0, 1000000.0};
                constexpr double GramsPerCubicKilometerToKilogramsPerCubicMeter =
                    1.0e-12;

                const bool publication_format =
                    coefficient_rows.size() == PublicationAltitudesMeters.size() &&
                    std::all_of(
                        coefficient_rows.begin(), coefficient_rows.end(),
                        [](const CsvRow& row) { return row.values.size() == 8; });
                const bool explicit_altitude_format =
                    !coefficient_rows.empty() &&
                    std::all_of(
                        coefficient_rows.begin(), coefficient_rows.end(),
                        [](const CsvRow& row) { return row.values.size() == 9; });

                if (!publication_format && !explicit_altitude_format)
                {
                    AddError(diagnostics, "SCN-CHP-COLUMNS",
                        "atmosphere.chp_coefficient_csv_file",
                        "Expected exactly 50 rows of eight publication coefficients, "
                        "or legacy rows containing altitude followed by eight SI coefficients.");
                }
                else
                {
                    double previous_altitude =
                        -std::numeric_limits<double>::infinity();
                    for (std::size_t row_index = 0;
                         row_index < coefficient_rows.size(); ++row_index)
                    {
                        const CsvRow& row = coefficient_rows[row_index];
                        const double altitude_m = publication_format
                            ? PublicationAltitudesMeters[row_index]
                            : row.values[0];
                        const std::size_t coefficient_offset =
                            publication_format ? 0 : 1;
                        const double coefficient_scale = publication_format
                            ? GramsPerCubicKilometerToKilogramsPerCubicMeter
                            : 1.0;

                        if (altitude_m <= previous_altitude)
                        {
                            AddError(diagnostics, "SCN-CHP-ALTITUDE",
                                "atmosphere.chp_coefficient_csv_file",
                                "Altitudes must be strictly increasing.", row.line);
                        }

                        CubicHarrisPriesterDensitySample sample;
                        sample.altitude_m = altitude_m;
                        for (std::size_t i = 0; i < 4; ++i)
                        {
                            sample.maximum_density_coefficients_kgpm3[i] =
                                row.values[coefficient_offset + i] * coefficient_scale;
                            sample.minimum_density_coefficients_kgpm3[i] =
                                row.values[coefficient_offset + 4 + i] * coefficient_scale;
                        }
                        target.cubic_harris_priester.density_samples.push_back(sample);
                        previous_altitude = altitude_m;
                    }
                }
            }

            std::vector<CsvRow> molecular_rows;
            const std::filesystem::path molecular_path = ResolveReferencedPath(
                scenario_file_path, source.chp_molecular_profile_csv_path);
            if (ReadNumericCsv(
                    molecular_path, "atmosphere.chp_molecular_profile_csv_file",
                    diagnostics, molecular_rows))
            {
                double previous_altitude = -std::numeric_limits<double>::infinity();
                for (const CsvRow& row : molecular_rows)
                {
                    if (row.values.size() != 4)
                    {
                        AddError(diagnostics, "SCN-CHP-MOLECULAR-COLUMNS",
                            "atmosphere.chp_molecular_profile_csv_file",
                            "Expected altitude,temperature,mean_particle_mass,"
                            "collision_cross_section.", row.line);
                        continue;
                    }
                    if (row.values[0] <= previous_altitude || row.values[1] <= 0.0 ||
                        row.values[2] <= 0.0 || row.values[3] <= 0.0)
                        AddError(diagnostics, "SCN-CHP-MOLECULAR-VALUE",
                            "atmosphere.chp_molecular_profile_csv_file",
                            "Altitudes must increase and all molecular values must be positive.",
                            row.line);
                    target.thermodynamic_profile.push_back(
                        {row.values[0], row.values[1], row.values[2], row.values[3]});
                    previous_altitude = row.values[0];
                }
            }
        }

        AerodynamicCoefficientSample ConvertAerodynamicRow(
            const AerodynamicDatabaseRow& source)
        {
            AerodynamicCoefficientSample target;
            target.molecular_speed_ratio = source.speed_ratio;
            target.knudsen_number = source.knudsen_number;
            target.incoming_flow_direction_body =
                source.gas_flow_direction_body.Normalized();
            target.articulation_coordinates = source.articulation_coordinates;
            target.force_coefficients_body = source.force_coefficients_body;
            target.moment_coefficients_body_about_reference =
                source.moment_coefficients_body;
            return target;
        }

        void LoadAerodynamicDatabase(
            const AerodynamicDatabase& source,
            const std::size_t articulation_count,
            const std::filesystem::path& scenario_file_path,
            Diagnostics& diagnostics,
            AerodynamicCoefficientDatabase& target)
        {
            if (!source.rows.empty())
            {
                for (std::size_t index = 0; index < source.rows.size(); ++index)
                {
                    const AerodynamicDatabaseRow& row = source.rows[index];
                    if (row.articulation_coordinates.size() != articulation_count)
                        AddError(diagnostics, "SCN-AERO-ETA",
                            "aerodynamics.database.rows[" + std::to_string(index) +
                                "].articulation_coordinates",
                            "The eta vector length must equal the spacecraft DOF count.");
                    if (row.gas_flow_direction_body.Norm() <= 1.0e-15)
                        AddError(diagnostics, "SCN-AERO-DIRECTION",
                            "aerodynamics.database.rows[" + std::to_string(index) +
                                "].gas_flow_direction_body",
                            "The gas-flow direction must be nonzero.");
                    target.samples.push_back(ConvertAerodynamicRow(row));
                }
                return;
            }

            const std::filesystem::path path = ResolveReferencedPath(
                scenario_file_path, source.csv_file_path);
            std::vector<CsvRow> rows;
            if (!ReadNumericCsv(path, "aerodynamics.database.csv_file", diagnostics, rows))
                return;
            const std::size_t expected_columns = 11 + articulation_count;
            for (const CsvRow& row : rows)
            {
                if (row.values.size() != expected_columns)
                {
                    AddError(diagnostics, "SCN-AERO-COLUMNS",
                        "aerodynamics.database.csv_file",
                        "Expected " + std::to_string(expected_columns) +
                            " columns for a spacecraft with " +
                            std::to_string(articulation_count) + " DOFs.",
                        row.line);
                    continue;
                }
                AerodynamicCoefficientSample sample;
                sample.molecular_speed_ratio = row.values[0];
                sample.knudsen_number = row.values[1];
                sample.incoming_flow_direction_body =
                    Vec3d{row.values[2], row.values[3], row.values[4]}.Normalized();
                if (sample.incoming_flow_direction_body.Norm() <= 1.0e-15)
                    AddError(diagnostics, "SCN-AERO-DIRECTION",
                        "aerodynamics.database.csv_file",
                        "Gas-flow direction must be nonzero.", row.line);
                sample.articulation_coordinates.assign(
                    row.values.begin() + 5,
                    row.values.begin() + 5 + articulation_count);
                const std::size_t output = 5 + articulation_count;
                sample.force_coefficients_body = {
                    row.values[output], row.values[output + 1], row.values[output + 2]};
                sample.moment_coefficients_body_about_reference = {
                    row.values[output + 3], row.values[output + 4], row.values[output + 5]};
                target.samples.push_back(std::move(sample));
            }
        }
    }

    bool CompileScenario(
        const ScenarioDocument& document,
        const std::filesystem::path& scenario_file_path,
        const ScenarioCompilerServices& services,
        SimulationRequest& request,
        Diagnostics& diagnostics)
    {
        diagnostics.clear();
        request = SimulationRequest{};
        request.scenario_name = document.scenario.name;
        request.integrator_kind = document.scenario.integrator;
        request.output_mode = document.scenario.output_mode;
        request.mass_flow_convention = document.scenario.mass_flow_convention;
        request.maximum_integrator_step_seconds =
            document.scenario.maximum_integrator_step_seconds;
        request.initial_integrator_step_seconds =
            document.scenario.initial_integrator_step_seconds;
        request.absolute_tolerance = document.scenario.absolute_tolerance;
        request.relative_tolerance = document.scenario.relative_tolerance;
        request.output_step_seconds = document.scenario.output_step_seconds;
        request.maximum_integration_steps = document.scenario.maximum_integration_steps;
        request.maximum_output_samples = document.scenario.maximum_output_samples;

        ConvertUtc(
            document.scenario.start_utc, "scenario.start_utc", services,
            diagnostics, request.start_ephemeris_time_tdb_seconds);
        if (document.scenario.end_mode == EndMode::FinalUtc)
        {
            ConvertUtc(
                document.scenario.final_utc, "scenario.final_utc", services,
                diagnostics, request.final_ephemeris_time_tdb_seconds);
        }
        else
        {
            // Preserve the authored elapsed duration itself. Reconstructing it later
            // by subtracting two large absolute ET values can add a sub-ULP tail.
            request.requested_duration_seconds =
                document.scenario.duration_seconds;
            request.final_ephemeris_time_tdb_seconds =
                request.start_ephemeris_time_tdb_seconds +
                document.scenario.duration_seconds;
        }
        const bool invalid_time_order =
            document.scenario.end_mode == EndMode::Duration
                ? !std::isfinite(document.scenario.duration_seconds) ||
                    document.scenario.duration_seconds <= 0.0
                : request.final_ephemeris_time_tdb_seconds <=
                    request.start_ephemeris_time_tdb_seconds;
        if (invalid_time_order)
            AddError(diagnostics, "SCN-TIME-ORDER", "scenario",
                "The final epoch must be later than the start epoch.");

        request.initial_state.position_icrf_m = document.initial_state.position_icrf_m;
        request.initial_state.velocity_icrf_mps = document.initial_state.velocity_icrf_mps;
        request.initial_state.attitude_body_to_icrf =
            document.initial_state.attitude_body_to_icrf;
        request.initial_state.angular_velocity_body_radps =
            document.initial_state.angular_velocity_body_radps;

        std::vector<std::size_t> order;
        std::unordered_map<std::string, std::size_t> source_by_name;
        double total_initial_mass_kg = 0.0;
        double minimum_reachable_mass_kg = 0.0;
        bool component_masses_valid = true;
        for (std::size_t index = 0; index < document.components.size(); ++index)
        {
            const Component& component = document.components[index];
            const std::string path =
                "components[" + std::to_string(index) + "]";
            const bool initial_mass_valid =
                std::isfinite(component.initial_mass_kg) &&
                component.initial_mass_kg >= 0.0;
            const bool minimum_mass_valid =
                std::isfinite(component.minimum_mass_kg) &&
                component.minimum_mass_kg >= 0.0;

            if (!initial_mass_valid)
                AddError(diagnostics, "SCN-COMPONENT-MASS",
                    path + ".initial_mass_kg",
                    "Initial component mass must be finite and nonnegative.");
            if (!minimum_mass_valid)
                AddError(diagnostics, "SCN-COMPONENT-MASS",
                    path + ".minimum_mass_kg",
                    "Minimum component mass must be finite and nonnegative.");
            if (initial_mass_valid && minimum_mass_valid &&
                component.minimum_mass_kg > component.initial_mass_kg)
                AddError(diagnostics, "SCN-COMPONENT-MASS",
                    path + ".minimum_mass_kg",
                    "Minimum component mass cannot exceed initial component mass.");

            const bool mass_range_valid = initial_mass_valid &&
                minimum_mass_valid &&
                component.minimum_mass_kg <= component.initial_mass_kg;
            component_masses_valid &= mass_range_valid;
            if (!mass_range_valid) continue;

            total_initial_mass_kg += component.initial_mass_kg;
            minimum_reachable_mass_kg += component.variable_mass
                ? component.minimum_mass_kg
                : component.initial_mass_kg;
        }
        if (component_masses_valid && !document.components.empty())
        {
            if (!std::isfinite(total_initial_mass_kg) ||
                total_initial_mass_kg <= 0.0)
                AddError(diagnostics, "SCN-SPACECRAFT-INITIAL-MASS",
                    "components",
                    "Total initial spacecraft mass must be finite and greater than zero.");
            if (!std::isfinite(minimum_reachable_mass_kg) ||
                minimum_reachable_mass_kg <= 0.0)
                AddError(diagnostics, "SCN-SPACECRAFT-MINIMUM-MASS",
                    "components",
                    "Minimum reachable spacecraft mass must be finite and greater than zero.");
        }
        BuildComponentOrder(document, diagnostics, order, source_by_name);

        std::vector<std::size_t> backend_by_source(
            document.components.size(), kInvalidIndex);
        std::unordered_map<std::string, std::size_t> backend_by_name;
        std::unordered_map<std::string, std::size_t> backend_by_id;
        std::size_t variable_mass_index = 0;
        std::size_t articulation_count = 0;
        for (const std::size_t source_index : order)
        {
            const Component& source = document.components[source_index];
            ComponentDefinition target;
            target.name = source.name;
            target.initial_mass_kg = source.initial_mass_kg;
            target.minimum_mass_kg = source.minimum_mass_kg;
            target.variable_mass_state_index = source.variable_mass
                ? variable_mass_index++
                : kInvalidIndex;
            target.center_of_mass_component_m = source.local_center_of_mass_m;
            target.inertia_centroid_component_kgm2 = InertiaMatrix(
                source.centroidal_inertia);
            target.origin_body_m = source.origin_body_m;
            target.component_to_body =
                source.component_to_body.Normalized().ToRotationMatrix();

            if (!source.parent_component_name.empty())
            {
                const auto parent = backend_by_name.find(
                    Lower(source.parent_component_name));
                if (parent != backend_by_name.end())
                    target.parent_component_index = parent->second;
                target.articulation_to_parent.parent_anchor_component_m =
                    source.parent_anchor_m;
                target.articulation_to_parent.child_anchor_component_m =
                    source.child_anchor_m;
                target.articulation_to_parent.child_to_parent_at_zero =
                    source.child_to_parent_zero_orientation.Normalized().ToRotationMatrix();
            }
            for (std::size_t dof_index = 0;
                 dof_index < source.degrees_of_freedom.size(); ++dof_index)
            {
                const JointDof& source_dof = source.degrees_of_freedom[dof_index];
                ArticulationDof target_dof;
                target_dof.name = source_dof.name;
                target_dof.motion = source_dof.motion == JointMotion::Translation
                    ? ArticulationMotion::Translation
                    : ArticulationMotion::Rotation;
                IsUnitDirection(
                    source_dof.axis,
                    "components[" + std::to_string(source_index) + "].dofs[" +
                        std::to_string(dof_index) + "].axis",
                    diagnostics, target_dof.axis_joint);
                target_dof.initial_coordinate = source_dof.initial_coordinate;
                target_dof.initial_rate = source_dof.initial_rate;
                target_dof.limits.minimum_coordinate =
                    source_dof.minimum_coordinate.value_or(
                        -std::numeric_limits<double>::infinity());
                target_dof.limits.maximum_coordinate =
                    source_dof.maximum_coordinate.value_or(
                        std::numeric_limits<double>::infinity());
                target_dof.limits.maximum_absolute_rate =
                    source_dof.maximum_absolute_rate;
                target_dof.limits.maximum_absolute_effort =
                    source_dof.maximum_absolute_effort;
                target.articulation_to_parent.dofs.push_back(std::move(target_dof));
                ++articulation_count;
            }

            const std::size_t backend_index = request.vehicle.components.size();
            backend_by_source[source_index] = backend_index;
            backend_by_name[Lower(source.name)] = backend_index;
            if (!source.id.empty()) backend_by_id[Lower(source.id)] = backend_index;
            request.vehicle.components.push_back(std::move(target));
        }

        for (std::size_t index = 0; index < document.thrusters.size(); ++index)
        {
            const Thruster& source = document.thrusters[index];
            const std::string path = "thrusters[" + std::to_string(index) + "]";
            ThrusterDefinition target;
            target.name = source.name;
            target.mode = source.mode;
            const auto mount = backend_by_name.find(Lower(source.mount_component_name));
            const auto tank = backend_by_name.find(Lower(source.propellant_component_name));
            if (mount == backend_by_name.end())
                AddError(diagnostics, "SCN-THRUSTER-MOUNT", path + ".mount_component",
                    "The mount component does not exist.");
            else
                target.component_index = mount->second;
            if (tank == backend_by_name.end())
                AddError(diagnostics, "SCN-THRUSTER-TANK", path + ".propellant_component",
                    "The propellant component does not exist.");
            else
            {
                target.propellant_component_index = tank->second;
                if (request.vehicle.components[tank->second].variable_mass_state_index ==
                    kInvalidIndex)
                    AddError(diagnostics, "SCN-THRUSTER-TANK", path + ".propellant_component",
                        "Every thruster must deplete a variable-mass component.");
            }
            target.application_point_component_m =
                source.application_point_component_m;
            IsUnitDirection(source.direction_component, path + ".direction_component",
                diagnostics, target.direction_component);

            if (source.ignition_time_mode == ThrusterTimeMode::Utc)
            {
                ConvertUtc(source.ignition_utc, path + ".ignition_utc", services,
                    diagnostics, target.ignition_ephemeris_time_tdb_seconds);
                target.ignition_elapsed_time_seconds =
                    target.ignition_ephemeris_time_tdb_seconds -
                    request.start_ephemeris_time_tdb_seconds;
            }
            else
            {
                target.ignition_elapsed_time_seconds =
                    source.ignition_elapsed_seconds;
                target.ignition_ephemeris_time_tdb_seconds =
                    request.start_ephemeris_time_tdb_seconds +
                    target.ignition_elapsed_time_seconds;
            }
            if (source.never_shuts_down)
            {
                target.shutdown_ephemeris_time_tdb_seconds =
                    std::numeric_limits<double>::infinity();
                target.shutdown_elapsed_time_seconds =
                    std::numeric_limits<double>::infinity();
            }
            else if (source.shutdown_time_mode == ThrusterTimeMode::Utc)
            {
                ConvertUtc(source.shutdown_utc, path + ".shutdown_utc", services,
                    diagnostics, target.shutdown_ephemeris_time_tdb_seconds);
                target.shutdown_elapsed_time_seconds =
                    target.shutdown_ephemeris_time_tdb_seconds -
                    request.start_ephemeris_time_tdb_seconds;
            }
            else
            {
                target.shutdown_elapsed_time_seconds =
                    source.shutdown_elapsed_seconds;
                target.shutdown_ephemeris_time_tdb_seconds =
                    request.start_ephemeris_time_tdb_seconds +
                    target.shutdown_elapsed_time_seconds;
            }
            if (!source.never_shuts_down &&
                target.shutdown_elapsed_time_seconds <=
                    target.ignition_elapsed_time_seconds)
                AddError(diagnostics, "SCN-THRUSTER-TIME", path,
                    "Shutdown must occur after ignition.");

            if (source.mode == ThrusterMode::PrescribedProfile)
            {
                LoadScalarCurve(source.prescribed_thrust, true, scenario_file_path,
                    path + ".prescribed_thrust", diagnostics,
                    target.constant_thrust_n, target.thrust_profile_n);
                LoadScalarCurve(source.prescribed_specific_impulse, false,
                    scenario_file_path, path + ".prescribed_specific_impulse",
                    diagnostics, target.constant_specific_impulse_seconds,
                    target.specific_impulse_profile_seconds);
            }
            else
            {
                target.maximum_thrust_n = source.maximum_thrust_n;
                if (!(source.maximum_thrust_n > 0.0))
                    AddError(diagnostics, "SCN-THRUSTER-MAX", path + ".maximum_thrust_n",
                        "A commanded thruster needs positive maximum thrust.");
            }
            request.vehicle.thrusters.push_back(std::move(target));
        }

        for (std::size_t index = 0; index < document.reaction_wheels.size(); ++index)
        {
            const ReactionWheel& source = document.reaction_wheels[index];
            const std::string path = "reaction_wheels[" + std::to_string(index) + "]";
            ReactionWheelDefinition target;
            target.name = source.name;
            const auto mount = backend_by_name.find(Lower(source.mount_component_name));
            if (mount == backend_by_name.end())
                AddError(diagnostics, "SCN-WHEEL-MOUNT", path + ".mount_component",
                    "The mount component does not exist.");
            else
                target.component_index = mount->second;
            IsUnitDirection(source.axis_component, path + ".axis_component",
                diagnostics, target.axis_component);
            target.initial_momentum_nms = source.initial_momentum_nms;
            target.maximum_momentum_nms = source.maximum_absolute_momentum_nms;
            if (!(target.maximum_momentum_nms > 0.0))
                AddError(diagnostics, "SCN-WHEEL-LIMIT",
                    path + ".maximum_absolute_momentum_nms",
                    "Maximum absolute wheel momentum must be positive.");
            request.vehicle.reaction_wheels.push_back(std::move(target));
        }

        request.gravity.include_first_post_newtonian_correction =
            document.include_first_post_newtonian_correction;
        request.gravity.ephemeris_provider = services.ephemeris_provider;
        std::unordered_set<std::string> body_keys;
        for (std::size_t index = 0; index < document.celestial_bodies.size(); ++index)
        {
            const CelestialBody& source = document.celestial_bodies[index];
            const std::string path =
                "gravity.bodies[" + std::to_string(index) + "]";
            const CelestialCatalogEntry* catalog =
                FindCelestialCatalogEntry(source.catalog_key);
            if (catalog == nullptr)
            {
                AddError(diagnostics, "SCN-GRAVITY-CATALOG", path + ".catalog_key",
                    "The catalog key is unknown.");
                continue;
            }
            if (!body_keys.insert(Lower(catalog->catalog_key)).second)
            {
                AddError(diagnostics, "SCN-GRAVITY-DUPLICATE", path + ".catalog_key",
                    "A catalog body may appear only once.");
                continue;
            }
            GravityBody target;
            target.name = catalog->spice_target;
            target.naif_id = catalog->naif_id;
            // Catalog group keys also organize independent HUD entries such as
            // the Sun. Runtime barycenter grouping applies only to barycenters
            // and their physical members.
            target.gravity_system_name =
                catalog->source_role == GravitySourceRole::Independent
                    ? std::string{}
                    : catalog->group_key;
            target.gravity_source_role = catalog->source_role;
            target.gravity_enabled = source.gravity_enabled;
            target.automatic_gravity_activation_radius_m =
                source.automatic_activation_radius_m;
            target.barycenter_resolution_radius_m =
                source.barycenter_resolution_radius_m;
            LoadHarmonicModel(
                source, *catalog, scenario_file_path,
                path + ".harmonic_model_csv_file", diagnostics, target);
            request.gravity.bodies.push_back(std::move(target));
        }
        if (!request.gravity.bodies.empty() && !services.ephemeris_provider)
            AddError(diagnostics, "SCN-SPICE-PROVIDER", "gravity",
                "A SPICE ephemeris provider is required for configured celestial bodies.");

        const SolarRadiationPressure& source_srp =
            document.solar_radiation_pressure;
        request.solar_radiation.enabled = source_srp.enabled;
        request.solar_radiation.sun_body_name = ResolveSpiceName(
            source_srp.sun_body_name, "srp.sun_body", diagnostics);
        request.solar_radiation.pressure_at_one_au_pa =
            source_srp.pressure_at_one_au_pa;
        request.solar_radiation.compute_eclipse_shadow = source_srp.compute_eclipse;
        request.solar_radiation.compute_component_shadows =
            source_srp.compute_component_shadows;
        for (std::size_t index = 0;
             index < source_srp.occulting_body_names.size(); ++index)
            request.solar_radiation.occulting_body_names.push_back(ResolveSpiceName(
                source_srp.occulting_body_names[index],
                "srp.occulting_bodies[" + std::to_string(index) + "]", diagnostics));
        for (std::size_t index = 0; index < source_srp.optical_facets.size(); ++index)
        {
            const OpticalFacet& source = source_srp.optical_facets[index];
            const std::string path = "srp.facets[" + std::to_string(index) + "]";
            std::size_t component_index = kInvalidIndex;
            if (!source.component_id.empty())
            {
                const auto found = backend_by_id.find(Lower(source.component_id));
                if (found != backend_by_id.end()) component_index = found->second;
            }
            if (component_index == kInvalidIndex && !source.component_name.empty())
            {
                const auto found = backend_by_name.find(Lower(source.component_name));
                if (found != backend_by_name.end()) component_index = found->second;
            }
            if (component_index == kInvalidIndex)
            {
                AddError(diagnostics, "SCN-SRP-COMPONENT", path,
                    "The facet cannot be matched to a component ID or name.");
                continue;
            }
            const OpticalProperties& optics = source.optical_properties;
            const double optical_sum = optics.absorption +
                optics.specular_reflection + optics.diffuse_reflection;
            if (optics.absorption < 0.0 || optics.specular_reflection < 0.0 ||
                optics.diffuse_reflection < 0.0 ||
                std::abs(optical_sum - 1.0) > 1.0e-9)
                AddError(diagnostics, "SCN-SRP-OPTICS", path + ".optics",
                    "Opaque optical fractions must be nonnegative and sum to one.");
            tgsim::OpticalFacet target;
            target.name = source.name;
            target.component_index = component_index;
            target.vertices_component_m = {
                source.vertex0_component_m,
                source.vertex1_component_m,
                source.vertex2_component_m};
            target.absorption = optics.absorption;
            target.specular_reflection = optics.specular_reflection;
            target.diffuse_reflection = optics.diffuse_reflection;
            request.solar_radiation.facets.push_back(std::move(target));
        }
        if (source_srp.enabled && request.solar_radiation.facets.empty())
            AddError(diagnostics, "SCN-SRP-FACETS", "srp.facets",
                "Enabled SRP requires a generated proxy facet array.");

        const Atmosphere& source_atmosphere = document.atmosphere;
        request.atmosphere.enabled = source_atmosphere.enabled;
        request.atmosphere.central_body_name = ResolveSpiceName(
            source_atmosphere.central_body_name,
            "atmosphere.central_body", diagnostics);
        request.atmosphere.model_kind =
            source_atmosphere.model == AtmosphereModel::CubicHarrisPriesterEarth
                ? AtmosphereModelKind::CubicHarrisPriesterEarth
                : AtmosphereModelKind::TabulatedProfile;
        if (source_atmosphere.enabled)
        {
            if (source_atmosphere.model == AtmosphereModel::UploadedProfile)
                LoadAtmosphereProfile(
                    source_atmosphere, scenario_file_path,
                    diagnostics, request.atmosphere);
            else
                LoadCubicHarrisPriester(
                    source_atmosphere, scenario_file_path,
                    diagnostics, request.atmosphere);
            request.atmosphere.cubic_harris_priester.centered_average_f107_sfu =
                source_atmosphere.centered_average_f107_sfu;
        }

        const Aerodynamics& source_aero = document.aerodynamics;
        request.aerodynamics.enabled = source_aero.enabled;
        request.aerodynamics.reference_area_m2 = source_aero.reference_area_m2;
        request.aerodynamics.reference_length_m = source_aero.reference_length_m;
        request.aerodynamics.minimum_dynamic_pressure_pa =
            source_aero.minimum_dynamic_pressure_pa;
        request.aerodynamics.maximum_dynamic_pressure_pa =
            source_aero.maximum_valid_dynamic_pressure_pa;
        request.aerodynamics.constant_drag_fallback_enabled =
            source_aero.enabled && source_aero.constant_drag_fallback_enabled;
        request.aerodynamics.fallback_drag_coefficient =
            source_aero.fallback_drag_coefficient;
        request.aerodynamics.coefficient_database.enabled =
            source_aero.enabled && source_aero.database.enabled;
        request.aerodynamics.coefficient_database.moment_reference_point_body_m =
            source_aero.database.moment_reference_center_body_m;
        request.aerodynamics.coefficient_database.interpolation =
            source_aero.database.interpolation == AerodynamicInterpolation::NearestRow
                ? AerodynamicDatabaseInterpolationMethod::NearestNeighbor
                : AerodynamicDatabaseInterpolationMethod::InverseDistance;
        request.aerodynamics.coefficient_database.extrapolation =
            source_aero.database.extrapolation == AerodynamicExtrapolation::NearestRow
                ? AerodynamicDatabaseExtrapolationMethod::NearestNeighbor
                : AerodynamicDatabaseExtrapolationMethod::UseConstantDragFallback;
        request.aerodynamics.coefficient_database.nearest_neighbor_count =
            source_aero.database.neighbor_count;
        request.aerodynamics.coefficient_database.inverse_distance_power =
            source_aero.database.inverse_distance_power;
        request.aerodynamics.coefficient_database.maximum_normalized_neighbor_distance =
            source_aero.database.maximum_normalized_neighbor_distance.value_or(
                std::numeric_limits<double>::infinity());
        if (source_aero.enabled && source_aero.database.enabled &&
            source_aero.database.interpolation ==
                AerodynamicInterpolation::InverseDistance &&
            source_aero.database.neighbor_count == 0)
            AddError(diagnostics, "SCN-AERO-NEIGHBOR-COUNT",
                "aerodynamics.database.neighbor_count",
                "Inverse-distance interpolation requires a positive neighbor count.");
        if (source_aero.enabled && source_aero.database.enabled)
            LoadAerodynamicDatabase(
                source_aero.database, articulation_count, scenario_file_path,
                diagnostics, request.aerodynamics.coefficient_database);

        if (document.control.mode == ControlMode::CompiledUserController)
        {
            if (!services.controller)
                AddError(diagnostics, "SCN-CONTROLLER", "control",
                    "The selected compiled controller was not loaded.");
            request.control.controller = services.controller;
        }

        return !HasErrors(diagnostics);
    }
}
