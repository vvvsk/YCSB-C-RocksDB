//  Copyright (c) 2017-present, Facebook, Inc.  All rights reserved.
//  This source code is licensed under both the GPLv2 (found in the
//  COPYING file in the root directory) and Apache 2.0 License
//  (found in the LICENSE.Apache file in the root directory).

#pragma once

#ifndef ROCKSDB_LITE

#include "rocksdb/db.h"
#include "rocksdb/types.h"

namespace ROCKSDB_NAMESPACE {

// Data associated with a particular version of a key. A database may internally
// store multiple versions of a same user key due to snapshots, compaction not
// happening yet, etc.

/*
 * 与键的特定版本相关联的数据。
 * 由于快照、压缩尚未发生等原因，数据库可能在内部存储同一个用户键的多个版本。
 *
 * */
struct KeyVersion {
  KeyVersion() : user_key(""), value(""), sequence(0), type(0) {}

  KeyVersion(const std::string& _user_key, const std::string& _value,
             SequenceNumber _sequence, int _type)
      : user_key(_user_key), value(_value), sequence(_sequence), type(_type) {}

  std::string user_key;
  std::string value;
  SequenceNumber sequence;
  // TODO(ajkr): we should provide a helper function that converts the int to a
  // string describing the type for easier debugging.
  int type;
};

// Returns listing of all versions of keys in the provided user key range.
// The range is inclusive-inclusive, i.e., [`begin_key`, `end_key`], or
// `max_num_ikeys` has been reached. Since all those keys returned will be
// copied to memory, if the range covers too many keys, the memory usage
// may be huge. `max_num_ikeys` can be used to cap the memory usage.
// The result is inserted into the provided vector, `key_versions`.
/*
 * 返回所提供的用户键范围内所有版本的键的列表。
 * 范围是包含-包含的，即[' begin_key '， ' end_key ']，或' max_num_ikeys '已达到。
 * 由于所有返回的键都将被复制到内存中，如果范围覆盖了太多的键，内存使用可能会非常大。
 * ' max_num_ikeys '可以用来限制内存使用。
 * 结果被插入到提供的vector ' key_versions '中。
 * */
Status GetAllKeyVersions(DB* db, Slice begin_key, Slice end_key,
                         size_t max_num_ikeys,
                         std::vector<KeyVersion>* key_versions);

Status GetAllKeyVersions(DB* db, ColumnFamilyHandle* cfh, Slice begin_key,
                         Slice end_key, size_t max_num_ikeys,
                         std::vector<KeyVersion>* key_versions);

}  // namespace ROCKSDB_NAMESPACE

#endif  // ROCKSDB_LITE
