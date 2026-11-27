# Pavlov OpenXR binding checkpoint

## Tested configuration: 24 November 2026

Branch: `bindings/pavlov-openxr`. User tested with Valve Index and etee
controllers in Pavlov training/shooting range. This section supersedes the
historical checkpoints below.

- User confirms firing, menu selection, grip/release, weapon-wheel activation,
  turning and movement work without the earlier control overlap.
- Index finger directly supplies trigger axis; click activates at 70% and
  releases at 55%. These thresholds do not gate the continuous trigger axis.
  Removed pinch trigger mappings and corrected nonexistent `/actions/ue/in/fire`.
- Middle-finger pull directly supplies grip axis and grip force. No simulated
  actions or custom grip thresholds remain in the final configuration.
- Left/right pads supply joystick X/Y with 20%/35% deadzones respectively.
  Left pad retains stick-click and other existing Index trackpad mappings.
- Right pad supplies only turning and wheel-selection axes. Right lower-slider
  double tap sends right stick-click and trackpad-touch for wheel activation;
  this avoids ordinary pad touches and palm contact opening the wheel.
- Side finger/thumb single click sends Index B on both hands for magazine
  release/contextual actions. Upper slider single click sends A. Left lower
  slider is unbound. User reports online instructions support holding the
  thumb input for menu behaviour; pause/menu behaviour outside training remains
  unverified, so this is not a confirmed universal pause-menu gesture.
- User earlier observed thumb input raising index values, a released middle value
  sometimes remaining at 0.13, and right-controller disconnects. The final test
  reports controls working, but these observations are not hardware fixes.
- Still to verify separately: sprint, complete reload/fire-mode coverage,
  haptics and pause/menu behaviour outside the shooting range.
- Name follows the BONELAB preset convention; description explains current
  physical controls and contextual A/B behaviour without promising sprint.
- JSON/mapping checks pass; the installed eteeConnect binding matches the repo.

## Historical tests and diagnostics

The following records describe earlier configurations, not the final mappings.

## Follow-up test adjustment: 10 November 2026

- Latest user-requested rollback: restored the earliest saved OpenXR test
  binding from `TG0-Pavlov-turning-only-dcd8d1a1931d4f99add669436aa4a755/previous-binding.json`
  in the local temporary directory. It has 38 sources, pinch plus index-value
  triggers (digital index thresholds 70%/65%), single-click slider A/B actions,
  and the original right-pad stick and trackpad mappings. This predates the
  turning-only and sensitivity changes. Their original overlap and accidental
  activation issues may return. Pavlov offsets remain disabled. Re-test required.
- Previous rollback (superseded above): both value-based variants failed to select Pavlov
  menu items, including after reload. Restored the force-based index mappings
  from checkpoint `91b63d0`, which the user previously confirmed selected items.
  Re-test that baseline after restarting; no resolution of the inward pose is
  claimed. The earlier diagnostic changes below are history, not active settings.
- Latest diagnostic: Home's right Tool Trigger reads 0.00 off, 0.30-0.40 resting,
  and 1.00 with a light squeeze. Tool Use also changes TRUE/FALSE correctly.
- The separate value-based click conversion failed to select Pavlov menu items.
  Replaced it on both hands with Home's tested combined `trigger` source:
  native finger click to Pavlov trigger click and finger pull to trigger axis,
  with empty parameters. Custom 80%/75% click thresholds are no longer applied.
  Trigger touch remains unchanged. Pavlov menu and firing tests are pending.
- User confirmed index-force squeezing selects main-menu items. Reaching the
  slider while holding a gun changes grip enough to drop the gun; magazine
  release remains unverified.
- Earlier in this session, both index triggers were changed to finger value rather than
  force. Digital click still activates at 80% and releases at 75%; the continuous
  axis supplies raw finger value, so these thresholds do not gate analog firing.
- Trigger touch and other controls are unchanged. Test menu selection and
  single-shot/automatic firing, including immediate release, before accepting
  this adjustment. The checkpoint details and hash below describe 6 November.

## 6 November checkpoint implementation

- Replaced the obsolete SteamVR Input draft with Pavlov's current OpenXR
  `/actions/controllers` and `/actions/ue` actions, using the Index interaction profile.
- Grip and aim poses use etee's `handgrip` and `tip` poses respectively.
- Both hands use grip value and grip force for grabbing.
- Both index fingers supply trigger force and trigger touch. The digital trigger
  click uses force with 80% activation and 75% release. The continuous trigger
  axis is raw force; those digital thresholds do not gate the analog axis.
- Pinch trigger mappings are removed to isolate accidental firing.
- Slider up/down double taps provide Index A/B button actions on each hand.
  Menu, magazine and fire-mode behavior still needs a complete action-by-action test.
- Left trackpad retains movement, stick click and Index trackpad mappings.
- Right trackpad supplies horizontal turning through joystick mode with a 70%
  deadzone and exponent 2. Its overlapping weapon-selection mappings are removed.
- A Pavlov per-app offset slot exists but is disabled, with all offsets zero.
  The experimental additional 45-degree rotation is no longer applied.

## 6 November test evidence

- User reports picking up, dropping and turning work in the latest test.
- Magazine replacement/reloading could not be figured out; it is unresolved.
- User reports inward-facing controller angles and no Index controller models
  in the latest test. Do not treat orientation or model selection as fixed.
- Earlier testing reported shots continuing briefly after index-finger removal,
  possibly with an AK-47. Force-only firing was installed afterward; its
  sensitivity and release behavior are not yet confirmed as resolved.
- A headset display failure interrupted an earlier test; its cause is unknown.
- SteamVR's log records the eteeConnect Pavlov default binding loading at
  17:30:35 on 6 November. This confirms a load during the session, rather than
  guaranteeing the active binding at every moment of the test.
- At this checkpoint, the repository binding and installed eteeConnect binding
  have identical SHA-256 hashes:
  `2C955145EC4A6153C6692485F472650C318CE797DB0B67340481761F1A99372F`.

## Historical follow-up checklist (6 November)

1. Confirm the active etee controller binding in SteamVR and inspect its latest
   log entries before changing offsets. The tracker has a separate binding.
2. Reproduce the inward angle using the same binding and controller configuration;
   determine whether the grip pose, aim pose or app offset needs correcting.
3. Test magazine ejection, grabbing a fresh magazine, inserting it, and chambering
   a round. Identify the game's A/B functions and any missing bolt/fire-mode action
   before assigning new controls.
4. Test index-force firing and immediate release in single-shot mode, then
   automatic mode. Watch both trigger axis and click values in the debugger.
5. Check both hands, menu selection, movement, turning, sprint, grabbing,
   dropping, weapon selection and haptics before calling the preset complete.

The installed driver is under Steam's `steamapps/common/eteeConnect` directory.
Its binding is `resources/input/bindings/steam.app.555160_etee_controller.json`.
No launcher scripts, DLL changes or deployable packages are part of this checkpoint.
