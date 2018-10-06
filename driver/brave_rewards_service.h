/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef BRAVE_REWARDS_SERVICE_
#define BRAVE_REWARDS_SERVICE_

//CHROMIUM
//#include "base/macros.h"
//#include "base/observer_list.h"
//#include "components/keyed_service/core/keyed_service.h"

#include "content_site.h"
#include <string>
#include <list>
#include <functional>
#include "brave_rewards_service_observer.h"
#include "bat/ledger/ledger.h"
#include "balance_report.h"

namespace brave_rewards {

bool IsMediaLink(const std::string & url,
                 const std::string & first_party_url,
                 const std::string & referrer);

class BraveRewardsServiceObserver;


using GetContentSiteListCallback = std::function<void(std::unique_ptr<ContentSiteList>, uint32_t /* next_record */)>;

class KeyedService {
public:
  virtual ~KeyedService() {};
  virtual void Init() {};
  virtual void Shutdown() {};
};


class BraveRewardsService : public KeyedService {
public:
  BraveRewardsService();
  ~BraveRewardsService() override;

  BraveRewardsService(const BraveRewardsService &) = delete;
  BraveRewardsService & operator= (const BraveRewardsService &) = delete;

  // KeyedService:///////////////////////////////////////////////////////////////
  void Shutdown() override;


  // Ledger interface:///////////////////////////////////////////////////////////////
  virtual void CreateWallet() = 0;
  virtual void GetWalletProperties() = 0;
  virtual void GetContentSiteList(uint32_t start,
                                  uint32_t limit,
                                const GetContentSiteListCallback& callback) = 0;
  virtual void GetGrant(const std::string& lang, const std::string& paymentId) = 0;
  virtual void GetGrantCaptcha() = 0;
  virtual void SolveGrantCaptcha(const std::string& solution) const = 0;
  virtual std::string GetWalletPassphrase() const = 0;
  virtual unsigned int GetNumExcludedSites() const = 0;
  virtual void RecoverWallet(std::string passPhrase) const = 0;
  virtual void ExcludePublisher(std::string publisherKey) const = 0;
  virtual void RestorePublishers() = 0;
  virtual void OnLoad(uint32_t tab_id, const std::string & gurl) = 0;

  virtual void OnUnload(uint32_t tab_id) = 0;
  virtual void OnShow(uint32_t tab_id) = 0;
  virtual void OnHide(uint32_t tab_id) = 0;
  virtual void OnForeground(uint32_t tab_id) = 0;
  virtual void OnBackground(uint32_t tab_id) = 0;
  virtual void OnMediaStart(uint32_t tab_id) = 0;
  virtual void OnMediaStop(uint32_t tab_id) = 0;
  virtual void OnXHRLoad(uint32_t tab_id,
      const std::string & url,
	  const std::string& first_party_url,
      const std::string& referrer) = 0;
  virtual void OnPostData(uint32_t tab_id,
                          const std::string & url,
                          const std::string & first_party_url,
                          const std::string & referrer,
                          const std::string & post_data) = 0;


  virtual uint64_t GetReconcileStamp() const = 0;
  virtual std::map<std::string, std::string> GetAddresses() const = 0;
  virtual void SetRewardsMainEnabled(bool enabled) const = 0;
  virtual void SetPublisherMinVisitTime(uint64_t duration_in_seconds) const = 0;
  virtual void SetPublisherMinVisits(unsigned int visits) const = 0;
  virtual void SetPublisherAllowNonVerified(bool allow) const = 0;
  virtual void SetPublisherAllowVideos(bool allow) const = 0;

  virtual void SetContributionAmount(double amount) const = 0;
  virtual void SetUserChangedContribution() const = 0;
  virtual void SetAutoContribute(bool enabled) const = 0;

  virtual std::map<std::string, brave_rewards::BalanceReport> GetAllBalanceReports() = 0;

	virtual void GetCurrentBalanceReport() = 0;
  virtual bool IsWalletCreated() = 0;
  virtual void GetPublisherActivityFromUrl(uint64_t windowId, const std::string& url) = 0;

  //Testing
  virtual void TestingJoinAllRunningTasks() = 0;
  virtual void AllowTimersRun(uint32_t timers) = 0;


  //helpers//////////////////////////////////////////////////////////////////////////////////
  void AddObserver(BraveRewardsServiceObserver* observer) { observers_.push_back(observer); };
  void RemoveObserver(BraveRewardsServiceObserver* observer) { observers_.remove(observer); };

protected:
  std::list<BraveRewardsServiceObserver *> observers_;
};

}  // namespace brave_rewards

#endif  // BRAVE_REWARDS_SERVICE_
