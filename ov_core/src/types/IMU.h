/*
 * OpenVINS: An Open Platform for Visual-Inertial Research
 * Copyright (C) 2018-2023 Patrick Geneva
 * Copyright (C) 2018-2023 Guoquan Huang
 * Copyright (C) 2018-2023 OpenVINS Contributors
 * Copyright (C) 2018-2019 Kevin Eckenhoff
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program.  If not, see <https://www.gnu.org/licenses/>.
 */

#ifndef OV_TYPE_TYPE_IMU_H
#define OV_TYPE_TYPE_IMU_H

#include "PoseJPL.h"
#include "utils/quat_ops.h"

namespace ov_type {

/**
 * @brief Derived Type class that implements an IMU state
 *
 * Contains a PoseJPL, Vec velocity, Vec gyro bias, and Vec accel bias.
 * This should be similar to that of the standard MSCKF state besides the ordering.
 * The pose is first, followed by velocity, etc.
 */
class IMU : public Type {

public:
  IMU() : Type(21) {

    // Create all the sub-variables
    _pose = std::shared_ptr<PoseJPL>(new PoseJPL());
    _v = std::shared_ptr<Vec>(new Vec(3));
    _bg = std::shared_ptr<Vec>(new Vec(3));
    _ba = std::shared_ptr<Vec>(new Vec(3));
    _keyframe_pose = std::shared_ptr<PoseJPL>(new PoseJPL());

    // Set our default state value
    Eigen::VectorXd imu0 = Eigen::VectorXd::Zero(23, 1);
    imu0(3) = 1.0;
    imu0(19) = 1.0;
    set_value_internal(imu0);
    set_fej_internal(imu0);
  }

  ~IMU() {}

  /**
   * @brief Sets id used to track location of variable in the filter covariance
   *
   * Note that we update the sub-variables also.
   *
   * @param new_id entry in filter covariance corresponding to this variable
   */
  void set_local_id(int new_id) override {
    _id = new_id;
    _pose->set_local_id(new_id);
    _v->set_local_id(_pose->id() + ((new_id != -1) ? _pose->size() : 0));
    _bg->set_local_id(_v->id() + ((new_id != -1) ? _v->size() : 0));
    _ba->set_local_id(_bg->id() + ((new_id != -1) ? _bg->size() : 0));
    _keyframe_pose->set_local_id(_ba->id() + ((new_id != -1) ? _ba->size() : 0));
  }

  /**
   * @brief Performs update operation using JPLQuat update for orientation, then vector updates for
   * position, velocity, gyro bias, accel bias, keyframe orientation, and keyframe position (in that order).
   *
   * @param dx 21 DOF vector encoding update using the following order (q, p, v, bg, ba, q_k, p_k)
   */
  void update(const Eigen::VectorXd &dx) override {

    assert(dx.rows() == _size);

    Eigen::Matrix<double, 23, 1> newX = _value;

    Eigen::Matrix<double, 4, 1> dq;
    dq << .5 * dx.block(0, 0, 3, 1), 1.0;
    dq = ov_core::quatnorm(dq);

    Eigen::Matrix<double, 4, 1> d_keyframe_q;
    d_keyframe_q << .5 * dx.block(15, 0, 3, 1), 1.0;
    d_keyframe_q = ov_core::quatnorm(d_keyframe_q);

    newX.block(0, 0, 4, 1) = ov_core::quat_multiply(dq, quat());
    newX.block(4, 0, 3, 1) += dx.block(3, 0, 3, 1);

    newX.block(7, 0, 3, 1) += dx.block(6, 0, 3, 1);
    newX.block(10, 0, 3, 1) += dx.block(9, 0, 3, 1);
    newX.block(13, 0, 3, 1) += dx.block(12, 0, 3, 1);

    newX.block(16, 0, 4, 1) = ov_core::quat_multiply(d_keyframe_q, keyframe_quat());
    newX.block(20, 0, 3, 1) += dx.block(18, 0, 3, 1);

    set_value(newX);
  }

  /**
   * @brief Sets the value of the estimate
   * @param new_value New value we should set
   */
  void set_value(const Eigen::MatrixXd &new_value) override { set_value_internal(new_value); }

  /**
   * @brief Sets the value of the first estimate
   * @param new_value New value we should set
   */
  void set_fej(const Eigen::MatrixXd &new_value) override { set_fej_internal(new_value); }

  /**
   * @brief Reset the keyframe states to zero
   */
  void reset_keyframe_states() {
    ov_type::PoseJPL new_keyframe;
    keyframe_pose()->set_value(new_keyframe.value());
    keyframe_pose()->set_fej(new_keyframe.value());
    _value.block(16, 0, 7, 1) = new_keyframe.value();
    _fej.block(16, 0, 7, 1) = new_keyframe.value();
  }

  std::shared_ptr<Type> clone() override {
    auto Clone = std::shared_ptr<Type>(new IMU());
    Clone->set_value(value());
    Clone->set_fej(fej());
    return Clone;
  }

  std::shared_ptr<Type> check_if_subvariable(const std::shared_ptr<Type> check) override {
    if (check == _pose) {
      return _pose;
    } else if (check == _pose->check_if_subvariable(check)) {
      return _pose->check_if_subvariable(check);
    } else if (check == _v) {
      return _v;
    } else if (check == _bg) {
      return _bg;
    } else if (check == _ba) {
      return _ba;
    } else if (check == _keyframe_pose) {
      return _keyframe_pose;
    } else if (check == _keyframe_pose->check_if_subvariable(check)) {
      return _keyframe_pose->check_if_subvariable(check);
    }
    return nullptr;
  }

