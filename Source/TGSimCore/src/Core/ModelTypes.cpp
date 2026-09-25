// Copyright (c) 2026 Breno Raiher.
// SPDX-License-Identifier: MIT

#include "TGSim/Core/ModelTypes.h"

// Implements reusable configuration-data behavior, currently scalar time curves.

#include <algorithm>

namespace tgsim
{
    namespace
    {
        double LinearSegmentValue(
            const ScalarSample& a,
            const ScalarSample& b,
            double curve_time_seconds)
        {
            const double span = b.time_seconds - a.time_seconds;
            if (span <= 0.0) return a.value;
            // y(t) = y_a + [(t-t_a)/(t_b-t_a)] (y_b-y_a).
            const double alpha = (curve_time_seconds - a.time_seconds) / span;
            return a.value + alpha * (b.value - a.value);
        }

        double LinearSegmentRate(
            const ScalarSample& a,
            const ScalarSample& b)
        {
            const double span = b.time_seconds - a.time_seconds;
            const double value_span = b.value - a.value;
            if (!std::isfinite(span) || span <= 0.0 ||
                !std::isfinite(value_span))
            {
                return 0.0;
            }
            return value_span / span;
        }

        bool HasValidTimeAxis(const std::vector<ScalarSample>& samples)
        {
            for (std::size_t index = 0; index < samples.size(); ++index)
            {
                if (!std::isfinite(samples[index].time_seconds) ||
                    (index > 0 && samples[index].time_seconds <=
                        samples[index - 1].time_seconds))
                {
                    return false;
                }
            }
            return true;
        }
    }

    double ScalarCurve::ValueAt(double curve_time_seconds) const
    {
        if (samples.empty()) return default_value;
        if (samples.size() == 1) return samples.front().value;

        // Curves are a public input contract. A malformed time axis is rejected by
        // SimulationEngine validation; this guard keeps direct callers deterministic.
        if (!HasValidTimeAxis(samples)) return default_value;

        if (curve_time_seconds < samples.front().time_seconds)
        {
            if (extrapolation == ScalarExtrapolationMethod::UseDefaultValue) return default_value;
            if (extrapolation == ScalarExtrapolationMethod::ExtendEndpointSlope)
                return LinearSegmentValue(samples[0], samples[1], curve_time_seconds);
            return samples.front().value;
        }
        if (curve_time_seconds > samples.back().time_seconds)
        {
            if (extrapolation == ScalarExtrapolationMethod::UseDefaultValue) return default_value;
            if (extrapolation == ScalarExtrapolationMethod::ExtendEndpointSlope)
                return LinearSegmentValue(samples[samples.size() - 2], samples.back(), curve_time_seconds);
            return samples.back().value;
        }
        if (curve_time_seconds == samples.front().time_seconds) return samples.front().value;
        if (curve_time_seconds == samples.back().time_seconds) return samples.back().value;

        const auto upper = std::upper_bound(
            samples.begin(), samples.end(), curve_time_seconds,
            [](double time, const ScalarSample& sample) { return time < sample.time_seconds; });
        const ScalarSample& b = *upper;
        const ScalarSample& a = *(upper - 1);
        return LinearSegmentValue(a, b, curve_time_seconds);
    }

    double ScalarCurve::RateAt(double curve_time_seconds) const
    {
        if (samples.size() < 2 || !std::isfinite(curve_time_seconds) ||
            !HasValidTimeAxis(samples))
        {
            return 0.0;
        }

        if (curve_time_seconds < samples.front().time_seconds)
        {
            return extrapolation ==
                    ScalarExtrapolationMethod::ExtendEndpointSlope
                ? LinearSegmentRate(samples[0], samples[1])
                : 0.0;
        }
        if (curve_time_seconds >= samples.back().time_seconds)
        {
            return extrapolation ==
                    ScalarExtrapolationMethod::ExtendEndpointSlope
                ? LinearSegmentRate(
                    samples[samples.size() - 2], samples.back())
                : 0.0;
        }

        // upper_bound selects the segment to the right at every interior knot.
        const auto upper = std::upper_bound(
            samples.begin(), samples.end(), curve_time_seconds,
            [](double time, const ScalarSample& sample)
            {
                return time < sample.time_seconds;
            });
        return LinearSegmentRate(*(upper - 1), *upper);
    }
}
