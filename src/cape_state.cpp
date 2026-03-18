#include "cape_state.hxx"
#include "cape_data.hxx"
#include <SMS/Player/Mario.hxx>
#include <SMS/Player/MarioGamePad.hxx>
#include <math.h>

static constexpr f32 DEG_TO_RAD = 3.14159265f / 180.0f;

static void exitGlide(TMario *player, CapeData *cape) {
    cape->isGliding = false;
    player->changePlayerStatus(STATE_FALL, 0, false);
}

static void updateFlightAngles(TMario *player, CapeData *cape) {
    f32 stickY = player->mControllerWork->mStickV;
    f32 stickX = player->mControllerWork->mStickH;

    // Push forward = dive (decrease pitch / more negative)
    cape->glidePitch -= stickY * CAPE_PITCH_RATE;
    if (cape->glidePitch < CAPE_PITCH_MIN)
        cape->glidePitch = CAPE_PITCH_MIN;
    if (cape->glidePitch > CAPE_PITCH_MAX)
        cape->glidePitch = CAPE_PITCH_MAX;

    cape->glideYaw += stickX * CAPE_TURN_RATE;
}

static void updateFlightSpeed(CapeData *cape) {
    if (cape->glidePitch < -5.0f) {
        // Diving — gain speed
        cape->glideSpeed += CAPE_DIVE_ACCEL;
        if (cape->glideSpeed > CAPE_MAX_DIVE_SPEED)
            cape->glideSpeed = CAPE_MAX_DIVE_SPEED;
    } else if (cape->glidePitch > 5.0f) {
        // Climbing — lose speed
        cape->glideSpeed -= CAPE_CLIMB_DECEL;
    } else {
        // Neutral — gentle drag
        cape->glideSpeed -= CAPE_DRAG_DECEL;
    }

    if (cape->glideSpeed < 0.0f)
        cape->glideSpeed = 0.0f;
}

static void applyFlightVelocity(TMario *player, CapeData *cape) {
    f32 pitchRad = cape->glidePitch * DEG_TO_RAD;
    f32 yawRad   = cape->glideYaw * DEG_TO_RAD;

    f32 horizSpeed = cape->glideSpeed * cosf(pitchRad);
    f32 vertSpeed  = cape->glideSpeed * sinf(pitchRad);

    player->mSpeed.x = horizSpeed * sinf(yawRad);
    player->mSpeed.y = vertSpeed;
    player->mSpeed.z = horizSpeed * cosf(yawRad);

    // 360 degrees = 65536 s16 units => 182.04 units/degree
    player->mAngle.y = (s16)(cape->glideYaw * 182.04f);
}

bool capeGlideState(TMario *player) {
    CapeData *cape = getCapeData(player);
    if (!cape) {
        player->changePlayerStatus(STATE_FALL, 0, false);
        return true;
    }

    // --- Exit conditions ---

    // Cape expired
    if (!cape->hasCape) {
        exitGlide(player, cape);
        return true;
    }

    // R released
    bool rHeld = (player->mControllerWork->mInput & TMarioControllerWork::R);
    if (!rHeld) {
        exitGlide(player, cape);
        return true;
    }

    // B pressed this frame
    bool bPressed = (player->mControllerWork->mFrameInput & TMarioControllerWork::B);
    if (bPressed) {
        exitGlide(player, cape);
        return true;
    }

    // Stall — too slow
    if (cape->glideSpeed < CAPE_STALL_SPEED) {
        exitGlide(player, cape);
        return true;
    }

    // --- Collision checks ---

    // Ground
    if (player->mTranslation.y <= player->mFloorBelow + 10.0f) {
        exitGlide(player, cape);
        return true;
    }

    // Wall
    if (player->mWallTriangle != nullptr) {
        exitGlide(player, cape);
        return true;
    }

    // Water surface
    if (player->mTranslation.y <= player->mWaterHeight) {
        exitGlide(player, cape);
        return true;
    }

    // --- Update flight ---
    updateFlightAngles(player, cape);
    updateFlightSpeed(cape);
    applyFlightVelocity(player, cape);

    // Ceiling bounce
    if (player->mRoofTriangle != nullptr) {
        player->mSpeed.y = -2.0f;
        cape->glideSpeed *= 0.8f;
        cape->glidePitch = -10.0f;
    }

    return false;  // state continues
}
