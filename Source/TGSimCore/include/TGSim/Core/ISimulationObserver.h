// Copyright (c) 2026 Breno Raiher.
// SPDX-License-Identifier: MIT

#pragma once

// Optional reporting interface. It observes a run but never participates in physics.

#include <string>

namespace tgsim
{
    class ISimulationObserver
    {
    public:
        virtual ~ISimulationObserver() = default;

        /// Receives completion percentage and a short current-stage description.
        virtual void OnProgress(double percent, const std::string& status) = 0;
        /// Receives a human-readable simulation event or diagnostic message.
        virtual void OnLog(const std::string& message) = 0;
        /// Polled by the engine and adaptive integrator. Unreal may implement this
        /// with an atomic flag set by a Cancel button.
        virtual bool IsCancellationRequested() const { return false; }
    };
}
