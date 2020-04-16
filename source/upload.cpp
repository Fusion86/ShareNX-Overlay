/*
 * Copyright (c) 2020 Behemoth
 *
 * This file is part of ShareNX.
 *
 * This program is free software; you can redistribute it and/or modify it
 * under the terms and conditions of the GNU General Public License,
 * version 2, as published by the Free Software Foundation.
 *
 * This program is distributed in the hope it will be useful, but WITHOUT
 * ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or
 * FITNESS FOR A PARTICULAR PURPOSE.  See the GNU General Public License for
 * more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program.  If not, see <http://www.gnu.org/licenses/>.
 */
#include "upload.hpp"

#include <sys/select.h>
#include <curl/curl.h>

#include <tesla.hpp>

#include "json.hpp"

namespace web {

    size_t StringWrite(const char *contents, size_t size, size_t nmemb, std::string *userp) {
        userp->append(contents, size * nmemb);
        return size * nmemb;
    }

    std::string UploadImage(const CapsAlbumFileId &fileId) {
        u64 size = 0;
        Result rc = capsaGetAlbumFileSize(&fileId, &size);
        if (R_FAILED(rc))
            return "can't get size";

        void *imgBuffer = malloc(size);
        if (!imgBuffer)
            return "malloc failed";

        u64 actualSize = 0;
        rc = capsaLoadAlbumFile(&fileId, &actualSize, imgBuffer, size);
        if (R_FAILED(rc)) {
            free(imgBuffer);
            return "failed to load img";
        }

        CURL *curl = curl_easy_init();
        if (!curl)
            return "failed to start curl";

        curl_mime *mime = curl_mime_init(curl);
        curl_mimepart *file_part = curl_mime_addpart(mime);

        curl_mime_filename(file_part, "switch.jpg");
        curl_mime_name(file_part, "image");
        curl_mime_data(file_part, (const char *)imgBuffer, actualSize);

        std::string urlresponse = std::string();

        curl_slist *chunk = NULL;
        chunk = curl_slist_append(chunk, "Authorization: Client-ID client_id_here");

        curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, StringWrite);
        curl_easy_setopt(curl, CURLOPT_WRITEDATA, (void *)&urlresponse);
        curl_easy_setopt(curl, CURLOPT_CUSTOMREQUEST, "POST");
        curl_easy_setopt(curl, CURLOPT_MIMEPOST, mime);
        curl_easy_setopt(curl, CURLOPT_URL, "https://api.imgur.com/3/image");
        curl_easy_setopt(curl, CURLOPT_SSL_VERIFYPEER, 0L);
        curl_easy_setopt(curl, CURLOPT_HTTPHEADER, chunk);

        CURLcode res = CURLE_OK;
        tsl::hlp::doWithSmSession([&] {
            res = curl_easy_perform(curl);
        });

        long http_code = 0;
        curl_easy_getinfo(curl, CURLINFO_RESPONSE_CODE, &http_code);

        // For when you need to debug something, you also need to #include <fstream>
        // fsdevMountSdmc();
        // std::ofstream of("sdmc:/sharenx.log");
        // of.write(urlresponse.c_str(), urlresponse.size());
        // of.close();
        // fsdevUnmountDevice("sdmc");

        if (res != CURLE_OK) {
            urlresponse = "curl failed " + std::to_string(res);
        } else if (http_code != 200) {
            urlresponse = "failed with " + std::to_string(http_code);
        } else {
            nlohmann::json j = nlohmann::json::parse(urlresponse);
            urlresponse = j["data"]["link"]; // Should always be set because http_code == 200
        }

        free(imgBuffer);

        curl_mime_free(mime);
        curl_easy_cleanup(curl);

        return urlresponse;
    }

}
