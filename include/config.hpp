#include <toml.hpp>
#include <spdlog/spdlog.h>

namespace as_scrobbler
{
    namespace config
    {
        bool enable_preload;
        bool redirect_scrobbles;
        std::string scrobble_server;
        int server_port;
        std::string api_key;

        void init()
        {
            spdlog::info("Loading config");
            auto data = toml::parse("preload_enabler.toml");

            enable_preload = toml::find<bool>(data, "enable_preload");
            redirect_scrobbles = toml::find<bool>(data, "redirect_scrobbles");
            scrobble_server = toml::find<std::string>(data, "scrobble_server");
            server_port = toml::find<int>(data, "server_port");
            api_key = toml::find<std::string>(data, "api_key");

        }
    }
}