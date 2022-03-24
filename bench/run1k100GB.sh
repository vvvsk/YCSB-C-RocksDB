#/bin/bash
ulimit -c unlimited
ulimit -n 65535

today=`date +%Y-%m-%d-%H:%M:%S`

#load
mod="nvme-load-1k100G"
echo "load start"
test_data_save_path="../testdata"
db_path="/mnt/nvme/testRocksdb"
backup_path="/mnt/hdd1/testRocksdbBackup"
t=20
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



exe_path="../build/ycsbc"
threads=36
patternA="../workloads-test/workloada1KB100GB.spec"
db_config="../db_config/rocksdb_config.ini"
${exe_path} -db rocksdb load -dbPath ${db_path} -threads ${threads} -P ${patternA} -dbConfig ${db_config} > ${test_data_save_path}/load1KB100GB_${today}_${mod}.txt 2>&1
cp -r ${db_path}/* ${backup_path} #备份

#workloada

rm -rf ${db_path}/* # 删除上次操作的数据
cp -r ${backup_path}/* ${db_path} ## 载入这次的数据
sleep ${t}
mod="nvme-workloada-1k100G"
echo "runA start"
${exe_path} -db rocksdb run -dbPath ${db_path} -threads ${threads} -P ${patternA} -dbConfig ${db_config} > ${test_data_save_path}/run1KB100GB_${today}_${mod}.txt 2>&1



#workloadb
rm -rf ${db_path}/* # 删除上次操作的数据
cp -r ${backup_path}/* ${db_path} ## 载入这次的数据
sleep ${t}
mod="nvme-workloadb-1k100G"
echo "runB start"
patternB="../workloads-test/workloadb1KB100GB.spec"
${exe_path} -db rocksdb run -dbPath ${db_path} -threads ${threads} -P ${patternB} -dbConfig ${db_config} > ${test_data_save_path}/run1KB100GB_${today}_${mod}.txt 2>&1




#workloadc
rm -rf ${db_path}/* # 删除上次操作的数据
cp -r ${backup_path}/* ${db_path} ## 载入这次的数据
sleep ${t}
mod="nvme-workloadc-1k100G"
echo "runC start"
patternC="../workloads-test/workloadc1KB100GB.spec"
${exe_path} -db rocksdb run -dbPath ${db_path} -threads ${threads} -P ${patternC} -dbConfig ${db_config} > ${test_data_save_path}/run1KB100GB_${today}_${mod}.txt 2>&1






#workloadd

rm -rf ${db_path}/*
cp -r ${backup_path}/* ${db_path}
sleep ${t}
mod="nvme-workloadd-1k100G"
echo "runD start"
patternD="../workloads-test/workloadd1KB100GB.spec"
${exe_path} -db rocksdb run -dbPath ${db_path} -threads ${threads} -P ${patternD} -dbConfig ${db_config} > ${test_data_save_path}/run1KB100GB_${today}_${mod}.txt 2>&1

#workloade
rm -rf ${db_path}/* # 删除上次操作的数据
cp -r ${backup_path}/* ${db_path} ## 载入这次的数据
sleep ${t}
mod="nvme-workloade-1k100G"
echo "rune start"
patternE="../workloads-test/workloade1KB100GB.spec"
${exe_path} -db rocksdb run -dbPath ${db_path} -threads ${threads} -P ${patternE} -dbConfig ${db_config} > ${test_data_save_path}/run1KB100GB_${today}_${mod}.txt 2>&1



#workloadf
rm -rf ${db_path}/* # 删除上次操作的数据
cp -r ${backup_path}/* ${db_path} ## 载入这次的数据
sleep ${t}
mod="nvme-workloadf-1k100G"
echo "runF start"
patternF="../workloads-test/workloadf1KB100GB.spec"
${exe_path} -db rocksdb run -dbPath ${db_path} -threads ${threads} -P ${patternF} -dbConfig ${db_config} > ${test_data_save_path}/run1KB100GB_${today}_${mod}.txt 2>&1


