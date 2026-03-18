#include <BetterSMS/module.hxx>
#include <BetterSMS/game.hxx>
#include <BetterSMS/object.hxx>
#include <BetterSMS/player.hxx>
#include <BetterSMS/settings.hxx>
#include <BetterSMS/stage.hxx>
#include <SMS/Player/Mario.hxx>
#include <SMS/Player/MarioGamePad.hxx>
#include <SMS/System/Application.hxx>
#include <SMS/System/GameSequence.hxx>
#include "cape_box.hxx"
#include "cape_data.hxx"
#include "cape_timer.hxx"
#include "cape_state.hxx"

void updateCapeVisual(TMario *player);

static BetterSMS::Settings::SettingsGroup sSettingsGroup(1, 0, BetterSMS::Settings::Priority::MODE);
static BetterSMS::ModuleInfo sModuleInfo("Cape Powerup", 1, 0, &sSettingsGroup);
static CapeData sPlayerCapeData;
static bool sFlightActivatedThisJump = false;

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

    // Reset activation flag when Mario is on the ground
    if (!(player->mState & 0x800)) {
        sFlightActivatedThisJump = false;
    }

    // Activate flight on triple jump (STATE_TRIPLE_J = 0x882), only once per jump
    bool tripleJump = ((player->mState & 0xFFF) == 0x882);
    bool inWater = (player->mState & 0x2000);

    if (tripleJump && !inWater && !sFlightActivatedThisJump) {
        sFlightActivatedThisJump = true;
        startCapeFlight(player);
    }

    updateCapeVisual(player);
}

BETTER_SMS_FOR_CALLBACK void onStageExit(TApplication *app) {
    sPlayerCapeData.persistAcrossLoad = false;
}

// DEBUG: Skip intro, go straight to Delfino Plaza episode 0
BETTER_SMS_FOR_CALLBACK void onGameBoot(TApplication *app) {
    app->mNextScene.set(TGameSequence::AREA_BIANCO, 1, JDrama::TFlagT<u16>(0));
}

static void initModule() {
    BetterSMS::registerModule(sModuleInfo);
    Game::addBootCallback(onGameBoot);
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
