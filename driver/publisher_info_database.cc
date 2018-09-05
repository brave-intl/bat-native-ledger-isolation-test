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

    //create meta table
    *db_ << "create table if not exists meta (key longvarchar not null unique primary key, value longvarchar);";
    *db_ << "insert or replace into meta (key,value) values (?,?)" << "version" << GetCurrentVersion();
    *db_ << "insert or replace into meta (key,value) values (?,?)" << "last_compatible_version" << kCompatibleVersionNumber;

    if (!CreatePublisherInfoTable() || !CreateContributionInfoTable())
      return false;

    CreateContributionInfoIndex();

    // Version check.
    bool version_status = EnsureCurrentVersion();
    if (version_status != true)
      return version_status;

    *db_ << "commit;"; // commit all the changes.

                        /*
    memory_pressure_listener_.reset(new base::MemoryPressureListener(
        base::Bind(&PublisherInfoDatabase::OnMemoryPressure,
        base::Unretained(this))));
    */

    initialized_ = true;
    return initialized_;
  }
  catch (sqlite::sqlite_exception e)
  {
    std::cout << "Unexpected error " << e.what() << std::endl;
    initialized_ = false;
    if (transactionStarted) {
      *db_ << "rollback;"; // commit all the changes.
    }
  }
  catch (...)
  {
    std::cout << "Unknown error\n";
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
  std::string sql;
  sql.append("create table if not exists ");
  sql.append(name);
  sql.append(
    "("
    "publisher_id LONGVARCHAR NOT NULL,"
    "value DOUBLE DEFAULT 0 NOT NULL,"
    "date INTEGER DEFAULT 0 NOT NULL);");

  try
  {
    *db_ << sql.c_str();
    succeded = true;
  }
  catch (sqlite::sqlite_exception e)
  {
    std::cout << "Unexpected error " << e.what() << std::endl;
    succeded = false;
  }

  return succeded;
}

bool PublisherInfoDatabase::CreateContributionInfoIndex() {
  //DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  bool succeded = false;
  try {
    *db_ << "CREATE INDEX IF NOT EXISTS contribution_info_publisher_id_index " << "ON contribution_info (publisher_id)";
    succeded = true;
  }
  catch (sqlite::sqlite_exception e)
  {
    std::cout << "Unexpected error " << e.what() << std::endl;
    succeded = false;
  }
  return succeded;
}

bool PublisherInfoDatabase::CreatePublisherInfoTable() {
  //DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  bool succeded = false;
  const char* name = "publisher_info";

  // Update InsertOrUpdatePublisherInfo() if you add anything here
  std::string sql;
  sql.append("CREATE TABLE IF NOT EXISTS ");
  sql.append(name);
  sql.append(
      "("
      "id LONGVARCHAR PRIMARY KEY,"
      "duration INTEGER DEFAULT 0 NOT NULL,"
      "score DOUBLE DEFAULT 0 NOT NULL,"
      "pinned BOOLEAN DEFAULT 0 NOT NULL,"
      "percent INTEGER DEFAULT 0 NOT NULL,"
      "weight DOUBLE DEFAULT 0 NOT NULL,"
      "excluded BOOLEAN DEFAULT 0 NOT NULL,"
      "category INTEGER NOT NULL,"
      "month INTEGER NOT NULL,"
      "year INTEGER NOT NULL,"
      "favIconURL LONGVARCHAR DEFAULT '' NOT NULL)");

  try {
    *db_ << sql.c_str();
    succeded = true;
  }
  catch (sqlite::sqlite_exception e)
  {
    std::cout << "Unexpected error " << e.what() << std::endl;
    succeded = false;
  }
  return succeded;
}

bool PublisherInfoDatabase::InsertOrUpdatePublisherInfo(
    const ledger::PublisherInfo& info) {
  //DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  bool succeded = false;
  try
  {
    bool initialized = Init();
    if (!initialized)
      return false;

    auto ps = *db_ <<
      "INSERT OR REPLACE INTO publisher_info "
      "(id, duration, score, pinned, percent, "
      "weight, excluded, category, month, year, favIconURL) "
      "VALUES (?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?);";

    ps << info.id;
    ps << (int)info.duration;
    ps << info.score;
    ps << info.pinned;
    ps << (int)info.percent;
    ps << info.weight;
    ps << info.excluded;
    ps << info.category;
    ps << info.month;
    ps << info.year;
    ps << info.favIconURL;

    ps.execute();
    ps.used(false); //to execute even if it was used
    succeded = true;
  }
  catch (sqlite::sqlite_exception e)
  {
    std::cout << "Unexpected error " << e.what() << std::endl;
    succeded = false;
  }
  return succeded;
}

bool PublisherInfoDatabase::Find(int start,
  int limit,
  const ledger::PublisherInfoFilter& filter,
  ledger::PublisherInfoList* list) {
  //DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  if (list == nullptr) {
    return false;
  }

  bool initialized = Init();

  if (!initialized)
    return false;

  std::ostringstream query;
  query<< "SELECT id, duration, score, pinned, percent, "<<
    "weight, excluded, category, month, year, favIconURL "<<
    "FROM publisher_info "<<
    "WHERE 1 = 1";

  if (!filter.id.empty()) {
    query << " AND id = ";
    query << filter.id;
  }

  if (filter.category != ledger::PUBLISHER_CATEGORY::ALL_CATEGORIES) {
    query << " AND category = ";
    query << filter.category;
  }

  if (filter.month != ledger::PUBLISHER_MONTH::ANY) {
    query << " AND month = ";
    query << filter.month;
  }

  if (filter.year > 0) {
    query << " AND year = ";
    query << filter.year;
  }

  if (start > 1)
    query << " OFFSET " << std::to_string(start);

  if (limit > 0)
    query << " LIMIT " + std::to_string(limit);

  for (const auto& it : filter.order_by) {
    query << " ORDER BY " << it.first;
    query << (it.second ? "ASC" : "DESC");
  }

  try
  {
    *db_ << query.str().c_str() >>
      [&](std::string _id, uint64_t _duration, double _score, bool _pinned, uint64_t _percent, double _weight,
        bool _excluded, int _category, int _month, int _year, std::string _favurl) {
      ledger::PUBLISHER_MONTH month(static_cast<ledger::PUBLISHER_MONTH>(_month));
      ledger::PublisherInfo info(_id, month, _year);
      info.duration = _duration;
      info.score = _score;
      info.pinned = _pinned;
      info.percent = _percent;
      info.weight = _weight;
      info.excluded = _excluded;
      info.category = static_cast<ledger::PUBLISHER_CATEGORY>(_category);
      info.favIconURL = _favurl;
      list->push_back(info);
    };
  }
  catch (sqlite::sqlite_exception e)
  {
    std::cout << "Unexpected error " << e.what() << std::endl;
  }

  return list;
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