  /// Rotation access
  Eigen::Matrix<double, 3, 3> Rot() const { return _pose->Rot(); }

  /// FEJ Rotation access
  Eigen::Matrix<double, 3, 3> Rot_fej() const { return _pose->Rot_fej(); }

  /// Rotation access quaternion
  Eigen::Matrix<double, 4, 1> quat() const { return _pose->quat(); }

  /// FEJ Rotation access quaternion
  Eigen::Matrix<double, 4, 1> quat_fej() const { return _pose->quat_fej(); }

  /// Position access
  Eigen::Matrix<double, 3, 1> pos() const { return _pose->pos(); }

  /// FEJ position access
  Eigen::Matrix<double, 3, 1> pos_fej() const { return _pose->pos_fej(); }

  /// Velocity access
  Eigen::Matrix<double, 3, 1> vel() const { return _v->value(); }

  // FEJ velocity access
  Eigen::Matrix<double, 3, 1> vel_fej() const { return _v->fej(); }

  /// Gyro bias access
  Eigen::Matrix<double, 3, 1> bias_g() const { return _bg->value(); }

  /// FEJ gyro bias access
  Eigen::Matrix<double, 3, 1> bias_g_fej() const { return _bg->fej(); }

  /// Accel bias access
  Eigen::Matrix<double, 3, 1> bias_a() const { return _ba->value(); }

  // FEJ accel bias access
  Eigen::Matrix<double, 3, 1> bias_a_fej() const { return _ba->fej(); }

  /// Keyframe Rotation access
  Eigen::Matrix<double, 3, 3> keyframe_Rot() const { return _keyframe_pose->Rot(); }

  /// Keyframe FEJ Rotation access
  Eigen::Matrix<double, 3, 3> keyframe_Rot_fej() const { return _keyframe_pose->Rot_fej(); }

  /// Keyframe Rotation access quaternion
  Eigen::Matrix<double, 4, 1> keyframe_quat() const { return _keyframe_pose->quat(); }

  /// Keyframe FEJ Rotation access quaternion
  Eigen::Matrix<double, 4, 1> keyframe_quat_fej() const { return _keyframe_pose->quat_fej(); }

  /// Keyframe Position access
  Eigen::Matrix<double, 3, 1> keyframe_pos() const { return _keyframe_pose->pos(); }

  /// Keyframe FEJ position access
  Eigen::Matrix<double, 3, 1> keyframe_pos_fej() const { return _keyframe_pose->pos_fej(); }

  /// Pose type access
  std::shared_ptr<PoseJPL> pose() { return _pose; }

  /// Quaternion type access
  std::shared_ptr<JPLQuat> q() { return _pose->q(); }

  /// Position type access
  std::shared_ptr<Vec> p() { return _pose->p(); }

  /// Velocity type access
  std::shared_ptr<Vec> v() { return _v; }

  /// Gyroscope bias access
  std::shared_ptr<Vec> bg() { return _bg; }

  /// Acceleration bias access
  std::shared_ptr<Vec> ba() { return _ba; }

  /// Keyframe Pose type access
  std::shared_ptr<PoseJPL> keyframe_pose() { return _keyframe_pose; }

  /// Keyframe Quaternion type access
  std::shared_ptr<JPLQuat> keyframe_q() { return _keyframe_pose->q(); }

  /// Keyframe Position type access
  std::shared_ptr<Vec> keyframe_p() { return _keyframe_pose->p(); }

protected:
  /// Pose subvariable
  std::shared_ptr<PoseJPL> _pose;

  /// Velocity subvariable
  std::shared_ptr<Vec> _v;

  /// Gyroscope bias subvariable
  std::shared_ptr<Vec> _bg;

  /// Acceleration bias subvariable
  std::shared_ptr<Vec> _ba;

  /// Keyframe pose subvariable, used by partial-update msckf algorithm to construct multi-agent backend
  std::shared_ptr<PoseJPL> _keyframe_pose;

  /**
   * @brief Sets the value of the estimate
   * @param new_value New value we should set
   */
  void set_value_internal(const Eigen::MatrixXd &new_value) {

    assert(new_value.rows() == 23);
    assert(new_value.cols() == 1);

    _pose->set_value(new_value.block(0, 0, 7, 1));
    _v->set_value(new_value.block(7, 0, 3, 1));
    _bg->set_value(new_value.block(10, 0, 3, 1));
    _ba->set_value(new_value.block(13, 0, 3, 1));
    _keyframe_pose->set_value(new_value.block(16, 0, 7, 1));

    _value = new_value;
  }

  /**
   * @brief Sets the value of the first estimate
   * @param new_value New value we should set
   */
  void set_fej_internal(const Eigen::MatrixXd &new_value) {

    assert(new_value.rows() == 23);
    assert(new_value.cols() == 1);

    _pose->set_fej(new_value.block(0, 0, 7, 1));
    _v->set_fej(new_value.block(7, 0, 3, 1));
    _bg->set_fej(new_value.block(10, 0, 3, 1));
    _ba->set_fej(new_value.block(13, 0, 3, 1));
    _keyframe_pose->set_fej(new_value.block(16, 0, 7, 1));

    _fej = new_value;
  }
};

} // namespace ov_type

#endif // OV_TYPE_TYPE_IMU_H
