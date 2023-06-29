#include "Util/Quaternion.h"

#include <cmath>

double DegToRad(int degrees) {
  return degrees * M_PI / 180;
}
double RadToDeg(double rad) {
  return rad * 180 / M_PI;
}

vr::HmdVector3_t GetPosition(const vr::HmdMatrix34_t& matrix) {
  vr::HmdVector3_t vector;

  vector.v[0] = matrix.m[0][3];
  vector.v[1] = matrix.m[1][3];
  vector.v[2] = matrix.m[2][3];

  return vector;
}

vr::HmdVector3_t CombinePosition(const vr::HmdMatrix34_t& matrix, const vr::HmdVector3_t& vec) {
  vr::HmdVector3_t vector;

  vector.v[0] = matrix.m[0][3] + vec.v[0];
  vector.v[1] = matrix.m[1][3] + vec.v[1];
  vector.v[2] = matrix.m[2][3] + vec.v[2];

  return vector;
}

vr::HmdQuaternion_t GetRotation(const vr::HmdMatrix34_t& matrix) {
  vr::HmdQuaternion_t q{};

  q.w = sqrt(fmax(0, 1 + matrix.m[0][0] + matrix.m[1][1] + matrix.m[2][2])) / 2;
  q.x = sqrt(fmax(0, 1 + matrix.m[0][0] - matrix.m[1][1] - matrix.m[2][2])) / 2;
  q.y = sqrt(fmax(0, 1 - matrix.m[0][0] + matrix.m[1][1] - matrix.m[2][2])) / 2;
  q.z = sqrt(fmax(0, 1 - matrix.m[0][0] - matrix.m[1][1] + matrix.m[2][2])) / 2;

  q.x = copysign(q.x, matrix.m[2][1] - matrix.m[1][2]);
  q.y = copysign(q.y, matrix.m[0][2] - matrix.m[2][0]);
  q.z = copysign(q.z, matrix.m[1][0] - matrix.m[0][1]);

  return q;
}

vr::HmdMatrix33_t GetRotationMatrix(const vr::HmdMatrix34_t& matrix) {
  vr::HmdMatrix33_t result = {
      {{matrix.m[0][0], matrix.m[0][1], matrix.m[0][2]}, {matrix.m[1][0], matrix.m[1][1], matrix.m[1][2]}, {matrix.m[2][0], matrix.m[2][1], matrix.m[2][2]}}};

  return result;
}
vr::HmdVector3_t MultiplyMatrix(const vr::HmdMatrix33_t& matrix, const vr::HmdVector3_t& vector) {
  vr::HmdVector3_t result;

  result.v[0] = matrix.m[0][0] * vector.v[0] + matrix.m[0][1] * vector.v[1] + matrix.m[0][2] * vector.v[2];
  result.v[1] = matrix.m[1][0] * vector.v[0] + matrix.m[1][1] * vector.v[1] + matrix.m[1][2] * vector.v[2];
  result.v[2] = matrix.m[2][0] * vector.v[0] + matrix.m[2][1] * vector.v[1] + matrix.m[2][2] * vector.v[2];

  return result;
}

vr::HmdQuaternion_t EulerToQuaternion(double roll, double pitch, double yaw) {
  double cr = cos(roll * 0.5);
  double sr = sin(roll * 0.5);
  double cp = cos(pitch * 0.5);
  double sp = sin(pitch * 0.5);
  double cy = cos(yaw * 0.5);
  double sy = sin(yaw * 0.5);

  vr::HmdQuaternion_t q;
  q.w = cr * cp * cy + sr * sp * sy;
  q.x = cr * sp * cy + sr * cp * sy;  // pitch
  q.y = cr * cp * sy - sr * sp * cy;  // yaw
  q.z = sr * cp * cy - cr * sp * sy; // roll

  return q;
}

/// Multiplies two quaterniones together in quaternion multiplcation order r*q which applies the r rotation on top of the q rotation
vr::HmdQuaternion_t MultiplyQuaternion(const vr::HmdQuaternion_t& rhs, const vr::HmdQuaternion_t& lhs) {
  return {
      lhs.w * rhs.w - lhs.x * rhs.x - lhs.y * rhs.y - lhs.z * rhs.z,
      lhs.w * rhs.x + lhs.x * rhs.w + lhs.y * rhs.z - lhs.z * rhs.y,
      lhs.w * rhs.y - lhs.x * rhs.z + lhs.y * rhs.w + lhs.z * rhs.x,
      lhs.w * rhs.z + lhs.x * rhs.y - lhs.y * rhs.x + lhs.z * rhs.w,
  };
}

