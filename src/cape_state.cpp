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
static constexpr f32 TAKEOFF_RISE_SPEED = 42.0f;     // same as a normal jump's power
static constexpr f32 TAKEOFF_FORWARD_SPEED = 32.0f;   // same as max run speed

// Flight
static constexpr f32 FLIGHT_GRAVITY = 0.25f;    // half of normal gravity — gliding is floaty
static constexpr f32 DRAG = 0.02f;              // very slight — speed barely decays at neutral
static constexpr f32 MAX_SPEED = 70.0f;
static constexpr f32 STALL_SPEED = 3.0f;
static constexpr f32 YAW_SPEED = 0.67f;

// Stick forward = dive: adds downward speed, gains forward speed from gravity
static constexpr f32 DIVE_DOWN_FORCE = 5.0f;    // extra downward per frame at full stick (on top of gravity)
static constexpr f32 DIVE_SPEED_GAIN = 1.2f;    // forward speed gained per frame at full dive

// Stick back = climb: trades forward speed for upward movement
static constexpr f32 CLIMB_UP_FORCE = 1.2f;     // upward force at full stick (before speed scaling)
static constexpr f32 CLIMB_SPEED_COST = 0.09f;  // climbing costs meaningful speed

static s32 sTakeoffTimer = 0;
static s32 sFlightFrames = 0;
static bool sInTakeoff = false;
static f32 sVerticalSpeed = 0.0f;
static bool sFlightAnimSet = false;  // only set animation once to avoid sound restart

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
    sInTakeoff = true;
}

static void endFlight(TMario *player, CapeData *cape) {
    cape->isGliding = false;
    player->mAngle.z = 0;
    player->mSpeed.x = 0.0f;
    player->mSpeed.y = 0.0f;
    player->mSpeed.z = 0.0f;
    player->mForwardSpeed = 0.0f;
    sVerticalSpeed = 0.0f;
    sInTakeoff = false;
    // Force Mario into fall state so he doesn't resume the triple jump
    player->changePlayerStatus(0x88C, 0, false);  // STATE_FALL
}

