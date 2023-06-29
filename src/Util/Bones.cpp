#include "Util/Bones.h"

#include <math.h>

#include <algorithm>
#include <utility>

#include "Util/DriverLog.h"
#include "Util/Quaternion.h"
#define PI 3.14159265

static const float c_maxSplayAngle = 10.0f;

static const std::array<float, 4> emptyRotation = {0.0f, 0.0f, 0.0f, 0.0f};
static const std::array<float, 3> emptyTranslation = {0.0f, 0.0f, 0.0f};

// Poses for right hand
static struct accessoryAnimConfig {
  // left->right up->down
  std::pair<int, int> thumbpadX;
  std::pair<int, int> thumbpadY;

  std::pair<int, int> thumbpad_NW_SW;
  std::pair<int, int> thumbpad_NE_SE;
  std::pair<int, int> thumbpad_NW_NE;
  std::pair<int, int> thumbpad_SW_SE;
  std::pair<int, int> slider;
  std::pair<int, int> proximity;
};

static accessoryAnimConfig AccessoryAnimConfig = {
    {9, 7},    // Trackpad W->E - Touch
    {11, 5},   // Trackpad N->S
    {10, 4},   // Trackpad NW->SW
    {12, 6},   // Trackpad NE->SE
    {4, 6},    // Trackpad NW->NE
    {10, 12},  // Trackpad SW->SE
    {31, 30},  // Slider
    {25, 27}   // Proximity
};

static float Lerp(const float& a, const float& b, const float& f) {
  return a + f * (b - a);
}

enum class FingerIndex : int { Thumb = 0, IndexFinger, MiddleFinger, RingFinger, PinkyFinger, Unknown = -1 };

static FingerIndex GetFingerFromBoneIndex(const HandSkeletonBone& bone) {
  switch (bone) {
    case HandSkeletonBone::Thumb0:
    case HandSkeletonBone::Thumb1:
    case HandSkeletonBone::Thumb2:
    case HandSkeletonBone::AuxThumb:
      return FingerIndex::Thumb;

    case HandSkeletonBone::IndexFinger0:
    case HandSkeletonBone::IndexFinger1:
    case HandSkeletonBone::IndexFinger2:
    case HandSkeletonBone::IndexFinger3:
    case HandSkeletonBone::AuxIndexFinger:
      return FingerIndex::IndexFinger;

    case HandSkeletonBone::MiddleFinger0:
    case HandSkeletonBone::MiddleFinger1:
    case HandSkeletonBone::MiddleFinger2:
    case HandSkeletonBone::MiddleFinger3:
    case HandSkeletonBone::AuxMiddleFinger:
      return FingerIndex::MiddleFinger;

    case HandSkeletonBone::RingFinger0:
    case HandSkeletonBone::RingFinger1:
    case HandSkeletonBone::RingFinger2:
    case HandSkeletonBone::RingFinger3:
    case HandSkeletonBone::AuxRingFinger:
      return FingerIndex::RingFinger;

    case HandSkeletonBone::PinkyFinger0:
    case HandSkeletonBone::PinkyFinger1:
    case HandSkeletonBone::PinkyFinger2:
    case HandSkeletonBone::PinkyFinger3:
    case HandSkeletonBone::AuxPinkyFinger:
      return FingerIndex::PinkyFinger;

    default:
      return FingerIndex::Unknown;
  }
}

static HandSkeletonBone GetRootFingerBoneFromFingerIndex(const FingerIndex& finger) {
  switch (finger) {
    case FingerIndex::Thumb:
      return HandSkeletonBone::Thumb0;
    case FingerIndex::IndexFinger:
      return HandSkeletonBone::IndexFinger0;
    case FingerIndex::MiddleFinger:
      return HandSkeletonBone::MiddleFinger0;
    case FingerIndex::RingFinger:
      return HandSkeletonBone::RingFinger0;
    case FingerIndex::PinkyFinger:
      return HandSkeletonBone::PinkyFinger0;
  }
}

