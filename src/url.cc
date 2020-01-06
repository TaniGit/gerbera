/*MT*
    
    MediaTomb - http://www.mediatomb.cc/
    
    url.cc - this file is part of MediaTomb.
    
    Copyright (C) 2005 Gena Batyan <bgeradz@mediatomb.cc>,
                       Sergey 'Jin' Bostandzhyan <jin@mediatomb.cc>
    
    Copyright (C) 2006-2010 Gena Batyan <bgeradz@mediatomb.cc>,
                            Sergey 'Jin' Bostandzhyan <jin@mediatomb.cc>,
                            Leonhard Wimmer <leo@mediatomb.cc>
    
    MediaTomb is free software; you can redistribute it and/or modify
    it under the terms of the GNU General Public License version 2
    as published by the Free Software Foundation.
    
    MediaTomb is distributed in the hope that it will be useful,
    but WITHOUT ANY WARRANTY; without even the implied warranty of
    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
    GNU General Public License for more details.
    
    You should have received a copy of the GNU General Public License
    version 2 along with MediaTomb; if not, write to the Free Software
    Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA 02110-1301, USA.
    
    $Id$
*/

/// \file url.cc

#ifdef HAVE_CURL

#include "url.h"
#include "common.h"
#include "config/config_manager.h"
#include "util/rexp.h"
#include "util/tools.h"
#include <pthread.h>

#include <sstream>

std::string URL::download(std::string URL, long* HTTP_retcode,
    CURL* curl_handle, bool only_header,
    bool verbose, bool redirect)
{
    CURLcode res;
    bool cleanup = false;
    char error_buffer[CURL_ERROR_SIZE] = { '\0' };

    if (curl_handle == nullptr) {
        curl_handle = curl_easy_init();
        cleanup = true;
        if (curl_handle == nullptr)
            throw std::runtime_error("Invalid curl handle!\n");
    }

    std::ostringstream buffer;

    curl_easy_reset(curl_handle);

    if (verbose) {
        bool logEnabled;
#ifdef TOMBDEBUG
        logEnabled = !ConfigManager::isDebugLogging();
#else
        logEnabled = ConfigManager::isDebugLogging();
#endif
        if (logEnabled)
            curl_easy_setopt(curl_handle, CURLOPT_VERBOSE, 1);
    }
    // some web sites send unexpected stuff, seems they need a user agent
    // that they know...
    curl_easy_setopt(curl_handle, CURLOPT_USERAGENT,
        "Mozilla/5.0 (X11; U; Linux x86_64; en-US; rv:1.9.1.6) Gecko/20091216 Fedora/3.5.6-1.fc12 Firefox/3.5.6");
    curl_easy_setopt(curl_handle, CURLOPT_URL, URL.c_str());
    curl_easy_setopt(curl_handle, CURLOPT_ERRORBUFFER, error_buffer);
    curl_easy_setopt(curl_handle, CURLOPT_CONNECTTIMEOUT, 20); // seconds

    /// \todo it would be a good idea to allow both variants, i.e. retrieve
    /// the headers and data in one go when needed
    if (only_header) {
        curl_easy_setopt(curl_handle, CURLOPT_NOBODY, 1);
        curl_easy_setopt(curl_handle, CURLOPT_HEADERFUNCTION, URL::dl);
        curl_easy_setopt(curl_handle, CURLOPT_HEADERDATA, &buffer);
    } else {
        curl_easy_setopt(curl_handle, CURLOPT_WRITEFUNCTION, URL::dl);
        curl_easy_setopt(curl_handle, CURLOPT_WRITEDATA, &buffer);
    }

    if (redirect) {
        curl_easy_setopt(curl_handle, CURLOPT_FOLLOWLOCATION, 1);
        curl_easy_setopt(curl_handle, CURLOPT_MAXREDIRS, -1);
    }

    res = curl_easy_perform(curl_handle);
    if (res != CURLE_OK) {
        log_error("{}", error_buffer);
        if (cleanup)
            curl_easy_cleanup(curl_handle);
        throw std::runtime_error(error_buffer);
    }

    res = curl_easy_getinfo(curl_handle, CURLINFO_RESPONSE_CODE, HTTP_retcode);
    if (res != CURLE_OK) {
        log_error("{}", error_buffer);
        if (cleanup)
            curl_easy_cleanup(curl_handle);
        throw std::runtime_error(error_buffer);
    }

    if (cleanup)
        curl_easy_cleanup(curl_handle);

    return buffer.str();
}

