#pragma once

#include "sim/math.h"
#include "sim/vec3.h"

// Ambient conditions that affect ball flight. Separate from BallProperties
// because these describe the air/field, not the ball itself, and are the
// natural place for a future "weather preset" to live (milestone 9/10).
namespace sim {

struct Environment {
    // Air density is derived (see air_density_kgpm3 below) rather than
    // stored redundantly, but the inputs the spec calls for ("Air density
    // from temperature, pressure, humidity, altitude") are kept here so the
    // property editor (milestone 9) has real physical knobs instead of a
    // single opaque density number.
    double temperature_celsius = 20.0;
    double pressure_pa = 101325.0;     // sea-level standard pressure
    double relative_humidity = 0.5;    // 0..1
    double altitude_m = 0.0;           // only used if pressure_pa is left at sea-level std

    // Constant wind for now (milestone 4 scope). Height-dependent profile
    // and gusts are milestone 10 polish ("wind" is listed both as a base
    // flight input and again under milestone 10 polish -- gust/turbulence
    // modeling from the sim RNG is the part deferred).
    Vec3 wind_mps{0.0, 0.0, 0.0};

    // Magnus/drag reference: dynamic viscosity of air, Sutherland-ish
    // constant approximation good enough near typical game-day temperatures
    // (doesn't vary enough between 0-35 C to matter for Reynolds number
    // here). Pa*s.
    double dynamic_viscosity_pas = 1.81e-5;

    // Computes air density via the ideal-gas-with-humidity formula (e.g.
    // Picard et al. 2008 CIPM-2007 simplified form): moist air density is
    // the sum of dry-air and water-vapor partial densities, each via
    // rho = p / (R_specific * T). Altitude adjusts pressure using the
    // standard barometric formula only when pressure_pa is left at the
    // sea-level default (an explicit pressure reading always wins).
    double air_density_kgpm3() const {
        constexpr double kRSpecificDryAir = 287.058;       // J/(kg*K)
        constexpr double kRSpecificWaterVapor = 461.495;   // J/(kg*K)
        const double t_kelvin = temperature_celsius + 273.15;

        double pressure = pressure_pa;
        if (pressure == 101325.0 && altitude_m != 0.0) {
            // Barometric formula, troposphere (valid to ~11 km).
            constexpr double kLapseRate = 0.0065;    // K/m
            constexpr double kSeaLevelTemp = 288.15; // K
            constexpr double kGasExponent = 5.25588; // g*M/(R*L)
            pressure = 101325.0 * math::pow(1.0 - (kLapseRate * altitude_m) / kSeaLevelTemp, kGasExponent);
        }

        // Saturation vapor pressure (Tetens' formula, valid -40..50 C), Pa.
        const double p_sat = 610.78 * math::exp((17.27 * temperature_celsius) / (temperature_celsius + 237.3));
        const double p_vapor = relative_humidity * p_sat;
        const double p_dry = pressure - p_vapor;

        return (p_dry / (kRSpecificDryAir * t_kelvin)) + (p_vapor / (kRSpecificWaterVapor * t_kelvin));
    }
};

}  // namespace sim
