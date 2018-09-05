/* This Source Code Form is subject to the terms of the Mozilla Public
* License, v. 2.0. If a copy of the MPL was not distributed with this
* file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef BAT_LEDGER_URL_FETCHER_
#define BAT_LEDGER_URL_FETCHER_

#include <string>
#include <memory>
#include <cassert>
#include <sstream>

#include <curl/curl.h>

#include "bat/ledger/ledger_client.h"

namespace bat_ledger_urlfetcher
{
  class URLFetcher;

  class URLFetcherDelegate {
  public:
    // This will be called when the URL has been fetched, successfully or not.
    // Use accessor methods on |source| to get the results.
    virtual void OnURLFetchComplete(const URLFetcher* source) = 0;

  protected:
    virtual ~URLFetcherDelegate() = default;
  };

  class URLFetcher {
  public:

    enum ResponseCode {
      RESPONSE_CODE_INVALID = -1,
      HTTP_OK = 200
    };

    URLFetcher(const std::string & url, ledger::URL_METHOD metod, URLFetcherDelegate * url_delegate);
    virtual ~URLFetcher();

    void Start();
    int GetResponseCode() const;
    void AddExtraRequestHeader(const std::string & header);
    void SetUploadData(const std::string & contentType, const std::string & content);
    void GetResponseAsString(std::string * response) const;
    bool GetStatus() const;
    void GetUrl(std::string & url) const;

    static std::unique_ptr <URLFetcher> Create(const std::string & url, ledger::URL_METHOD metod, URLFetcherDelegate * url_delegate);
  private:

    URLFetcherDelegate * url_delegate_;
    std::string url_;
    ledger::URL_METHOD metod_;

    CURL *curl_;
    long response_code_;
    std::ostringstream response_data_;
  };

};  //namespace  bat_ledger_urlfetcher
#endif //BAT_LEDGER_URL_FETCHER_
