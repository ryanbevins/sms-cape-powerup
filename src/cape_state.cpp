#include "cape_state.hxx"
#include "cape_data.hxx"
#include <SMS/Player/Mario.hxx>
#include <SMS/Player/MarioGamePad.hxx>
#include <math.h>

static constexpr f32 PI = 3.14159265f;
static constexpr f32 DEG_TO_RAD = PI / 180.0f;

// ==============================================
// Values based on actual SMS physics:
//   Normal gravity = 0.5/frame
//   Jump power = 42 (mSpeed.y on jump)
//   Max run speed = 32 (mForwardSpeed)
// ==============================================

// Takeoff
static constexpr s32 TAKEOFF_FRAMES = 90;
static constexpr f32 TAKEOFF_RISE_SPEED = 42.0f;
static constexpr f32 TAKEOFF_FORWARD_SPEED = 32.0f;

// Flight
static constexpr f32 FLIGHT_GRAVITY = 0.25f;
static constexpr f32 DRAG = 0.02f;
static constexpr f32 MAX_SPEED = 70.0f;
static constexpr f32 STALL_SPEED = 3.0f;
static constexpr f32 YAW_SPEED = 0.67f;

// Dive
static constexpr f32 DIVE_DOWN_FORCE = 5.0f;
static constexpr f32 DIVE_SPEED_GAIN = 1.2f;

// Climb
static constexpr f32 CLIMB_UP_FORCE = 1.2f;
static constexpr f32 CLIMB_SPEED_COST = 0.09f;

// Ground pound dive bomb
static constexpr f32 DIVEBOMB_SPEED = -25.0f;  // straight down, fast

// Smooth landing
static constexpr f32 LANDING_SPEED_CARRY = 0.6f;  // carry 60% of flight speed into run

static s32 sTakeoffTimer = 0;
static s32 sFlightFrames = 0;
static bool sInTakeoff = false;
static f32 sVerticalSpeed = 0.0f;
static bool sFlightAnimSet = false;
static f32 sCurrentRoll = 0.0f;
static f32 sCurrentPitchTilt = 0.0f;
static bool sDiveBombing = false;

void startCapeFlight(TMario *player) {
    CapeData *cape = getCapeData(player);
    if (!cape) return;

    cape->isGliding = true;
    cape->glideSpeed = TAKEOFF_FORWARD_SPEED;
    cape->glidePitch = 0.0f;
    cape->glideYaw = (f32)(player->mAngle.y) * (180.0f / 32768.0f);
    sTakeoffTimer = TAKEOFF_FRAMES;
    sFlightFrames = 0;
    sVerticalSpeed = 0.0f;
    sFlightAnimSet = false;
    sCurrentRoll = 0.0f;
    sCurrentPitchTilt = 0.0f;
    sDiveBombing = false;
    sInTakeoff = true;
}

static void endFlight(TMario *player, CapeData *cape, bool smoothLand) {
    f32 landingSpeed = cape->glideSpeed;
    f32 landingYaw = cape->glideYaw * DEG_TO_RAD;

    cape->isGliding = false;
    player->mAngle.z = 0;
    player->mAngle.x = 0;
    sVerticalSpeed = 0.0f;
    sInTakeoff = false;
    sCurrentRoll = 0.0f;
    sCurrentPitchTilt = 0.0f;
    sDiveBombing = false;

    if (smoothLand && landingSpeed > 5.0f) {
        // Carry momentum into a run
        f32 carrySpeed = landingSpeed * LANDING_SPEED_CARRY;
        player->mSpeed.x = carrySpeed * sinf(landingYaw);
        player->mSpeed.y = 0.0f;
        player->mSpeed.z = carrySpeed * cosf(landingYaw);
        player->mForwardSpeed = carrySpeed;
    } else {
        player->mSpeed.x = 0.0f;
        player->mSpeed.y = 0.0f;
        player->mSpeed.z = 0.0f;
        player->mForwardSpeed = 0.0f;
    }

    player->changePlayerStatus(0x88C, 0, false);  // STATE_FALL
}

