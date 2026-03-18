#pragma once

#include <SMS/MapObj/MapObjGeneral.hxx>
#include <SMS/MapObj/MapObjInit.hxx>
#include <JSystem/JDrama/JDRNameRef.hxx>
#include <JSystem/JSupport/JSUMemoryStream.hxx>

class TCapeBox : public TMapObjGeneral {
public:
    TCapeBox(const char *name);
    ~TCapeBox() override = default;

    void load(JSUMemoryInputStream &stream) override;
    void control() override;
    bool receiveMessage(THitActor *sender, u32 message) override;

    static JDrama::TNameRef *instantiate() {
        return new TCapeBox("CapeBox");
    }

private:
    bool mBroken;
};

extern ObjData capeBoxData;
