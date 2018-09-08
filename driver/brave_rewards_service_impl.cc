/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "stdafx.h"
#include "brave_rewards_service_impl.h"

/*
#include "base/bind.h"
#include "base/guid.h"
#include "base/files/file_util.h"
#include "base/files/important_file_writer.h"
#include "base/sequenced_task_runner.h"
#include "base/task_runner_util.h"
#include "base/task_scheduler/post_task.h"
#include "base/threading/sequenced_task_runner_handle.h"
*/
#include "bat/ledger/ledger.h"
#include "brave_rewards_service_observer.h"
#include "bat/ledger/ledger_callback_handler.h"
#include "bat_helper_platform.h"

#include "ledger_task_runner_impl.h"
#include "publisher_info_database.h"
#include "wallet_properties.h"
/*
#include "chrome/browser/browser_process_impl.h"
#include "chrome/browser/profiles/profile.h"
#include "net/url_request/url_fetcher.h"
#include "url/gurl.h"
*/

#include <fstream>
#include <locale>
#include <codecvt>
#include <functional>
#include <algorithm>
#include <ctime>
#include <chrono>

//#define COM_NO_WINDOWS_H
#include <combaseapi.h>

#include <curl/curl.h> //for curl_easy_escape

#include <boost/date_time/posix_time/posix_time.hpp>

// TODO, just for test purpose
static bool created_wallet = false;
//

using namespace std::placeholders;

namespace brave_rewards {

namespace {

 bool parseUrl(const std::string & url, std::string & tld, std::string & host, std::string & path) {
   URL_COMPONENTSA urlcomponents;
   memset(&urlcomponents, 0, sizeof(URL_COMPONENTSA));
   urlcomponents.dwStructSize = sizeof(URL_COMPONENTSA);       // size of this structure. Used in version check

   std::unique_ptr<char[]> pschema(new char[16]);
   urlcomponents.lpszScheme = pschema.get();         // pointer to scheme name
   urlcomponents.dwSchemeLength = 16;     // length of scheme name

   std::unique_ptr<char[]> phost(new char[512]);
   urlcomponents.lpszHostName = phost.get();       // pointer to host name
   urlcomponents.dwHostNameLength = 256;   // length of host name


   std::unique_ptr<char[]> ppath(new char[1024]);
   urlcomponents.lpszUrlPath = ppath.get();        // pointer to URL-path
   urlcomponents.dwUrlPathLength = 1024;    // length of URL-path

   BOOL succeded = InternetCrackUrlA(url.c_str(), url.size(), 0, &urlcomponents);
   if (succeded) {
     tld.assign(urlcomponents.lpszHostName, urlcomponents.dwHostNameLength);
     host.assign(urlcomponents.lpszHostName, urlcomponents.dwHostNameLength);
     path.assign(urlcomponents.lpszUrlPath, urlcomponents.dwUrlPathLength);

     if (std::count(tld.begin(), tld.end(), '.') > 1) {
       auto pos = tld.find('.');
       tld = tld.substr(++pos);
     }
   }
   return (succeded) ? true : false;
}

void GetLocalMonthYear(ledger::PUBLISHER_MONTH& month, int* year) {
  time_t tt = std::time(nullptr);
  tm tm1;
  localtime_s(&tm1, &tt);
  *year = tm1.tm_year + 1900;
  month = (ledger::PUBLISHER_MONTH)(tm1.tm_mon + 1);
}

class LedgerURLLoaderImpl : public ledger::LedgerURLLoader {
 public:
  LedgerURLLoaderImpl(uint64_t request_id, bat_ledger_urlfetcher::URLFetcher* fetcher) :
    request_id_(request_id),
    fetcher_(fetcher) {}
  ~LedgerURLLoaderImpl() override = default;

  void Start() override {
    fetcher_->Start();
  }

