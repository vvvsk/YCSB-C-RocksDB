#include "core/client.h"
#include "core/core_workload.h"
#include "core/timer.h"
#include "core/utils.h"
#include "db/db_factory.h"
#include "rocksdb/iostats_context.h"
#include "rocksdb/perf_context.h"
#include "utils/histogram.h"
#include <chrono>
#include <cstring>
#include <future>
#include <iostream>
#include <memory>
#include <string>
#include <thread>
#include <vector>

using namespace std;

////statistics
atomic<uint64_t> ops_cnt[ycsbc::Operation::READMODIFYWRITE + 1];    //操作个数
atomic<uint64_t> ops_time[ycsbc::Operation::READMODIFYWRITE + 1];   //微秒
////

#define INTERVAL_OPS 10000

void UsageMessage(const char *command);
bool StrStartWith(const char *str, const char *pre);
string ParseCommandLine(int argc, const char *argv[], utils::Properties &props);

void mergePerf(shared_ptr<rocksdb::PerfContext> a,
               shared_ptr<rocksdb::PerfContext> b) {
  a->write_memtable_time += b->write_memtable_time;
  a->write_wal_time += b->write_wal_time;
  a->write_pre_and_post_process_time += b->write_pre_and_post_process_time;
  a->write_scheduling_flushes_compactions_time +=
      b->write_scheduling_flushes_compactions_time;
  a->write_delay_time += b->write_delay_time;
  a->write_thread_wait_nanos += b->write_thread_wait_nanos;
}

int record(shared_ptr<ycsbc::DB> db, atomic<bool> *isFinish, int duration) {
  while (!(*isFinish)) {
    std::this_thread::sleep_for(std::chrono::seconds(duration)); // sleep 10s
    db->printStats();
  }
  return 0;
}
int DelegateClient(shared_ptr<ycsbc::DB> db, ycsbc::CoreWorkload *wl,
                   const int num_ops, bool is_loading,
                   shared_ptr<ycsbc::Histogram> his,
                   shared_ptr<rocksdb::PerfContext> perf_Context,
                   shared_ptr<rocksdb::IOStatsContext> iostats_Context) {
  db->Init();
  //db->EnablePerf();
  if (db->getName() == "rocksdb") {
    rocksdb::SetPerfLevel(rocksdb::PerfLevel::kEnableTimeExceptForMutex);
    rocksdb::get_perf_context()->Reset();
    rocksdb::get_iostats_context()->Reset();
  }

  ycsbc::Client client(*db, *wl);
  utils::Timer<utils::t_microseconds> timer1;
  utils::Timer<utils::t_microseconds> timer2;
  int oks = 0;
  for (int i = 0; i < num_ops; ++i) {
    //可以在这里统计完成一定量的ops花了多少时间
    if (i==0) {
        timer1.Start();
    }
    else if(i%INTERVAL_OPS==0){
      auto duration = timer1.End();
      fprintf(stderr, "... finished %d ops take :%7.5f s \n",INTERVAL_OPS,duration*1e-6);
      fflush(stderr);
      timer1.Start();
    }

    timer2.Start();
    //根据 is_loading 选择载入数据还是trancs
    if (is_loading) {
      oks += client.DoInsert();
    } else {
      oks += client.DoTransaction();
    }
    double duration = timer2.End(); //每一次操作花费时间
    his->AddFast(duration);
  }
  if (db->getName() == "rocksdb") {
    db->DisablePerf();
    *perf_Context = *rocksdb::get_perf_context();
    *iostats_Context = *rocksdb::get_iostats_context();
  }
  db->Close();
  return oks;
}

