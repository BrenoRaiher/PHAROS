// Copyright (c) 2026 Breno Raiher.
// SPDX-License-Identifier: MIT

#include "StandaloneSpiceProvider.h"
#include "StandaloneControllerAdapter.h"

#include "TGSim/Scenario/ScenarioCompiler.h"
#include "TGSim/Scenario/ScenarioDiagnostics.h"
#include "TGSim/Scenario/ScenarioFile.h"
#include "TGSim/Core/ISimulationObserver.h"
#include "TGSim/Simulation/SimulationEngine.h"

#include <algorithm>
#include <chrono>
#include <filesystem>
#include <fstream>
#include <iomanip>
#include <iostream>
#include <memory>
#include <string>

#if defined(_WIN32)
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <Windows.h>
#else
#include <poll.h>
#include <unistd.h>
#endif

namespace
{
    struct Arguments
    {
        std::filesystem::path scenario_file;
        std::filesystem::path output_directory;
        std::filesystem::path kernel_directory;
        bool validate_only = false;
    };

    std::string ProtocolLine(std::string value)
    {
        std::replace(value.begin(), value.end(), '\r', ' ');
        std::replace(value.begin(), value.end(), '\n', ' ');
        std::replace(value.begin(), value.end(), '\t', ' ');
        return value;
    }

    void EmitEvent(const std::string& kind, const std::string& payload)
    {
        std::cout << "PHAROS_EVENT\t" << kind << "\t\t\t"
                  << ProtocolLine(payload) << '\n' << std::flush;
    }

    void EmitProgress(const double percent, const std::string& status)
    {
        std::cout << "PHAROS_EVENT\tPROGRESS\t"
                  << std::fixed << std::setprecision(3)
                  << std::clamp(percent, 0.0, 100.0) << '\t'
                  << ProtocolLine(status) << '\n' << std::flush;
    }

    void ReadAvailableStandardInput(std::string& pending_input)
    {
#if defined(_WIN32)
        const HANDLE input = GetStdHandle(STD_INPUT_HANDLE);
        if (input == nullptr || input == INVALID_HANDLE_VALUE ||
            GetFileType(input) != FILE_TYPE_PIPE)
        {
            return;
        }

        for (;;)
        {
            DWORD available = 0;
            if (!PeekNamedPipe(
                    input, nullptr, 0, nullptr, &available, nullptr) ||
                available == 0)
            {
                return;
            }

            char buffer[256];
            DWORD bytes_read = 0;
            const DWORD requested =
                std::min<DWORD>(available, sizeof(buffer));
            if (!ReadFile(
                    input,
                    buffer,
                    requested,
                    &bytes_read,
                    nullptr) ||
                bytes_read == 0)
            {
                return;
            }
            pending_input.append(buffer, bytes_read);
        }
#else
        pollfd descriptor{};
        descriptor.fd = STDIN_FILENO;
        descriptor.events = POLLIN;
        while (poll(&descriptor, 1, 0) > 0 &&
            (descriptor.revents & POLLIN) != 0)
        {
            char buffer[256];
            const ssize_t bytes_read = read(
                STDIN_FILENO, buffer, sizeof(buffer));
            if (bytes_read <= 0) return;
            pending_input.append(
                buffer, static_cast<std::size_t>(bytes_read));
            descriptor.revents = 0;
        }
#endif
    }

    bool ConsumeCancellationCommand(std::string& pending_input)
    {
        std::size_t newline = std::string::npos;
        while ((newline = pending_input.find('\n')) != std::string::npos)
        {
            std::string command = pending_input.substr(0, newline);
            pending_input.erase(0, newline + 1);
            if (!command.empty() && command.back() == '\r')
            {
                command.pop_back();
            }
            if (command == "CANCEL") return true;
        }
        return false;
    }

    class RunnerObserver final : public tgsim::ISimulationObserver
    {
    public:
        void OnProgress(
            const double backend_percent,
            const std::string& status) override
        {
            // Reserve 0-10% for runner setup and 95-100% for output commit.
            const double mapped_percent = 10.0 +
                0.85 * std::clamp(backend_percent, 0.0, 100.0);
            const auto now = std::chrono::steady_clock::now();
            const bool status_changed = status != last_status_;
            const bool enough_progress =
                mapped_percent >= last_percent_ + 0.1;
            const bool enough_time =
                now - last_emission_ >= std::chrono::milliseconds(100);
            if (!status_changed && !enough_progress && !enough_time &&
                mapped_percent < 100.0)
            {
                return;
            }

            last_percent_ = mapped_percent;
            last_status_ = status;
            last_emission_ = now;
            EmitProgress(mapped_percent, status);
        }

        void OnLog(const std::string& message) override
        {
            EmitEvent("LOG", message);
        }

