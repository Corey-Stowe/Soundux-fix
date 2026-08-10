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
            std::string cachePath;
            //* Permanent offline saves land here (also the path for the "My Sounds" tab).
            std::string offlineSoundsPath;

            void save();
            void load();
            static const std::string path;
        };
    } // namespace Objects
} // namespace Soundux
