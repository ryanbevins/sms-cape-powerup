#include <BetterSMS/module.hxx>
#include <BetterSMS/player.hxx>
#include <BetterSMS/stage.hxx>
#include <SMS/Player/Mario.hxx>
#include "cape_data.hxx"
#include "cape_timer.hxx"

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
}

BETTER_SMS_FOR_CALLBACK void onStageExit(TApplication *app) {
    sPlayerCapeData.persistAcrossLoad = false;
}

static void initModule() {
    BetterSMS::registerModule(sModuleInfo);
    Player::addInitCallback(onPlayerInit);
    Player::addUpdateCallback(onPlayerUpdate);
    Stage::addExitCallback(onStageExit);
}

KURIBO_MODULE_BEGIN("Cape Powerup", "SMS Decomp", "1.0") {
    KURIBO_EXECUTE_ON_LOAD { initModule(); }
    KURIBO_EXECUTE_ON_UNLOAD { }
}
KURIBO_MODULE_END()