void updateCapeGlide(TMario *player) {
    CapeData *cape = getCapeData(player);
    if (!cape || !cape->isGliding)
        return;

    if (!cape->hasCape) {
        endFlight(player, cape);
        return;
    }

    u32 padInput = player->mController->mButtons.mInput;
    if (padInput & 0x4) {  // D-pad Down exits
        endFlight(player, cape);
        return;
    }

    // mStickV/H range is roughly -122 to 122, normalize to -1 to 1
    f32 stickY = player->mControllerWork->mStickV / 122.0f;
    f32 stickX = player->mControllerWork->mStickH / 122.0f;
    if (stickY > 1.0f) stickY = 1.0f;
    if (stickY < -1.0f) stickY = -1.0f;
    if (stickX > 1.0f) stickX = 1.0f;
    if (stickX < -1.0f) stickX = -1.0f;

    // Prevent spin jump during flight by clearing the stick rotation counter
    // _534 (offset 0x534) is the stick rotation history count used by checkStickRotate()
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
            player->setAnimation(0xAE, 0.0f);  // dive_wait, speed 0 = hold pose
        }

        if (sTakeoffTimer <= 0) {
            sInTakeoff = false;
            sFlightFrames = 0;
            cape->glideSpeed = TAKEOFF_FORWARD_SPEED;
            sVerticalSpeed = 0.0f;  // start flight with zero vertical, gravity will pull
        }
        return;
    }

    // ========================================
    // FLIGHT
    // ========================================
    sFlightFrames++;

    // Yaw — now with normalized stick (-1 to 1), this should be sensible
    if (stickX > 0.2f || stickX < -0.2f) {
        cape->glideYaw -= stickX * YAW_SPEED;
    }
    // Sync Mario's visual rotation to our yaw
    player->mAngle.y = (s16)(cape->glideYaw * (32768.0f / 180.0f));

    // Roll — tilt Mario's model into the turn like SM64 wing cap
    static f32 sCurrentRoll = 0.0f;
    f32 targetRoll = -stickX * 35.0f;  // max 35 degrees of bank at full stick
    sCurrentRoll += (targetRoll - sCurrentRoll) * 0.15f;  // smooth interpolation
    player->mAngle.z = (s16)(sCurrentRoll * (32768.0f / 180.0f));

    // --- Vertical speed: accumulates with drag ---
    // Gravity always pulls down
    sVerticalSpeed -= FLIGHT_GRAVITY;

    // Vertical drag — the faster you're moving vertically, the more resistance
    // This makes climbs and dives naturally plateau instead of being linear
    sVerticalSpeed *= 0.96f;

    if (stickY > 0.15f) {
        // DIVING: extra downward force + gain forward speed
        sVerticalSpeed -= DIVE_DOWN_FORCE * stickY;
        cape->glideSpeed += DIVE_SPEED_GAIN * stickY;
    } else if (stickY < -0.15f) {
        // CLIMBING: push upward, scaled by speed (fast = strong climb)
        // Lift decreases as vertical speed increases (diminishing returns)
        f32 climbInput = -stickY;  // 0 to 1
        f32 speedRatio = cape->glideSpeed / 20.0f;
        if (speedRatio > 2.0f) speedRatio = 2.0f;
        if (speedRatio < 0.3f) speedRatio = 0.3f;
        f32 liftReduction = 1.0f;
        if (sVerticalSpeed > 0.0f) {
            // The higher we're already going up, the less additional lift we get
            liftReduction = 1.0f / (1.0f + sVerticalSpeed * 0.15f);
        }
        sVerticalSpeed += CLIMB_UP_FORCE * climbInput * speedRatio * liftReduction;
        cape->glideSpeed -= CLIMB_SPEED_COST * climbInput;
    }

    // Clamp vertical speed so you can't rocket up or plummet
    if (sVerticalSpeed > 30.0f) sVerticalSpeed = 30.0f;
    if (sVerticalSpeed < -30.0f) sVerticalSpeed = -30.0f;

    // Keep glide animation locked every frame (game tries to override it)
    // setAnimation internally checks if ID matches, won't restart if same
    player->setAnimation(0xAE, 0.0f);  // dive_wait, speed 0 = hold pose

    // Forward speed: drag + clamp
    cape->glideSpeed -= DRAG;
    if (cape->glideSpeed > MAX_SPEED) cape->glideSpeed = MAX_SPEED;
    if (cape->glideSpeed < 0.0f) cape->glideSpeed = 0.0f;

    // Stall (after grace)
    if (sFlightFrames > 60 && cape->glideSpeed < STALL_SPEED) {
        endFlight(player, cape);
        return;
    }

    // --- Apply velocity via mSpeed ---
    f32 yawRad = cape->glideYaw * DEG_TO_RAD;
    player->mSpeed.x = cape->glideSpeed * sinf(yawRad);
    player->mSpeed.y = sVerticalSpeed;
    player->mSpeed.z = cape->glideSpeed * cosf(yawRad);
    player->mForwardSpeed = cape->glideSpeed;

    // Water collision
    if (player->mTranslation.y <= player->mWaterHeight && player->mWaterHeight > -10000.0f) {
        endFlight(player, cape);
        return;
    }

    // Wall collision
    if (player->mWallTriangle != nullptr) {
        endFlight(player, cape);
        return;
    }

    // Ground collision (after grace)
    if (sFlightFrames > 60 && sVerticalSpeed < 0.0f && player->mTranslation.y <= player->mFloorBelow + 10.0f) {
        player->mTranslation.y = player->mFloorBelow;
        endFlight(player, cape);
        return;
    }
}

bool capeGlideState(TMario *player) {
    return false;
}
