/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "stdafx.h"
#include "media_publisher_info_backend.h"
#include "bat_helper_platform.h"
#include "leveldb/db.h"
#include <boost/filesystem.hpp>

/*
#include "base/files/file_util.h"
#include "third_party/leveldatabase/env_chromium.h"
#include "third_party/leveldatabase/src/include/leveldb/db.h"
#include "third_party/leveldatabase/src/include/leveldb/iterator.h"
#include "third_party/leveldatabase/src/include/leveldb/options.h"
#include "third_party/leveldatabase/src/include/leveldb/status.h"
*/

namespace brave_rewards {

MediaPublisherInfoBackend::MediaPublisherInfoBackend(const std::string & path) :
    path_(path) {
  //DETACH_FROM_SEQUENCE(sequence_checker_);
}

MediaPublisherInfoBackend::~MediaPublisherInfoBackend() {}

bool MediaPublisherInfoBackend::Put(const std::string& key,
                               const std::string& value) {
  //DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  bool initialized = EnsureInitialized();
  DCHECK(initialized);

  if (!initialized)
    return false;

  leveldb::WriteOptions options;
  LOG(ERROR) << "!!!MediaPublisherInfoBackend::Put";
  leveldb::Status status = db_->Put(options, key, value);
  if (status.ok())
    return true;

  return false;
}

bool MediaPublisherInfoBackend::Get(const std::string& lookup,
                               std::string* value) {
  LOG(ERROR) << "!!!MediaPublisherInfoBackend::Get";
  //DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  bool initialized = EnsureInitialized();
  DCHECK(initialized);

  if (!initialized)
    return false;

  leveldb::ReadOptions options;
  leveldb::Status status = db_->Get(options, lookup, value);
  if (status.ok())
    return true;

  return false;
}

bool MediaPublisherInfoBackend::EnsureInitialized() {
  //DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  if (db_.get())
    return true;

  leveldb::Options options;
  options.create_if_missing = true;
  std::string path = path_;
  leveldb::DB * db_ptr = nullptr;
  leveldb::Status status = leveldb::DB::Open(options, path, &db_ptr);

  if (status.IsCorruption()) {
    LOG(ERROR) << "Deleting corrupt database";
    //base::DeleteFile(path_, true);
    boost::filesystem::remove_all(path_);
    status = leveldb::DB::Open(options, path, &db_ptr);
  }
  if (status.ok()) {
    db_.reset(db_ptr);
    //CHECK(db_);
    return true;
  }
  LOG(WARNING) << "Unable to open " << path << ": "
    << status.ToString();
  return false;
}

}  // namespace brave_rewards
