#include "myinstants.hpp"
#include <fancy.hpp>
#include <fstream>
#include <httplib.h>
#include <nlohmann/json.hpp>
#include <set>
#include <string>

namespace Soundux
{
    namespace Objects
    {
        namespace
        {
            constexpr auto kHost = "www.myinstants.com";
            constexpr int kTimeoutSec = 10;
            //* The API returns 10 results per page; we merge 5 pages so the UI shows 50.
            constexpr int kApiPageSize = 10;
            constexpr int kResultsPerPage = 50;

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

            //* Fetch a single JSON API page and append the parsed results to `out`.
            //* Returns the number of results parsed, or -1 if the request failed.
            int fetchApiPage(httplib::SSLClient &cli, const std::string &query, int apiPage,
                             std::vector<MyInstantResult> &out)
            {
                //* Use the JSON API instead of scraping the HTML pages: httplib 0.9 fails
                //* (Error::Read) on the large chunked + gzip bodies Cloudflare serves for HTML,
                //* while API responses use Content-Length and work reliably.
                std::string path = "/api/v1/instants/?format=json";
                if (!query.empty())
                {
                    path += "&name=" + urlEncode(query);
                }
                path += "&page=" + std::to_string(apiPage);

                auto res = cli.Get(path.c_str());
                if (!res || res->status != 200)
                {
                    return -1;
                }

                auto json = nlohmann::json::parse(res->body, nullptr, false);
                if (json.is_discarded() || !json.contains("results"))
                {
                    return -1;
                }

                int count = 0;
                for (const auto &item : json["results"])
                {
                    MyInstantResult r;
                    r.name   = item.value("name", "");
                    r.slug   = item.value("slug", "");
                    r.mp3Url = item.value("sound", "");
                    if (!r.slug.empty() && !r.mp3Url.empty())
                    {
                        out.push_back(std::move(r));
                        ++count;
                    }
                }
                return count;
            }

            //* Merge the API pages that make up one UI page (kResultsPerPage entries).
            std::vector<MyInstantResult> fetchUiPage(const std::string &query, int page)
            {
                std::vector<MyInstantResult> results;
                results.reserve(kResultsPerPage);

                httplib::SSLClient cli(kHost, 443);
                cli.set_connection_timeout(kTimeoutSec);
                cli.set_read_timeout(kTimeoutSec);
                cli.set_follow_location(true);

                const int pages = kResultsPerPage / kApiPageSize;
                const int firstApiPage = (page - 1) * pages + 1;
                std::set<std::string> seen;
                for (int i = 0; i < pages; ++i)
                {
                    const auto before = results.size();
                    const int parsed = fetchApiPage(cli, query, firstApiPage + i, results);
                    if (parsed < 0)
                    {
                        break; //* Network failure
                    }
                    //* Defensive de-dupe (API should not repeat across pages, but be safe).
                    for (auto it = results.begin() + static_cast<std::ptrdiff_t>(before); it != results.end();)
                    {
                        if (!seen.insert(it->slug).second)
                        {
                            it = results.erase(it);
                        }
                        else
                        {
                            ++it;
                        }
                    }
                    if (parsed < kApiPageSize)
                    {
                        break; //* Short page => no more results available
                    }
                }
                return results;
            }
        } // namespace

        std::vector<MyInstantResult> MyInstants::search(const std::string &query, int page)
        {
            if (query.empty())
            {
                return {};
            }
            return fetchUiPage(query, page);
        }

        std::vector<MyInstantResult> MyInstants::list(int page)
        {
            return fetchUiPage("", page);
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

            //* Cloudflare intermittently resets the connection mid-transfer (httplib 0.9 turns
            //* that into Error::Read). Retry a few times with a fresh connection - it succeeds
            //* on the next attempt almost always.
            for (int attempt = 1; attempt <= 3; ++attempt)
            {
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
                out.close();

                if (res && res->status == 200)
                {
                    return true;
                }

                Fancy::fancy.logTime().warning()
                    << "MyInstants download failed (attempt " << attempt << "/3): "
                    << mp3Url << std::endl;
                std::remove(destPath.c_str());
            }
            return false;
        }
    } // namespace Objects
} // namespace Soundux