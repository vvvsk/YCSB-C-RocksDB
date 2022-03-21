// Copyright (c) 2011-present, Facebook, Inc.  All rights reserved.
//  This source code is licensed under both the GPLv2 (found in the
//  COPYING file in the root directory) and Apache 2.0 License
//  (found in the LICENSE.Apache file in the root directory).

#ifndef ROCKSDB_LITE

#pragma once

#include <map>
#include <string>
#include <unordered_set>
#include <vector>

#include "rocksdb/cache.h"
#include "rocksdb/db.h"

namespace ROCKSDB_NAMESPACE {

// Returns the current memory usage of the specified DB instances.
// 返回当前DB实例的内存使用情况。
class MemoryUtil {
 public:
  enum UsageType : int {
    // Memory usage of all the mem-tables.
    // 所有的memtable的内存使用
    kMemTableTotal = 0,
    // Memory usage of those un-flushed mem-tables.
    // 未 flush 的memtables
    kMemTableUnFlushed = 1,
    // Memory usage of all the table readers.
    // tabler 读的内存使用
    kTableReadersTotal = 2,
    // Memory usage by Cache.
    kCacheTotal = 3,
    kNumUsageTypes = 4
  };

  // Returns the approximate memory usage of different types in the input
  // list of DBs and Cache set.  For instance, in the output map
  // usage_by_type, usage_by_type[kMemTableTotal] will store the memory
  // usage of all the mem-tables from all the input rocksdb instances.
  //
  // Note that for memory usage inside Cache class, we will
  // only report the usage of the input "cache_set" without
  // including those Cache usage inside the input list "dbs"
  // of DBs.

  /*
   * 返回数据库和cache set的输入列表中不同类型的内存使用情况。
   * 例如，在输出映射usage_by_type中，usage_by_type[kMemTableTotal]
   * 将存储来自输入实例rocksdb的所有memtables的内存使用情况注意，
   * 对于Cache类内部的内存使用情况，
   * 我们将只报告输入的"cache_set"的使用情况，而不包括那些输入列表"dbs"中的缓存使用情况。
   *
   * */
  static Status GetApproximateMemoryUsageByType(
      const std::vector<DB*>& dbs,
      const std::unordered_set<const Cache*> cache_set,
      std::map<MemoryUtil::UsageType, uint64_t>* usage_by_type);
};
}  // namespace ROCKSDB_NAMESPACE
#endif  // !ROCKSDB_LITE
