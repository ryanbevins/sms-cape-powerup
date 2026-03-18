#include "cape_timer.hxx"
#include "cape_data.hxx"
#include <SMS/Player/Mario.hxx>
#include <SMS/Player/Watergun.hxx>

void giveCapeTo(TMario *player) {
    CapeData *cape = getCapeData(player);
    if (!cape)
        return;

    if (cape->hasCape) {
        cape->timer = CAPE_TIMER_DURATION;
        return;
    }

    TWaterGun *fludd = player->mFludd;
    if (fludd) {
        cape->storedNozzle = fludd->mCurrentNozzle;
        cape->storedSecondNozzle = fludd->mSecondNozzle;
        cape->storedWater = fludd->mCurrentWater;
    }

    cape->hasCape = true;
    cape->timer = CAPE_TIMER_DURATION;
    cape->isGliding = false;
    cape->persistAcrossLoad = true;

    if (fludd) {
        fludd->mIsEmitWater = false;
    }
}

void removeCape(TMario *player) {
    CapeData *cape = getCapeData(player);
    if (!cape || !cape->hasCape)
        return;

    TWaterGun *fludd = player->mFludd;
    if (fludd) {
        fludd->mCurrentNozzle = cape->storedNozzle;
        fludd->mSecondNozzle = cape->storedSecondNozzle;
        fludd->mCurrentWater = cape->storedWater;
    }

    cape->hasCape = false;
    cape->isGliding = false;
    cape->timer = 0.0f;
    cape->persistAcrossLoad = false;
}

void tickCapeTimer(TMario *player) {
    CapeData *cape = getCapeData(player);
    if (!cape || !cape->hasCape)
        return;

    cape->timer -= (1.0f / 30.0f);

    TWaterGun *fludd = player->mFludd;
    if (fludd) {
        fludd->mIsEmitWater = false;
    }

    if (cape->timer <= 0.0f) {
        cape->timer = 0.0f;
        if (cape->isGliding) {
            cape->isGliding = false;
            player->changePlayerStatus(STATE_FALL, 0, false);
        }
        removeCape(player);
    }
}
