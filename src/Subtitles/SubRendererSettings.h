#pragma once
#include "OpenTypeLangTags.h"
#include "STSStyle.h"

class SubRendererSettings {
public:
    SubRendererSettings() {}
    virtual ~SubRendererSettings() {}

    STSStyle defaultStyle;
    bool overrideDefaultStyle = false;
    bool overrideAllStyles = false;

    bool renderSSAUsingLibass = false;
    bool renderSRTUsingLibass = false;

    OpenTypeLang::HintStr openTypeLangHint = { 0 };
};

extern SubRendererSettings GetSubRendererSettings();