vr::HmdQuaternion_t InverseQuaternion(const vr::HmdQuaternion_t& q) {
  vr::HmdQuaternion_t result;

  result.w = q.w;
  result.x = -q.x;
  result.y = -q.y;
  result.z = -q.z;

  return result;
}

/// <summary>
/// Returns a quaternion rotation that rotates z degrees around the z axis (Yaw), x degrees around the x axis (Roll), and y degrees around the y axis (Pitch) applied in
/// that order.
/// </summary>
/// <param name="euler">Euler angle in order roll, pitch yaw</param>
/// <returns>HmdQuaternion equivalent quaternion rotation</returns>
vr::HmdQuaternion_t EulerToQuaternionYawRollPitch(const vr::HmdVector3_t euler) {
  vr::HmdQuaternion_t result;
  double xOver2 = euler.v[0] * (double)0.5f;
  double yOver2 = euler.v[1] * (double)0.5f;
  double zOver2 = euler.v[2] * (double)0.5f;

  double sinXOver2 = sin(xOver2);
  double cosXOver2 = cos(xOver2);
  double sinYOver2 = sin(yOver2);
  double cosYOver2 = cos(yOver2);
  double sinZOver2 = sin(zOver2);
  double cosZOver2 = cos(zOver2);

  result.x = cosYOver2 * sinXOver2 * cosZOver2 + sinYOver2 * cosXOver2 * sinZOver2;
  result.y = sinYOver2 * cosXOver2 * cosZOver2 - cosYOver2 * sinXOver2 * sinZOver2;
  result.z = cosYOver2 * cosXOver2 * sinZOver2 - sinYOver2 * sinXOver2 * cosZOver2;
  result.w = cosYOver2 * cosXOver2 * cosZOver2 + sinYOver2 * sinXOver2 * sinZOver2;

  return result;
}

/// <summary>
/// Returns a euler rotation that rotates z degrees around the z axis (Yaw), x degrees around the x axis (Roll), and y degrees around the y axis (Pitch), applied in that
/// order, which is equivalent to the quaternion supplied.
/// </summary>
/// <param name="quaternion">input quaternion</param>
/// <returns>HmdVector3_t in form roll pitch yaw</returns>
vr::HmdVector3_t QuaternionToEulerYawRollPitch(const vr::HmdQuaternion_t q) {
  vr::HmdVector3_t euler;
  // if the input quaternion is normalized, this is exactly one. Otherwise, this acts as a correction factor for the quaternion's not-normalizedness
  double unit = (q.x * q.x) + (q.y * q.y) + (q.z * q.z) + (q.w * q.w);

  // this will have a magnitude of 0.5 or greater if and only if this is a singularity case
  float test = q.x * q.w - q.y * q.z;

  if (test > 0.4995f * unit)  // singularity at north pole
  {
    euler.v[0] = M_PI / 2;
    euler.v[1] = (2 * atan2(q.y / unit, q.x / unit));
    euler.v[2] = 0;
  } else if (test < -0.4995f * unit)  // singularity at south pole
  {
    euler.v[0] = -M_PI / 2;
    euler.v[1] = (-2 * atan2(q.y / unit, q.x / unit));
    euler.v[2] = 0;
  } else  // no singularity - this is the majority of cases
  {
    euler.v[0] = asin(2 * (q.w * q.x - q.y * q.z) / unit);
    euler.v[1] = atan2((2 / unit * q.w * q.y + 2 / unit * q.z * q.x), (1 - 2 / unit * (q.x * q.x + q.y * q.y)));
    euler.v[2] = atan2((2 / unit * q.w * q.z + 2 / unit * q.x * q.y), (1 - 2 / unit * (q.z * q.z + q.x * q.x)));
  }

  return euler;
}

vr::HmdQuaternion_t operator-(const vr::HmdQuaternion_t& q) {
  return {q.w, -q.x, -q.y, -q.z};
}