        bool IsCancellationRequested() const override
        {
            if (cancellation_requested_) return true;

            // UE writes "CANCEL\n" to the child stdin pipe. Polling here keeps
            // shutdown deterministic; a detached blocking iostream reader can
            // otherwise hold the process after the result files reach 100%.
            ReadAvailableStandardInput(pending_input_);
            cancellation_requested_ =
                ConsumeCancellationCommand(pending_input_);
            return cancellation_requested_;
        }

    private:
        mutable std::string pending_input_;
        mutable bool cancellation_requested_ = false;
        double last_percent_ = -1.0;
        std::string last_status_;
        std::chrono::steady_clock::time_point last_emission_{};
    };

    void PrintUsage()
    {
        std::cout
            << "Usage: PHAROSScenarioRunner <scenario.tgscn> [options]\n"
            << "  --output <directory>     Output directory (default: scenario directory)\n"
            << "  --kernel-dir <directory> SPICE kernel directory\n"
            << "  --validate-only          Parse and compile without propagation\n";
    }

    bool ParseArguments(const int argc, char** argv, Arguments& arguments)
    {
        if (argc < 2) return false;
        std::error_code path_error;
        arguments.scenario_file =
            std::filesystem::absolute(argv[1], path_error).lexically_normal();
        if (path_error) arguments.scenario_file = argv[1];
        for (int index = 2; index < argc; ++index)
        {
            const std::string argument = argv[index];
            if (argument == "--validate-only")
            {
                arguments.validate_only = true;
            }
            else if ((argument == "--output" || argument == "--kernel-dir") &&
                     index + 1 < argc)
            {
                const std::filesystem::path value = argv[++index];
                if (argument == "--output") arguments.output_directory = value;
                else arguments.kernel_directory = value;
            }
            else
            {
                std::cerr << "Unknown or incomplete option: " << argument << '\n';
                return false;
            }
        }
        if (arguments.output_directory.empty())
            arguments.output_directory = arguments.scenario_file.parent_path();
        else
            arguments.output_directory =
                std::filesystem::absolute(
                    arguments.output_directory, path_error).lexically_normal();
        if (arguments.kernel_directory.empty())
        {
            const std::filesystem::path executable_directory =
                std::filesystem::absolute(argv[0], path_error).parent_path();
            const std::filesystem::path candidates[] = {
                executable_directory / "SPICEKernels",
                executable_directory / ".." / ".." / ".." /
                    "Content" / "SPICEKernels",
                std::filesystem::current_path() / "Content" / "SPICEKernels"};
            arguments.kernel_directory = candidates[2];
            for (const std::filesystem::path& candidate : candidates)
            {
                if (std::filesystem::is_directory(candidate))
                {
                    arguments.kernel_directory = candidate.lexically_normal();
                    break;
                }
            }
        }
        else
            arguments.kernel_directory =
                std::filesystem::absolute(
                    arguments.kernel_directory, path_error).lexically_normal();
        return true;
    }

    bool CommitTemporaryFile(
        const std::filesystem::path& temporary_path,
        const std::filesystem::path& final_path,
        std::string& error)
    {
        std::error_code filesystem_error;
        std::filesystem::remove(final_path, filesystem_error);
        filesystem_error.clear();
        std::filesystem::rename(
            temporary_path, final_path, filesystem_error);
        if (filesystem_error)
        {
            error = "Could not commit " + final_path.string() + ": " +
                filesystem_error.message();
            std::filesystem::remove(temporary_path, filesystem_error);
            return false;
        }
        return true;
    }

    bool WriteSolutionCsv(
        const std::filesystem::path& path,
        const tgsim::SimulationResult& result,
        std::string& error)
    {
        std::filesystem::path temporary_path = path;
        temporary_path += ".tmp";
        std::error_code remove_error;
        std::filesystem::remove(temporary_path, remove_error);

        {
            std::ofstream output(
                temporary_path, std::ios::binary | std::ios::trunc);
            if (!output)
            {
                error = "Could not create " + temporary_path.string();
                return false;
            }
            for (std::size_t column = 0;
                 column < result.column_names.size(); ++column)
            {
                if (column != 0) output << ',';
                output << '"';
                for (const char character : result.column_names[column])
                {
                    if (character == '"') output << '"';
                    output << character;
                }
                output << '"';
            }
            output << '\n' << std::setprecision(17);
            for (const std::vector<double>& row : result.solution_array)
            {
                for (std::size_t column = 0; column < row.size(); ++column)
                {
                    if (column != 0) output << ',';
                    output << row[column];
                }
                output << '\n';
            }
            output.close();
            if (!output)
            {
                error = "Could not finish writing " +
                    temporary_path.string();
                std::filesystem::remove(temporary_path, remove_error);
                return false;
            }
        }

        return CommitTemporaryFile(temporary_path, path, error);
    }