  uint64_t request_id() override {
    return request_id_;
  }

private:
  uint64_t request_id_;
  bat_ledger_urlfetcher::URLFetcher* fetcher_;  // NOT OWNED
};


ContentSite PublisherInfoToContentSite(
    const ledger::PublisherInfo& publisher_info) {
  ContentSite content_site(publisher_info.id);
  content_site.percentage = publisher_info.percent;
  content_site.verified = publisher_info.verified;
  content_site.name = publisher_info.name;
  content_site.url = publisher_info.url;
  content_site.provider = publisher_info.provider;
  content_site.favicon_url = publisher_info.favicon_url;
  return content_site;
}
std::string LoadStateOnFileTaskRunner(const std::string& path) {
  std::string data;
  bool success = true;
  try
  {
    std::ifstream in(path);
    data.assign((std::istreambuf_iterator<char>(in)), std::istreambuf_iterator<char>());
  }
  catch (std::exception &)
  {
    success = false;
  }
  // Make sure the file isn't empty.
  if (!success || data.empty()) {
    LOG(ERROR) << "Failed to read file: " << path;
    return std::string();
  }
  return data;
}

bool SaveMediaPublisherInfoOnFileTaskRunner(
    const std::string& media_key,
    const std::string& publisher_id,
    PublisherInfoDatabase* backend) {
  if (backend && backend->InsertOrUpdateMediaPublisherInfo(media_key, publisher_id))
    return true;

  return false;
}

std::unique_ptr<ledger::PublisherInfo>
LoadMediaPublisherInfoListOnFileTaskRunner(
    const std::string media_key,
    PublisherInfoDatabase* backend) {
  std::unique_ptr<ledger::PublisherInfo> info;
  if (!backend)
    return info;

  info = backend->GetMediaPublisherInfo(media_key);
  return info;
}

bool SavePublisherInfoOnFileTaskRunner(
    const ledger::PublisherInfo publisher_info,
    PublisherInfoDatabase* backend) {
  if (backend && backend->InsertOrUpdatePublisherInfo(publisher_info))
    return true;

  return false;
}

ledger::PublisherInfoList LoadPublisherInfoListOnFileTaskRunner(
    uint32_t start,
    uint32_t limit,
    ledger::PublisherInfoFilter filter,
    PublisherInfoDatabase* backend) {
  ledger::PublisherInfoList list;
  if (!backend)
    return list;

  //ignore_result(backend->Find(start, limit, filter, &list));

  return list;
}

void GetContentSiteListInternal(
    uint32_t start,
    uint32_t limit,
    const GetContentSiteListCallback& callback,
    const ledger::PublisherInfoList& publisher_list,
    uint32_t next_record) {
  std::unique_ptr<ContentSiteList> site_list(new ContentSiteList);
  for (ledger::PublisherInfoList::const_iterator it =
      publisher_list.begin(); it != publisher_list.end(); ++it) {
    site_list->push_back(PublisherInfoToContentSite(*it));
  }
  callback(std::move(site_list), next_record);
}
static uint64_t next_id = 1;
}  // namespace

bool IsMediaLink(const std::string & url,
                 const std::string & first_party_url,
                 const std::string & referrer) {
  return ledger::Ledger::IsMediaLink(url,
                                     first_party_url,
                                     referrer);
}

// `callback` has a WeakPtr so this won't crash if the file finishes
// writing after BraveRewardsServiceImpl has been destroyed

void BraveRewardsServiceImpl::PostWriteCallback(
  //const base::Callback<void(bool success)>& callback,
  std::function < void(bool success)> & callback,
  //scoped_refptr<base::SequencedTaskRunner> reply_task_runner,
  bool write_success) {
  // We can't run |callback| on the current thread. Bounce back to
  // the |reply_task_runner| which is the correct sequenced thread.
  //reply_task_runner->PostTask(FROM_HERE, base::Bind(callback, write_success));

  bat_ledger::LedgerTaskRunnerImpl::Task t = std::bind(callback, write_success);
  std::unique_ptr<ledger::LedgerTaskRunner> task(new bat_ledger::LedgerTaskRunnerImpl(t));
  RunTask(std::move(task));
}

uint64_t BraveRewardsServiceImpl::GetCurrentTimeStamp() {
  auto start = std::chrono::system_clock::now().time_since_epoch();
  return std::chrono::duration_cast<std::chrono::milliseconds> (start).count();
}


BraveRewardsServiceImpl::BraveRewardsServiceImpl(Profile* profile) :
    profile_(profile),
    ledger_(ledger::Ledger::CreateInstance(this)),
  /*
  file_task_runner_(base::CreateSequencedTaskRunnerWithTraits(
        {base::MayBlock(), base::TaskPriority::BACKGROUND,
         base::TaskShutdownBehavior::BLOCK_SHUTDOWN})), */

