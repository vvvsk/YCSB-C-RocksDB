#/bin/bash
ulimit -c unlimited
ulimit -n 65535

echo "run start"

./build/ycsbc -db rocksdb run -dbPath /mnt/sdb/testRocksdb -threads 8 -P ../workloads/workloada1KB20GB.spec -dbConfig ../db_config/rocksdb_config.ini > run1KB20GB.txt 2>&1

