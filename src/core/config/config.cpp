#include "config.hpp"
#include <chrono>
#include <fancy.hpp>
#include <filesystem>
#include <fstream>
#include <helper/json/bindings.hpp>
#include <string>

namespace Soundux::Objects
{
    const std::string Config::path = []() -> std::string {
#if defined(__linux__)
        const auto *configPath = std::getenv("XDG_CONFIG_HOME"); // NOLINT
        if (configPath)
        {
            return std::string(configPath) + "/Soundux/config.json";
        }
        return std::string(std::getenv("HOME")) + "/.config/Soundux/config.json"; // NOLINT
#elif defined(_WIN32)
        char *buffer;
        std::size_t size;
        _dupenv_s(&buffer, &size, "APPDATA");
        auto rtn = std::string(buffer) + "\\Soundux\\config.json";
        free(buffer);

        return rtn;
#endif
    }();

    void Config::save()
    {
        try
        {
            std::filesystem::path configFile(path);
            if (!std::filesystem::exists(path))
            {
                std::filesystem::create_directories(configFile.parent_path());
            }

            //* Write to a temporary file first and move it in place afterwards, so that a crash or
            //* full disk can never leave a half-written (empty) config behind that would later be
            //* treated as "corrupted" and be overwritten with defaults.
            auto tmpPath = configFile;
            tmpPath += ".tmp";

            {
                std::ofstream configStream(tmpPath, std::ios::out | std::ios::trunc);
                configStream << nlohmann::json(*this).dump();
                configStream.flush();

                if (!configStream.good())
                {
                    std::error_code ec;
                    std::filesystem::remove(tmpPath, ec);
                    Fancy::fancy.logTime().failure() << "Failed to write config: write error" << std::endl;
                    return;
                }
            }

            std::error_code ec;
            std::filesystem::rename(tmpPath, configFile, ec);
            if (ec)
            {
                //* Fallback for filesystems that do not allow renaming over an existing file.
                std::filesystem::remove(configFile, ec);
                ec.clear();
                std::filesystem::rename(tmpPath, configFile, ec);
                if (ec)
                {
                    Fancy::fancy.logTime().failure() << "Failed to write config: " << ec.message() << std::endl;
                    return;
                }
            }

            Fancy::fancy.logTime().success() << "Config written" << std::endl;
        }
        catch (const std::exception &e)
        {
            Fancy::fancy.logTime().failure() << "Failed to write config: " >> e.what() << std::endl;
        }
        catch (...)
        {
            Fancy::fancy.logTime().failure() << "Failed to write config" << std::endl;
        }
    }
    void Config::load()
    {
        try
        {
            if (!std::filesystem::exists(path))
            {
                Fancy::fancy.logTime().warning() << "Config not found" << std::endl;
                return;
            }

            std::ifstream configStream(path);
            std::string content((std::istreambuf_iterator<char>(configStream)), std::istreambuf_iterator<char>());
            configStream.close();

            auto json = nlohmann::json::parse(content, nullptr, false);
            if (json.is_discarded())
            {
                Fancy::fancy.logTime().failure() << "Config seems corrupted, moving it to a backup..." << std::endl;

                //* Never silently overwrite the existing (possibly corrupted) config on exit.
                std::error_code ec;
                std::filesystem::path configFile(path);
                std::filesystem::rename(
                    path,
                    configFile.parent_path() /
                        ("config_corrupted_" +
                         std::to_string(std::chrono::system_clock::now().time_since_epoch().count()) + ".json"),
                    ec);
                if (ec)
                {
                    Fancy::fancy.logTime().warning()
                        << "Failed to backup corrupted config: " << ec.message() << std::endl;
                }
            }
            else
            {
                try
                {
                    auto conf = json.get<Config>();
                    data.set(conf.data);
                    settings = conf.settings;
                    Fancy::fancy.logTime().success()
                        << "Config read: " << data.getTabs().size() << " tab(s)" << std::endl;
                }
                catch (const std::exception &e)
                {
                    Fancy::fancy.logTime().warning()
                        << "Found possibly old config format (" << e.what() << "), moving old config..." << std::endl;

                    std::filesystem::path configFile(path);
                    std::filesystem::rename(
                        path,
                        configFile.parent_path() /
                            ("config_old_" +
                             std::to_string(std::chrono::system_clock::now().time_since_epoch().count()) + ".json"));
                }
            }
        }
        catch (const std::exception &e)
        {
            Fancy::fancy.logTime().warning() << "Failed to read config: " << e.what() << std::endl;
        }
        catch (...)
        {
            Fancy::fancy.logTime().warning() << "Failed to read config" << std::endl;
        }
    }
} // namespace Soundux::Objects