    ledger_state_path_(profile_->GetPath().append("\\ledger_state")),
    publisher_state_path_(profile_->GetPath().append("\\publisher_state")),
    publisher_info_db_path_(profile->GetPath().append("\\publisher_info")),
    publisher_list_path_(profile->GetPath().append("\\publishers_list")),
    publisher_info_backend_(new PublisherInfoDatabase(publisher_info_db_path_)),
    timer_id_ (0u),
    max_number_timers_ (2u)
{
}

BraveRewardsServiceImpl::~BraveRewardsServiceImpl() {
}

void BraveRewardsServiceImpl::Init() {
  ledger_->Initialize();
}

void BraveRewardsServiceImpl::CreateWallet() {
  if (created_wallet) {
    return;
  }
  ledger_->CreateWallet();
  created_wallet = true;
}



void BraveRewardsServiceImpl::GetContentSiteList(
    uint32_t start, uint32_t limit,
    const GetContentSiteListCallback& callback) {
  ledger::PublisherInfoFilter filter;
  filter.category = ledger::PUBLISHER_CATEGORY::AUTO_CONTRIBUTE;

  ledger::PUBLISHER_MONTH month;
  int year;
  GetLocalMonthYear(month, &year);
  filter.month = month;
  filter.year = year;
  filter.order_by.push_back(std::pair<std::string, bool>("percent", false));

  ledger_->GetPublisherInfoList(start, limit,
      filter,
      std::bind(&GetContentSiteListInternal,
                start,
                limit,
                callback, _1, _2));
}

void BraveRewardsServiceImpl::OnLoad(uint32_t tab_id, const std::string & url) {

  URL_COMPONENTS urlcomponents;
  InternetCrackUrlA(url.c_str(), url.size(), 0, &urlcomponents);

  //schema, host, port
  std::string tld,host,path;
  parseUrl(url, tld, host, path);



  if (tld == "")
    return;

  time_t now = std::time(nullptr);
  ledger::PUBLISHER_MONTH month;
  int year;
  GetLocalMonthYear(month, &year);

  ledger::VisitData data(tld,
                         host,
                         path,
                         tab_id,
                         month,
                         year,
                         tld,
                         url,
                         "",
                         "");
  ledger_->OnLoad(data, GetCurrentTimeStamp());
}

void BraveRewardsServiceImpl::OnUnload(uint32_t tab_id) {
  ledger_->OnUnload(tab_id, GetCurrentTimeStamp());
}

void BraveRewardsServiceImpl::OnShow(uint32_t tab_id) {
  ledger_->OnShow(tab_id, GetCurrentTimeStamp());
}

void BraveRewardsServiceImpl::OnHide(uint32_t tab_id) {
  ledger_->OnHide(tab_id, GetCurrentTimeStamp());
}

void BraveRewardsServiceImpl::OnForeground(uint32_t tab_id) {
  ledger_->OnForeground(tab_id, GetCurrentTimeStamp());
}

void BraveRewardsServiceImpl::OnBackground(uint32_t tab_id) {
  ledger_->OnBackground(tab_id, GetCurrentTimeStamp());
}

void BraveRewardsServiceImpl::OnMediaStart(uint32_t tab_id) {
  ledger_->OnMediaStart(tab_id, GetCurrentTimeStamp());
}

void BraveRewardsServiceImpl::OnMediaStop(uint32_t tab_id) {
  ledger_->OnMediaStop(tab_id, GetCurrentTimeStamp());
}

void BraveRewardsServiceImpl::OnPostData(uint32_t tab_id,
                                    const std::string & url,
                                    const std::string & first_party_url,
                                    const std::string & referrer,
                                    const std::string& post_data) {
  std::string output;
  //url::RawCanonOutputW<1024> canonOutput;
  //url::DecodeURLEscapeSequences(post_data.c_str(),
  //                              post_data.length(),
  //                              &canonOutput);
  //output = base::UTF16ToUTF8(base::StringPiece16(canonOutput.data(),
  //                                               canonOutput.length()));
  braveledger_bat_helper::DecodeURLChars(post_data, output);

  if (output.empty())
    return;

  time_t now = std::time(nullptr);
  ledger::PUBLISHER_MONTH month;
  int year;
  GetLocalMonthYear(month, &year);

  ledger::VisitData visit_data(
      "",
      "",
      url,
      tab_id,
      month,
      year,
      "",
      "",
      "",
      "");

  ledger_->OnPostData(url,
                      first_party_url,
                      referrer,
                      output,
                      visit_data);
}

void BraveRewardsServiceImpl::OnXHRLoad(uint32_t tab_id,
                                   const std::string & url,
                                   const std::string & first_party_url,
                                   const std::string & referrer) {
  std::map<std::string, std::string> parts;
  //Temporary commented out
  /*
  for (net::QueryIterator it(url); !it.IsAtEnd(); it.Advance()) {
    parts[it.GetKey()] = it.GetUnescapedValue();
  }
  */

  time_t now = std::time(nullptr);
  ledger::PUBLISHER_MONTH month;
  int year;
  GetLocalMonthYear(month, &year);
  ledger::VisitData data("", "", url, tab_id,
                         month, year,
                         "", "", "", "");

  ledger_->OnXHRLoad(tab_id,
                     url,
                     parts,
                     first_party_url,
                     referrer,
                     data);
}

void BraveRewardsServiceImpl::LoadMediaPublisherInfo(const std::string& media_key,
                                ledger::PublisherInfoCallback callback) {

  std::unique_ptr<ledger::PublisherInfo> pi = LoadMediaPublisherInfoListOnFileTaskRunner(media_key, publisher_info_backend_.get());

  bat_ledger::LedgerTaskRunnerImpl::Task t = std::bind(&BraveRewardsServiceImpl::OnMediaPublisherInfoLoaded, this, callback, std::shared_ptr<ledger::PublisherInfo>(pi.release()));
  std::unique_ptr<ledger::LedgerTaskRunner> task(new bat_ledger::LedgerTaskRunnerImpl(t));
  RunTask(std::move(task));
}

void BraveRewardsServiceImpl::OnMediaPublisherInfoLoaded(
    ledger::PublisherInfoCallback callback,
    std::shared_ptr<ledger::PublisherInfo> info) {

  ledger::Result r = (!info) ? ledger::Result::NOT_FOUND : ledger::Result::OK;

  //make a copy of PublisherInfo and attach it to unique_ptr
  callback(r, std::make_unique <ledger::PublisherInfo>(*info));
}

void BraveRewardsServiceImpl::SaveMediaPublisherInfo(
    const std::string& media_key,
    const std::string& publisher_id) {

  bool success = SaveMediaPublisherInfoOnFileTaskRunner(media_key, publisher_id, publisher_info_backend_.get());
  bat_ledger::LedgerTaskRunnerImpl::Task t = std::bind(&BraveRewardsServiceImpl::OnMediaPublisherInfoSaved, this, success);
  std::unique_ptr<ledger::LedgerTaskRunner> task(new bat_ledger::LedgerTaskRunnerImpl(t));
  RunTask(std::move(task));

}

void BraveRewardsServiceImpl::OnMediaPublisherInfoSaved(bool success) {
  if (!success) {
    LOG(1) << "Error in OnMediaPublisherInfoSaved";
  }
}

std::string BraveRewardsServiceImpl::URIEncode(const std::string& value) {
  std::string result;
  CURL *curl = curl_easy_init();
  if (curl) {
    char *output = curl_easy_escape(curl, value.c_str(), value.size());
    if (output) {
      result = output;
      curl_free(output);
    }
    curl_easy_cleanup(curl);
  }

  return result;
}

std::string BraveRewardsServiceImpl::GenerateGUID() const {
  //return base::GenerateGUID();
  GUID guid;
  std::string s_guid;
  if (S_OK == CoCreateGuid(&guid))
  {
    wchar_t szGUID[64] = { 0 };
    if (StringFromGUID2(guid, szGUID, 64))
    {
      std::wstring wstr(szGUID);
      std::wstring_convert <std::codecvt_utf8<wchar_t>, wchar_t> convert;
      std::string s = convert.to_bytes(wstr);
      s_guid.assign(s.begin() + 1, s.end() - 1); //exclude braces
    }
  }

  return s_guid;
}

void BraveRewardsServiceImpl::Shutdown() {
  fetchers_.clear();
  ledger_.reset();
  BraveRewardsService::Shutdown();
}

void BraveRewardsServiceImpl::OnWalletInitialized(ledger::Result result) {
  TriggerOnWalletInitialized(result);
}

void BraveRewardsServiceImpl::OnWalletProperties(ledger::Result result,
                          std::unique_ptr<ledger::WalletInfo> info) {
  TriggerOnWalletProperties(result, std::move(info));
}

void BraveRewardsServiceImpl::OnGrant(ledger::Result result,
                                 const ledger::Grant& grant) {
  TriggerOnGrant(result, grant);
}

void BraveRewardsServiceImpl::OnGrantCaptcha(const std::string& image) {
  TriggerOnGrantCaptcha(image);
}

void BraveRewardsServiceImpl::OnRecoverWallet(ledger::Result result,
                                    double balance,
                                    const std::vector<ledger::Grant>& grants) {
  TriggerOnRecoverWallet(result, balance, grants);
}

void BraveRewardsServiceImpl::OnGrantFinish(ledger::Result result,
                                       const ledger::Grant& grant) {
  ledger::BalanceReportInfo report_info;
  time_t now = std::time(nullptr);
  ledger::PUBLISHER_MONTH month;
  int year;
  GetLocalMonthYear(month, &year);
  ledger_->GetBalanceReport(month, year, &report_info);
  report_info.grants_ += 10.0; // TODO NZ convert probi to
  ledger_->SetBalanceReport(month, year, report_info);
  TriggerOnGrantFinish(result, grant);
}


void BraveRewardsServiceImpl::OnReconcileComplete(ledger::Result result,
                                              const std::string& viewing_id) {
  LOG(ERROR) << "reconcile complete " << viewing_id;
  // TODO - TriggerOnReconcileComplete
}


void BraveRewardsServiceImpl::LoadLedgerState(ledger::LedgerCallbackHandler* handler) {

  /*
  base::PostTaskAndReplyWithResult(file_task_runner_.get(), FROM_HERE,
      base::Bind(&LoadStateOnFileTaskRunner, ledger_state_path_),
      base::Bind(&BraveRewardsServiceImpl::OnLedgerStateLoaded,
                     AsWeakPtr(),
                     base::Unretained(handler)));
  */


  std::string data =  LoadStateOnFileTaskRunner(ledger_state_path_);

  bat_ledger::LedgerTaskRunnerImpl::Task t = std::bind(&BraveRewardsServiceImpl::OnLedgerStateLoaded, this, handler, data);
  std::unique_ptr<ledger::LedgerTaskRunner> task(new bat_ledger::LedgerTaskRunnerImpl(t));
  RunTask(std::move(task));
}

void BraveRewardsServiceImpl::OnLedgerStateLoaded(
    ledger::LedgerCallbackHandler* handler,
    const std::string& data) {

  handler->OnLedgerStateLoaded(data.empty() ? ledger::Result::ERROR
                                            : ledger::Result::OK,
                               data);
}

void BraveRewardsServiceImpl::LoadPublisherState(
    ledger::LedgerCallbackHandler* handler) {

  std::string data = LoadStateOnFileTaskRunner(publisher_state_path_);

  bat_ledger::LedgerTaskRunnerImpl::Task t = std::bind(&BraveRewardsServiceImpl::OnPublisherStateLoaded, this, handler, data);
  std::unique_ptr<ledger::LedgerTaskRunner> task(new bat_ledger::LedgerTaskRunnerImpl(t));
  RunTask(std::move(task));
}

void BraveRewardsServiceImpl::OnPublisherStateLoaded(
    ledger::LedgerCallbackHandler* handler,
    const std::string& data) {
  handler->OnPublisherStateLoaded(data.empty() ? ledger::Result::ERROR
                                               : ledger::Result::OK,
                                  data);
}


void BraveRewardsServiceImpl::SaveLedgerState(const std::string& ledger_state,
                                      ledger::LedgerCallbackHandler* handler) {

  /*
  base::ImportantFileWriter writer(ledger_state_path_, file_task_runner_);

  writer.RegisterOnNextWriteCallbacks(
      base::Closure(),
      base::Bind(
        &PostWriteCallback,
        base::Bind(&BraveRewardsServiceImpl::OnLedgerStateSaved, AsWeakPtr(),
            base::Unretained(handler)),
        base::SequencedTaskRunnerHandle::Get()));

  writer.WriteNow(std::make_unique<std::string>(ledger_state));
  */

  bool success = true;
  try
  {
    std::ofstream f(ledger_state_path_);
    f << ledger_state;
    f.close();
  }
  catch (std::exception &)
  {
    success = false;
  }

  std::function <void(bool)> callback = std::bind(&BraveRewardsServiceImpl::OnLedgerStateSaved, this, handler, std::placeholders::_1);
  PostWriteCallback(callback, success);
}


void BraveRewardsServiceImpl::OnLedgerStateSaved(
    ledger::LedgerCallbackHandler* handler,
    bool success) {
  handler->OnLedgerStateSaved(success ? ledger::Result::OK
                                      : ledger::Result::ERROR);
}

void BraveRewardsServiceImpl::SavePublisherState(const std::string& publisher_state,
                                      ledger::LedgerCallbackHandler* handler) {

  /*
  base::ImportantFileWriter writer(publisher_state_path_, file_task_runner_);

  writer.RegisterOnNextWriteCallbacks(
      base::Closure(),
      base::Bind(
        &PostWriteCallback,
        base::Bind(&BraveRewardsServiceImpl::OnPublisherStateSaved, AsWeakPtr(),
            base::Unretained(handler)),
        base::SequencedTaskRunnerHandle::Get()));

  writer.WriteNow(std::make_unique<std::string>(publisher_state));
  */


  bool success = true;
  try
  {
    std::ofstream f(publisher_state_path_);
    f << publisher_state;
    f.close();
  }
  catch (std::exception &)
  {
    success = false;
  }

  std::function <void(bool)> callback = std::bind(&BraveRewardsServiceImpl::OnPublisherStateSaved, this, handler, std::placeholders::_1);
  PostWriteCallback(callback, success);
}


void BraveRewardsServiceImpl::OnPublisherStateSaved(
    ledger::LedgerCallbackHandler* handler,
    bool success) {
  handler->OnPublisherStateSaved(success ? ledger::Result::OK
                                         : ledger::Result::ERROR);
}


void BraveRewardsServiceImpl::SavePublisherInfo(
    std::unique_ptr<ledger::PublisherInfo> publisher_info,
    ledger::PublisherInfoCallback callback) {

  bool success = SavePublisherInfoOnFileTaskRunner(*publisher_info, publisher_info_backend_.get());

  bat_ledger::LedgerTaskRunnerImpl::Task t = std::bind(&BraveRewardsServiceImpl::OnPublisherInfoSaved, this, callback, std::shared_ptr<ledger::PublisherInfo>(publisher_info.release()), success);
  std::unique_ptr<ledger::LedgerTaskRunner> task(new bat_ledger::LedgerTaskRunnerImpl(t));
  RunTask(std::move(task));
}

void BraveRewardsServiceImpl::OnPublisherInfoSaved(
    ledger::PublisherInfoCallback callback,
    std::shared_ptr<ledger::PublisherInfo> info,
    bool success) {

  //make a copy of PublisherInfo and attach it to unique_ptr
  callback(success ? ledger::Result::OK : ledger::Result::ERROR, std::make_unique <ledger::PublisherInfo>(*info));
  TriggerOnContentSiteUpdated();
}


void BraveRewardsServiceImpl::LoadPublisherInfo(
    ledger::PublisherInfoFilter filter,
    ledger::PublisherInfoCallback callback) {

	/*
	  base::PostTaskAndReplyWithResult(file_task_runner_.get(), FROM_HERE,
      base::Bind(&LoadPublisherInfoListOnFileTaskRunner,
          // set limit to 2 to make sure there is
          // only 1 valid result for the filter
          0, 2, filter, publisher_info_backend_.get()),
      base::Bind(&BraveRewardsServiceImpl::OnPublisherInfoLoaded,
                     AsWeakPtr(),
                     callback));
	*/

  ledger::PublisherInfoList list = LoadPublisherInfoListOnFileTaskRunner(0,2, filter, publisher_info_backend_.get());
  bat_ledger::LedgerTaskRunnerImpl::Task t = std::bind(&BraveRewardsServiceImpl::OnPublisherInfoLoaded, this, callback, list);
  std::unique_ptr<ledger::LedgerTaskRunner> task(new bat_ledger::LedgerTaskRunnerImpl(t));
  RunTask(std::move(task));
}

void BraveRewardsServiceImpl::OnPublisherInfoLoaded(
    ledger::PublisherInfoCallback callback,
    const ledger::PublisherInfoList list) {

  if (list.size() == 0) {
    callback(ledger::Result::NOT_FOUND,
      std::unique_ptr<ledger::PublisherInfo>());
    return;
  }
  else if (list.size() > 1) {
    callback(ledger::Result::TOO_MANY_RESULTS,
      std::unique_ptr<ledger::PublisherInfo>());
    return;
  }
  else {
    callback(ledger::Result::OK,
      std::make_unique<ledger::PublisherInfo>(list[0]));
  }
}

void BraveRewardsServiceImpl::LoadPublisherInfoList(
  uint32_t start,
  uint32_t limit,
  ledger::PublisherInfoFilter filter,
  ledger::GetPublisherInfoListCallback callback) {

  ledger::PublisherInfoList pubList = LoadPublisherInfoListOnFileTaskRunner(start, limit, filter, publisher_info_backend_.get());
  bat_ledger::LedgerTaskRunnerImpl::Task t = std::bind(&BraveRewardsServiceImpl::OnPublisherInfoListLoaded, this, start, limit, callback, pubList);
  std::unique_ptr<ledger::LedgerTaskRunner> task(new bat_ledger::LedgerTaskRunnerImpl(t));
  RunTask(std::move(task));
}

void BraveRewardsServiceImpl::OnPublisherInfoListLoaded(
    uint32_t start,
    uint32_t limit,
    ledger::GetPublisherInfoListCallback callback,
    const ledger::PublisherInfoList& list) {
  uint32_t next_record = 0;
  if (list.size() == limit)
    next_record = start + limit + 1;

  callback(std::cref(list), next_record);
}

std::unique_ptr<ledger::LedgerURLLoader> BraveRewardsServiceImpl::LoadURL(const std::string& url,
                 const std::vector<std::string>& headers,
                 const std::string& content,
                 const std::string& contentType,
                 const ledger::URL_METHOD& method,
                 ledger::LedgerCallbackHandler* handler) {
  //net::URLFetcher::RequestType request_type = URLMethodToRequestType(method);

  bat_ledger_urlfetcher::URLFetcher* fetcher = bat_ledger_urlfetcher::URLFetcher::Create( url, method, this).release();

  //fetcher->SetRequestContext(g_browser_process->system_request_context());

  if (!content.empty())
    fetcher->SetUploadData(contentType, content);

  for (size_t i = 0; i < headers.size(); i++)
    fetcher->AddExtraRequestHeader(headers[i]);

  /*
  FetchCallback callback = base::Bind(
      &ledger::LedgerCallbackHandler::OnURLRequestResponse,
      base::Unretained(handler),
      next_id);
   */

  FetchCallback callback = std::bind(&ledger::LedgerCallbackHandler::OnURLRequestResponse, handler, next_id, std::placeholders::_1, std::placeholders::_2, std::placeholders::_3);

  std::lock_guard<std::mutex> lk(vec_mx_);
  fetchers_[fetcher] = callback;
  std::unique_ptr<ledger::LedgerURLLoader> loader(new LedgerURLLoaderImpl(next_id++, fetcher));
  return loader;
}

void BraveRewardsServiceImpl::OnURLFetchComplete(const bat_ledger_urlfetcher::URLFetcher* source) {

  std::unique_lock<std::mutex> lk(vec_mx_);
  if (fetchers_.find(source) == fetchers_.end())
    return;

  auto callback = fetchers_[source];
  fetchers_.erase(source);
  lk.unlock();

  int response_code = source->GetResponseCode();
  std::string body;
  if (response_code == bat_ledger_urlfetcher::URLFetcher::ResponseCode::HTTP_OK && source->GetStatus())
  {
    source->GetResponseAsString(&body);
  }

  std::string url;
  source->GetUrl(url);

  //get rid of previous  URLFetcher
  delete source;


  bat_ledger::LedgerTaskRunnerImpl::Task t = std::bind(callback, url, response_code, body);
  std::unique_ptr<ledger::LedgerTaskRunner> task(new bat_ledger::LedgerTaskRunnerImpl(t));
  RunTask(std::move(task));
}

void BraveRewardsServiceImpl::RunIOTask(
    std::unique_ptr<ledger::LedgerTaskRunner> task) {

  RunTask(std::move(task));
  /*
  file_task_runner_->PostTask(FROM_HERE,
      base::BindOnce(&ledger::LedgerTaskRunner::Run, std::move(task)));
  */
}

void BraveRewardsServiceImpl::RunTask(
      std::unique_ptr<ledger::LedgerTaskRunner> task) {

#if !defined BAT_CLIENT_SINGLE_THREAD
  std::lock_guard<std::mutex> lk(vec_mx_);
  tasks_in_progress_.emplace_back( [t = std::move(task)]() { t->Run(); });
#else
  task->Run();
#endif

  /*
  content::BrowserThread::PostTask(content::BrowserThread::UI, FROM_HERE,
      base::BindOnce(&ledger::LedgerTaskRunner::Run,
                     std::move(task)));
  */
}


void BraveRewardsServiceImpl::TriggerOnWalletInitialized(int error_code) {
  for (auto& observer : observers_)
    observer->OnWalletInitialized(this, error_code);
}

void BraveRewardsServiceImpl::TriggerOnWalletProperties(int error_code,
    std::unique_ptr<ledger::WalletInfo> wallet_info) {
  std::unique_ptr<brave_rewards::WalletProperties> wallet_properties;

  if (wallet_info) {
    wallet_properties.reset(new brave_rewards::WalletProperties);
    wallet_properties->probi = wallet_info->probi_;
    wallet_properties->balance = wallet_info->balance_;
    wallet_properties->rates = wallet_info->rates_;
    wallet_properties->parameters_choices = wallet_info->parameters_choices_;
    wallet_properties->parameters_range = wallet_info->parameters_range_;
    wallet_properties->parameters_days = wallet_info->parameters_days_;

    for (size_t i = 0; i < wallet_info->grants_.size(); i ++) {
      ledger::Grant grant;

      grant.altcurrency = wallet_info->grants_[i].altcurrency;
      grant.probi = wallet_info->grants_[i].probi;
      grant.expiryTime = wallet_info->grants_[i].expiryTime;

      wallet_properties->grants.push_back(grant);
    }
  }

  for (auto& observer : observers_)
    observer->OnWalletProperties(this, error_code, std::move(wallet_properties));
}
void BraveRewardsServiceImpl::GetWalletProperties() {
  /*
  if (ready().is_signaled()) {
    ledger_->GetWalletProperties();
  } else {
    ready().Post(FROM_HERE,
        base::Bind(&brave_rewards::RewardsService::GetWalletProperties,
            base::Unretained(this)));
  }
  */
  ledger_->GetWalletProperties();
}
void BraveRewardsServiceImpl::GetGrant(const std::string& lang,
    const std::string& payment_id) {
  ledger_->GetGrant(lang, payment_id);
}
void BraveRewardsServiceImpl::TriggerOnGrant(ledger::Result result,
                                        const ledger::Grant& grant) {
  ledger::Grant properties;

  properties.promotionId = grant.promotionId;
  properties.altcurrency = grant.altcurrency;
  properties.probi = grant.probi;
  properties.expiryTime = grant.expiryTime;

  for (auto& observer : observers_)
    observer->OnGrant(this, result, properties);
}

void BraveRewardsServiceImpl::GetGrantCaptcha() {
  ledger_->GetGrantCaptcha();
}

void BraveRewardsServiceImpl::TriggerOnGrantCaptcha(const std::string& image) {
  for (auto& observer : observers_)
    observer->OnGrantCaptcha(this, image);
}

std::string BraveRewardsServiceImpl::GetWalletPassphrase() const {
  return ledger_->GetWalletPassphrase();
}

void BraveRewardsServiceImpl::RecoverWallet(const std::string passPhrase) const {
  return ledger_->RecoverWallet(passPhrase);
}



void BraveRewardsServiceImpl::TriggerOnRecoverWallet(ledger::Result result,
                                                double balance,
                                    const std::vector<ledger::Grant>& grants) {
  std::vector<ledger::Grant> newGrants;
  for (size_t i = 0; i < grants.size(); i ++) {
    ledger::Grant grant;

    grant.altcurrency = grants[i].altcurrency;
    grant.probi = grants[i].probi;
    grant.expiryTime = grants[i].expiryTime;

    newGrants.push_back(grant);
  }
  for (auto& observer : observers_)
    observer->OnRecoverWallet(this, result, balance, newGrants);
}

void BraveRewardsServiceImpl::SolveGrantCaptcha(const std::string& solution) const {
  return ledger_->SolveGrantCaptcha(solution);
}

void BraveRewardsServiceImpl::TriggerOnGrantFinish(ledger::Result result,
                                              const ledger::Grant& grant) {
  ledger::Grant properties;

  properties.promotionId = grant.promotionId;
  properties.altcurrency = grant.altcurrency;
  properties.probi = grant.probi;
  properties.expiryTime = grant.expiryTime;

  for (auto& observer : observers_)
    observer->OnGrantFinish(this, result, properties);
}

uint64_t BraveRewardsServiceImpl::GetReconcileStamp() const {
  return ledger_->GetReconcileStamp();
}

std::map<std::string, std::string> BraveRewardsServiceImpl::GetAddresses() const {
  std::map<std::string, std::string> addresses;
  addresses.emplace("BAT", ledger_->GetBATAddress());
  addresses.emplace("BTC", ledger_->GetBTCAddress());
  addresses.emplace("ETH", ledger_->GetETHAddress());
  addresses.emplace("LTC", ledger_->GetLTCAddress());
  return addresses;
}

void BraveRewardsServiceImpl::SetPublisherMinVisitTime(
    uint64_t duration_in_seconds) const {
  return ledger_->SetPublisherMinVisitTime(duration_in_seconds);
}

void BraveRewardsServiceImpl::SetPublisherMinVisits(unsigned int visits) const {
  return ledger_->SetPublisherMinVisits(visits);
}

void BraveRewardsServiceImpl::SetPublisherAllowNonVerified(bool allow) const {
  return ledger_->SetPublisherAllowNonVerified(allow);
}

void BraveRewardsServiceImpl::SetPublisherAllowVideos(bool allow) const {
  return ledger_->SetPublisherAllowVideos(allow);
}

void BraveRewardsServiceImpl::SetContributionAmount(double amount) const {
  return ledger_->SetContributionAmount(amount);
}

void BraveRewardsServiceImpl::SetAutoContribute(bool enabled) const {
  return ledger_->SetAutoContribute(enabled);
}

void BraveRewardsServiceImpl::TriggerOnContentSiteUpdated() {
  for (auto& observer : observers_)
    observer->OnContentSiteUpdated(this);
}

void BraveRewardsServiceImpl::SavePublishersList(const std::string& publishers_list,
                                      ledger::LedgerCallbackHandler* handler) {
  /*
  base::ImportantFileWriter writer(
      publisher_list_path_, file_task_runner_);

  writer.RegisterOnNextWriteCallbacks(
      base::Closure(),
      base::Bind(
        &PostWriteCallback,
        base::Bind(&BraveRewardsServiceImpl::OnPublishersListSaved, AsWeakPtr(),
            base::Unretained(handler)),
        base::SequencedTaskRunnerHandle::Get()));

  writer.WriteNow(std::make_unique<std::string>(publishers_list));
  */

  bool success = true;
  try
  {
    std::ofstream f(publisher_list_path_);
    f << publishers_list;
    f.close();
  }
  catch (std::exception &)
  {
    success = false;
  }

  std::function <void(bool)> callback = std::bind(&BraveRewardsServiceImpl::OnPublishersListSaved, this, handler, std::placeholders::_1);
  PostWriteCallback(callback, success);
}

void BraveRewardsServiceImpl::OnPublishersListSaved(
    ledger::LedgerCallbackHandler* handler,
    bool success) {
  handler->OnPublishersListSaved(success ? ledger::Result::OK
                                         : ledger::Result::ERROR);
}


void BraveRewardsServiceImpl::LoadPublisherList(
    ledger::LedgerCallbackHandler* handler) {
  /*
  base::PostTaskAndReplyWithResult(file_task_runner_.get(), FROM_HERE,
                                   base::Bind(&LoadStateOnFileTaskRunner, publisher_list_path_),
                                   base::Bind(&BraveRewardsServiceImpl::OnPublisherListLoaded,
                                              AsWeakPtr(),
                                              base::Unretained(handler)));
  */

  std::string data = LoadStateOnFileTaskRunner(publisher_list_path_);

  bat_ledger::LedgerTaskRunnerImpl::Task t = std::bind(&BraveRewardsServiceImpl::OnPublisherListLoaded, this, handler, data);
  std::unique_ptr<ledger::LedgerTaskRunner> task(new bat_ledger::LedgerTaskRunnerImpl(t));
  RunTask(std::move(task));
}

void BraveRewardsServiceImpl::OnPublisherListLoaded(
    ledger::LedgerCallbackHandler* handler,
    const std::string& data) {
  handler->OnPublisherListLoaded(
      data.empty() ? ledger::Result::NO_PUBLISHER_LIST
                   : ledger::Result::OK,
      data);
}

std::map<std::string, brave_rewards::BalanceReport> BraveRewardsServiceImpl::GetAllBalanceReports() {
  std::map<std::string, ledger::BalanceReportInfo> reports = ledger_->GetAllBalanceReports();

  std::map<std::string, brave_rewards::BalanceReport> newReports;
  for (auto const& report : reports) {
    brave_rewards::BalanceReport newReport;
    const ledger::BalanceReportInfo oldReport = report.second;
    newReport.opening_balance = oldReport.opening_balance_;
    newReport.closing_balance = oldReport.closing_balance_;
    newReport.grants = oldReport.grants_;
    newReport.earning_from_ads = oldReport.earning_from_ads_;
    newReport.auto_contribute = oldReport.auto_contribute_;
    newReport.recurring_donation = oldReport.recurring_donation_;
    newReport.one_time_donation = oldReport.one_time_donation_;

    newReports[report.first] = newReport;
  }

  return newReports;
}

void BraveRewardsServiceImpl::TestingJoinAllRunningTasks() {
  while (true)
  {
    std::unique_lock<std::mutex> lk(vec_mx_);
    if (!tasks_in_progress_.empty())
    {
      std::thread t = std::move(tasks_in_progress_.back());
      tasks_in_progress_.pop_back();
      lk.unlock();
      if (t.joinable()) {
        t.join();
      }
      std::this_thread::sleep_for(std::chrono::seconds(2));
    }
    else {
      std::cout << std::endl << "The task vector is empty" << std::endl;
      break;
    }
  }
}

void BraveRewardsServiceImpl::SetTimer(uint64_t time_offset, uint32_t & timer_id) {
  std::unique_lock<std::mutex> lk(timer_mx_);

  //don't set more timers than allowed, otherwise TestingJoinAllRunningTasks can wait forever
  if (timer_id_ == max_number_timers_) {
    std::cout << std::endl << "timers limit has been exceeded: " << max_number_timers_ << std::endl;
    return;
  }

  timer_id = ++timer_id_;
  timers_.emplace_back(io_, boost::posix_time::seconds(time_offset));
  boost::asio::deadline_timer & t = timers_.back();
  lk.unlock();

  t.async_wait([timer_id, this](const boost::system::error_code& e) {
    if (!e) {
      std::cout << std::endl << "timer expired: " << timer_id << std::endl;
      ledger_->OnTimer(timer_id);
    }
    else if (e == boost::asio::error::operation_aborted) {
      std::cout << std::endl << "timer has been cancelled: " << timer_id << std::endl;
    }
    else
    { //error
      std::cout << std::endl << "timer error: " << timer_id << std::endl;
    }
  });

  bat_ledger::LedgerTaskRunnerImpl::Task t1 = [time_offset,timer_id,this]() {
    std::cout << std::endl << "timer wait thread started: " << time_offset<< ">>>"  << timer_id << std::endl;
    io_.run();
    io_.reset();
    std::cout << std::endl << "timer wait thread ended: " << time_offset << ">>>" << timer_id << std::endl;
  };
  std::unique_ptr<ledger::LedgerTaskRunner> task(new bat_ledger::LedgerTaskRunnerImpl(t1));
  RunTask(std::move(task));
}

void BraveRewardsServiceImpl::AllowTimersRun(uint32_t timers) {
  max_number_timers_ = timers;
}



}  // namespace brave_rewards
