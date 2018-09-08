/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

#include"stdafx.h"

#include "publisher_info_database.h"

#include <stdint.h>
#include <string>
#include <sstream>
#include <iostream>
#include <memory>

/*
#include "base/bind.h"
#include "base/command_line.h"
#include "base/files/file_util.h"
#include "build/build_config.h"
#include "sql/meta_table.h"
#include "sql/statement.h"
#include "sql/transaction.h"
*/


namespace brave_rewards {

namespace {

const int kCurrentVersionNumber = 1;
const int kCompatibleVersionNumber = 1;

}  // namespace

PublisherInfoDatabase::PublisherInfoDatabase(const std::string & db_path) :
    db_path_(db_path),
    initialized_(false) {
  //DETACH_FROM_SEQUENCE(sequence_checker_);
}

PublisherInfoDatabase::~PublisherInfoDatabase() {
}

bool PublisherInfoDatabase::MetaTableInit() {
  bool succeded = false;

  if (!initialized_)
    return false;

  try {
    //create meta table
    *db_ << "create table if not exists meta (key longvarchar not null unique primary key, value longvarchar);";
    *db_ << "insert or replace into meta (key,value) values (?,?)" << "version" << GetCurrentVersion();
    *db_ << "insert or replace into meta (key,value) values (?,?)" << "last_compatible_version" << kCompatibleVersionNumber;
  }
  catch (sqlite::sqlite_exception e)
  {
    std::cout << "sqlite_exception: " << e.what() << std::endl;
    succeded = false;
  }
  return succeded;
}

bool PublisherInfoDatabase::Init() {
  //DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  if (initialized_)
    return true;
  bool transactionStarted = false;

  try {

    sqlite::sqlite_config config;
    config.flags = sqlite::OpenFlags::READWRITE | sqlite::OpenFlags::CREATE; //default
    db_ = std::make_unique<sqlite::database>(db_path_, config);

    *db_ << "begin;"; // begin a transaction ...
    transactionStarted = true;

    if (!CreatePublisherInfoTable() ||
      !CreateContributionInfoTable() ||
      !CreateActivityInfoTable() ||
      !CreateMediaPublisherInfoTable())
      return false;

    CreateContributionInfoIndex();
    CreateActivityInfoIndex();



    // Version check.
    bool version_status = EnsureCurrentVersion();
    if (version_status != true)
      return false;

    *db_ << "commit;"; // commit all the changes.

    /*
    memory_pressure_listener_.reset(new base::MemoryPressureListener(
        base::Bind(&PublisherInfoDatabase::OnMemoryPressure,
        base::Unretained(this))));
    */

    initialized_ = true;
    return initialized_;
  }
  //catch all exceptions
  catch (std::exception & e)
  {
    std::cout << "std::exception: " << e.what() << std::endl;
    initialized_ = false;
    if (transactionStarted) {
      *db_ << "rollback;"; // commit all the changes.
    }
  }

  return initialized_;
}

bool PublisherInfoDatabase::CreateContributionInfoTable() {
  //DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  const char* name = "contribution_info";
  bool succeded = false;

  // Note: revise implementation for InsertOrUpdateRowByID() if you add any
  // new constraints to the schema.
  std::ostringstream sql;

  sql << "create table if not exists " <<  name <<
    " (publisher_id LONGVARCHAR NOT NULL,"
    "value DOUBLE DEFAULT 0 NOT NULL,"
    "date INTEGER DEFAULT 0 NOT NULL,"
    "CONSTRAINT fk_contribution_info_publisher_id"
    "    FOREIGN KEY (publisher_id)"
    "    REFERENCES publisher_info (publisher_id)"
    "    ON DELETE CASCADE)";

  try
  {
    *db_ << sql.str().c_str();
    succeded = true;
  }
  catch (sqlite::sqlite_exception e)
  {
    std::cout << "sqlite_exception:" << e.what() << std::endl;
    succeded = false;
  }

  return succeded;
}

bool PublisherInfoDatabase::CreateContributionInfoIndex() {
  //DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  bool succeded = false;
  try {
    *db_ << "CREATE INDEX IF NOT EXISTS contribution_info_publisher_id_index ON contribution_info (publisher_id);";
    succeded = true;
  }
  catch (sqlite::sqlite_exception e)
  {
    std::cout << "sqlite_exception:" << e.what() << std::endl;
    succeded = false;
  }
  return succeded;
}

bool PublisherInfoDatabase::CreatePublisherInfoTable() {
  //DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  bool succeded = false;
  const char* name = "publisher_info";

  // Update InsertOrUpdatePublisherInfo() if you add anything here
  std::ostringstream sql;
  sql<<"CREATE TABLE IF NOT EXISTS " << name <<
    " (publisher_id LONGVARCHAR PRIMARY KEY NOT NULL UNIQUE,"
    "verified BOOLEAN DEFAULT 0 NOT NULL,"
    "excluded INTEGER DEFAULT 0 NOT NULL,"
    "name TEXT NOT NULL,"
    "favIcon TEXT NOT NULL,"
    "url TEXT NOT NULL,"
    "provider TEXT NOT NULL)";

  try {
    *db_ << sql.str().c_str();
    succeded = true;
  }
  catch (sqlite::sqlite_exception e)
  {
    std::cout << "sqlite_exception: " << e.what() << std::endl;
    succeded = false;
  }
  return succeded;
}


bool PublisherInfoDatabase::CreateActivityInfoTable() {
  //DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  bool succeded = false;
  const char* name = "activity_info";

  // Update InsertOrUpdatePublisherInfo() if you add anything here
  std::ostringstream sql;
  sql << "CREATE TABLE IF NOT EXISTS " << name <<
    " (publisher_id LONGVARCHAR NOT NULL,"
    "duration INTEGER DEFAULT 0 NOT NULL,"
    "score DOUBLE DEFAULT 0 NOT NULL,"
    "percent INTEGER DEFAULT 0 NOT NULL,"
    "weight DOUBLE DEFAULT 0 NOT NULL,"
    "category INTEGER NOT NULL,"
    "month INTEGER NOT NULL,"
    "year INTEGER NOT NULL,"
    "CONSTRAINT fk_activity_info_publisher_id"
    "    FOREIGN KEY (publisher_id)"
    "    REFERENCES publisher_info (publisher_id)"
    "    ON DELETE CASCADE)";

  try {
    *db_ << sql.str().c_str();
    succeded = true;
  }
  catch (sqlite::sqlite_exception e)
  {
    std::cout << "sqlite_exception: " << e.what() << std::endl;
    succeded = false;
  }
  return succeded;
}


bool PublisherInfoDatabase::CreateActivityInfoIndex() {
  //DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  bool succeded = false;
  try {
    *db_ << "CREATE INDEX IF NOT EXISTS activity_info_publisher_id_index "  <<    "ON activity_info (publisher_id)";
    succeded = true;
  }
  catch (sqlite::sqlite_exception e)
  {
    std::cout << "sqlite_exception: " << e.what() << std::endl;
    succeded = false;
  }
  return succeded;
}


bool PublisherInfoDatabase::CreateMediaPublisherInfoTable() {
  //DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  bool succeded = false;
  const char* name = "media_publisher_info";

  // Update InsertOrUpdatePublisherInfo() if you add anything here
  std::ostringstream sql;
  sql << "CREATE TABLE IF NOT EXISTS " << name <<
    " (media_key TEXT NOT NULL PRIMARY KEY UNIQUE,"
    "publisher_id LONGVARCHAR NOT NULL,"
    "CONSTRAINT fk_media_publisher_info_publisher_id"
    "    FOREIGN KEY (publisher_id)"
    "    REFERENCES publisher_info (publisher_id)"
    "    ON DELETE CASCADE)";

  try {
    *db_ << sql.str().c_str();
    succeded = true;
  }
  catch (sqlite::sqlite_exception e)
  {
    std::cout << "sqlite_exception: " << e.what() << std::endl;
    succeded = false;
  }
  return succeded;
}

/////////
bool PublisherInfoDatabase::InsertOrUpdatePublisherInfo(
    const ledger::PublisherInfo& info) {
  //DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  bool succeded = false;
  try
  {
    bool initialized = Init();
    if (!initialized)
      return false;
    {
      auto ps = *db_ <<
        "INSERT OR REPLACE INTO publisher_info "
        "(publisher_id, verified, excluded, "
        "name, url, provider, favIcon) "
        "VALUES (?, ?, ?, ?, ?, ?, ?)";

      ps << info.id;
      ps << info.verified;
      ps << static_cast<int>(info.excluded);
      ps << info.name;
      ps << info.url;
      ps << info.provider;
      ps << info.favicon_url;

      ps.execute();
      ps.used(true); //to execute even if it was used

      if (!info.month || !info.year) {
        return true;
      }
    }

    ///////////////////////////////////////////////////////
    {
      auto ps1 = *db_ << "SELECT publisher_id FROM activity_info WHERE " <<
        "publisher_id=? AND category=? AND month=? AND year=?";

      ps1 << info.id;
      ps1 << info.category;
      ps1 << info.month;
      ps1 << info.year;
      bool row_exixst = false;
      ps1 >> [&]() {
        row_exixst = true;
      };

      ps1++;
      ps1.used(true);

      if (row_exixst)
      {
        auto ps2 = *db_ << "UPDATE activity_info SET " <<
          "duration=?, score=?, percent=?, " <<
          "weight=? WHERE " <<
          "publisher_id=? AND category=? " <<
          "AND month=? AND year=?";

        ps2 << info.duration;
        ps2 << info.score;
        ps2 << info.percent;
        ps2 << info.weight;
        ps2 << info.id;
        ps2 << info.category;
        ps2 << info.month;
        ps2 << info.year;

        ps2.execute();
        ps2.used(true);
        return true;
      }
    }

    ///////////////////////////////////////////////////////

    auto ps3 = *db_ << "INSERT INTO activity_info " <<
      "(publisher_id, duration, score, percent, " <<
      "weight, category, month, year) " <<
      "VALUES (?, ?, ?, ?, ?, ?, ?, ?)";

    ps3 << info.id;
    ps3 << info.duration;
    ps3 << info.score;
    ps3 << info.percent;
    ps3 << info.weight;
    ps3 << info.category;
    ps3 << info.month;
    ps3 << info.year;

    ps3.execute();
    ps3.used(true);

  }
  catch (sqlite::sqlite_exception e)
  {
    std::cout << "Unexpected error " << e.what() << std::endl;
    succeded = false;
  }
  return succeded;
}


bool PublisherInfoDatabase::InsertOrUpdateMediaPublisherInfo(
  const std::string& media_key, const std::string& publisher_id) {
  //DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  bool succeded = false;

  bool initialized = Init();
  if (!initialized)
    return false;

  try
  {
    *db_ << "INSERT OR REPLACE INTO media_publisher_info "
      "(media_key, publisher_id) "
      "VALUES (?, ?)" << media_key << publisher_id;
    succeded = true;
  }
  catch (sqlite::sqlite_exception e)
  {
    std::cout << "sqlite_exception: " << e.what() << std::endl;
    succeded = false;
  }

  return succeded;
}


std::unique_ptr<ledger::PublisherInfo>
PublisherInfoDatabase::GetMediaPublisherInfo(const std::string& media_key) {
  //DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  std::unique_ptr<ledger::PublisherInfo> info;

  bool initialized = Init();
  //DCHECK(initialized);
  if (!initialized)
    return info;

  try {
    *db_ << "SELECT pi.publisher_id, pi.name, pi.url, pi.favIcon "
      "FROM media_publisher_info as mpi "
      "INNER JOIN publisher_info AS pi ON mpi.publisher_id = pi.publisher_id "
      "WHERE mpi.media_key=?" << media_key >>
      [&](std::string _id, std::string _name, std::string _url, std::string _favicon_url) {
      info.reset(new ledger::PublisherInfo());
      info->id = _id;
      info->name = _name;
      info->url = _url;
      info->favicon_url = _favicon_url;
    };
  }
  catch (sqlite::sqlite_exception e)
  {
    std::cout << "sqlite_exception: " << e.what() << std::endl;
  }
  return info;
}



bool PublisherInfoDatabase::Find(int start,
  int limit,
  const ledger::PublisherInfoFilter& filter,
  ledger::PublisherInfoList* list) {
  //DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  bool succeded = false;

  if (list == nullptr) {
    return false;
  }

  bool initialized = Init();

  if (!initialized)
    return false;

  std::ostringstream query;
  query<< "SELECT ai.publisher_id, ai.duration, ai.score, ai.percent, "
      "ai.weight, pi.verified, pi.excluded, ai.category, ai.month, ai.year, pi.name, "
      "pi.url, pi.provider, pi.favIcon "
      "FROM activity_info AS ai "
      "INNER JOIN publisher_info AS pi ON ai.publisher_id = pi.publisher_id "
    "WHERE 1 = 1";

  if (!filter.id.empty()) {
    query << " AND ai.publisher_id = \'" << filter.id << "\'";
  }

  if (filter.category != ledger::PUBLISHER_CATEGORY::ALL_CATEGORIES) {
    query << " AND ai.category = ";
    query << filter.category;
  }

  if (filter.month != ledger::PUBLISHER_MONTH::ANY) {
    query << " AND ai.month = ";
    query << filter.month;
  }

  if (filter.year > 0) {
    query << " AND ai.year = ";
    query << filter.year;
  }

  for (const auto& it : filter.order_by) {
    query << " ORDER BY " << it.first;
    query << (it.second ? " ASC" : " DESC");
  }

  if (limit > 0)
  {
    query << " LIMIT " << limit;

    if (start > 1)
      query << " OFFSET " << start;
  }

  try
  {
    *db_ << query.str().c_str() >>
      [&](std::string _id, uint64_t _duration, double _score, uint64_t _percent, double _weight,
        bool _verified, int _excluded, int _category, int _month, int _year, std::string _name, std::string _url, std::string _provider, std::string _favurl) {

      ledger::PUBLISHER_MONTH month(static_cast<ledger::PUBLISHER_MONTH>(_month));
      ledger::PublisherInfo info(_id, month, _year);

      info.duration = _duration;
      info.score = _score;
      info.percent = _percent;
      info.weight = _weight;
      info.verified = _verified;
      info.excluded = static_cast<ledger::PUBLISHER_EXCLUDE>(_excluded);
      info.category = static_cast<ledger::PUBLISHER_CATEGORY>(_category);
      info.name = _name;
      info.url = _url;
      info.provider = _provider;
      info.favicon_url = _favurl;

      list->push_back(info);
    };

    succeded = true;
  }
  catch (sqlite::sqlite_exception e)
  {
    std::cout << "Unexpected error " << e.what() << std::endl;
    succeded = false;
  }

  return succeded;
}

// static
int PublisherInfoDatabase::GetCurrentVersion() {
  return kCurrentVersionNumber;
}


// Migration -------------------------------------------------------------------

bool PublisherInfoDatabase::EnsureCurrentVersion() {
  //DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  // We can't read databases newer than we were designed for.
  bool succeded = true;
  int last_compatible_version = 0;

  try {
    *db_ << "SELECT value FROM meta WHERE key=?" << "last_compatible_version" >> last_compatible_version;

    if (last_compatible_version > kCurrentVersionNumber) {
      std::cout << "Publisher info database is too new." << std::endl;
      succeded = false;
    }

    int version = 0;
    *db_ << "SELECT value FROM meta WHERE key=?" << "version" >> version;
  }
  catch (sqlite::sqlite_exception e)
  {
    std::cout << "Unexpected error " << e.what() << std::endl;
    succeded = false;
  }


  // Put migration code here
  // When the version is too old, we just try to continue anyway, there should
  // not be a released product that makes a database too old for us to handle.
  //LOG_IF(WARNING, cur_version < GetCurrentVersion()) <<
 //        "History database version " << cur_version << " is too old to handle.";

  return succeded;
}

}  // namespace brave_rewards