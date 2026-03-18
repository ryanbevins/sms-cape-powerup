#include <BetterSMS/module.hxx>
#include <BetterSMS/object.hxx>
#include <BetterSMS/player.hxx>
#include <BetterSMS/stage.hxx>
#include <SMS/Player/Mario.hxx>
#include <SMS/Player/MarioGamePad.hxx>
#include "cape_box.hxx"
#include "cape_data.hxx"
#include "cape_timer.hxx"
#include "cape_state.hxx"

void updateCapeVisual(TMario *player);

static BetterSMS::ModuleInfo sModuleInfo("Cape Powerup", 1, 0, nullptr);
static CapeData sPlayerCapeData;

BETTER_SMS_FOR_CALLBACK void onPlayerInit(TMario *player, bool isMario) {
    if (!isMario)
        return;

    if (sPlayerCapeData.persistAcrossLoad && sPlayerCapeData.hasCape) {
        Player::registerData(player, CAPE_DATA_KEY, &sPlayerCapeData);
        return;
    }

    initCapeData(&sPlayerCapeData);
    Player::registerData(player, CAPE_DATA_KEY, &sPlayerCapeData);
}

BETTER_SMS_FOR_CALLBACK void onPlayerUpdate(TMario *player, bool isMario) {
    if (!isMario)
        return;
    tickCapeTimer(player);

    CapeData *cape = getCapeData(player);
    if (!cape || !cape->hasCape || cape->isGliding)
        return;

    // R pressed this frame while airborne -> enter glide
    bool rPressed = (player->mControllerWork->mFrameInput & TMarioControllerWork::R);
    bool airborne = (player->mState & 0x800);

    if (rPressed && airborne) {
        cape->isGliding = true;
        cape->glideSpeed = CAPE_BASE_GLIDE_SPEED;
        cape->glidePitch = 0.0f;
        cape->glideYaw   = (f32)(player->mAngle.y) / 182.04f;
        player->changePlayerStatus(STATE_CAPE_GLIDE, 0, false);
    }

    updateCapeVisual(player);
}

BETTER_SMS_FOR_CALLBACK void onStageExit(TApplication *app) {
    sPlayerCapeData.persistAcrossLoad = false;
}

static void initModule() {
    BetterSMS::registerModule(sModuleInfo);
    Player::addInitCallback(onPlayerInit);
    Player::addUpdateCallback(onPlayerUpdate);
    Stage::addExitCallback(onStageExit);
    Player::registerStateMachine(STATE_CAPE_GLIDE, capeGlideState);

    BetterSMS::Objects::registerObjectAsMapObj("CapeBox", &capeBoxData, TCapeBox::instantiate);
}

KURIBO_MODULE_BEGIN("Cape Powerup", "SMS Decomp", "1.0") {
    KURIBO_EXECUTE_ON_LOAD { initModule(); }
    KURIBO_EXECUTE_ON_UNLOAD { }
}
KURIBO_MODULE_END()