int main(const int argc, const char *argv[]) {
  utils::Properties props;
  string file_name = ParseCommandLine(argc, argv, props);

  shared_ptr<ycsbc::DB> db = ycsbc::DBFactory::CreateDB(props);
  if (!db) {
    cout << "Unknown database name " << props["dbname"] << endl;
    exit(0);
  }

  ycsbc::CoreWorkload wl;
  wl.Init(props);

  const int num_threads = stoi(props.GetProperty("threadcount", "1"));
  atomic<bool> *isFinish = new atomic<bool>(false);

  vector<future<int>> actual_ops;
  vector<shared_ptr<ycsbc::Histogram>> histograms;
  vector<shared_ptr<rocksdb::PerfContext>> perfs;
  vector<shared_ptr<rocksdb::IOStatsContext>> ioStatss;
  utils::Timer<utils::t_microseconds> timer;

  string pattern = props.GetProperty("pattern");

  int duration = stoi(props.GetProperty("duration", "0"));
  if (duration) {
    auto r = async(launch::async, record, db, isFinish, duration);
  }
  // thread recordT(record,isFinish);
  //recordT.detach();

  // Loads data
  if (pattern == "load" || pattern == "both") {
    // 载入的recordcount
    uint64_t total_ops = stoi(props[ycsbc::CoreWorkload::RECORD_COUNT_PROPERTY]);
    timer.Start();
    // 插入数据开始

    for (int i = 0; i < num_threads; ++i) {
      shared_ptr<ycsbc::Histogram> his = make_shared<ycsbc::Histogram>();
      his->Clear();
      histograms.emplace_back(his);
      if (props["dbname"] == "rocksdb") {
        shared_ptr<rocksdb::PerfContext> perf_Context = make_shared<rocksdb::PerfContext>();
        shared_ptr<rocksdb::IOStatsContext> iostats_Context = make_shared<rocksdb::IOStatsContext>();
        perfs.emplace_back(perf_Context);
        ioStatss.emplace_back(iostats_Context);
        actual_ops.emplace_back(async(launch::async, DelegateClient, db, &wl,
                                      total_ops / num_threads, true, his,
                                      perf_Context, iostats_Context));
      } else {
        actual_ops.emplace_back(async(launch::async, DelegateClient, db, &wl,
                                      total_ops / num_threads, true, his,
                                      nullptr, nullptr));
      }
    }
    // 插入数据结束
    assert((int)actual_ops.size() == num_threads);

    int sum = 0;
    for (auto &n : actual_ops) {
      assert(n.valid());
      sum += n.get();
    }
    *isFinish = true; // finish tasks
    // r.get();
    delete isFinish;

    auto last_time = timer.End(); // last 是毫秒
    for (int i = 1; i < num_threads; i++) {
      histograms[0]->Merge(*histograms[i]);
    }
    printf("********** load result **********\n");
    printf("loading records:%d  use time:%.3f s  IOPS:%.2f iops (%.2f us/op)\n", 
    sum, 1.0 * last_time*1e-6, 1.0 * sum * 1e6 / last_time, 1.0 * last_time / sum);
    printf("*********************************\n");


    cerr << histograms[0]->ToString() << endl;

    if (props["dbname"] == "rocksdb" &&props.CounProperty("stat")) {
      shared_ptr<rocksdb::PerfContext> perf_Context =make_shared<rocksdb::PerfContext>();
      perf_Context->Reset();
      for (int i = 0; i < num_threads; i++) {
        mergePerf(perf_Context, perfs[i]);
      }
      cerr << "============================ DB statistics==========================="<< endl;
      db->printStats();
      cerr << "============================ DB perf/io statistics==========================="<< endl;
      cerr << perf_Context->ToString() << endl;
    }
  }

  // Peforms transactions
  if (pattern == "run" || pattern == "both") {

    for(int j = 0; j < ycsbc::Operation::READMODIFYWRITE + 1; j++){
      ops_cnt[j].store(0);
      ops_time[j].store(0);
    }



    actual_ops.clear();
    histograms.clear();
    perfs.clear();
    ioStatss.clear();
    int total_ops = stoi(props[ycsbc::CoreWorkload::OPERATION_COUNT_PROPERTY]);
    
    
    timer.Start();
    for (int i = 0; i < num_threads; ++i) {
      shared_ptr<ycsbc::Histogram> his = make_shared<ycsbc::Histogram>();
      his->Clear();
      histograms.emplace_back(his);
      if (props["dbname"] == "rocksdb") {
        shared_ptr<rocksdb::PerfContext> perf_Context =
            make_shared<rocksdb::PerfContext>();
        shared_ptr<rocksdb::IOStatsContext> iostats_Context =
            make_shared<rocksdb::IOStatsContext>();

        perfs.emplace_back(perf_Context);

        actual_ops.emplace_back(async(launch::async, DelegateClient, db, &wl,
                                      total_ops / num_threads, false, his,
                                      perf_Context, iostats_Context));
      } else {
        actual_ops.emplace_back(async(launch::async, DelegateClient, db, &wl,
                                      total_ops / num_threads, false, his,
                                      nullptr, nullptr));
      }
    }

    assert((int)actual_ops.size() == num_threads);

    int sum = 0;
    for (auto &n : actual_ops) {
      assert(n.valid());
      sum += n.get();
    }
    *isFinish = true; // finish tasks
    delete isFinish;
    auto last_time = timer.End();


    uint64_t temp_cnt[ycsbc::Operation::READMODIFYWRITE + 1];
    uint64_t temp_time[ycsbc::Operation::READMODIFYWRITE + 1];

    for(int j = 0; j < ycsbc::Operation::READMODIFYWRITE + 1; j++){
      temp_cnt[j] = ops_cnt[j].load(std::memory_order_relaxed);
      temp_time[j] = ops_time[j].load(std::memory_order_relaxed);
    }


    printf("********** run result **********\n");
    printf("all opeartion records:%d  use time:%.3f s  IOPS:%.2f iops (%.2f us/op)\n\n", sum, 1.0 * last_time*1e-6, 1.0 * sum * 1e6 / last_time, 1.0 * last_time / sum);
    if ( temp_cnt[ycsbc::INSERT] )          printf("insert ops:%7lu  use time:%7.3f s  IOPS:%7.2f iops (%.2f us/op)\n", temp_cnt[ycsbc::INSERT], 1.0 * temp_time[ycsbc::INSERT]*1e-6, 1.0 * temp_cnt[ycsbc::INSERT] * 1e6 / temp_time[ycsbc::INSERT], 1.0 * temp_time[ycsbc::INSERT] / temp_cnt[ycsbc::INSERT]);
    if ( temp_cnt[ycsbc::READ] )            printf("read ops  :%7lu  use time:%7.3f s  IOPS:%7.2f iops (%.2f us/op)\n", temp_cnt[ycsbc::READ], 1.0 * temp_time[ycsbc::READ]*1e-6, 1.0 * temp_cnt[ycsbc::READ] * 1e6 / temp_time[ycsbc::READ], 1.0 * temp_time[ycsbc::READ] / temp_cnt[ycsbc::READ]);
    if ( temp_cnt[ycsbc::UPDATE] )          printf("update ops:%7lu  use time:%7.3f s  IOPS:%7.2f iops (%.2f us/op)\n", temp_cnt[ycsbc::UPDATE], 1.0 * temp_time[ycsbc::UPDATE]*1e-6, 1.0 * temp_cnt[ycsbc::UPDATE] * 1e6 / temp_time[ycsbc::UPDATE], 1.0 * temp_time[ycsbc::UPDATE] / temp_cnt[ycsbc::UPDATE]);
    if ( temp_cnt[ycsbc::SCAN] )            printf("scan ops  :%7lu  use time:%7.3f s  IOPS:%7.2f iops (%.2f us/op)\n", temp_cnt[ycsbc::SCAN], 1.0 * temp_time[ycsbc::SCAN]*1e-6, 1.0 * temp_cnt[ycsbc::SCAN] * 1e6 / temp_time[ycsbc::SCAN], 1.0 * temp_time[ycsbc::SCAN] / temp_cnt[ycsbc::SCAN]);
    if ( temp_cnt[ycsbc::READMODIFYWRITE] ) printf("rmw ops   :%7lu  use time:%7.3f s  IOPS:%7.2f iops (%.2f us/op)\n", temp_cnt[ycsbc::READMODIFYWRITE], 1.0 * temp_time[ycsbc::READMODIFYWRITE]*1e-6, 1.0 * temp_cnt[ycsbc::READMODIFYWRITE] * 1e6 / temp_time[ycsbc::READMODIFYWRITE], 1.0 * temp_time[ycsbc::READMODIFYWRITE] / temp_cnt[ycsbc::READMODIFYWRITE]);
    printf("********************************\n");



    for (int i = 1; i < num_threads; i++) {
      histograms[0]->Merge(*histograms[i]);
    }
    cerr << histograms[0]->ToString() << endl;
    if (props["dbname"] == "rocksdb" &&props.CounProperty("stat")) {
      cout<<"dasdasdaaaaaaaaaaaa";
      shared_ptr<rocksdb::PerfContext> perf_Context =
          make_shared<rocksdb::PerfContext>();
      perf_Context->Reset();
      for (int i = 0; i < num_threads; i++) {
        mergePerf(perf_Context, perfs[i]);
      }
      cerr<< "============================statistics==========================="<< endl;
      db->printStats();

      cerr << "============================ perf/io statistics==========================="
           << endl;
      cerr << perf_Context->ToString() << endl;
    }
  }
}

