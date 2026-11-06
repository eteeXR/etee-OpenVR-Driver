# Pavlov OpenXR binding checkpoint

Saved 6 November 2026 on `bindings/pavlov-openxr`. This is unfinished work,
not a release-ready preset.

## Current implementation

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

## Latest test evidence

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

## Resume here

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
