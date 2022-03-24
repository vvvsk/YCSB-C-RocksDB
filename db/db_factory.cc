#include "db/db_factory.h"

#include "db/basic_db.h"
// #include "db/lock_stl_db.h"
#include "db/rocksdb_db.h"
// #include "db/tbb_rand_db.h"
// #include "db/tbb_scan_db.h"
#include <string>

using namespace std;
using ycsbc::DB;
using ycsbc::DBFactory;

shared_ptr<DB> DBFactory::CreateDB(utils::Properties &props) {
  if (props["dbname"] == "basic") {
    return make_shared<BasicDB>();
  } else if (props["dbname"] == "rocksdb") {
    return make_shared<RocksDB>(props["dbPath"].c_str(),
                                props.GetProperty("dbConfig", ""));
  } else
    return NULL;
}
