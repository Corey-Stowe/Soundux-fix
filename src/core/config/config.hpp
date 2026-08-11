#pragma once
#include <core/objects/data.hpp>
#include <core/objects/settings.hpp>
#include <string>

namespace Soundux
{
    namespace Objects
    {
        struct Config
        {
            Data data;
            Settings settings;

            //* Temporary cache for streamed/previewed online sounds (auto-deleted after playback).
            std::string cachePath = Config::defaultCachePath();
            //* Permanent offline saves land here (also the path for the "My Sounds" tab).
            std::string offlineSoundsPath = Config::defaultOfflinePath();

            //* Platform defaults for the above (used when the config has none stored).
            static std::string defaultCachePath();
            static std::string defaultOfflinePath();

            void save();
            void load();
            static const std::string path;
        };
    } // namespace Objects
} // namespace Soundux
