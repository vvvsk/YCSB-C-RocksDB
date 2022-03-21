# YCSB-C

Yahoo! Cloud Serving Benchmark in C++, a C++ version of YCSB (https://github.com/brianfrankcooper/YCSB/wiki)

## Quick Start

To build YCSB-C on Ubuntu, for example:

Run Workload A with a [RocksDB](https://github.com/facebook/rocksdb)-based
implementation of the database, for example:
```
./ycsbc -db basic load -P ../workloads/workloadtest.spec
./ycsbc -db rocksdb load -threads 4 -dbPath ./testDir -P ../workloads/workloada.spec -dbConfig ../db_config/rocksdb_config.ini
```
Also reference run.sh and run.sh for the command line. See help by
invoking `./ycsbc` without any arguments.

Note that we do not have load and run commands as the original YCSB. Specify
how many records to load by the recordcount property. Reference properties
files in the workloads dir.


在db_impl下添加自己选择的rocksdb包含的头文件，以及编译好librocksdb.a