vr::HmdQuaternion_t operator*(const vr::HmdQuaternion_t& q, const vr::HmdQuaternion_t& r) {
  vr::HmdQuaternion_t result{};

  result.w = r.w * q.w - r.x * q.x - r.y * q.y - r.z * q.z;
  result.x = r.w * q.x + r.x * q.w - r.y * q.z + r.z * q.y;
  result.y = r.w * q.y + r.x * q.z + r.y * q.w - r.z * q.x;
  result.z = r.w * q.z - r.x * q.y + r.y * q.x + r.z * q.w;

  return result;
}

vr::HmdQuaternionf_t operator*(const vr::HmdQuaternionf_t& q, const vr::HmdQuaternion_t& r) {
  vr::HmdQuaternionf_t result{};

  result.w = r.w * q.w - r.x * q.x - r.y * q.y - r.z * q.z;
  result.x = r.w * q.x + r.x * q.w - r.y * q.z + r.z * q.y;
  result.y = r.w * q.y + r.x * q.z + r.y * q.w - r.z * q.x;
  result.z = r.w * q.z - r.x * q.y + r.y * q.x + r.z * q.w;

  return result;
}

vr::HmdVector3_t operator+(const vr::HmdMatrix34_t& matrix, const vr::HmdVector3_t& vec) {
  vr::HmdVector3_t vector{};

  vector.v[0] = matrix.m[0][3] + vec.v[0];
  vector.v[1] = matrix.m[1][3] + vec.v[1];
  vector.v[2] = matrix.m[2][3] + vec.v[2];

  return vector;
}

vr::HmdVector3_t operator*(const vr::HmdMatrix33_t& matrix, const vr::HmdVector3_t& vec) {
  vr::HmdVector3_t result{};

  result.v[0] = matrix.m[0][0] * vec.v[0] + matrix.m[0][1] * vec.v[1] + matrix.m[0][2] * vec.v[2];
  result.v[1] = matrix.m[1][0] * vec.v[0] + matrix.m[1][1] * vec.v[1] + matrix.m[1][2] * vec.v[2];
  result.v[2] = matrix.m[2][0] * vec.v[0] + matrix.m[2][1] * vec.v[1] + matrix.m[2][2] * vec.v[2];

  return result;
}

vr::HmdVector3_t operator-(const vr::HmdVector3_t& vec, const vr::HmdMatrix34_t& matrix) {
  return {vec.v[0] - matrix.m[0][3], vec.v[1] - matrix.m[1][3], vec.v[2] - matrix.m[2][3]};
}

vr::HmdVector3d_t operator+(const vr::HmdVector3d_t& vec1, const vr::HmdVector3d_t& vec2) {
  return {vec1.v[0] + vec2.v[0], vec1.v[1] + vec2.v[1], vec1.v[2] + vec2.v[2]};
}

vr::HmdVector3_t operator+(const vr::HmdVector3_t& vec1, const vr::HmdVector3_t& vec2) {
  return {vec1.v[0] + vec2.v[0], vec1.v[1] + vec2.v[1], vec1.v[2] + vec2.v[2]};
}

vr::HmdVector3d_t operator-(const vr::HmdVector3d_t& vec1, const vr::HmdVector3d_t& vec2) {
  return {vec1.v[0] - vec2.v[0], vec1.v[1] - vec2.v[1], vec1.v[2] - vec2.v[2]};
}

vr::HmdVector3d_t operator*(const vr::HmdVector3d_t& vec, const vr::HmdQuaternion_t& q) {
  const vr::HmdQuaternion_t qVec = {0.0, vec.v[0], vec.v[1], vec.v[2]};

  const vr::HmdQuaternion_t qResult = q * qVec * -q;

  return {qResult.x, qResult.y, qResult.z};
}

vr::HmdVector3_t operator*(const vr::HmdVector3_t& vec, const vr::HmdQuaternion_t& q) {
  const vr::HmdQuaternion_t qVec = {1.0, vec.v[0], vec.v[1], vec.v[2]};

  const vr::HmdQuaternion_t qResult = q * qVec * -q;

  return {static_cast<float>(qResult.x), static_cast<float>(qResult.y), static_cast<float>(qResult.z)};
}

bool operator==(const vr::HmdVector3d_t& v1, const vr::HmdVector3d_t& v2) {
  return v1.v[0] == v2.v[0] && v1.v[1] == v2.v[1] && v1.v[2] == v2.v[2];
}

bool operator==(const vr::HmdQuaternion_t& q1, const vr::HmdQuaternion_t& q2) {
  return q1.w == q2.w && q1.x == q2.x && q1.y == q2.y && q1.z == q2.z;
}