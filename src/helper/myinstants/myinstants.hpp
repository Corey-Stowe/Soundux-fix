#pragma once
#include <string>
#include <vector>

namespace Soundux
{
    namespace Objects
    {
        struct MyInstantResult
        {
            std::string name;
            std::string slug;
            std::string mp3Url; //* Full URL: https://www.myinstants.com/media/sounds/xxx.mp3
        };

        class MyInstants
        {
          public:
            //* Search MyInstants by keyword (HTML scrape, ~72 results/page, 1-based page).
            //* Returns empty vector on network/parse failure.
            static std::vector<MyInstantResult> search(const std::string &query, int page = 1);

            //* Browse all instants via native JSON API (10 results/page, 1-based page).
            static std::vector<MyInstantResult> list(int page = 1);

            //* Download mp3Url to destPath. Returns true on success.
            static bool download(const std::string &mp3Url, const std::string &destPath);
        };
    } // namespace Objects
} // namespace Soundux