std::unique_ptr<URL::Stat> URL::getInfo(std::string URL, CURL* curl_handle)
{
    long retcode;
    bool cleanup = false;
    CURLcode res;
    double cl;
    char* ct;
    char* c_url;
    char error_buffer[CURL_ERROR_SIZE] = { '\0' };
    std::string mt;
    std::string used_url;

    if (curl_handle == nullptr) {
        curl_handle = curl_easy_init();
        cleanup = true;
        if (curl_handle == nullptr)
            throw std::runtime_error("Invalid curl handle!\n");
    }

    download(URL, &retcode, curl_handle, true, true, true);
    if (retcode != 200) {
        if (cleanup)
            curl_easy_cleanup(curl_handle);
        throw std::runtime_error("Error retrieving information from " + URL + " HTTP return code: " + std::to_string(retcode));
    }
    /*    
    Ref<RExp> getMT(new RExp());
    
    try
    {
        getMT->compile("\nContent-Type: ([^\n]+)\n", REG_ICASE);
    }
    catch (const std::runtime_error& ex)
    {
        if (cleanup)
            curl_easy_cleanup(curl_handle);

        throw ex;
    }

    Ref<Matcher> matcher = getMT->matcher(buffer->toString());
    std::string mt;
    if (matcher->next())
        mt = trim_string(matcher->group(1));
    else
        mt = MIMETYPE_DEFAULT;

    log_debug("Extracted content type: {}", mt.c_str());

    Ref<RExp> getCL(new RExp());

    try
    {
        getCL->compile("\nContent-Length: ([^\n]+)\n", REG_ICASE);
    }
    catch (const std::runtime_error& ex)
    {
        if (cleanup)
            curl_easy_cleanup(curl_handle);

        throw ex;
    }

    matcher = getCL->matcher(buffer->toString());
    off_t cl;
    if (matcher->next())
        cl = trim_string(matcher->group(1)).toOFF_T();
    else
        cl = -1;
*/
    res = curl_easy_getinfo(curl_handle, CURLINFO_CONTENT_LENGTH_DOWNLOAD, &cl);
    if (res != CURLE_OK) {
        log_error("{}", error_buffer);
        if (cleanup)
            curl_easy_cleanup(curl_handle);
        throw std::runtime_error(error_buffer);
    }

    res = curl_easy_getinfo(curl_handle, CURLINFO_CONTENT_TYPE, &ct);
    if (res != CURLE_OK) {
        log_error("{}", error_buffer);
        if (cleanup)
            curl_easy_cleanup(curl_handle);
        throw std::runtime_error(error_buffer);
    }

    if (ct == nullptr)
        mt = MIMETYPE_DEFAULT;
    else
        mt = ct;

    log_debug("Extracted content length: %lld", (long long)cl);

    res = curl_easy_getinfo(curl_handle, CURLINFO_EFFECTIVE_URL, &c_url);
    if (res != CURLE_OK) {
        log_error("{}", error_buffer);
        if (cleanup)
            curl_easy_cleanup(curl_handle);
        throw std::runtime_error(error_buffer);
    }

    if (c_url == nullptr)
        used_url = URL;
    else
        used_url = c_url;

    auto st = std::make_unique<Stat>(used_url, (off_t)cl, mt);

    if (cleanup)
        curl_easy_cleanup(curl_handle);

    return st;
}

size_t URL::dl(void* buf, size_t size, size_t nmemb, void* data)
{
    auto& oss = *reinterpret_cast<std::ostringstream*>(data);

    size_t s = size * nmemb;
    oss << std::string(reinterpret_cast<const char*>(buf), s);

    return s;
}

#endif //HAVE_CURL
