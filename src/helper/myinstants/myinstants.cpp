#include "myinstants.hpp"
#include <fancy.hpp>
#include <fstream>
#include <httplib.h>
#include <nlohmann/json.hpp>
#include <regex>
#include <string>

namespace Soundux
{
    namespace Objects
    {
        namespace
        {
            constexpr auto kHost = "www.myinstants.com";
            constexpr int kTimeoutSec = 10;

            //* URL-encode a query string (spaces -> %20, etc.)
            std::string urlEncode(const std::string &src)
            {
                std::string out;
                out.reserve(src.size() * 3);
                for (unsigned char c : src)
                {
                    if (std::isalnum(c) || c == '-' || c == '_' || c == '.' || c == '~')
                    {
                        out += static_cast<char>(c);
                    }
                    else
                    {
                        char buf[4];
                        snprintf(buf, sizeof(buf), "%%%02X", c);
                        out += buf;
                    }
                }
                return out;
            }
        } // namespace

        std::vector<MyInstantResult> MyInstants::search(const std::string &query, int page)
        {
            std::vector<MyInstantResult> results;
            if (query.empty())
                return results;

            httplib::SSLClient cli(kHost, 443);
            cli.set_connection_timeout(kTimeoutSec);
            cli.set_read_timeout(kTimeoutSec);
            cli.set_follow_location(true);

            std::string path = "/en/search/?name=" + urlEncode(query);
            if (page > 1)
            {
                path += "&page=" + std::to_string(page);
            }

            auto res = cli.Get(path.c_str());
            if (!res || res->status != 200)
            {
                Fancy::fancy.logTime().warning() << "MyInstants search failed for: " << query << std::endl;
                return results;
            }

            const auto &html = res->body;

            static const std::regex playRe(
                R"(play\('(/media/sounds/[^']+\.mp3)',\s*'[^']*',\s*'([^']+)'\))");
            static const std::regex nameRe(
                R"(class="instant-link[^"]*">([^<]+)<)");

            std::vector<std::string> mp3s, slugs, names;

            for (auto it = std::sregex_iterator(html.begin(), html.end(), playRe);
                 it != std::sregex_iterator(); ++it)
            {
                mp3s.push_back((*it)[1].str());
                slugs.push_back((*it)[2].str());
            }
            for (auto it = std::sregex_iterator(html.begin(), html.end(), nameRe);
                 it != std::sregex_iterator(); ++it)
            {
                std::string name = (*it)[1].str();
                auto first = name.find_first_not_of(" \t\r\n");
                auto last  = name.find_last_not_of(" \t\r\n");
                if (first != std::string::npos)
                    names.push_back(name.substr(first, last - first + 1));
            }

            const auto count = std::min({mp3s.size(), slugs.size(), names.size()});
            for (std::size_t i = 0; i < count; ++i)
            {
                MyInstantResult r;
                r.slug   = slugs[i];
                r.name   = names[i];
                r.mp3Url = "https://" + std::string(kHost) + mp3s[i];
                results.push_back(std::move(r));
            }

            return results;
        }

        std::vector<MyInstantResult> MyInstants::list(int page)
        {
            std::vector<MyInstantResult> results;

            httplib::SSLClient cli(kHost, 443);
            cli.set_connection_timeout(kTimeoutSec);
            cli.set_read_timeout(kTimeoutSec);
            cli.set_follow_location(true);

            std::string path =
                "/api/v1/instants/?format=json&page=" + std::to_string(page);
            auto res = cli.Get(path.c_str());

            if (!res || res->status != 200)
            {
                Fancy::fancy.logTime().warning() << "MyInstants list failed (page "
                                                 << page << ")" << std::endl;
                return results;
            }

            auto json = nlohmann::json::parse(res->body, nullptr, false);
            if (json.is_discarded() || !json.contains("results"))
            {
                Fancy::fancy.logTime().warning() << "MyInstants list: bad JSON" << std::endl;
                return results;
            }

            for (const auto &item : json["results"])
            {
                MyInstantResult r;
                r.name   = item.value("name", "");
                r.slug   = item.value("slug", "");
                r.mp3Url = item.value("sound", "");
                if (!r.slug.empty() && !r.mp3Url.empty())
                    results.push_back(std::move(r));
            }

            return results;
        }

        bool MyInstants::download(const std::string &mp3Url, const std::string &destPath)
        {
            //* Strip scheme+host to get just the path component
            const std::string prefix = "https://";
            std::string urlPath = mp3Url;
            if (urlPath.substr(0, prefix.size()) == prefix)
            {
                auto slash = urlPath.find('/', prefix.size());
                urlPath = (slash != std::string::npos) ? urlPath.substr(slash) : "/";
            }

            httplib::SSLClient cli(kHost, 443);
            cli.set_connection_timeout(kTimeoutSec);
            cli.set_read_timeout(30);
            cli.set_follow_location(true);

            std::ofstream out(destPath, std::ios::binary | std::ios::trunc);
            if (!out)
            {
                Fancy::fancy.logTime().failure() << "MyInstants: cannot open dest "
                                                 << destPath << std::endl;
                return false;
            }

            auto res = cli.Get(urlPath.c_str(), [&out](const char *data, std::size_t len) {
                out.write(data, static_cast<std::streamsize>(len));
                return true;
            });

            if (!res || res->status != 200)
            {
                Fancy::fancy.logTime().warning() << "MyInstants download failed: "
                                                 << mp3Url << std::endl;
                out.close();
                std::remove(destPath.c_str());
                return false;
            }

            return true;
        }
    } // namespace Objects
} // namespace Soundux