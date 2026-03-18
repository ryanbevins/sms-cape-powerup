#include "cape_data.hxx"
#include <SMS/Player/Mario.hxx>
#include <SMS/Player/Watergun.hxx>

void updateCapeVisual(TMario *player) {
    CapeData *cape = getCapeData(player);
    if (!cape)
        return;

    if (cape->hasCape) {
        // Calculate fade for last 10 seconds
        f32 elapsed = CAPE_TIMER_DURATION - cape->timer;
        if (elapsed > CAPE_FADE_START) {
            f32 fadeRatio = cape->timer / (CAPE_TIMER_DURATION - CAPE_FADE_START);
            // fadeRatio: 1.0 (solid) -> 0.0 (invisible)
            // TODO: Apply to cape model material alpha when model exists
            (void)fadeRatio;
        }

        // TODO: Hide FLUDD backpack model while cape is active
        // TODO: Render cape model attached to Mario's back joint
    }
}
