#include "sim/pitching_machine.h"

namespace sim {

BallState pitching_machine_launch(const Vec3& release_point, const Vec3& target_point, double speed_mps,
                                   const Vec3& spin_rad_s) {
    BallState ball{};
    ball.position = release_point;
    ball.orientation = Quat::identity();
    ball.angular_velocity = spin_rad_s;
    const Vec3 direction = (target_point - release_point).normalized();
    ball.velocity = direction * speed_mps;
    return ball;
}

}  // namespace sim
