#/bin/bash
ulimit -c unlimited
ulimit -n 65535

today=`date +%Y-%m-%d-%H:%M:%S`

#load
mod="nvme-load-375M"
echo "load start"

db_path="/mnt/nvme/testRocksdb"
backup_path="/mnt/hdd1/testRocksdbBackup"
# 保证目录存在
if [ ! -d ${db_path} ];then
    mkdir ${db_path}
fi
if [ ! -d ${backup_path} ];then
    mkdir ${backup_path}
fi


exe_path="../build/ycsbc"
threads=40
patternA="../workloads/workloada1KB20GB.spec"
db_config="../db_config/rocksdb_config.ini"
${exe_path} -db rocksdb load -dbPath ${db_path} -threads ${threads} -P ${patternA} -dbConfig ${db_config} > load1KB20GB_${today}_${mod}.txt 2>&1

cp -r ${db_path}/* ${backup_path}

#workloada
mod="nvme-workloada-375M"
echo "runA start"
${exe_path} -db rocksdb run -dbPath ${db_path} -threads ${threads} -P ${patternA} -dbConfig ${db_config} > run1KB20GB_${today}_${mod}.txt 2>&1


rm -rf ${db_path}
cp -r ${backup_path}/* ${db_path}
#workloadb
mod="nvme-workloadb-375M"
echo "runB start"
patternB="../workloads/workloadb1KB20GB.spec"
${exe_path} -db rocksdb run -dbPath ${db_path} -threads ${threads} -P ${patternB} -dbConfig ${db_config} > run1KB20GB_${today}_${mod}.txt 2>&1


rm -rf ${db_path}
cp -r ${backup_path}/* ${db_path}/

#workloadb
mod="nvme-workloadd-375M"
echo "runD start"
patternD="../workloads/workloadd1KB20GB.spec"
${exe_path} -db rocksdb run -dbPath ${db_path} -threads ${threads} -P ${patternD} -dbConfig ${db_config} > run1KB20GB_${today}_${mod}.txt 2>&1