    bool WriteSummary(
        const std::filesystem::path& path,
        const tgsim::SimulationResult& result,
        std::string& error)
    {
        std::filesystem::path temporary_path = path;
        temporary_path += ".tmp";
        std::error_code remove_error;
        std::filesystem::remove(temporary_path, remove_error);

        {
            std::ofstream output(
                temporary_path, std::ios::binary | std::ios::trunc);
            if (!output)
            {
                error = "Could not create " + temporary_path.string();
                return false;
            }
            output << "success=" << (result.success ? "true" : "false") << '\n';
            output << "message=" << result.message << '\n';
            output << "sample_count=" << result.samples.size() << '\n';
            output << "column_count=" << result.column_names.size() << '\n';
            output.close();
            if (!output)
            {
                error = "Could not finish writing " +
                    temporary_path.string();
                std::filesystem::remove(temporary_path, remove_error);
                return false;
            }
        }
        return CommitTemporaryFile(temporary_path, path, error);
    }
}

int main(const int argc, char** argv)
{
    std::cout.setf(std::ios::unitbuf);
    Arguments arguments;
    if (!ParseArguments(argc, argv, arguments))
    {
        PrintUsage();
        return 2;
    }

    EmitProgress(1.0, "Loading scenario file");
    tgsim::scenario::ScenarioDocument document;
    tgsim::scenario::Diagnostics diagnostics;
    if (!tgsim::scenario::LoadScenarioFile(
            arguments.scenario_file, document, diagnostics))
    {
        std::cerr << tgsim::scenario::FormatDiagnostics(diagnostics);
        return 3;
    }

    EmitProgress(3.0, "Loading SPICE kernels");
    auto spice = std::make_shared<StandaloneSpiceProvider>();
    std::string error;
    if (!spice->LoadKernels(arguments.kernel_directory, error))
    {
        std::cerr << error << '\n';
        return 5;
    }

    tgsim::scenario::ScenarioCompilerServices services;
    services.ephemeris_provider = spice;
    services.utc_to_ephemeris_time =
        [spice](const std::string& utc, double& et, std::string& conversion_error)
        {
            return spice->ConvertUtcToEphemerisTime(utc, et, conversion_error);
        };

    std::shared_ptr<StandaloneControllerAdapter> controller;
    if (document.control.mode ==
        tgsim::scenario::ControlMode::CompiledUserController)
    {
        EmitProgress(5.0, "Loading user controller");
        const std::filesystem::path controller_path =
            tgsim::scenario::ResolveReferencedPath(
                arguments.scenario_file,
                document.control.controller_dll_path);
        controller = StandaloneControllerAdapter::Create(
            controller_path, error);
        if (controller == nullptr)
        {
            std::cerr << error << '\n';
            return 4;
        }
        services.controller = controller;
    }

    EmitProgress(7.0, "Compiling SimulationRequest");
    tgsim::SimulationRequest request;
    if (!tgsim::scenario::CompileScenario(
            document, arguments.scenario_file, services, request, diagnostics))
    {
        std::cerr << tgsim::scenario::FormatDiagnostics(diagnostics);
        return 6;
    }
    const std::string warnings = tgsim::scenario::FormatDiagnostics(diagnostics);
    if (!warnings.empty()) std::cerr << warnings;
    if (arguments.validate_only)
    {
        const std::string validation_error =
            tgsim::SimulationEngine().ValidateRequest(request);
        if (!validation_error.empty())
        {
            std::cerr << "SimulationRequest validation failed: "
                      << validation_error << '\n';
            return 6;
        }
        std::cout << "Scenario is valid and compiles to SimulationRequest.\n";
        return 0;
    }

    EmitProgress(9.0, "Preparing output directory");
    std::error_code directory_error;
    std::filesystem::create_directories(arguments.output_directory, directory_error);
    if (directory_error)
    {
        std::cerr << "Could not create output directory: "
                  << directory_error.message() << '\n';
        return 7;
    }

    RunnerObserver observer;
    const tgsim::SimulationResult result =
        tgsim::SimulationEngine().Run(request, &observer);
    if (!result.success)
    {
        EmitEvent("ERROR", result.message);
    }
    const std::string stem = arguments.scenario_file.stem().string();
    const std::filesystem::path csv_path =
        arguments.output_directory / (stem + "_solution.csv");
    const std::filesystem::path summary_path =
        arguments.output_directory / (stem + "_summary.txt");
    EmitProgress(96.0, "Writing result files");
    if (!WriteSolutionCsv(csv_path, result, error) ||
        !WriteSummary(summary_path, result, error))
    {
        std::cerr << error << '\n';
        return 8;
    }
    EmitProgress(100.0, result.success
        ? "Simulation complete"
        : "Simulation failed");
    EmitEvent("RESULT", csv_path.string());
    std::cout << result.message << '\n'
              << "Solution: " << csv_path.string() << '\n'
              << "Summary: " << summary_path.string() << '\n';
    return result.success ? 0 : 9;
}
