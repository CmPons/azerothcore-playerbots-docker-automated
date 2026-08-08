#include "AHPriceConfig.h"
#include "Config.h"
#include "Log.h"

bool g_AHPriceEnable = false;
bool g_AHPriceHideUnauctionable = true;

void AHPriceLoadConfig()
{
    g_AHPriceEnable = sConfigMgr->GetOption<bool>("AHBotPrice.Enable", false);
    g_AHPriceHideUnauctionable = sConfigMgr->GetOption<bool>("AHBotPrice.HideUnauctionable", true);
    LOG_INFO("server.loading", "[AHPrice] Enable={} HideUnauctionable={}",
        g_AHPriceEnable ? 1 : 0, g_AHPriceHideUnauctionable ? 1 : 0);
}
