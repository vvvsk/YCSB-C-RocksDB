# version of cmake
cmake_minimum_required(VERSION 3.13)

# define the name of project
project(ycsbc LANGUAGES CXX)

#specify c++ compiler version
set(CMAKE_CXX_STANDARD 11)
SET(CMAKE_BUILD_TYPE "Debug")

add_executable(ycsbc "")

target_compile_options(ycsbc PRIVATE -g -Wall -pthread)

target_sources(ycsbc
  PRIVATE
    "ycsbc.cc"
    "core/core_workload.cc"
    "db/db_factory.cc"
    "db/hashtable_db.cc"
    "db/rocksdb_db.cc"
    "utils/histogram.cc"
)

target_include_directories(ycsbc
  PRIVATE
  ${PROJECT_SOURCE_DIR}
  ${PROJECT_SOURCE_DIR}/db_impl/rocksdb/include
)

target_link_directories(ycsbc
  PRIVATE
  ${PROJECT_SOURCE_DIR}/db_impl/
)

 
target_link_libraries(ycsbc
  librocksdb-original-6.25.3.a
  pthread
  tbb
  zstd
  lz4
  bz2
  z
  snappy
  dl
  )