static void TransformLeftBone(vr::VRBoneTransform_t& bone, const HandSkeletonBone& boneIndex, bool rotation) {
  switch (boneIndex) {
    case HandSkeletonBone::Root: {
      return;
    }
    case HandSkeletonBone::Thumb0:
    case HandSkeletonBone::IndexFinger0:
    case HandSkeletonBone::MiddleFinger0:
    case HandSkeletonBone::RingFinger0:
    case HandSkeletonBone::PinkyFinger0: {
      if (!rotation) break;

      const vr::HmdQuaternionf_t quat = bone.orientation;
      bone.orientation.w = -quat.x;
      bone.orientation.x = quat.w;
      bone.orientation.y = -quat.z;
      bone.orientation.z = quat.y;
      break;
    }
    case HandSkeletonBone::Wrist:
    case HandSkeletonBone::AuxIndexFinger:
    case HandSkeletonBone::AuxThumb:
    case HandSkeletonBone::AuxMiddleFinger:
    case HandSkeletonBone::AuxRingFinger:
    case HandSkeletonBone::AuxPinkyFinger: {
      if (!rotation) break;
      bone.orientation.y *= -1;
      bone.orientation.z *= -1;
      break;
    }
    default: {
      if (rotation) break;
      bone.position.v[1] *= -1;
      bone.position.v[2] *= -1;
    }
  }

  if (rotation) return;

  bone.position.v[0] *= -1;
}
static bool IsAuxBone(const HandSkeletonBone& boneIndex) {
  return boneIndex == HandSkeletonBone::AuxThumb || boneIndex == HandSkeletonBone::AuxIndexFinger || boneIndex == HandSkeletonBone::AuxMiddleFinger ||
         boneIndex == HandSkeletonBone::AuxRingFinger || boneIndex == HandSkeletonBone::AuxPinkyFinger;
}

BoneAnimator::BoneAnimator(const std::string& curlAnimFileName, const std::string& accessoryAnimFileName) {
  curlModelManager_ = std::make_unique<GLTFModelManager>(curlAnimFileName);
  accessoryModelManager_ = std::make_unique<GLTFModelManager>(accessoryAnimFileName);

  loaded_ = curlModelManager_->Load() && accessoryModelManager_->Load();

  accumulator_.fill(0.0f);

  if (!loaded_) DriverLog("Unable to load model file.");
}

void BoneAnimator::ComputeSkeletonTransforms(vr::VRBoneTransform_t* skeleton, const VRCommInputData_t& inputData) {
  if (!loaded_) return;

  for (size_t i = 0; i < NUM_BONES; i++) {
    const FingerIndex finger = GetFingerFromBoneIndex(static_cast<HandSkeletonBone>(i));
    const int iFinger = static_cast<int>(finger);
    HandSkeletonBone boneIndex = static_cast<HandSkeletonBone>(i);

    // ------------------------------- Finger curl animations -------------------------------
    {
      // Remap the values so that the closed finger aligns with the touch values
      float fingerPull = finger != FingerIndex::Unknown ? inputData.fingers[iFinger].pull : inputData.gesture.gripPull;
      float fingerForce = finger != FingerIndex::Unknown ? inputData.fingers[iFinger].force : inputData.gesture.gripForce;
      float curl = RemapCurl(fingerPull, fingerForce, touchThreshold);  // From Touch to Click -> no curl

      const float alpha = 0.2f;
      accumulator_[i] = (alpha * curl) + (1.0f - alpha) * accumulator_[i];

      // Apply curl value
      const AnimationData animationData = curlModelManager_->GetAnimationDataByInterp(boneIndex, accumulator_[i]);

      const float interp = std::clamp((accumulator_[i] - animationData.startTime) / (animationData.endTime - animationData.startTime), 0.0f, 1.0f);

      ApplyTransformForBone(skeleton[i], boneIndex, animationData, interp, inputData.isRight);
    }

    // Accesory Animations
    {
      const float x = inputData.thumbpad.x;  // Normalise
      const float y = inputData.thumbpad.y;

      const float norm_x = (inputData.thumbpad.x + 1.0f) / 2.0f;  // Normalise
      const float norm_y = (inputData.thumbpad.y + 1.0f) / 2.0f;

      const float norm_slider = (inputData.slider.value + 1.0f) / 2.0f; // Normalise

      // ------------------------------- Trackpad Touch/Clicked -------------------------------
      if (finger == FingerIndex::Thumb && inputData.thumbpad.touch) {
        std::pair<std::pair<int, int>, float> trackpadAnimation =
            TrackpadFindFrame(x, y, norm_x, norm_y, lowerCenterThreshold, upperCenterThreshold, inputData.thumbpad.touch, inputData.isRight);
        AnimationData animationData = accessoryModelManager_->GetAnimationDataByKeyframes(boneIndex, trackpadAnimation.first.first, trackpadAnimation.first.second);
        ApplyTransformForBone(skeleton[i], boneIndex, animationData, trackpadAnimation.second, inputData.isRight);
      }

      // ------------------------------- Tracker proximity animations -------------------------------
      if (finger == FingerIndex::Thumb && inputData.proximity.value > proximityValThreshold) {
        AnimationData animationData =
            accessoryModelManager_->GetAnimationDataByKeyframes(boneIndex, AccessoryAnimConfig.proximity.first, AccessoryAnimConfig.proximity.second);
        ApplyTransformForBone(skeleton[i], boneIndex, animationData, inputData.proximity.value, inputData.isRight);
      }

      // ------------------------------- Slider animations -------------------------------
      /*
      if (finger == FingerIndex::Thumb && inputData.slider.touch && !inputData.gesture.gripTouch &&
          inputData.fingers[0].pull < 0.2f && !inputData.proximity.touch && !inputData.thumbpad.touch) {
         AnimationData animationData;
         animationData = accessoryModelManager_->GetAnimationDataByKeyframes(boneIndex, AccessoryAnimConfig.slider.first, AccessoryAnimConfig.slider.second);
         ApplyTransformForBone(skeleton[i], boneIndex, animationData, norm_slider, inputData.isRight);
       }
      */
    }
  }
}

