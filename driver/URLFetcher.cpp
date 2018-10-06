#include "stdafx.h"
#include <sstream>
#include <thread>
#include <iostream>
#include "URLFetcher.h"

namespace bat_ledger_urlfetcher
{

  typedef size_t(__cdecl *CurlWriteDataCallbackSignature)(char *ptr, size_t size, size_t nmemb, void *userdata);

  URLFetcher::URLFetcher(const std::string & url, ledger::URL_METHOD metod, URLFetcherDelegate * url_delegate): url_delegate_ (url_delegate),
    url_(url), metod_ (metod)
  {
    curl_ = curl_easy_init();
    if (nullptr == curl_)
    {
      throw std::runtime_error("Failed to initialize CURL");
    }
  }

  URLFetcher::~URLFetcher() {
    curl_easy_cleanup(curl_);
  }

  void URLFetcher::Start() {

    curl_easy_setopt(curl_, CURLOPT_URL, url_.c_str());
    curl_easy_setopt(curl_, CURLOPT_FOLLOWLOCATION, 1L);
    curl_easy_setopt(curl_, CURLOPT_SSL_VERIFYPEER, 0L);

    if (ledger::URL_METHOD::GET != metod_)
    {
      //we do only GET, PUT(CURLOPT_UPLOAD) and POST
      CURLoption requestType = (ledger::URL_METHOD::PUT == metod_) ? CURLOPT_UPLOAD : CURLOPT_POST;
      curl_easy_setopt(curl_, requestType, 1L);
    }

    //define writecallback
    auto writecallback = [](char* ptr, size_t size, size_t nmemb, void* resultBody) {
      std::ostringstream & os = *(static_cast<std::ostringstream*>(resultBody));
      os << std::string(ptr, size * nmemb);
      return size * nmemb;
    };
    curl_easy_setopt(curl_, CURLOPT_WRITEDATA, &response_data_);

    //convert to __cdecl
    curl_easy_setopt(curl_, CURLOPT_WRITEFUNCTION, static_cast<CurlWriteDataCallbackSignature> (writecallback));

    CURLcode res = curl_easy_perform(curl_);
    if (res != CURLE_OK) {
      std::cout << std::endl << "curl_easy_perform() failed: " << curl_easy_strerror(res) << std::endl;
    }
    else
    {
      curl_easy_getinfo(curl_, CURLINFO_RESPONSE_CODE, &response_code_);
      std::cout << std::endl << "curl_easy_perform() got a response: " << response_data_.str() << std::endl;
    }

    url_delegate_->OnURLFetchComplete(this);
  }

  int URLFetcher::GetResponseCode() const {
    return static_cast<int>(response_code_);
  }

  void URLFetcher::AddExtraRequestHeader(const std::string & header) {
    struct curl_slist *list = NULL;
    list = curl_slist_append(list, header.c_str());
    curl_easy_setopt(curl_, CURLOPT_HTTPHEADER, list);
  }

  void URLFetcher::SetUploadData(const std::string & contentType, const std::string & content) {
    curl_easy_setopt(curl_, CURLOPT_POSTFIELDS, content.c_str());
  }

  void URLFetcher::GetResponseAsString(std::string * response) const {
    *response = response_data_.str();
  }

  bool URLFetcher::GetStatus() const {
    return (ResponseCode::HTTP_OK == response_code_)? true : false ;
  }

  std::unique_ptr <URLFetcher> URLFetcher::Create(const std::string & url, ledger::URL_METHOD metod,  URLFetcherDelegate * url_delegate) {
    return std::make_unique <URLFetcher>(url, metod,url_delegate);
  }

  void URLFetcher::GetUrl(std::string & url) const {
    url = url_;
  }

  void URLFetcher::GetResponseHeaders(std::map<std::string, std::string> & headers) const {

  }

};