void updateCapeGlide(TMario *player) {
    CapeData *cape = getCapeData(player);
    if (!cape || !cape->isGliding)
        return;

    if (!cape->hasCape) {
        endFlight(player, cape, false);
        return;
    }

    u32 padInput = player->mController->mButtons.mInput;
    if (padInput & 0x4) {  // D-pad Down exits
        endFlight(player, cape, false);
        return;
    }

    // mStickV/H range is roughly -122 to 122, normalize to -1 to 1
    f32 stickY = player->mControllerWork->mStickV / 122.0f;
    f32 stickX = player->mControllerWork->mStickH / 122.0f;
    if (stickY > 1.0f) stickY = 1.0f;
    if (stickY < -1.0f) stickY = -1.0f;
    if (stickX > 1.0f) stickX = 1.0f;
    if (stickX < -1.0f) stickX = -1.0f;

    // Prevent spin jump during flight
    player->_534 = 0;

    // ========================================
    // TAKEOFF
    // ========================================
    if (sInTakeoff) {
        sTakeoffTimer--;

        f32 yawRad = cape->glideYaw * DEG_TO_RAD;
        f32 riseFactor = 1.0f;
        if (sTakeoffTimer < 30)
            riseFactor = (f32)sTakeoffTimer / 30.0f;

        player->mSpeed.x = TAKEOFF_FORWARD_SPEED * sinf(yawRad);
        player->mSpeed.y = TAKEOFF_RISE_SPEED * riseFactor;
        player->mSpeed.z = TAKEOFF_FORWARD_SPEED * cosf(yawRad);
        player->mForwardSpeed = TAKEOFF_FORWARD_SPEED;

        // Transition to glide pose partway through takeoff
        if (sTakeoffTimer < 60) {
            player->setAnimation(0xAE, 0.0f);
        }

        if (sTakeoffTimer <= 0) {
            sInTakeoff = false;
            sFlightFrames = 0;
            cape->glideSpeed = TAKEOFF_FORWARD_SPEED;
            sVerticalSpeed = 0.0f;
        }
        return;
    }

    // ========================================
    // DIVEBOMB — B pressed during flight
    // ========================================
    u32 padFrame = player->mController->mButtons.mFrameInput;
    if ((padFrame & 0x200) && !sDiveBombing) {  // B button
        sDiveBombing = true;
        cape->glideSpeed *= 0.3f;  // kill most forward speed
    }

    if (sDiveBombing) {
        // Straight down, fast
        sVerticalSpeed = DIVEBOMB_SPEED;
        player->mSpeed.x = cape->glideSpeed * sinf(cape->glideYaw * DEG_TO_RAD) * 0.5f;
        player->mSpeed.y = sVerticalSpeed;
        player->mSpeed.z = cape->glideSpeed * cosf(cape->glideYaw * DEG_TO_RAD) * 0.5f;
        player->mForwardSpeed = cape->glideSpeed * 0.5f;

        // Pitch Mario forward for divebomb look
        player->mAngle.x = (s16)(45.0f * (32768.0f / 180.0f));
        player->mAngle.z = 0;
        player->mAngle.y = (s16)(cape->glideYaw * (32768.0f / 180.0f));

        player->setAnimation(0xAE, 0.0f);

        // Ground collision — divebomb always checks
        if (player->mTranslation.y <= player->mFloorBelow + 15.0f) {
            player->mTranslation.y = player->mFloorBelow;
            player->mAngle.x = 0;
            // TODO: ground pound shockwave / damage enemies
            endFlight(player, cape, false);
            return;
        }

        // Water collision
        if (player->mTranslation.y <= player->mWaterHeight && player->mWaterHeight > -10000.0f) {
            player->mAngle.x = 0;
            endFlight(player, cape, false);
            return;
        }
        return;
    }

    // ========================================
    // FLIGHT
    // ========================================
    sFlightFrames++;

    // Yaw
    if (stickX > 0.2f || stickX < -0.2f) {
        cape->glideYaw -= stickX * YAW_SPEED;
    }
    player->mAngle.y = (s16)(cape->glideYaw * (32768.0f / 180.0f));

    // Roll — bank into turns
    f32 targetRoll = -stickX * 35.0f;
    sCurrentRoll += (targetRoll - sCurrentRoll) * 0.15f;
    player->mAngle.z = (s16)(sCurrentRoll * (32768.0f / 180.0f));

    // Pitch tilt — nose down when diving, nose up when climbing
    f32 targetPitch = 0.0f;
    if (stickY > 0.15f) {
        targetPitch = stickY * 30.0f;   // tilt forward up to 30 degrees
    } else if (stickY < -0.15f) {
        targetPitch = stickY * 20.0f;   // tilt back up to 20 degrees
    }
    sCurrentPitchTilt += (targetPitch - sCurrentPitchTilt) * 0.12f;
    player->mAngle.x = (s16)(sCurrentPitchTilt * (32768.0f / 180.0f));

    // --- Vertical speed ---
    sVerticalSpeed -= FLIGHT_GRAVITY;
    sVerticalSpeed *= 0.96f;

    if (stickY > 0.15f) {
        sVerticalSpeed -= DIVE_DOWN_FORCE * stickY;
        cape->glideSpeed += DIVE_SPEED_GAIN * stickY;
    } else if (stickY < -0.15f) {
        f32 climbInput = -stickY;
        f32 speedRatio = cape->glideSpeed / 20.0f;
        if (speedRatio > 2.0f) speedRatio = 2.0f;
        if (speedRatio < 0.3f) speedRatio = 0.3f;
        f32 liftReduction = 1.0f;
        if (sVerticalSpeed > 0.0f) {
            liftReduction = 1.0f / (1.0f + sVerticalSpeed * 0.15f);
        }
        sVerticalSpeed += CLIMB_UP_FORCE * climbInput * speedRatio * liftReduction;
        cape->glideSpeed -= CLIMB_SPEED_COST * climbInput;
    }

    if (sVerticalSpeed > 30.0f) sVerticalSpeed = 30.0f;
    if (sVerticalSpeed < -30.0f) sVerticalSpeed = -30.0f;

    // Glide animation
    player->setAnimation(0xAE, 0.0f);

    // Forward speed
    cape->glideSpeed -= DRAG;
    if (cape->glideSpeed > MAX_SPEED) cape->glideSpeed = MAX_SPEED;
    if (cape->glideSpeed < 0.0f) cape->glideSpeed = 0.0f;

    // Stall
    if (sFlightFrames > 60 && cape->glideSpeed < STALL_SPEED) {
        endFlight(player, cape, false);
        return;
    }

    // Apply velocity
    f32 yawRad = cape->glideYaw * DEG_TO_RAD;
    player->mSpeed.x = cape->glideSpeed * sinf(yawRad);
    player->mSpeed.y = sVerticalSpeed;
    player->mSpeed.z = cape->glideSpeed * cosf(yawRad);
    player->mForwardSpeed = cape->glideSpeed;

    // Water collision
    if (player->mTranslation.y <= player->mWaterHeight && player->mWaterHeight > -10000.0f) {
        endFlight(player, cape, false);
        return;
    }

    // Wall collision
    if (player->mWallTriangle != nullptr) {
        endFlight(player, cape, false);
        return;
    }

    // Ground collision (smooth landing)
    if (sFlightFrames > 60 && sVerticalSpeed < 0.0f && player->mTranslation.y <= player->mFloorBelow + 10.0f) {
        player->mTranslation.y = player->mFloorBelow;
        endFlight(player, cape, true);  // smooth land with momentum carry
        return;
    }
}

bool capeGlideState(TMario *player) {
    return false;
}
