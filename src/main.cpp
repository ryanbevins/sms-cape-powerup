#include <BetterSMS/module.hxx>
#include <BetterSMS/object.hxx>
#include <BetterSMS/player.hxx>
#include <BetterSMS/settings.hxx>
#include <BetterSMS/stage.hxx>
#include <SMS/Player/Mario.hxx>
#include <SMS/Player/MarioGamePad.hxx>
#include "cape_box.hxx"
#include "cape_data.hxx"
#include "cape_timer.hxx"
#include "cape_state.hxx"

void updateCapeVisual(TMario *player);

static BetterSMS::Settings::SettingsGroup sSettingsGroup(1, 0, BetterSMS::Settings::Priority::MODE);
static BetterSMS::ModuleInfo sModuleInfo("Cape Powerup", 1, 0, &sSettingsGroup);
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
    if (!cape)
        return;

    if (!player->mControllerWork || !player->mController)
        return;

    // DEBUG: D-pad Up to give cape
    u32 padButtons = player->mController->mButtons.mInput;
    if ((padButtons & 0x8) && !cape->hasCape) {
        giveCapeTo(player);
    }

    // If gliding, run flight physics (no custom state needed)
    if (cape->isGliding) {
        updateCapeGlide(player);
        // If glide just ended this frame, make sure roll is reset
        if (!cape->isGliding) {
            player->mAngle.z = 0;
        }
        return;
    }

    // Safety: always zero roll when not gliding
    player->mAngle.z = 0;

    if (!cape->hasCape)
        return;

    // Activate flight: jump while running fast
    // A button just pressed + moving fast enough on ground
    u32 padFrame = player->mController->mButtons.mFrameInput;
    bool aPressed = (padFrame & 0x100);  // A button
    bool airborne = (player->mState & 0x800);
    bool fastEnough = (player->mForwardSpeed >= 20.0f);

    // Also allow D-pad Right as debug trigger while airborne
    u32 padInput = player->mController->mButtons.mInput;
    bool dpadRight = (padInput & 0x2);

    if ((aPressed && fastEnough && !airborne) || (dpadRight && airborne)) {
        startCapeFlight(player);
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
    BetterSMS::Objects::registerObjectAsMapObj("CapeBox", &capeBoxData, TCapeBox::instantiate);
}

KURIBO_MODULE_BEGIN("Cape Powerup", "SMS Decomp", "1.0") {
    KURIBO_EXECUTE_ON_LOAD { initModule(); }
    KURIBO_EXECUTE_ON_UNLOAD { }
}
KURIBO_MODULE_END()
