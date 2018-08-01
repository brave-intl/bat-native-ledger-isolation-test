 /* This Source Code Form is subject to the terms of the Mozilla Public
  * License, v. 2.0. If a copy of the MPL was not distributed with this file,
  * You can obtain one at http://mozilla.org/MPL/2.0/. */

 #ifndef BRAVE_REWARDS_SERVICE_IMPL_
 #define BRAVE_REWARDS_SERVICE_IMPL_

 #include <memory>
#include <vector>
#include <thread>
#include <mutex>
#include <locale>
#include <codecvt>

#include <shlobj.h>

//#include "base/files/file_path.h"
//#include "base/observer_list.h"
//#include "base/memory/weak_ptr.h"
#include "bat/ledger/ledger_client.h"
#include "brave_rewards_service.h"
#include "url_request_handler.h"
#include "URLFetcher.h"
//#include "content/public/browser/browser_thread.h"
//#include "net/url_request/url_fetcher_delegate.h"


namespace ledger {
class Ledger;
class LedgerCallbackHandler;
}


class Profile
{
public:
  std::string GetPath() {
    PWSTR path = NULL;
    std::string spath;
    HRESULT hr = SHGetKnownFolderPath(FOLDERID_Profile, 0, NULL, &path);
    if (SUCCEEDED(hr)) {
      std::wstring_convert <std::codecvt_utf8<wchar_t>, wchar_t> convert;
      spath = convert.to_bytes(path);
    }
    CoTaskMemFree(path);
    return spath;
  }
};

namespace brave_rewards {


class BraveRewardsServiceImpl : public BraveRewardsService,
                            public ledger::LedgerClient ,public bat_ledger_urlfetcher::URLFetcherDelegate /*,public base::SupportsWeakPtr<BraveRewardsServiceImpl>*/
{
public:
  BraveRewardsServiceImpl(Profile* profile);
  ~BraveRewardsServiceImpl() override;

  BraveRewardsServiceImpl(const BraveRewardsServiceImpl &) = delete;
  BraveRewardsServiceImpl & operator= (const BraveRewardsServiceImpl &) = delete;

  // KeyedService///////////////////////////////////////////////////////////////////////////
  void Shutdown() override;


  //Ledger interface///////////////////////////////////////////////////////////////////////
  void CreateWallet() override;
  void SaveVisit(const std::string& publisher,
                 uint64_t duration,
                 bool ignoreMinTime) override;

  void SetPublisherMinVisitTime(uint64_t duration_in_milliseconds) override;
  void SetPublisherMinVisits(unsigned int visits) override;
  void SetPublisherAllowNonVerified(bool allow) override;
  void SetContributionAmount(double amount) override;

  uint64_t GetPublisherMinVisitTime() const override; // In milliseconds
  unsigned int GetPublisherMinVisits() const override;
  bool GetPublisherAllowNonVerified() const override;
  double GetContributionAmount() const override;

  void TestingJoinAllRunningTasks();

private:
  typedef std::function<void(int, const std::string&)> FetchCallback;

  void OnLedgerStateSaved(ledger::LedgerCallbackHandler* handler,bool success);

  void OnLedgerStateLoaded(ledger::LedgerCallbackHandler* handler,const std::string& data);

  void OnPublisherStateSaved(ledger::LedgerCallbackHandler* handler, bool success);

  void OnPublisherStateLoaded(ledger::LedgerCallbackHandler* handler,const std::string& data);



  void PostWriteCallback(std::function < void(bool success)> & callback, bool write_success);
  void TriggerOnWalletCreated(int error_code);

  // ledger::LedgerClient/////////////////////////////////////////////////////////////////////////
  std::string GenerateGUID() const override;
  void OnWalletCreated(ledger::Result result) override;
  void OnReconcileComplete(ledger::Result result,
                           const std::string& viewing_id) override;
  void LoadLedgerState(ledger::LedgerCallbackHandler* handler) override;
  void LoadPublisherState(ledger::LedgerCallbackHandler* handler) override;
  void SaveLedgerState(const std::string& ledger_state,
                       ledger::LedgerCallbackHandler* handler) override;
  void SavePublisherState(const std::string& publisher_state,
                          ledger::LedgerCallbackHandler* handler) override;

  std::unique_ptr<ledger::LedgerURLLoader> LoadURL(const std::string& url,
                   const std::vector<std::string>& headers,
                   const std::string& content,
                   const std::string& contentType,
                   const ledger::URL_METHOD& method,
                   ledger::LedgerCallbackHandler* handler) override;

  void RunIOTask(std::unique_ptr<ledger::LedgerTaskRunner> task) override;
  void RunTask(std::unique_ptr<ledger::LedgerTaskRunner> task) override;

  // URLFetcherDelegate impl
  void OnURLFetchComplete(const bat_ledger_urlfetcher::URLFetcher* source) /*override*/;

  std::unique_ptr<ledger::Ledger> ledger_;

  Profile* profile_;  // NOT OWNED
  //const scoped_refptr<base::SequencedTaskRunner> file_task_runner_;
  std::mutex vec_mx_;
  std::vector <std::thread> tasks_in_progress_;

  const std::string ledger_state_path_;
  const std::string publisher_state_path_;

  std::map<const bat_ledger_urlfetcher::URLFetcher*, FetchCallback> fetchers_;

};

}  // namespace brave_rewards

#endif  // BRAVE_REWARDS_SERVICE_IMPL_
