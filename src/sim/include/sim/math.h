#pragma once

#include <cmath>

// Every transcendental function the sim uses goes through here instead of
// calling <cmath> directly. Today this just forwards to libm; the point is
// a single call site to swap in a deterministic implementation later if
// cross-platform bit-exactness ever becomes a hard requirement. It is NOT
// promised now -- libm's sin/cos/etc. can differ in the last bit or two
// between platforms/compilers, which is why the project's determinism
// promise is "same binary, same inputs, same outputs, bit for bit," not
// "same across platforms." A hash-printing tool (tools/sim_headless)
// exists so cross-platform hash comparisons can be measured, not assumed.
//
// Sim state is double throughout (spec: "Physics state in double, render
// data in float"), so every wrapper here operates on double.
namespace sim::math {

inline double sin(double x) { return std::sin(x); }
inline double cos(double x) { return std::cos(x); }
inline double tan(double x) { return std::tan(x); }
inline double asin(double x) { return std::asin(x); }
inline double acos(double x) { return std::acos(x); }
inline double atan(double x) { return std::atan(x); }
inline double atan2(double y, double x) { return std::atan2(y, x); }
inline double exp(double x) { return std::exp(x); }
inline double log(double x) { return std::log(x); }
inline double pow(double base, double exponent) { return std::pow(base, exponent); }
inline double sqrt(double x) { return std::sqrt(x); }
inline double fabs(double x) { return std::fabs(x); }
inline double fmod(double x, double y) { return std::fmod(x, y); }

inline constexpr double kPi = 3.14159265358979323846;
inline constexpr double kTwoPi = 2.0 * kPi;

}  // namespace sim::math
