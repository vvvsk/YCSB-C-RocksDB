#/bin/bash
ulimit -c unlimited
ulimit -n 65535

today=`date +%Y-%m-%d-%H:%M:%S`

#load
mod="nvme-load-375M"
echo "load start"
test_data_save_path="../testdata"
db_path="/mnt/nvme/test"
backup_path="/mnt/hdd1/testRocksdbBackup"
# 保证目录存在
if [ ! -d ${db_path} ];then
    mkdir ${db_path}
fi
if [ ! -d ${backup_path} ];then
    mkdir ${backup_path}
fi
if [ ! -d ${test_data_save_path} ];then
    mkdir ${test_data_save_path}
fi


duration=100
exe_path="../build/ycsbc"
threads=36
patternA="../workloads/workloada.spec"
db_config="../db_config/rocksdb_config.ini"
${exe_path} -db rocksdb load -dbPath ${db_path} -threads ${threads} -P ${patternA} -dbConfig ${db_config} > ${test_data_save_path}/load1KB20GB_${today}_${mod}.txt 2>&1
#cp -r ${db_path}/* ${backup_path}