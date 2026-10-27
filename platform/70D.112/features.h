#include "all_features.h"

#undef FEATURE_QUICK_ERASE    // Canon has it
#undef FEATURE_IMAGE_EFFECTS  // None working
#undef FEATURE_AF_PATTERNS    // Canon has it
#undef FEATURE_MLU_HANDHELD   // Not needed, Canon's silent mode is much better
#undef FEATURE_SHUTTER_LOCK   // Canon has a dedicated button for it
#undef FEATURE_IMAGE_POSITION // assume it is not needed with a variangle display
#undef FEATURE_ARROW_SHORTCUTS // No suitable button found

/* FPS override: DISABLED, and this is the authoritative state.
 * Timer B cannot be used on the 70D (untraceable problems, causes vertical
 * banding).  A Timer-A-only variant via fps_criteria=3 ("HiJello, FastTv")
 * was experimented with in 2026-04 but was never validated on hardware and
 * interferes with LiveView init (display corruption).  Do not re-enable
 * without a hardware test; older notes claiming this is enabled are stale. */
#undef FEATURE_FPS_OVERRIDE

/* see comments in lens.c */
#undef FEATURE_FOLLOW_FOCUS
#undef FEATURE_RACK_FOCUS

/* Focus stacking is still buggy on this body ("takes 1 behind and 1 before",
 * and all subsequent frames land at the same position regardless of the menu).
 * all_features.h enables it whenever CONFIG_PROP_REQUEST_CHANGE is set, so it
 * has to be turned off explicitly here. */
#undef FEATURE_FOCUS_STACKING

/* disable RAW zebras as they cause problems in QR and LV too */
#undef FEATURE_RAW_ZEBRAS

// we got enough infos on main display and top lcd.
// Mainly this got disabled due to the bottom line
// (time, battery) flickering 
// ToDo: alternative coordinates not using outer ones
// custom builds may include it til we achieve it
#undef FEATURE_FLEXINFO

/* Audio meters are enabled unconditionally by all_features.h; no need to
 * repeat it here.  FEATURE_AUDIO_REMOTE_SHOT is deliberately NOT enabled:
 * all_features.h only defines it inside CONFIG_AUDIO_CONTROLS, because
 * without audio controls there is no sound in photo mode, and
 * CONFIG_AUDIO_CONTROLS is off for the 70D (see internals.h). */

/* Features ported from 6D/5D3 (DIGIC V peers) */
#define FEATURE_ZOOM_TRICK_5D3 /* Double-click to zoom shortcut (5D3/6D) */
/* FEATURE_KEN_ROCKWELL_ZOOM_5D3 removed: the flag is not tested anywhere in
 * the ML tree, so enabling it was a silent no-op. */
#define FEATURE_SWAP_INFO_PLAY       /* Swap info display in playback mode (6D) */
#define FEATURE_LV_FOCUS_BOX_SNAP_TO_X5_RAW /* Snap focus box to x5 in raw mode (5D3) */
//~ #define FEATURE_FOCUS_PEAK_DISP_FILTER /* Focus peaking as display filter (6D) — may corrupt LV */
