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

#include "ledger_task_runner_impl.h"
#include "publisher_info_backend.h"
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
#include <chrono>

//#define COM_NO_WINDOWS_H
#include <combaseapi.h>

#include <curl/curl.h> //for curl_easy_escape

// TODO, just for test purpose
static bool created_wallet = false;
//

using namespace std::placeholders;

namespace brave_rewards {

namespace {

void GetLocalMonthYear(ledger::PUBLISHER_MONTH& month, std::string& year) {
  auto now = std::chrono::system_clock::now();
  time_t tt = std::chrono::system_clock::to_time_t(now);
  tm tm1;
  localtime_s(&tm1, &tt);
  year = std::to_string(tm1.tm_year + 1900);
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


bool SavePublisherInfoOnFileTaskRunner(
    ledger::PublisherInfo publisher_info,
    PublisherInfoBackend* backend) {
  if (backend && backend->Put(publisher_info.key, publisher_info.ToJSON())) {
    return true;
  }

  return false;
}

std::unique_ptr<ledger::PublisherInfo> LoadPublisherInfoOnFileTaskRunner(
    const std::string& key,
    PublisherInfoBackend* backend) {
  std::unique_ptr<ledger::PublisherInfo> info;
   std::string json;
  if (backend && backend->Get(key, &json)) {
    info.reset(
        new ledger::PublisherInfo(ledger::PublisherInfo::FromJSON(json)));
  }

  return info;
}

ledger::PublisherInfoList LoadPublisherInfoListOnFileTaskRunner(
    uint32_t start,
    uint32_t limit,
    ledger::PublisherInfoFilter filter,
    const std::vector<std::string>& prefix,
    PublisherInfoBackend* backend) {
  ledger::PublisherInfoList list;
   std::vector<std::string> results;
  if (backend && backend->LoadWithPrefix(start, limit, prefix, results)) {
    for (std::vector<std::string>::const_iterator it =
        results.begin(); it != results.end(); ++it) {
      list.push_back(ledger::PublisherInfo::FromJSON(*it));
    }
  }
   return list;
}

static uint64_t next_id = 1;
}  // namespace



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
    publisher_info_backend_(new PublisherInfoBackend(publisher_info_db_path_)) {
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



void BraveRewardsServiceImpl::MakePayment(const ledger::PaymentData& payment_data) {
  ledger_->MakePayment(payment_data);
}

void BraveRewardsServiceImpl::AddRecurringPayment(const std::string& publisher_id, const double& value) {
  ledger_->AddRecurringPayment(publisher_id, value);
}

void BraveRewardsServiceImpl::SetBalanceReport(const ledger::BalanceReportInfo& report_info) {
  ledger::PUBLISHER_MONTH month = ledger::PUBLISHER_MONTH::JANUARY;
  std::string year;
  GetLocalMonthYear(month, year);
  ledger_->SetBalanceReport(year, month, report_info);
}

bool BraveRewardsServiceImpl::GetBalanceReport(ledger::BalanceReportInfo* report_info) const {
  ledger::PUBLISHER_MONTH month = ledger::PUBLISHER_MONTH::JANUARY;
  std::string year;
  GetLocalMonthYear(month, year);
  return ledger_->GetBalanceReport(year, month, report_info);
}

void BraveRewardsServiceImpl::OnLoad(const std::string& _tld,
            const std::string& _domain,
            const std::string& _path,
            uint32_t tab_id) {
  ledger::PUBLISHER_MONTH month = ledger::PUBLISHER_MONTH::JANUARY;
  std::string year;
  GetLocalMonthYear(month, year);
  ledger::VisitData visit_data(_tld, _domain, _path, tab_id, month, year);
  ledger_->OnLoad(visit_data, GetCurrentTimeStamp());
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

}

void BraveRewardsServiceImpl::OnMediaStop(uint32_t tab_id) {

}

uint64_t BraveRewardsServiceImpl::GetCurrentTimeStamp() {
  auto start = std::chrono::system_clock::now().time_since_epoch();
  return std::chrono::duration_cast<std::chrono::milliseconds> (start).count();
}

void BraveRewardsServiceImpl::OnXHRLoad(uint32_t tab_id, const std::string & url,
  const std::string& first_party_url, const std::string& referrer) {

  std::map<std::string, std::string> parts;

  // Temporary commented out
  //for (net::QueryIterator it(url); !it.IsAtEnd(); it.Advance()) {
  //  parts[it.GetKey()] = it.GetUnescapedValue();
  //}
  //ledger_->OnXHRLoad(tab_id, url, parts, /*first_party_url, referrer,*/ GetCurrentTimeStamp());
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

  //return net::EscapeQueryParamValue(value, false);
}

std::vector<ledger::ContributionInfo> BraveRewardsServiceImpl::GetRecurringDonationPublisherInfo() {
  return ledger_->GetRecurringDonationPublisherInfo();
}

void BraveRewardsServiceImpl::GetPublisherInfoList(
    uint32_t start, uint32_t limit,
    const ledger::PublisherInfoFilter& filter,
    ledger::GetPublisherInfoListCallback callback) {
  ledger_->GetPublisherInfoList(start, limit,
      filter,
      callback);
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
}

void BraveRewardsServiceImpl::LoadPublisherInfo(
    const std::string& publisher_key,
    ledger::PublisherInfoCallback callback) {

  std::unique_ptr<ledger::PublisherInfo> pub_info = LoadPublisherInfoOnFileTaskRunner(publisher_key, publisher_info_backend_.get());
  bat_ledger::LedgerTaskRunnerImpl::Task t = std::bind(&BraveRewardsServiceImpl::OnPublisherInfoLoaded, this, callback, std::shared_ptr<ledger::PublisherInfo>(pub_info.release()));
  std::unique_ptr<ledger::LedgerTaskRunner> task(new bat_ledger::LedgerTaskRunnerImpl(t));
  RunTask(std::move(task));
}

void BraveRewardsServiceImpl::OnPublisherInfoLoaded(
    ledger::PublisherInfoCallback callback,
    std::shared_ptr<ledger::PublisherInfo> info) {

  //make a copy of PublisherInfo and attach it to unique_ptr
  callback(ledger::Result::OK, std::make_unique <ledger::PublisherInfo>(*info));
}

void BraveRewardsServiceImpl::LoadPublisherInfoList(
    uint32_t start,
    uint32_t limit,
    ledger::PublisherInfoFilter filter,
    const std::vector<std::string>& prefix,
    ledger::GetPublisherInfoListCallback callback) {

  ledger::PublisherInfoList pubList = LoadPublisherInfoListOnFileTaskRunner(start, limit, filter, prefix, publisher_info_backend_.get());
  bat_ledger::LedgerTaskRunnerImpl::Task t = std::bind(&BraveRewardsServiceImpl::OnPublisherInfoListLoaded, this, start, limit, callback, std::cref(pubList));
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
void BraveRewardsServiceImpl::SetPublisherMinVisitTime(uint64_t duration_in_milliseconds) {
  ledger_->SetPublisherMinVisitTime(duration_in_milliseconds);
}

void BraveRewardsServiceImpl::SetPublisherMinVisits(unsigned int visits) {
  ledger_->SetPublisherMinVisits(visits);
}

void BraveRewardsServiceImpl::SetPublisherAllowNonVerified(bool allow) {
  ledger_->SetPublisherAllowNonVerified(allow);
}

void BraveRewardsServiceImpl::SetContributionAmount(double amount) {
  ledger_->SetContributionAmount(amount);
}

uint64_t BraveRewardsServiceImpl::GetPublisherMinVisitTime() const {
  return ledger_->GetPublisherMinVisitTime();
}

unsigned int BraveRewardsServiceImpl::GetPublisherMinVisits() const {
  return ledger_->GetPublisherMinVisits();
}

bool BraveRewardsServiceImpl::GetPublisherAllowNonVerified() const {
  return ledger_->GetPublisherAllowNonVerified();
}

double BraveRewardsServiceImpl::GetContributionAmount() const {
  return ledger_->GetContributionAmount();
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
  //GetWalletProperties();
}

void BraveRewardsServiceImpl::OnReconcileComplete(ledger::Result result,
                                              const std::string& viewing_id) {
  LOG(ERROR) << "reconcile complete " << viewing_id;
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

  FetchCallback callback = std::bind(&ledger::LedgerCallbackHandler::OnURLRequestResponse, handler, next_id, std::placeholders::_1, std::placeholders::_2);

  fetchers_[fetcher] = callback;

  std::unique_ptr<ledger::LedgerURLLoader> loader(new LedgerURLLoaderImpl(next_id++, fetcher));

  return loader;
}

void BraveRewardsServiceImpl::OnURLFetchComplete(const bat_ledger_urlfetcher::URLFetcher* source) {
  if (fetchers_.find(source) == fetchers_.end())
    return;

  auto callback = fetchers_[source];
  fetchers_.erase(source);

  int response_code = source->GetResponseCode();
  std::string body;
  if (response_code == bat_ledger_urlfetcher::URLFetcher::ResponseCode::HTTP_OK && source->GetStatus())
  {
    source->GetResponseAsString(&body);
  }

  //get rid of previous  URLFetcher
  delete source;

  bat_ledger::LedgerTaskRunnerImpl::Task t = std::bind(callback, response_code, body);
  std::unique_ptr<ledger::LedgerTaskRunner> task(new bat_ledger::LedgerTaskRunnerImpl(t));
  RunTask(std::move(task));
}

void BraveRewardsServiceImpl::OnWalletProperties(ledger::Result result,
                          std::unique_ptr<ledger::WalletInfo> info) {
  // TODO implement
  LOG(ERROR) << "!!!BraveRewardsServiceImpl::OnWalletProperties walletInfo.balance_ == " << info->balance_;
}

void BraveRewardsServiceImpl::GetWalletProperties() {
  ledger_->GetWalletProperties();
}

void BraveRewardsServiceImpl::GetGrant(const std::string& lang, const std::string& paymentId) {
  // TODO
}

void BraveRewardsServiceImpl::OnGrant(ledger::Result result, const ledger::Grant& grant) {
  // TODO
}
void BraveRewardsServiceImpl::GetGrantCaptcha() {
  // TODO
}
void BraveRewardsServiceImpl::OnGrantCaptcha(const std::string& image) {
  // TODO
}
void BraveRewardsServiceImpl::OnRecoverWallet(ledger::Result result, double balance,
  const std::vector<ledger::Grant>& grants) {
  // TODO
}
void BraveRewardsServiceImpl::OnGrantFinish(ledger::Result result, const ledger::Grant& grant) {
  // TODO
}

/*void BraveRewardsServiceImpl::SolvePromotionCaptcha(const std::string& solution) const {
  // TODO
}

std::string BraveRewardsServiceImpl::GetWalletPassphrase() const {
  // TODO
}

void BraveRewardsServiceImpl::RecoverWallet(const std::string passPhrase) const {
  // TODO
} */

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

void BraveRewardsServiceImpl::TriggerOnWalletInitialized(int error_code) {
  for (auto& observer : observers_)
    observer->OnWalletInitialized(this, error_code);
}

}  // namespace brave_rewards
