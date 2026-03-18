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
#include <JSystem/J2D/J2DTextBox.hxx>
#include <Dolphin/string.h>
#include <Dolphin/printf.h>
#include "cape_box.hxx"
#include "cape_data.hxx"
#include "cape_timer.hxx"
#include "cape_state.hxx"

void updateCapeVisual(TMario *player);

static BetterSMS::Settings::SettingsGroup sSettingsGroup(1, 0, BetterSMS::Settings::Priority::MODE);
static BetterSMS::ModuleInfo sModuleInfo("Cape Powerup", 1, 0, &sSettingsGroup);
static CapeData sPlayerCapeData;
static bool sFlightActivatedThisJump = false;

// Debug HUD
static J2DTextBox *sDebugText = nullptr;
static J2DTextBox *sDebugShadow = nullptr;
static char sDebugBuf[128];

BETTER_SMS_FOR_CALLBACK void onPlayerInit(TMario *player, bool isMario) {
    if (!isMario)
        return;

    if (sPlayerCapeData.persistAcrossLoad && sPlayerCapeData.hasCape) {
        Player::registerData(player, CAPE_DATA_KEY, &sPlayerCapeData);
        return;
    }

    initCapeData(&sPlayerCapeData);
    Player::registerData(player, CAPE_DATA_KEY, &sPlayerCapeData);

    // Auto-give cape on spawn (debug — remove when CapeBox is in use)
    giveCapeTo(player);
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

    // Update debug text
    if (cape->hasCape) {
        int timeLeft = (int)(cape->timer / 2.0f);  // divide by 2 since BSE runs at ~60hz
        snprintf(sDebugBuf, sizeof(sDebugBuf), "CAPE: %s  TIME: %ds  SPD: %.0f",
            cape->isGliding ? "FLYING" : "ACTIVE",
            timeLeft,
            cape->glideSpeed);
    } else {
        snprintf(sDebugBuf, sizeof(sDebugBuf), "CAPE: OFF");
    }
    if (sDebugText) {
        sDebugText->mStrPtr = sDebugBuf;
    }
    if (sDebugShadow) {
        sDebugShadow->mStrPtr = sDebugBuf;
    }

    // If gliding, run flight physics
    if (cape->isGliding) {
        updateCapeGlide(player);
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
    sDebugText = nullptr;
    sDebugShadow = nullptr;
}

// DEBUG: Skip intro
BETTER_SMS_FOR_CALLBACK void onGameBoot(TApplication *app) {
    app->mNextScene.set(TGameSequence::AREA_BIANCO, 1, JDrama::TFlagT<u16>(0));
}

BETTER_SMS_FOR_CALLBACK void onStageInit(TMarDirector *director) {
    // Create debug HUD text boxes
    sDebugText = new J2DTextBox(gpSystemFont->mFont, "CAPE: OFF");
    sDebugText->mGradientTop = {255, 255, 100, 255};
    sDebugText->mGradientBottom = {255, 200, 50, 255};

    sDebugShadow = new J2DTextBox(gpSystemFont->mFont, "CAPE: OFF");
    sDebugShadow->mGradientTop = {0, 0, 0, 200};
    sDebugShadow->mGradientBottom = {0, 0, 0, 200};

    snprintf(sDebugBuf, sizeof(sDebugBuf), "CAPE: OFF");
}

BETTER_SMS_FOR_CALLBACK void onStageDraw2D(TMarDirector *director,
                                           const J2DOrthoGraph *ortho) {
    if (!sDebugText || !sDebugShadow)
        return;
    // Top-left corner
    sDebugShadow->draw(17, 47);
    sDebugText->draw(16, 46);
}

static void initModule() {
    BetterSMS::registerModule(sModuleInfo);
    Game::addBootCallback(onGameBoot);
    Player::addInitCallback(onPlayerInit);
    Player::addUpdateCallback(onPlayerUpdate);
    Stage::addInitCallback(onStageInit);
    Stage::addDraw2DCallback(onStageDraw2D);
    Stage::addExitCallback(onStageExit);
    BetterSMS::Objects::registerObjectAsMapObj("CapeBox", &capeBoxData, TCapeBox::instantiate);
}

KURIBO_MODULE_BEGIN("Cape Powerup", "SMS Decomp", "1.0") {
    KURIBO_EXECUTE_ON_LOAD { initModule(); }
    KURIBO_EXECUTE_ON_UNLOAD { }
}
KURIBO_MODULE_END()
