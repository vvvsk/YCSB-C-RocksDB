//
//  rocksdb_db.cc
//  YCSB-C
//

#include "rocksdb_db.h"
#include "rocksdb/cache.h"
#include "rocksdb/filter_policy.h"
#include "rocksdb/flush_block_policy.h"
#include "utils/rocksdb_config.h"
#include "rocksdb/rate_limiter.h"
#include <cstdint>
#include <iostream>
#include <memory>
#include <string>

using namespace std;

namespace ycsbc {
RocksDB::RocksDB(const char *dbPath, 
const string dbConfig) : 
  noResult(0),
  disableWAL_(false),
  write_sync_(false),
  statistics(nullptr){

  name_ = "rocksdb";
  ConfigRocksDB config;
  if (dbConfig.empty()) {
    config.init("./RocksDBConfig/rocksdb_config.ini");
  } else {
    config.init(dbConfig);
  }

  rocksdb::Options options;
  
  SetOptions(&options,config);
  //开启profiling
  // cout<<"开启profiling"<<endl;
  // rocksdb::SetPerfLevel(rocksdb::PerfLevel::kEnableTimeExceptForMutex);
  // rocksdb::get_perf_context()->Reset();
  // rocksdb::get_iostats_context()->Reset();

   rocksdb::Status s = rocksdb::DB::Open(options, dbPath, &db_);
  if (!s.ok()) {
    cerr << "Can't open rocksdb " << dbPath << endl;
    exit(0);
  }

}

int RocksDB::Read(const std::string &table, const std::string &key,
                  const std::vector<std::string> *fields,
                  std::vector<KVPair> &result) {
  string value;
  rocksdb::Status s = db_->Get(rocksdb::ReadOptions(), key, &value);
  if (s.ok())
    return DB::kOK;
  if (s.IsNotFound()) {
    noResult++;
    // cout << noResult << "not found" << endl;
    return DB::kOK;
  } else {
    // cerr << "read error" << endl;
    exit(0);
  }
}

int RocksDB::Scan(const std::string &table, const std::string &key, int len,
                  const std::vector<std::string> *fields,
                  std::vector<std::vector<KVPair>> &result) {
  auto it = db_->NewIterator(rocksdb::ReadOptions());
  it->Seek(key);
  std::string val;
  std::string k;
  for (int i = 0; i < len && it->Valid(); i++) {
    k = it->key().ToString();
    val = it->value().ToString();
    it->Next();
  }
  delete it;
  return DB::kOK;
}

int RocksDB::Insert(const std::string &table, const std::string &key,
                    std::vector<KVPair> &values) {
  rocksdb::Status s;
  auto wo = rocksdb::WriteOptions();
  wo.sync = write_sync_; // 是否开启写同步
  wo.disableWAL = disableWAL_;
  for (KVPair &p : values) {
    s = db_->Put(wo, key, p.second);
    if (!s.ok()) {
      cerr << "insert error" << s.ToString() << "\n" << endl;
      exit(0);
    }
  }
  return DB::kOK;
}

int RocksDB::Update(const std::string &table, const std::string &key,
                    std::vector<KVPair> &values) {
  return Insert(table, key, values);
}

int RocksDB::Delete(const std::string &table, const std::string &key) {
   rocksdb::Status s;
  rocksdb::WriteOptions write_options = rocksdb::WriteOptions();
  if(write_sync_) {
      write_options.sync = true;
  }
  s = db_->Delete(write_options,key);
  if(!s.ok()){
      cerr<<"Delete error\n"<<endl;
      exit(0);
  }
  return DB::kOK;
}

void RocksDB::printStats() {
  string stats;
  db_->GetProperty("rocksdb.stats", &stats);
  cout << stats << endl;
  cout << "=========================== options.statistics=============================="<< endl;
  cout << statistics->ToString() << endl;

  // //输出perf & io
  // std::cout << "===================== perf/IO "
  //              "statistics================================"
  //           << std::endl;
  // std::cout << rocksdb::get_perf_context()->ToString() << std::endl;
  // std::cout << rocksdb::get_iostats_context()->ToString() << std::endl;
  // //关闭profiling
  // rocksdb::SetPerfLevel(rocksdb::PerfLevel::kDisable);
}

void EnablePerf() {
  rocksdb::SetPerfLevel(rocksdb::PerfLevel::kEnableTimeExceptForMutex);
  rocksdb::get_perf_context()->Reset();
  rocksdb::get_iostats_context()->Reset();
}

void DisablePerf() { rocksdb::SetPerfLevel(rocksdb::PerfLevel::kDisable); }

RocksDB::~RocksDB() { delete db_; }


void RocksDB::SetOptions(rocksdb::Options *options, ConfigRocksDB &config){
  rocksdb::BlockBasedTableOptions bbto;

  disableWAL_ = config.getDisableWAL();
  write_sync_ = config.getWriteSync();
  int bloomBits = config.getBloomBits();
  size_t blockCache = config.getBlockCache();
  bool compression = config.getCompression();
  bool directIO = config.getDirectIO();
  size_t memtable = config.getMemtable();

  //add by wzp
  bool rate_limit_bg_reads = config.getRateLimitBGReads();
  bool rate_limiter_auto_tuned = config.getRateLimiterAutoTune();
  uint64_t rate_limiter_bytes_per_sec = config.getRateLimiterBytesPerSec();
  uint64_t rate_limiter_refill_period_us = config.getRateLimiterRefillPeriodUS();
  // set optionssc
  
  // options.db_paths = {{"/mnt/vol1",
  //                      (uint64_t)1 * 1024 * 1024 * 1024},
  //                     {"/mnt/vol2",
  //                      (uint64_t)3 * 1024 * 1024 * 1024},
  //                     {"/mnt/vol3",
  //                      (uint64_t)300 * 1024 * 1024 * 1024}};
  options->statistics = rocksdb::CreateDBStatistics();
  statistics = options->statistics;
  options->create_if_missing = true;
  options->write_buffer_size = memtable;
  options->target_file_size_base = 64 << 20;    // 64MB
  options->use_direct_io_for_flush_and_compaction = directIO;
  options->max_write_buffer_number = config.getMemtableNum(); // 2个imm
  options->force_consistency_checks = false; // 一致性检查 零时关闭
  // options.compaction_pri = rocksdb::kMinOverlappingRatio;
  if (config.getTiered()) {
    options->compaction_style = rocksdb::kCompactionStyleUniversal;
  }
  
  options->max_background_jobs = config.getNumThreads();
  options->disable_auto_compactions = config.getNoCompaction(); //不要触发自动compaction 为假，那么就是要做自动做compaction

  if (rate_limiter_bytes_per_sec > 0) {
      options->rate_limiter.reset(rocksdb::NewGenericRateLimiter(
          rate_limiter_bytes_per_sec, rate_limiter_refill_period_us,
          10 /* fairness */,
          //rocksdb::RateLimiter::Mode::kAllIo
          rate_limit_bg_reads ? rocksdb::RateLimiter::Mode::kReadsOnly
                                    : rocksdb::RateLimiter::Mode::kWritesOnly,
          rate_limiter_auto_tuned));
  }
  if (!compression)
    options->compression = rocksdb::kNoCompression;
  else{
    options->compression = rocksdb::kSnappyCompression;
  }

  
  if (bloomBits > 0) {
    bbto.filter_policy.reset(rocksdb::NewBloomFilterPolicy(bloomBits));
  }
  bbto.block_cache = rocksdb::NewLRUCache(blockCache);
  options->table_factory.reset(rocksdb::NewBlockBasedTableFactory(bbto));

 


}


} // namespace ycsbc
