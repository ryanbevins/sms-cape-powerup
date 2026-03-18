#include "cape_data.hxx"
#include <BetterSMS/player.hxx>
#include <SMS/Player/Mario.hxx>

CapeData *getCapeData(TMario *player) {
    return static_cast<CapeData *>(
        Player::getRegisteredData(player, CAPE_DATA_KEY)
    );
}

void initCapeData(CapeData *data) {
    data->hasCape = false;
    data->timer = 0.0f;
    data->isGliding = false;
    data->glideSpeed = 0.0f;
    data->glidePitch = 0.0f;
    data->glideYaw = 0.0f;
    data->storedNozzle = 0;
    data->storedSecondNozzle = 0;
    data->storedWater = 0;
    data->persistAcrossLoad = false;
}
