#include "ScriptMgr.h"
#include "Log.h"
#include "AHPriceConfig.h"
#include "AHPriceCommand.h"

class AHPriceWorld : public WorldScript
{
public:
    AHPriceWorld() : WorldScript("AHPriceWorld") { }
    void OnAfterConfigLoad(bool /*reload*/) override { AHPriceLoadConfig(); }
};

void Addmod_ahbot_priceScripts()
{
    LOG_INFO("server.loading", "[AHPrice] Registering scripts.");
    new AHPriceWorld();
    new AHPriceCommand();
}