void BoneAnimator::ApplyTransformForBone(
    vr::VRBoneTransform_t& bone, const HandSkeletonBone& boneIndex, const AnimationData& animationData, const float interp, const bool rightHand) const {
  if (interp < 0.0f || interp > 1.0f) return;

  if (animationData.startTransform.rotation != emptyRotation) {
    bone.orientation.w = Lerp(animationData.startTransform.rotation[0], animationData.endTransform.rotation[0], interp);
    bone.orientation.x = Lerp(animationData.startTransform.rotation[1], animationData.endTransform.rotation[1], interp);
    bone.orientation.y = Lerp(animationData.startTransform.rotation[2], animationData.endTransform.rotation[2], interp);
    bone.orientation.z = Lerp(animationData.startTransform.rotation[3], animationData.endTransform.rotation[3], interp);
    if (!rightHand) TransformLeftBone(bone, boneIndex, true);
  }

  if (animationData.startTransform.translation != emptyTranslation) {
    bone.position.v[0] = Lerp(animationData.startTransform.translation[0], animationData.endTransform.translation[0], interp);
    bone.position.v[1] = Lerp(animationData.startTransform.translation[1], animationData.endTransform.translation[1], interp);
    bone.position.v[2] = Lerp(animationData.startTransform.translation[2], animationData.endTransform.translation[2], interp);
    if (!rightHand) TransformLeftBone(bone, boneIndex, false);
  }
  bone.position.v[3] = 1.0f;
};

void BoneAnimator::LoadDefaultSkeletonByHand(vr::VRBoneTransform_t* skeleton, const bool rightHand) {
  for (int i = 0; i < 31; i++) {
    Transform transform = curlModelManager_->GetTransformByBoneIndex((HandSkeletonBone)i);
    skeleton[i].orientation.w = transform.rotation[0];
    skeleton[i].orientation.x = transform.rotation[1];
    skeleton[i].orientation.y = transform.rotation[2];
    skeleton[i].orientation.z = transform.rotation[3];

    skeleton[i].position.v[0] = transform.translation[0];
    skeleton[i].position.v[1] = transform.translation[1];
    skeleton[i].position.v[2] = transform.translation[2];
    skeleton[i].position.v[3] = 1.0f;

    if (!rightHand) {
      TransformLeftBone(skeleton[i], (HandSkeletonBone)i, false);
      TransformLeftBone(skeleton[i], (HandSkeletonBone)i, true);
    }
  }
}

void BoneAnimator::LoadGripLimitSkeletonByHand(vr::VRBoneTransform_t* skeleton, const bool rightHand) {
  for (int i = 1; i < NUM_BONES; i++) {
    HandSkeletonBone boneIndex = (HandSkeletonBone)i;
    const AnimationData animationData = curlModelManager_->GetAnimationDataByInterp(boneIndex, 1.0f);
    ApplyTransformForBone(skeleton[i], boneIndex, animationData, 1.0f, rightHand);
  }
}

float BoneAnimator::RemapValue(float low1, float high1, float low2, float high2, float value1) {
  float newVal;
  if (value1 > high1) {
    newVal = 1.0f;
  } else {
    newVal = low2 + (value1 - low1) * (high2 - low2) / (high1 - low1);
  }
  return newVal;
}

float BoneAnimator::RemapCurl(float pull, float force, float touchThreshold) {
  float ratioForce = 0.25f;
  float curlValRange = 1.0f + (1 * ratioForce);

  float curlValUnmapped = RemapValue(0.0f, touchThreshold, 0.0f, 1.0f, pull) + (force * ratioForce);
  float curlVal = RemapValue(0.0f, curlValRange, 0.0f, 1.0f, curlValUnmapped);
  return curlVal;
}

