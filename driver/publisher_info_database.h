/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef BRAVE_REWARDS_PUBLISHER_INFO_DATABASE_H_
#define BRAVE_REWARDS_PUBLISHER_INFO_DATABASE_H_

#include <stddef.h>
/*
#include "base/compiler_specific.h"
#include "base/files/file_path.h"
#include "base/macros.h"
#include "base/memory/memory_pressure_listener.h"
#include "base/sequence_checker.h"
#include "build/build_config.h"
#include "sql/connection.h"
#include "sql/init_status.h"
#include "sql/meta_table.h"
*/

#include "bat/ledger/publisher_info.h"
#include "sqlite_modern_cpp.h"
#include "contribution_info.h"
#include "recurring_donation.h"

namespace brave_rewards {

class PublisherInfoDatabase {
 public:
  PublisherInfoDatabase(const std::string & db_path);
  ~PublisherInfoDatabase();

  PublisherInfoDatabase(const PublisherInfoDatabase &) = delete;
  PublisherInfoDatabase & operator = (const PublisherInfoDatabase &) = delete;

  bool InsertOrUpdatePublisherInfo(const ledger::PublisherInfo& info);
  bool InsertOrUpdateMediaPublisherInfo(const std::string& media_key, const std::string& publisher_id);
  bool InsertContributionInfo(const brave_rewards::ContributionInfo& info);
  bool InsertOrUpdateRecurringDonation(const brave_rewards::RecurringDonation& info);

  bool Find(int start,
            int limit,
            const ledger::PublisherInfoFilter& filter,
            ledger::PublisherInfoList* list);
  int Count(const ledger::PublisherInfoFilter& filter);

  std::unique_ptr<ledger::PublisherInfo> GetMediaPublisherInfo(
      const std::string& media_key);
  void GetRecurringDonations(ledger::PublisherInfoList* list);
  void GetTips(ledger::PublisherInfoList* list, ledger::PUBLISHER_MONTH month, int year);
  bool RemoveRecurring(const std::string& publisher_key);

  // Returns the current version of the publisher info database
  static int GetCurrentVersion();

 private:
  bool Init();
  bool MetaTableInit();

  bool CreateContributionInfoTable();
  bool CreatePublisherInfoTable();
  bool CreateMediaPublisherInfoTable();
  bool CreateActivityInfoTable();
  bool CreateContributionInfoIndex();

  bool CreateActivityInfoIndex();
  bool CreateRecurringDonationTable();
  bool CreateRecurringDonationIndex();

  std::string BuildFilterClauses(int start,
                           int limit,
                           const ledger::PublisherInfoFilter& filter);

  bool EnsureCurrentVersion();

  std::unique_ptr<sqlite::database> db_;
  const std::string db_path_;
  bool initialized_;

  //SEQUENCE_CHECKER(sequence_checker_);

};

}  // namespace brave_rewards

#endif  // BRAVE_REWARDS_PUBLISHER_INFO_DATABASE_H_