string ParseCommandLine(int argc, const char *argv[],
                        utils::Properties &props) {
  int argindex = 1;
  string filename;
  while (argindex < argc) {
    if (strcmp(argv[argindex], "-threads") == 0) {
      argindex++;
      if (argindex >= argc) {
        UsageMessage(argv[0]);
        exit(0);
      }
      props.SetProperty("threadcount", argv[argindex]);
      argindex++;
    } else if (strcmp(argv[argindex], "load") == 0) {
      props.SetProperty("pattern", "load");
      argindex++;
    } else if (strcmp(argv[argindex], "run") == 0) {
      props.SetProperty("pattern", "run");
      argindex++;
    } else if (strcmp(argv[argindex], "both") == 0) {
      props.SetProperty("pattern", "both");
      argindex++;
    } else if (strcmp(argv[argindex], "-db") == 0) {
      argindex++;
      if (argindex >= argc) {
        UsageMessage(argv[0]);
        exit(0);
      }
      props.SetProperty("dbname", argv[argindex]);
      argindex++;
    } 
    else if(strcmp(argv[argindex], "-stat") == 0){
      props.SetProperty("stat", "stat");
      argindex++;
    }
    
    else if (strcmp(argv[argindex], "-dbPath") == 0) {
      argindex++;
      if (argindex >= argc) {
        UsageMessage(argv[0]);
        exit(0);
      }
      props.SetProperty("dbPath", argv[argindex]);
      argindex++;
    } else if (strcmp(argv[argindex], "-dbConfig") == 0) {
      argindex++;
      if (argindex >= argc) {
        UsageMessage(argv[0]);
        exit(0);
      }
      props.SetProperty("dbConfig", argv[argindex]);
      argindex++;
    } else if (strcmp(argv[argindex], "-duration") == 0) {
      argindex++;
      if (argindex >= argc) {
        UsageMessage(argv[0]);
        exit(0);
      }
      props.SetProperty("duration", argv[argindex]);
      argindex++;
    } else if (strcmp(argv[argindex], "-host") == 0) {
      argindex++;
      if (argindex >= argc) {
        UsageMessage(argv[0]);
        exit(0);
      }
      props.SetProperty("host", argv[argindex]);
      argindex++;
    } else if (strcmp(argv[argindex], "-port") == 0) {
      argindex++;
      if (argindex >= argc) {
        UsageMessage(argv[0]);
        exit(0);
      }
      props.SetProperty("port", argv[argindex]);
      argindex++;
    } else if (strcmp(argv[argindex], "-slaves") == 0) {
      argindex++;
      if (argindex >= argc) {
        UsageMessage(argv[0]);
        exit(0);
      }
      props.SetProperty("slaves", argv[argindex]);
      argindex++;
    } else if (strcmp(argv[argindex], "-P") == 0) {
      argindex++;
      if (argindex >= argc) {
        UsageMessage(argv[0]);
        exit(0);
      }
      filename.assign(argv[argindex]);
      ifstream input(argv[argindex]);
      try {
        props.Load(input);
      } catch (const string &message) {
        cout << message << endl;
        exit(0);
      }
      input.close();
      argindex++;
    } else {
      cout << "Unknown option '" << argv[argindex] << "'" << endl;
      exit(0);
    }
  }

  if (argindex == 1 || argindex != argc) {
    UsageMessage(argv[0]);
    exit(0);
  }

  string pattern = props.GetProperty("pattern");
  if (pattern.empty()) {
    cout << "Unknown pattern" << endl;
    UsageMessage(argv[0]);
    exit(0);
  }

  return filename;
}

void UsageMessage(const char *command) {
  cout << "Usage: " << command << " [options]" << endl;
  cout << "Options:" << endl;
  cout << "  pattern: load/run/both" << endl;
  cout << "  load: load the database from file" << endl;

  cout << "  run: peforms transactions in the previous database" << endl;
  cout << "  both: load and run" << endl;
  cout << "  'load','run','both' must choose one" << endl;
  cout << "  -threads n: execute using n threads (default: 1)" << endl;
  cout << "  -d n: print DB stats in n seconds (default: 0,mean do not print "
          "stats info)"
       << endl;
  cout << "  -db dbname: specify the name of the DB to use (default: basic)"
       << endl;
  cout << "  -P propertyfile: load properties from the given file. Multiple "
          "files can"
       << endl;
  cout << "                   be specified, and will be processed in the order "
          "specified"
       << endl;
}

inline bool StrStartWith(const char *str, const char *pre) {
  return strncmp(str, pre, strlen(pre)) == 0;
}
