#include "cape_box.hxx"
#include "cape_data.hxx"
#include "cape_timer.hxx"
#include <SMS/Player/Mario.hxx>

static hit_data capeBoxHitData = {
    200.0f, 200.0f, 100.0f, 200.0f
};

static obj_hit_info capeBoxHitInfo = {
    1, 0x80000000, 0, &capeBoxHitData
};

ObjData capeBoxData = {
    .mMdlName         = "nozzleBox",
    .mObjectID        = 0x80000500,
    .mLiveManagerName = reinterpret_cast<const char *>(gLiveManagerName),
    .mObjKey          = reinterpret_cast<const char *>(gUnkManagerName),
    .mAnimInfo         = nullptr,
    .mObjCollisionData = &capeBoxHitInfo,
    .mMapCollisionInfo = nullptr,
    .mSoundInfo        = nullptr,
    .mPhysicalInfo     = nullptr,
    .mSinkData         = nullptr,
    ._28               = nullptr,
    .mBckMoveData      = nullptr,
    ._30               = 50.0f,
    .mUnkFlags         = 0x10004000,
    .mKeyCode          = 0
};

TCapeBox::TCapeBox(const char *name)
    : TMapObjGeneral(name)
    , mBroken(false)
{
}

void TCapeBox::load(JSUMemoryInputStream &stream) {
    TMapObjGeneral::load(stream);
}

void TCapeBox::control() {
    if (mBroken)
        return;
    TMapObjGeneral::control();
}

bool TCapeBox::receiveMessage(THitActor *sender, u32 message) {
    if (mBroken)
        return false;

    // Accept attack messages (body=1, ground pound=7, generic attack=0xE)
    if (message != 1 && message != 7 && message != 0xE)
        return TMapObjGeneral::receiveMessage(sender, message);

    // Verify sender is Mario
    if (sender != reinterpret_cast<THitActor *>(gpMarioAddress))
        return false;

    TMario *player = static_cast<TMario *>(sender);
    mBroken = true;
    giveCapeTo(player);

    // Remove the object from the scene
    makeObjDead();

    return true;
}
