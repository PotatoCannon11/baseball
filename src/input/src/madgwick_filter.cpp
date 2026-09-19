#include "input/madgwick_filter.h"

#include <cmath>

namespace input {

// The original reference implementation uses a fast approximate inverse
// square root (the classic Quake trick) because it targets embedded
// microcontrollers running this at high frequency. We run this in the
// input layer on a desktop CPU at a few hundred Hz at most, so there's no
// need for that approximation's ~0.2% error budget -- plain 1/sqrt(x) is
// cheap enough and more accurate.
float MadgwickFilter::inv_sqrt(float x) { return 1.0f / std::sqrt(x); }

}  // namespace input