std::pair<std::pair<int, int>, float> BoneAnimator::TrackpadFindFrame(
    float x, float y, float norm_x, float norm_y, float lowerCenterThreshold, float upperCenterThreshold, bool trackpadTouch, bool isRightHand) {
  std::pair<int, int> frameData;
  float interp;

  // North - South
  if (norm_x >= lowerCenterThreshold && norm_x <= upperCenterThreshold && std::fabs(x) <= std::fabs(y)) {
    if (trackpadTouch != true) {
      frameData = AccessoryAnimConfig.thumbpadY;
    } else {
      frameData = AccessoryAnimConfig.thumbpadY;
    }
    interp = norm_y;
  }

  // West - East
  else if (norm_y >= lowerCenterThreshold && norm_y <= upperCenterThreshold && std::fabs(x) > std::fabs(y)) {
    if (isRightHand) {
      if (trackpadTouch != true) {
        frameData = AccessoryAnimConfig.thumbpadX;
      } else {
        frameData = AccessoryAnimConfig.thumbpadX;
      }
    } else {
      if (trackpadTouch != true) {
        frameData = std::make_pair(AccessoryAnimConfig.thumbpadX.second, AccessoryAnimConfig.thumbpadX.first);
      } else {
        frameData = std::make_pair(AccessoryAnimConfig.thumbpadX.second, AccessoryAnimConfig.thumbpadX.first);
      }
    }
    interp = norm_x;
  }

  // North West - South West
  else if (norm_x < lowerCenterThreshold && norm_y > upperCenterThreshold || norm_x < lowerCenterThreshold && norm_y < lowerCenterThreshold) {
    if (isRightHand) {
      if (trackpadTouch != true) {
        frameData = AccessoryAnimConfig.thumbpad_NW_SW;
      } else {
        frameData = AccessoryAnimConfig.thumbpad_NW_SW;
      }
    } else {
      if (trackpadTouch != true) {
        frameData = AccessoryAnimConfig.thumbpad_NE_SE;
      } else {
        frameData = AccessoryAnimConfig.thumbpad_NE_SE;
      }
    }
    float originalRange = sin(45 * PI / 180) * 2;
    interp = RemapValue(0.0f, originalRange, 0.0f, 1.0f, norm_y);
  }

  // North East - South East
  else if (norm_x > upperCenterThreshold && norm_y > upperCenterThreshold || norm_x > upperCenterThreshold && norm_y < lowerCenterThreshold) {
    if (isRightHand) {
      if (trackpadTouch != true) {
        frameData = AccessoryAnimConfig.thumbpad_NE_SE;
      } else {
        frameData = AccessoryAnimConfig.thumbpad_NE_SE;
      }
    } else {
      if (trackpadTouch != true) {
        frameData = AccessoryAnimConfig.thumbpad_NW_SW;
      } else {
        frameData = AccessoryAnimConfig.thumbpad_NW_SW;
      }
    }
    float originalRange = sin(45 * PI / 180) * 2;
    interp = RemapValue(0.0f, originalRange, 0.0f, 1.0f, norm_y);
  }

  // North West - North East
  else if (norm_y > upperCenterThreshold && norm_x > upperCenterThreshold || norm_y > upperCenterThreshold && norm_x < lowerCenterThreshold) {
    if (isRightHand) {
      if (trackpadTouch != true) {
        frameData = AccessoryAnimConfig.thumbpad_NW_NE;
      } else {
        frameData = AccessoryAnimConfig.thumbpad_NW_NE;
      }
    } else {
      if (trackpadTouch != true) {
        frameData = std::make_pair(AccessoryAnimConfig.thumbpad_NW_NE.second, AccessoryAnimConfig.thumbpad_NW_NE.first);
      } else {
        frameData = std::make_pair(AccessoryAnimConfig.thumbpad_NW_NE.second, AccessoryAnimConfig.thumbpad_NW_NE.first);
      }
    }
    float originalRange = sin(45 * PI / 180) * 2;
    interp = RemapValue(0.0f, originalRange, 0.0f, 1.0f, norm_x);
  }

  // South West - South East
  else if (norm_y < lowerCenterThreshold && norm_x > upperCenterThreshold || norm_y < lowerCenterThreshold && norm_x < lowerCenterThreshold) {
    if (isRightHand) {
      if (trackpadTouch != true) {
        frameData = AccessoryAnimConfig.thumbpad_SW_SE;
      } else {
        frameData = AccessoryAnimConfig.thumbpad_SW_SE;
      }
    } else {
      if (trackpadTouch != true) {
        frameData = std::make_pair(AccessoryAnimConfig.thumbpad_SW_SE.second, AccessoryAnimConfig.thumbpad_SW_SE.first);
      } else {
        frameData = std::make_pair(AccessoryAnimConfig.thumbpad_SW_SE.second, AccessoryAnimConfig.thumbpad_SW_SE.first);
      }
    }
    float originalRange = sin(45 * PI / 180) * 2;
    interp = RemapValue(0.0f, originalRange, 0.0f, 1.0f, norm_x);
  }

  std::pair<std::pair<int, int>, float> result = std::make_pair(frameData, interp);
  return result;
}