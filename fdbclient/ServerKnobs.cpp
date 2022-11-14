/*
 * ServerKnobs.cpp
 *
 * This source file is part of the FoundationDB open source project
 *
 * Copyright 2013-2022 Apple Inc. and the FoundationDB project authors
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *     http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

#include "fdbclient/ServerKnobs.h"
#include "flow/CompressionUtils.h"
#include "flow/IRandom.h"
#include "flow/flow.h"

#define init(...) KNOB_FN(__VA_ARGS__, INIT_ATOMIC_KNOB, INIT_KNOB)(__VA_ARGS__)

ServerKnobs::ServerKnobs(Randomize randomize, ClientKnobs* clientKnobs, IsSimulated isSimulated) {
	initialize(randomize, clientKnobs, isSimulated);
}

void ServerKnobs::initialize(Randomize randomize, ClientKnobs* clientKnobs, IsSimulated isSimulated) {
	// clang-format off
	init( ALLOW_DANGEROUS_KNOBS,                               isSimulated );
	// Versions
	init( VERSIONS_PER_SECOND,                                   1e6 );
	init( MAX_VERSIONS_IN_FLIGHT,                100 * VERSIONS_PER_SECOND );
	init( MAX_VERSIONS_IN_FLIGHT_FORCED,         6e5 * VERSIONS_PER_SECOND ); //one week of versions
	init( ENABLE_VERSION_VECTOR,                               false );
	init( ENABLE_VERSION_VECTOR_TLOG_UNICAST,                  false );

	bool buggifyShortReadWindow = randomize && BUGGIFY && !ENABLE_VERSION_VECTOR;
	init( MAX_READ_TRANSACTION_LIFE_VERSIONS,      5 * VERSIONS_PER_SECOND ); if (randomize && BUGGIFY) MAX_READ_TRANSACTION_LIFE_VERSIONS = VERSIONS_PER_SECOND; else if (buggifyShortReadWindow) MAX_READ_TRANSACTION_LIFE_VERSIONS = std::max<int>(1, 0.1 * VERSIONS_PER_SECOND); else if( randomize && BUGGIFY ) MAX_READ_TRANSACTION_LIFE_VERSIONS = 10 * VERSIONS_PER_SECOND;
	init( MAX_WRITE_TRANSACTION_LIFE_VERSIONS,     5 * VERSIONS_PER_SECOND ); if (randomize && BUGGIFY) MAX_WRITE_TRANSACTION_LIFE_VERSIONS=std::max<int>(1, 1 * VERSIONS_PER_SECOND);
	init( MAX_COMMIT_BATCH_INTERVAL,                             2.0 ); if( randomize && BUGGIFY ) MAX_COMMIT_BATCH_INTERVAL = 0.5; // Each commit proxy generates a CommitTransactionBatchRequest at least this often, so that versions always advance smoothly
	MAX_COMMIT_BATCH_INTERVAL = std::min(MAX_COMMIT_BATCH_INTERVAL, MAX_READ_TRANSACTION_LIFE_VERSIONS/double(2*VERSIONS_PER_SECOND)); // Ensure that the proxy commits 2 times every MAX_READ_TRANSACTION_LIFE_VERSIONS, otherwise the master will not give out versions fast enough
	MAX_COMMIT_BATCH_INTERVAL = std::min(MAX_COMMIT_BATCH_INTERVAL, MAX_WRITE_TRANSACTION_LIFE_VERSIONS/double(2*VERSIONS_PER_SECOND)); // Ensure that the proxy commits 2 times every MAX_WRITE_TRANSACTION_LIFE_VERSIONS, otherwise the master will not give out versions fast enough
	init( MAX_VERSION_RATE_MODIFIER,                             0.1 );
	init( MAX_VERSION_RATE_OFFSET,               VERSIONS_PER_SECOND ); // If the calculated version is more than this amount away from the expected version, it will be clamped to this value. This prevents huge version jumps.
	init( ENABLE_VERSION_VECTOR_HA_OPTIMIZATION,               false );

	// TLogs
	init( TLOG_TIMEOUT,                                          0.4 ); //cannot buggify because of availability
	init( TLOG_SLOW_REJOIN_WARN_TIMEOUT_SECS,                     60 ); if( randomize && BUGGIFY ) TLOG_SLOW_REJOIN_WARN_TIMEOUT_SECS = deterministicRandom()->randomInt(5,10);
	init( TLOG_STORAGE_MIN_UPDATE_INTERVAL,                      0.5 );
	init( BUGGIFY_TLOG_STORAGE_MIN_UPDATE_INTERVAL,               30 );
	init( DESIRED_TOTAL_BYTES,                                150000 ); if( randomize && BUGGIFY ) DESIRED_TOTAL_BYTES = 10000;
	init( DESIRED_UPDATE_BYTES,                2*DESIRED_TOTAL_BYTES );
	init( UPDATE_DELAY,                                        0.001 );
	init( MAXIMUM_PEEK_BYTES,                                   10e6 );
	init( APPLY_MUTATION_BYTES,                                  1e6 );
	init( BUGGIFY_RECOVER_MEMORY_LIMIT,                          1e6 );
	init( BUGGIFY_WORKER_REMOVED_MAX_LAG,                         30 );
	init( UPDATE_STORAGE_BYTE_LIMIT,                             1e6 );
	init( TLOG_PEEK_DELAY,                                    0.0005 );
	init( LEGACY_TLOG_UPGRADE_ENTRIES_PER_VERSION,               100 );
	init( VERSION_MESSAGES_OVERHEAD_FACTOR_1024THS,             1072 ); // Based on a naive interpretation of the gcc version of std::deque, we would expect this to be 16 bytes overhead per 512 bytes data. In practice, it seems to be 24 bytes overhead per 512.
	init( VERSION_MESSAGES_ENTRY_BYTES_WITH_OVERHEAD, std::ceil(16.0 * VERSION_MESSAGES_OVERHEAD_FACTOR_1024THS / 1024) );
	init( LOG_SYSTEM_PUSHED_DATA_BLOCK_SIZE,                     1e5 );
	init( MAX_MESSAGE_SIZE,            std::max<int>(LOG_SYSTEM_PUSHED_DATA_BLOCK_SIZE, 1e5 + 2e4 + 1) + 8 ); // VALUE_SIZE_LIMIT + SYSTEM_KEY_SIZE_LIMIT + 9 bytes (4 bytes for length, 4 bytes for sequence number, and 1 byte for mutation type)
	init( TLOG_MESSAGE_BLOCK_BYTES,                             10e6 );
	init( TLOG_MESSAGE_BLOCK_OVERHEAD_FACTOR,      double(TLOG_MESSAGE_BLOCK_BYTES) / (TLOG_MESSAGE_BLOCK_BYTES - MAX_MESSAGE_SIZE) ); //1.0121466709838096006362758832473
	init( PEEK_TRACKER_EXPIRATION_TIME,                          600 ); if( randomize && BUGGIFY ) PEEK_TRACKER_EXPIRATION_TIME = deterministicRandom()->coinflip() ? 0.1 : 120;
	init( PEEK_USING_STREAMING,                                false ); if( randomize && isSimulated && BUGGIFY ) PEEK_USING_STREAMING = true;
	init( PARALLEL_GET_MORE_REQUESTS,                             32 ); if( randomize && BUGGIFY ) PARALLEL_GET_MORE_REQUESTS = 2;
	init( MULTI_CURSOR_PRE_FETCH_LIMIT,                           10 );
	init( MAX_QUEUE_COMMIT_BYTES,                               15e6 ); if( randomize && BUGGIFY ) MAX_QUEUE_COMMIT_BYTES = 5000;
	init( DESIRED_OUTSTANDING_MESSAGES,                         5000 ); if( randomize && BUGGIFY ) DESIRED_OUTSTANDING_MESSAGES = deterministicRandom()->randomInt(0,100);
	init( DESIRED_GET_MORE_DELAY,                              0.005 );
	init( CONCURRENT_LOG_ROUTER_READS,                             5 ); if( randomize && BUGGIFY ) CONCURRENT_LOG_ROUTER_READS = 1;
	init( LOG_ROUTER_PEEK_FROM_SATELLITES_PREFERRED,               1 ); if( randomize && BUGGIFY ) LOG_ROUTER_PEEK_FROM_SATELLITES_PREFERRED = 0;
	init( DISK_QUEUE_ADAPTER_MIN_SWITCH_TIME,                    1.0 );
	init( DISK_QUEUE_ADAPTER_MAX_SWITCH_TIME,                    5.0 );
	init( TLOG_SPILL_REFERENCE_MAX_PEEK_MEMORY_BYTES,            2e9 ); if ( randomize && BUGGIFY ) TLOG_SPILL_REFERENCE_MAX_PEEK_MEMORY_BYTES = 2e6;
	init( TLOG_SPILL_REFERENCE_MAX_BATCHES_PER_PEEK,           100 ); if ( randomize && BUGGIFY ) TLOG_SPILL_REFERENCE_MAX_BATCHES_PER_PEEK = 1;
	init( TLOG_SPILL_REFERENCE_MAX_BYTES_PER_BATCH,           16<<10 ); if ( randomize && BUGGIFY ) TLOG_SPILL_REFERENCE_MAX_BYTES_PER_BATCH = 500;
	init( DISK_QUEUE_FILE_EXTENSION_BYTES,                    10<<20 ); // BUGGIFYd per file within the DiskQueue
	init( DISK_QUEUE_FILE_SHRINK_BYTES,                      100<<20 ); // BUGGIFYd per file within the DiskQueue
	init( DISK_QUEUE_MAX_TRUNCATE_BYTES,                     2LL<<30 ); if ( randomize && BUGGIFY ) DISK_QUEUE_MAX_TRUNCATE_BYTES = 0;
	init( TLOG_DEGRADED_DURATION,                                5.0 );
	init( MAX_CACHE_VERSIONS,                                   10e6 );
	init( TLOG_IGNORE_POP_AUTO_ENABLE_DELAY,                   300.0 );
	init( TXS_POPPED_MAX_DELAY,                                  1.0 ); if ( randomize && BUGGIFY ) TXS_POPPED_MAX_DELAY = deterministicRandom()->random01();
	// In some rare simulation tests, particularly with log_spill:=1 configured, the 10 second limit is exceeded, causing SevError trace events
	// and simulation test failure. Increasing the knob value to 15.0 in simulation is a workaround to avoid these failures.
	init( TLOG_MAX_CREATE_DURATION,                             10.0 ); if (isSimulated) TLOG_MAX_CREATE_DURATION = 15.0;
	init( PEEK_LOGGING_AMOUNT,                                     5 );
	init( PEEK_LOGGING_DELAY,                                    5.0 );
	init( PEEK_RESET_INTERVAL,                                 300.0 ); if ( randomize && BUGGIFY ) PEEK_RESET_INTERVAL = 20.0;
	init( PEEK_MAX_LATENCY,                                      0.5 ); if ( randomize && BUGGIFY ) PEEK_MAX_LATENCY = 0.0;
	init( PEEK_COUNT_SMALL_MESSAGES,                           false ); if ( randomize && BUGGIFY ) PEEK_COUNT_SMALL_MESSAGES = true;
	init( PEEK_STATS_INTERVAL,                                  10.0 );
	init( PEEK_STATS_SLOW_AMOUNT,                                  2 );
	init( PEEK_STATS_SLOW_RATIO,                                 0.5 );
	// Buggified value must be larger than the amount of simulated time taken by snapshots, to prevent repeatedly failing
	// snapshots due to closed commit proxy connections
	init( PUSH_RESET_INTERVAL,                                 300.0 ); if ( randomize && BUGGIFY ) PUSH_RESET_INTERVAL = 40.0;
	init( PUSH_MAX_LATENCY,                                      0.5 ); if ( randomize && BUGGIFY ) PUSH_MAX_LATENCY = 0.0;
	init( PUSH_STATS_INTERVAL,                                  10.0 );
	init( PUSH_STATS_SLOW_AMOUNT,                                  2 );
	init( PUSH_STATS_SLOW_RATIO,                                 0.5 );
	init( TLOG_POP_BATCH_SIZE,                                  1000 ); if ( randomize && BUGGIFY ) TLOG_POP_BATCH_SIZE = 10;
	init( TLOG_POPPED_VER_LAG_THRESHOLD_FOR_TLOGPOP_TRACE,     250e6 );
	init( BLOCKING_PEEK_TIMEOUT,                                 0.4 );
	init( ENABLE_DETAILED_TLOG_POP_TRACE,                      false ); if ( randomize && BUGGIFY ) ENABLE_DETAILED_TLOG_POP_TRACE = true;
	init( PEEK_BATCHING_EMPTY_MSG,                             false ); if ( randomize && BUGGIFY ) PEEK_BATCHING_EMPTY_MSG = true;
	init( PEEK_BATCHING_EMPTY_MSG_INTERVAL,                    0.001 ); if ( randomize && BUGGIFY ) PEEK_BATCHING_EMPTY_MSG_INTERVAL = 0.01;
	init( POP_FROM_LOG_DELAY,                                      1 ); if ( randomize && BUGGIFY ) POP_FROM_LOG_DELAY = 0;

	// disk snapshot max timeout, to be put in TLog, storage and coordinator nodes
	init( MAX_FORKED_PROCESS_OUTPUT,                            1024 );
	init( SNAP_CREATE_MAX_TIMEOUT,        isSimulated ? 70.0 : 300.0 );
	init( SNAP_MINIMUM_TIME_GAP,                                 5.0 );
	init( SNAP_NETWORK_FAILURE_RETRY_LIMIT,                       10 );
	init( MAX_STORAGE_SNAPSHOT_FAULT_TOLERANCE,                    1 );
	init( MAX_COORDINATOR_SNAPSHOT_FAULT_TOLERANCE,                1 );
	init( SNAPSHOT_ALL_STATEFUL_PROCESSES,                     false ); if ( randomize && BUGGIFY ) SNAPSHOT_ALL_STATEFUL_PROCESSES = true;

	// Data distribution queue
	init( HEALTH_POLL_TIME,                                      1.0 );
	init( BEST_TEAM_STUCK_DELAY,                                 1.0 );
	init( DEST_OVERLOADED_DELAY,                                 0.2 );
	init( BG_REBALANCE_POLLING_INTERVAL,                        10.0 );
	init( BG_REBALANCE_SWITCH_CHECK_INTERVAL,                    5.0 ); if (randomize && BUGGIFY) BG_REBALANCE_SWITCH_CHECK_INTERVAL = 1.0;
	init( DD_QUEUE_LOGGING_INTERVAL,                             5.0 );
	init( DD_QUEUE_COUNTER_REFRESH_INTERVAL,                    60.0 );
	// 100 / 60 < 2 trace/sec ~ 2 * 200 = 400b/sec
	init( DD_QUEUE_COUNTER_MAX_LOG,                              100 ); if( randomize && BUGGIFY ) DD_QUEUE_COUNTER_MAX_LOG = 1;
	init( DD_QUEUE_COUNTER_SUMMARIZE,                           true );
	init( RELOCATION_PARALLELISM_PER_SOURCE_SERVER,                2 ); if( randomize && BUGGIFY ) RELOCATION_PARALLELISM_PER_SOURCE_SERVER = 1;
	init( RELOCATION_PARALLELISM_PER_DEST_SERVER,                 10 ); if( randomize && BUGGIFY ) RELOCATION_PARALLELISM_PER_DEST_SERVER = 1; // Note: if this is smaller than FETCH_KEYS_PARALLELISM, this will artificially reduce performance. The current default of 10 is probably too high but is set conservatively for now.
	init( DD_QUEUE_MAX_KEY_SERVERS,                              100 ); if( randomize && BUGGIFY ) DD_QUEUE_MAX_KEY_SERVERS = 1;
	init( DD_REBALANCE_PARALLELISM,                               50 );
	init( DD_REBALANCE_RESET_AMOUNT,                              30 );
	init( INFLIGHT_PENALTY_HEALTHY,                              1.0 );
	init( INFLIGHT_PENALTY_UNHEALTHY,                          500.0 );
	init( INFLIGHT_PENALTY_ONE_LEFT,                          1000.0 );
	init( USE_OLD_NEEDED_SERVERS,                              false );

	init( PRIORITY_RECOVER_MOVE,                                 110 );
	init( PRIORITY_REBALANCE_UNDERUTILIZED_TEAM,                 120 );
	init( PRIORITY_REBALANCE_READ_UNDERUTIL_TEAM,                121 );
	init( PRIORITY_REBALANCE_OVERUTILIZED_TEAM,                  122 );
	init( PRIORITY_REBALANCE_READ_OVERUTIL_TEAM,                 123 );
	init( PRIORITY_PERPETUAL_STORAGE_WIGGLE,                     139 );
	init( PRIORITY_TEAM_HEALTHY,                                 140 );
	init( PRIORITY_TEAM_CONTAINS_UNDESIRED_SERVER,               150 );
	init( PRIORITY_TEAM_REDUNDANT,                               200 );
	init( PRIORITY_MERGE_SHARD,                                  340 );
	init( PRIORITY_POPULATE_REGION,                              600 );
	init( PRIORITY_TEAM_UNHEALTHY,                               700 );
	init( PRIORITY_TEAM_2_LEFT,                                  709 );
	init( PRIORITY_TEAM_1_LEFT,                                  800 );
	init( PRIORITY_TEAM_FAILED,                                  805 );
	init( PRIORITY_TEAM_0_LEFT,                                  809 );
	init( PRIORITY_SPLIT_SHARD,                                  950 ); if( randomize && BUGGIFY ) PRIORITY_SPLIT_SHARD = 350;
	init( PRIORITY_ENFORCE_MOVE_OUT_OF_PHYSICAL_SHARD,           960 ); if( randomize && BUGGIFY ) PRIORITY_ENFORCE_MOVE_OUT_OF_PHYSICAL_SHARD = 360; // Set as the lowest priority

	// Data distribution
	init( SHARD_ENCODE_LOCATION_METADATA,                      false ); if( randomize && BUGGIFY )  SHARD_ENCODE_LOCATION_METADATA = true;
	init( ENABLE_DD_PHYSICAL_SHARD,                            false ); // EXPERIMENTAL; If true, SHARD_ENCODE_LOCATION_METADATA must be true; When true, optimization of data move between DCs is disabled
	init( MAX_PHYSICAL_SHARD_BYTES,                        500000000 ); // 500 MB; for ENABLE_DD_PHYSICAL_SHARD; smaller leads to larger number of physicalShard per storage server
 	init( PHYSICAL_SHARD_METRICS_DELAY,                        300.0 ); // 300 seconds; for ENABLE_DD_PHYSICAL_SHARD
	init( ANONYMOUS_PHYSICAL_SHARD_TRANSITION_TIME,            600.0 ); if( randomize && BUGGIFY )  ANONYMOUS_PHYSICAL_SHARD_TRANSITION_TIME = 0.0; // 600 seconds; for ENABLE_DD_PHYSICAL_SHARD
	init( READ_REBALANCE_CPU_THRESHOLD,                         15.0 );
	init( READ_REBALANCE_SRC_PARALLELISM,                         20 );
	init( READ_REBALANCE_SHARD_TOPK,  READ_REBALANCE_SRC_PARALLELISM * 2 );
	init( READ_REBALANCE_DIFF_FRAC,                               0.3);
	init( READ_REBALANCE_MAX_SHARD_FRAC,                          0.2); // FIXME: add buggify here when we have DD test, seems DD is pretty sensitive to this parameter
	init( RETRY_RELOCATESHARD_DELAY,                             0.1 );
	init( DATA_DISTRIBUTION_FAILURE_REACTION_TIME,              60.0 ); if( randomize && BUGGIFY ) DATA_DISTRIBUTION_FAILURE_REACTION_TIME = 1.0;
	bool buggifySmallShards = randomize && BUGGIFY;
	bool simulationMediumShards = !buggifySmallShards && isSimulated && randomize && !BUGGIFY; // prefer smaller shards in simulation
	// FIXME: increase this even more eventually
	init( MIN_SHARD_BYTES,                                  10000000 ); if( buggifySmallShards ) MIN_SHARD_BYTES = 40000; if (simulationMediumShards) MIN_SHARD_BYTES = 200000; //FIXME: data distribution tracker (specifically StorageMetrics) relies on this number being larger than the maximum size of a key value pair
	init( SHARD_BYTES_RATIO,                                       4 );
	init( SHARD_BYTES_PER_SQRT_BYTES,                             45 ); if( buggifySmallShards ) SHARD_BYTES_PER_SQRT_BYTES = 0;//Approximately 10000 bytes per shard
	init( MAX_SHARD_BYTES,                                 500000000 );
	init( KEY_SERVER_SHARD_BYTES,                          500000000 );
	init( SHARD_MAX_READ_DENSITY_RATIO,                           8.0); if (randomize && BUGGIFY) SHARD_MAX_READ_DENSITY_RATIO = 2.0;
	/*
		The bytesRead/byteSize radio. Will be declared as read hot when larger than this. 8.0 was chosen to avoid reporting table scan as read hot.
	*/
	init ( SHARD_READ_HOT_BANDWIDTH_MIN_PER_KSECONDS,      1666667 * 1000);
	/*
		The read bandwidth of a given shard needs to be larger than this value in order to be evaluated if it's read hot. The roughly 1.67MB per second is calculated as following:
			- Heuristic data suggests that each storage process can do max 500K read operations per second
			- Each read has a minimum cost of EMPTY_READ_PENALTY, which is 20 bytes
			- Thus that gives a minimum 10MB per second
			- But to be conservative, set that number to be 1/6 of 10MB, which is roughly 1,666,667 bytes per second
		Shard with a read bandwidth smaller than this value will never be too busy to handle the reads.
	*/
	init( SHARD_MAX_BYTES_READ_PER_KSEC_JITTER,     0.1 );
	bool buggifySmallBandwidthSplit = randomize && BUGGIFY;
	init( SHARD_MAX_BYTES_PER_KSEC,                 1LL*1000000*1000 ); if( buggifySmallBandwidthSplit ) SHARD_MAX_BYTES_PER_KSEC = 10LL*1000*1000;
	/* 1*1MB/sec * 1000sec/ksec
		Shards with more than this bandwidth will be split immediately.
		For a large shard (100MB), it will be split into multiple shards with sizes < SHARD_SPLIT_BYTES_PER_KSEC;
		all but one split shard will be moved; so splitting may cost ~100MB of work or about 10MB/sec over a 10 sec sampling window.
		If the sampling window is too much longer, the MVCC window will fill up while we wait.
		If SHARD_MAX_BYTES_PER_KSEC is too much lower, we could do a lot of data movement work in response to a small impulse of bandwidth.
		If SHARD_MAX_BYTES_PER_KSEC is too high relative to the I/O bandwidth of a given server, a workload can remain concentrated on a single
		team indefinitely, limiting performance.
		*/

	init( SHARD_MIN_BYTES_PER_KSEC,                100 * 1000 * 1000 ); if( buggifySmallBandwidthSplit ) SHARD_MIN_BYTES_PER_KSEC = 200*1*1000;
	/* 100*1KB/sec * 1000sec/ksec
		Shards with more than this bandwidth will not be merged.
		Obviously this needs to be significantly less than SHARD_MAX_BYTES_PER_KSEC, else we will repeatedly merge and split.
		It should probably be significantly less than SHARD_SPLIT_BYTES_PER_KSEC, else we will merge right after splitting.

		The number of extra shards in the database because of bandwidth splitting can't be more than about W/SHARD_MIN_BYTES_PER_KSEC, where
		W is the maximum bandwidth of the entire database in bytes/ksec.  For 250MB/sec write bandwidth, (250MB/sec)/(200KB/sec) = 1250 extra
		shards.

		The bandwidth sample maintained by the storage server needs to be accurate enough to reliably measure this minimum bandwidth.  See
		BANDWIDTH_UNITS_PER_SAMPLE.  If this number is too low, the storage server needs to spend more memory and time on sampling.
		*/

	init( SHARD_SPLIT_BYTES_PER_KSEC,              250 * 1000 * 1000 ); if( buggifySmallBandwidthSplit ) SHARD_SPLIT_BYTES_PER_KSEC = 50 * 1000 * 1000;
	/* 250*1KB/sec * 1000sec/ksec
		When splitting a shard, it is split into pieces with less than this bandwidth.
		Obviously this should be less than half of SHARD_MAX_BYTES_PER_KSEC.

		Smaller values mean that high bandwidth shards are split into more pieces, more quickly utilizing large numbers of servers to handle the
		bandwidth.

		Too many pieces (too small a value) may stress data movement mechanisms (see e.g. RELOCATION_PARALLELISM_PER_SOURCE_SERVER).

		If this value is too small relative to SHARD_MIN_BYTES_PER_KSEC immediate merging work will be generated.
		*/

	init( STORAGE_METRIC_TIMEOUT,         isSimulated ? 60.0 : 600.0 ); if( randomize && BUGGIFY ) STORAGE_METRIC_TIMEOUT = deterministicRandom()->coinflip() ? 10.0 : 30.0;
	init( METRIC_DELAY,                                          0.1 ); if( randomize && BUGGIFY ) METRIC_DELAY = 1.0;
	init( ALL_DATA_REMOVED_DELAY,                                1.0 );
	init( INITIAL_FAILURE_REACTION_DELAY,                       30.0 ); if( randomize && BUGGIFY ) INITIAL_FAILURE_REACTION_DELAY = 0.0;
	init( CHECK_TEAM_DELAY,                                     30.0 );
	init( PERPETUAL_WIGGLE_DELAY,                               50.0 );
	init( PERPETUAL_WIGGLE_DISABLE_REMOVER,                     true );
	init( LOG_ON_COMPLETION_DELAY,         DD_QUEUE_LOGGING_INTERVAL );
	init( BEST_TEAM_MAX_TEAM_TRIES,                               10 );
	init( BEST_TEAM_OPTION_COUNT,                                  4 );
	init( BEST_OF_AMT,                                             4 );
	init( SERVER_LIST_DELAY,                                     1.0 );
	init( RECRUITMENT_IDLE_DELAY,                                1.0 );
	init( STORAGE_RECRUITMENT_DELAY,                            10.0 );
	init( TSS_HACK_IDENTITY_MAPPING,                           false ); // THIS SHOULD NEVER BE SET IN PROD. Only for performance testing
	init( TSS_RECRUITMENT_TIMEOUT,       3*STORAGE_RECRUITMENT_DELAY ); if (randomize && BUGGIFY ) TSS_RECRUITMENT_TIMEOUT = 1.0; // Super low timeout should cause tss recruitments to fail
	init( TSS_DD_CHECK_INTERVAL,                                60.0 ); if (randomize && BUGGIFY ) TSS_DD_CHECK_INTERVAL = 1.0;    // May kill all TSS quickly
	init( DATA_DISTRIBUTION_LOGGING_INTERVAL,                    5.0 );
	init( DD_ENABLED_CHECK_DELAY,                                1.0 );
	init( DD_STALL_CHECK_DELAY,                                  0.4 ); //Must be larger than 2*MAX_BUGGIFIED_DELAY
	init( DD_LOW_BANDWIDTH_DELAY,         isSimulated ? 15.0 : 240.0 ); if( randomize && BUGGIFY ) DD_LOW_BANDWIDTH_DELAY = 0; //Because of delayJitter, this should be less than 0.9 * DD_MERGE_COALESCE_DELAY
	init( DD_MERGE_COALESCE_DELAY,       isSimulated ?  30.0 : 300.0 ); if( randomize && BUGGIFY ) DD_MERGE_COALESCE_DELAY = 0.001;
	init( STORAGE_METRICS_POLLING_DELAY,                         2.0 ); if( randomize && BUGGIFY ) STORAGE_METRICS_POLLING_DELAY = 15.0;
	init( STORAGE_METRICS_RANDOM_DELAY,                          0.2 );
	init( AVAILABLE_SPACE_RATIO_CUTOFF,                         0.05 );
	init( DESIRED_TEAMS_PER_SERVER,                                5 ); if( randomize && BUGGIFY ) DESIRED_TEAMS_PER_SERVER = deterministicRandom()->randomInt(1, 10);
	init( MAX_TEAMS_PER_SERVER,           5*DESIRED_TEAMS_PER_SERVER );
	init( DD_SHARD_SIZE_GRANULARITY,                         5000000 );
	init( DD_SHARD_SIZE_GRANULARITY_SIM,                      500000 ); if( randomize && BUGGIFY ) DD_SHARD_SIZE_GRANULARITY_SIM = 0;
	init( DD_MOVE_KEYS_PARALLELISM,                               15 ); if( randomize && BUGGIFY ) DD_MOVE_KEYS_PARALLELISM = 1;
	init( DD_FETCH_SOURCE_PARALLELISM,                          1000 ); if( randomize && BUGGIFY ) DD_FETCH_SOURCE_PARALLELISM = 1;
	init( DD_MERGE_LIMIT,                                       2000 ); if( randomize && BUGGIFY ) DD_MERGE_LIMIT = 2;
	init( DD_SHARD_METRICS_TIMEOUT,                             60.0 ); if( randomize && BUGGIFY ) DD_SHARD_METRICS_TIMEOUT = 0.1;
	init( DD_LOCATION_CACHE_SIZE,                            2000000 ); if( randomize && BUGGIFY ) DD_LOCATION_CACHE_SIZE = 3;
	init( MOVEKEYS_LOCK_POLLING_DELAY,                           5.0 );
	init( DEBOUNCE_RECRUITING_DELAY,                             5.0 );
	init( DD_FAILURE_TIME,                                       1.0 ); if( randomize && BUGGIFY ) DD_FAILURE_TIME = 10.0;
	init( DD_ZERO_HEALTHY_TEAM_DELAY,                            1.0 );
	init( REMOTE_KV_STORE,                                     false );
	init( REBOOT_KV_STORE_DELAY,                                 0.1 );
	init( REMOTE_KV_STORE_MAX_INIT_DURATION,                    10.0 );
	init( REBALANCE_MAX_RETRIES,                                 100 );
	init( DD_OVERLAP_PENALTY,                                  10000 );
	init( DD_EXCLUDE_MIN_REPLICAS,                                 1 );
	init( DD_VALIDATE_LOCALITY,                                 true ); if( randomize && BUGGIFY ) DD_VALIDATE_LOCALITY = false;
	init( DD_CHECK_INVALID_LOCALITY_DELAY,                       60  ); if( randomize && BUGGIFY ) DD_CHECK_INVALID_LOCALITY_DELAY = 1 + deterministicRandom()->random01() * 600;
	init( DD_ENABLE_VERBOSE_TRACING,                           false ); if( randomize && BUGGIFY ) DD_ENABLE_VERBOSE_TRACING = true;
	init( DD_SS_FAILURE_VERSIONLAG,                        250000000 );
	init( DD_SS_ALLOWED_VERSIONLAG,                        200000000 ); if( randomize && BUGGIFY ) { DD_SS_FAILURE_VERSIONLAG = deterministicRandom()->randomInt(15000000, 500000000); DD_SS_ALLOWED_VERSIONLAG = 0.75 * DD_SS_FAILURE_VERSIONLAG; }
	init( DD_SS_STUCK_TIME_LIMIT,                              300.0 ); if( randomize && BUGGIFY ) { DD_SS_STUCK_TIME_LIMIT = 200.0 + deterministicRandom()->random01() * 100.0; }
	init( DD_TEAMS_INFO_PRINT_INTERVAL,                           60 ); if( randomize && BUGGIFY ) DD_TEAMS_INFO_PRINT_INTERVAL = 10;
	init( DD_TEAMS_INFO_PRINT_YIELD_COUNT,                       100 ); if( randomize && BUGGIFY ) DD_TEAMS_INFO_PRINT_YIELD_COUNT = deterministicRandom()->random01() * 1000 + 1;
	init( DD_TEAM_ZERO_SERVER_LEFT_LOG_DELAY,                    120 ); if( randomize && BUGGIFY ) DD_TEAM_ZERO_SERVER_LEFT_LOG_DELAY = 5;
	init( DD_STORAGE_WIGGLE_PAUSE_THRESHOLD,                      10 ); if( randomize && BUGGIFY ) DD_STORAGE_WIGGLE_PAUSE_THRESHOLD = 1000;
	init( DD_STORAGE_WIGGLE_STUCK_THRESHOLD,                      20 );
	init( DD_STORAGE_WIGGLE_MIN_SS_AGE_SEC,   isSimulated ? 2 : 21 * 60 * 60 * 24 ); if(randomize && BUGGIFY) DD_STORAGE_WIGGLE_MIN_SS_AGE_SEC = isSimulated ? 0: 120;
	init( DD_TENANT_AWARENESS_ENABLED,                         false );
	init( STORAGE_QUOTA_ENABLED,                               false ); if(isSimulated) STORAGE_QUOTA_ENABLED = deterministicRandom()->coinflip();
	init( TENANT_CACHE_LIST_REFRESH_INTERVAL,                      2 ); if( randomize && BUGGIFY ) TENANT_CACHE_LIST_REFRESH_INTERVAL = deterministicRandom()->randomInt(1, 10);
	init( TENANT_CACHE_STORAGE_USAGE_REFRESH_INTERVAL,             2 ); if( randomize && BUGGIFY ) TENANT_CACHE_STORAGE_USAGE_REFRESH_INTERVAL = deterministicRandom()->randomInt(1, 10);
	init( TENANT_CACHE_STORAGE_QUOTA_REFRESH_INTERVAL,            10 ); if( randomize && BUGGIFY ) TENANT_CACHE_STORAGE_QUOTA_REFRESH_INTERVAL = deterministicRandom()->randomInt(1, 10);
	init( CP_FETCH_TENANTS_OVER_STORAGE_QUOTA_INTERVAL,            5 ); if( randomize && BUGGIFY ) CP_FETCH_TENANTS_OVER_STORAGE_QUOTA_INTERVAL = deterministicRandom()->randomInt(1, 10);

	// TeamRemover
	init( TR_FLAG_DISABLE_MACHINE_TEAM_REMOVER,                false ); if( randomize && BUGGIFY ) TR_FLAG_DISABLE_MACHINE_TEAM_REMOVER = deterministicRandom()->random01() < 0.1 ? true : false; // false by default. disable the consistency check when it's true
	init( TR_REMOVE_MACHINE_TEAM_DELAY,                         60.0 ); if( randomize && BUGGIFY ) TR_REMOVE_MACHINE_TEAM_DELAY =  deterministicRandom()->random01() * 60.0;
	init( TR_FLAG_REMOVE_MT_WITH_MOST_TEAMS,                    true ); if( randomize && BUGGIFY ) TR_FLAG_REMOVE_MT_WITH_MOST_TEAMS = deterministicRandom()->random01() < 0.1 ? true : false;
	init( TR_FLAG_DISABLE_SERVER_TEAM_REMOVER,                 false ); if( randomize && BUGGIFY ) TR_FLAG_DISABLE_SERVER_TEAM_REMOVER = deterministicRandom()->random01() < 0.1 ? true : false; // false by default. disable the consistency check when it's true
	init( TR_REMOVE_SERVER_TEAM_DELAY,                          60.0 ); if( randomize && BUGGIFY ) TR_REMOVE_SERVER_TEAM_DELAY =  deterministicRandom()->random01() * 60.0;
	init( TR_REMOVE_SERVER_TEAM_EXTRA_DELAY,                     5.0 ); if( randomize && BUGGIFY ) TR_REMOVE_SERVER_TEAM_EXTRA_DELAY =  deterministicRandom()->random01() * 10.0;

	init( DD_REMOVE_STORE_ENGINE_DELAY,                         60.0 ); if( randomize && BUGGIFY ) DD_REMOVE_STORE_ENGINE_DELAY =  deterministicRandom()->random01() * 60.0;

	// KeyValueStore SQLITE
	init( CLEAR_BUFFER_SIZE,                                   20000 );
	init( READ_VALUE_TIME_ESTIMATE,                           .00005 );
	init( READ_RANGE_TIME_ESTIMATE,                           .00005 );
	init( SET_TIME_ESTIMATE,                                  .00005 );
	init( CLEAR_TIME_ESTIMATE,                                .00005 );
	init( COMMIT_TIME_ESTIMATE,                                 .005 );
	init( CHECK_FREE_PAGE_AMOUNT,                                100 ); if( randomize && BUGGIFY ) CHECK_FREE_PAGE_AMOUNT = 5;
	init( SOFT_HEAP_LIMIT,                                     300e6 );

	init( SQLITE_PAGE_SCAN_ERROR_LIMIT,                        10000 );
	init( SQLITE_BTREE_PAGE_USABLE,                          4096 - 8);  // pageSize - reserveSize for page checksum
	init( SQLITE_CHUNK_SIZE_PAGES,                             25600 );  // 100MB
	init( SQLITE_CHUNK_SIZE_PAGES_SIM,                          1024 );  // 4MB
	init( SQLITE_READER_THREADS,                                  64 );  // number of read threads
	init( SQLITE_WRITE_WINDOW_SECONDS,                            -1 );
	init( SQLITE_CURSOR_MAX_LIFETIME_BYTES,                      1e6 ); if (buggifySmallShards || simulationMediumShards) SQLITE_CURSOR_MAX_LIFETIME_BYTES = MIN_SHARD_BYTES; if( randomize && BUGGIFY ) SQLITE_CURSOR_MAX_LIFETIME_BYTES = 0;
	init( SQLITE_WRITE_WINDOW_LIMIT,                              -1 );
	if( randomize && BUGGIFY ) {
		// Choose an window between .01 and 1.01 seconds.
		SQLITE_WRITE_WINDOW_SECONDS = 0.01 + deterministicRandom()->random01();
		// Choose random operations per second
		int opsPerSecond = deterministicRandom()->randomInt(1000, 5000);
		// Set window limit to opsPerSecond scaled down to window size
		SQLITE_WRITE_WINDOW_LIMIT = opsPerSecond * SQLITE_WRITE_WINDOW_SECONDS;
	}

	// Maximum and minimum cell payload bytes allowed on primary page as calculated in SQLite.
	// These formulas are copied from SQLite, using its hardcoded constants, so if you are
	// changing this you should also be changing SQLite.
	init( SQLITE_BTREE_CELL_MAX_LOCAL,  (SQLITE_BTREE_PAGE_USABLE - 12) * 64/255 - 23 );
	init( SQLITE_BTREE_CELL_MIN_LOCAL,  (SQLITE_BTREE_PAGE_USABLE - 12) * 32/255 - 23 );

	// Maximum FDB fragment key and value bytes that can fit in a primary btree page
	init( SQLITE_FRAGMENT_PRIMARY_PAGE_USABLE,
					SQLITE_BTREE_CELL_MAX_LOCAL
					 - 1 // vdbeRecord header length size
					 - 2 // max key length size
					 - 4 // max index length size
					 - 2 // max value fragment length size
	);

	// Maximum FDB fragment value bytes in an overflow page
	init( SQLITE_FRAGMENT_OVERFLOW_PAGE_USABLE,
					SQLITE_BTREE_PAGE_USABLE
					 - 4 // next pageNumber size
	);
	init( SQLITE_FRAGMENT_MIN_SAVINGS,                          0.20 );

	// KeyValueStoreSqlite spring cleaning
	init( SPRING_CLEANING_NO_ACTION_INTERVAL,                    1.0 ); if( randomize && BUGGIFY ) SPRING_CLEANING_NO_ACTION_INTERVAL = deterministicRandom()->coinflip() ? 0.1 : deterministicRandom()->random01() * 5;
	init( SPRING_CLEANING_LAZY_DELETE_INTERVAL,                  0.1 ); if( randomize && BUGGIFY ) SPRING_CLEANING_LAZY_DELETE_INTERVAL = deterministicRandom()->coinflip() ? 1.0 : deterministicRandom()->random01() * 5;
	init( SPRING_CLEANING_VACUUM_INTERVAL,                       1.0 ); if( randomize && BUGGIFY ) SPRING_CLEANING_VACUUM_INTERVAL = deterministicRandom()->coinflip() ? 0.1 : deterministicRandom()->random01() * 5;
	init( SPRING_CLEANING_LAZY_DELETE_TIME_ESTIMATE,            .010 ); if( randomize && BUGGIFY ) SPRING_CLEANING_LAZY_DELETE_TIME_ESTIMATE = deterministicRandom()->random01() * 5;
	init( SPRING_CLEANING_VACUUM_TIME_ESTIMATE,                 .010 ); if( randomize && BUGGIFY ) SPRING_CLEANING_VACUUM_TIME_ESTIMATE = deterministicRandom()->random01() * 5;
	init( SPRING_CLEANING_VACUUMS_PER_LAZY_DELETE_PAGE,          0.0 ); if( randomize && BUGGIFY ) SPRING_CLEANING_VACUUMS_PER_LAZY_DELETE_PAGE = deterministicRandom()->coinflip() ? 1e9 : deterministicRandom()->random01() * 5;
	init( SPRING_CLEANING_MIN_LAZY_DELETE_PAGES,                   0 ); if( randomize && BUGGIFY ) SPRING_CLEANING_MIN_LAZY_DELETE_PAGES = deterministicRandom()->randomInt(1, 100);
	init( SPRING_CLEANING_MAX_LAZY_DELETE_PAGES,                 1e9 ); if( randomize && BUGGIFY ) SPRING_CLEANING_MAX_LAZY_DELETE_PAGES = deterministicRandom()->coinflip() ? 0 : deterministicRandom()->randomInt(1, 1e4);
	init( SPRING_CLEANING_LAZY_DELETE_BATCH_SIZE,                100 ); if( randomize && BUGGIFY ) SPRING_CLEANING_LAZY_DELETE_BATCH_SIZE = deterministicRandom()->randomInt(1, 1000);
	init( SPRING_CLEANING_MIN_VACUUM_PAGES,                        1 ); if( randomize && BUGGIFY ) SPRING_CLEANING_MIN_VACUUM_PAGES = deterministicRandom()->randomInt(0, 100);
	init( SPRING_CLEANING_MAX_VACUUM_PAGES,                      1e9 ); if( randomize && BUGGIFY ) SPRING_CLEANING_MAX_VACUUM_PAGES = deterministicRandom()->coinflip() ? 0 : deterministicRandom()->randomInt(1, 1e4);

	// KeyValueStoreMemory
	init( REPLACE_CONTENTS_BYTES,                                1e5 );

	// KeyValueStoreRocksDB
	init( ROCKSDB_SET_READ_TIMEOUT,         		    !isSimulated );
	init( ROCKSDB_LEVEL_COMPACTION_DYNAMIC_LEVEL_BYTES,         true ); if( randomize && BUGGIFY )  ROCKSDB_LEVEL_COMPACTION_DYNAMIC_LEVEL_BYTES = false;
	init( ROCKSDB_SUGGEST_COMPACT_CLEAR_RANGE,                  true ); if( randomize && BUGGIFY )  ROCKSDB_SUGGEST_COMPACT_CLEAR_RANGE = false;
	init( ROCKSDB_READ_RANGE_ROW_LIMIT,                        65535 ); if( randomize && BUGGIFY )  ROCKSDB_READ_RANGE_ROW_LIMIT = deterministicRandom()->randomInt(2, 10);
	init( ROCKSDB_READER_THREAD_PRIORITY,                          0 );
	init( ROCKSDB_WRITER_THREAD_PRIORITY,                          0 );
	init( ROCKSDB_BACKGROUND_PARALLELISM,                          4 );
	init( ROCKSDB_READ_PARALLELISM,                                4 );
	// If true, do not process and store RocksDB logs
	init( ROCKSDB_MUTE_LOGS,                                    true );
	// Use a smaller memtable in simulation to avoid OOMs.
	int64_t memtableBytes = isSimulated ? 32 * 1024 : 512 * 1024 * 1024;
	init( ROCKSDB_MEMTABLE_BYTES,                      memtableBytes );
	init( ROCKSDB_LEVEL_STYLE_COMPACTION,                       true );
	init( ROCKSDB_UNSAFE_AUTO_FSYNC,                           false );
	init( ROCKSDB_PERIODIC_COMPACTION_SECONDS,                     0 );
	init( ROCKSDB_PREFIX_LEN,                                      0 );
	// If rocksdb block cache size is 0, the default 8MB is used.
	int64_t blockCacheSize = isSimulated ? 0 : 1024 * 1024 * 1024 /* 1GB */;
	init( ROCKSDB_BLOCK_CACHE_SIZE,                   blockCacheSize );
	init( ROCKSDB_METRICS_DELAY,                                60.0 );
	init( ROCKSDB_READ_VALUE_TIMEOUT,      isSimulated ? 5.0 : 200.0 );
	init( ROCKSDB_READ_VALUE_PREFIX_TIMEOUT, isSimulated ? 5.0 : 200.0 );
	init( ROCKSDB_READ_RANGE_TIMEOUT,       isSimulated ? 5.0 : 200.0 );
	init( ROCKSDB_READ_QUEUE_WAIT,                               1.0 );
	init( ROCKSDB_READ_QUEUE_HARD_MAX,                          1000 );
	init( ROCKSDB_READ_QUEUE_SOFT_MAX,                           500 );
	init( ROCKSDB_FETCH_QUEUE_HARD_MAX,                          100 );
	init( ROCKSDB_FETCH_QUEUE_SOFT_MAX,                           50 );
	init( ROCKSDB_HISTOGRAMS_SAMPLE_RATE,                      0.001 ); if( randomize && BUGGIFY ) ROCKSDB_HISTOGRAMS_SAMPLE_RATE = 0;
	init( ROCKSDB_READ_RANGE_ITERATOR_REFRESH_TIME,             30.0 ); if( randomize && BUGGIFY ) ROCKSDB_READ_RANGE_ITERATOR_REFRESH_TIME = 0.1;
	init( ROCKSDB_READ_RANGE_REUSE_ITERATORS,                   true ); if( randomize && BUGGIFY ) ROCKSDB_READ_RANGE_REUSE_ITERATORS = deterministicRandom()->coinflip();
	init( ROCKSDB_READ_RANGE_REUSE_BOUNDED_ITERATORS,          false ); if( randomize && BUGGIFY ) ROCKSDB_READ_RANGE_REUSE_BOUNDED_ITERATORS = deterministicRandom()->coinflip();
	init( ROCKSDB_READ_RANGE_BOUNDED_ITERATORS_MAX_LIMIT,        200 );
	// Set to 0 to disable rocksdb write rate limiting. Rate limiter unit: bytes per second.
	init( ROCKSDB_WRITE_RATE_LIMITER_BYTES_PER_SEC,                0 );
	// If true, enables dynamic adjustment of ROCKSDB_WRITE_RATE_LIMITER_BYTES according to the recent demand of background IO.
	init( ROCKSDB_WRITE_RATE_LIMITER_AUTO_TUNE,                 true );
	init( DEFAULT_FDB_ROCKSDB_COLUMN_FAMILY,                    "fdb");
	init( ROCKSDB_DISABLE_AUTO_COMPACTIONS,                    false ); // RocksDB default

	init( ROCKSDB_PERFCONTEXT_ENABLE,                          false ); if( randomize && BUGGIFY ) ROCKSDB_PERFCONTEXT_ENABLE = deterministicRandom()->coinflip();
	init( ROCKSDB_PERFCONTEXT_SAMPLE_RATE,                    0.0001 );
	init( ROCKSDB_METRICS_SAMPLE_INTERVAL,						  0.0);
	init( ROCKSDB_MAX_SUBCOMPACTIONS,                              2 );
	init( ROCKSDB_SOFT_PENDING_COMPACT_BYTES_LIMIT,      64000000000 ); // 64GB, Rocksdb option, Writes will slow down.
	init( ROCKSDB_HARD_PENDING_COMPACT_BYTES_LIMIT,     100000000000 ); // 100GB, Rocksdb option, Writes will stall.
	init( ROCKSDB_CAN_COMMIT_COMPACT_BYTES_LIMIT,        50000000000 ); // 50GB, Commit waits.
	// Enable this knob only for experminatal purpose, never enable this in production.
	// If enabled, all the committed in-memory memtable writes are lost on a crash.
	init( ROCKSDB_DISABLE_WAL_EXPERIMENTAL,                    false );
	// If ROCKSDB_SINGLEKEY_DELETES_ON_CLEARRANGE is enabled, disable ROCKSDB_ENABLE_CLEAR_RANGE_EAGER_READS knob.
	// These knobs have contrary functionality.
	init( ROCKSDB_SINGLEKEY_DELETES_ON_CLEARRANGE,             false ); if( randomize && BUGGIFY ) ROCKSDB_SINGLEKEY_DELETES_ON_CLEARRANGE = deterministicRandom()->coinflip();
	init( ROCKSDB_SINGLEKEY_DELETES_BYTES_LIMIT,              200000 ); // 200KB
	init( ROCKSDB_ENABLE_CLEAR_RANGE_EAGER_READS,               true ); if( randomize && BUGGIFY ) ROCKSDB_ENABLE_CLEAR_RANGE_EAGER_READS = deterministicRandom()->coinflip();
	// ROCKSDB_STATS_LEVEL=1 indicates rocksdb::StatsLevel::kExceptHistogramOrTimers
	init( ROCKSDB_STATS_LEVEL,                                     1 ); if( randomize && BUGGIFY ) ROCKSDB_STATS_LEVEL = deterministicRandom()->randomInt(0, 6);
	// Can commit will delay ROCKSDB_CAN_COMMIT_DELAY_ON_OVERLOAD seconds for
	// ROCKSDB_CAN_COMMIT_DELAY_TIMES_ON_OVERLOAD times, if rocksdb overloaded.
	// Set ROCKSDB_CAN_COMMIT_DELAY_TIMES_ON_OVERLOAD to 0, to disable
	init( ROCKSDB_CAN_COMMIT_DELAY_ON_OVERLOAD,                    1 );
	init( ROCKSDB_CAN_COMMIT_DELAY_TIMES_ON_OVERLOAD,              5 );
	init( ROCKSDB_COMPACTION_READAHEAD_SIZE,                   32768 ); // 32 KB, performs bigger reads when doing compaction.
	init( ROCKSDB_BLOCK_SIZE,                                  32768 ); // 32 KB, size of the block in rocksdb cache.
 	init( ENABLE_SHARDED_ROCKSDB,                              false );
	init( ROCKSDB_WRITE_BUFFER_SIZE,                         1 << 30 ); // 1G
	init( ROCKSDB_CF_WRITE_BUFFER_SIZE,                     64 << 20 ); // 64M, RocksDB default.
	init( ROCKSDB_MAX_TOTAL_WAL_SIZE,                              0 ); // RocksDB default.
	init( ROCKSDB_MAX_BACKGROUND_JOBS,                             2 ); // RocksDB default.
	init( ROCKSDB_DELETE_OBSOLETE_FILE_PERIOD,                 21600 ); // 6h, RocksDB default.
	init( ROCKSDB_PHYSICAL_SHARD_CLEAN_UP_DELAY, isSimulated ? 10.0 : 300.0 ); // Delays shard clean up, must be larger than ROCKSDB_READ_VALUE_TIMEOUT to prevent reading deleted shard.

	// Leader election
	bool longLeaderElection = randomize && BUGGIFY;
	init( MAX_NOTIFICATIONS,                                  100000 );
	init( MIN_NOTIFICATIONS,                                     100 );
	init( NOTIFICATION_FULL_CLEAR_TIME,                      10000.0 );
	init( CANDIDATE_MIN_DELAY,                                  0.05 );
	init( CANDIDATE_MAX_DELAY,                                   1.0 );
	init( CANDIDATE_GROWTH_RATE,                                 1.2 );
	init( POLLING_FREQUENCY,                                     2.0 ); if( longLeaderElection ) POLLING_FREQUENCY = 8.0;
	init( HEARTBEAT_FREQUENCY,                                   0.5 ); if( longLeaderElection ) HEARTBEAT_FREQUENCY = 1.0;

	// Commit CommitProxy and GRV CommitProxy
	init( START_TRANSACTION_BATCH_INTERVAL_MIN,                 1e-6 );
	init( START_TRANSACTION_BATCH_INTERVAL_MAX,                0.010 );
	init( START_TRANSACTION_BATCH_INTERVAL_LATENCY_FRACTION,     0.5 );
	init( START_TRANSACTION_BATCH_INTERVAL_SMOOTHER_ALPHA,       0.1 );
	init( START_TRANSACTION_BATCH_QUEUE_CHECK_INTERVAL,        0.001 );
	init( START_TRANSACTION_MAX_TRANSACTIONS_TO_START,        100000 );
	init( START_TRANSACTION_MAX_REQUESTS_TO_START,             10000 );
	init( START_TRANSACTION_RATE_WINDOW,                         2.0 );
	init( START_TRANSACTION_MAX_EMPTY_QUEUE_BUDGET,             10.0 );
	init( START_TRANSACTION_MAX_QUEUE_SIZE,                      1e6 );
	init( KEY_LOCATION_MAX_QUEUE_SIZE,                           1e6 );
	init( COMMIT_PROXY_LIVENESS_TIMEOUT,                        20.0 );

	init( COMMIT_TRANSACTION_BATCH_INTERVAL_FROM_IDLE,         0.0005 ); if( randomize && BUGGIFY ) COMMIT_TRANSACTION_BATCH_INTERVAL_FROM_IDLE = 0.005;
	init( COMMIT_TRANSACTION_BATCH_INTERVAL_MIN,                0.001 ); if( randomize && BUGGIFY ) COMMIT_TRANSACTION_BATCH_INTERVAL_MIN = 0.1;
	init( COMMIT_TRANSACTION_BATCH_INTERVAL_MAX,                0.020 );
	init( COMMIT_TRANSACTION_BATCH_INTERVAL_LATENCY_FRACTION,     0.1 );
	init( COMMIT_TRANSACTION_BATCH_INTERVAL_SMOOTHER_ALPHA,       0.1 );
	init( COMMIT_TRANSACTION_BATCH_COUNT_MAX,                   32768 ); if( randomize && BUGGIFY ) COMMIT_TRANSACTION_BATCH_COUNT_MAX = 1000; // Do NOT increase this number beyond 32768, as CommitIds only budget 2 bytes for storing transaction id within each batch
	init( COMMIT_BATCHES_MEM_BYTES_HARD_LIMIT,              8LL << 30 ); if (randomize && BUGGIFY) COMMIT_BATCHES_MEM_BYTES_HARD_LIMIT = deterministicRandom()->randomInt64(100LL << 20,  8LL << 30);
	init( COMMIT_BATCHES_MEM_FRACTION_OF_TOTAL,                   0.5 );
	init( COMMIT_BATCHES_MEM_TO_TOTAL_MEM_SCALE_FACTOR,           5.0 );

	// these settings disable batch bytes scaling.  Try COMMIT_TRANSACTION_BATCH_BYTES_MAX=1e6, COMMIT_TRANSACTION_BATCH_BYTES_SCALE_BASE=50000, COMMIT_TRANSACTION_BATCH_BYTES_SCALE_POWER=0.5?
	init( COMMIT_TRANSACTION_BATCH_BYTES_MIN,                  100000 );
	init( COMMIT_TRANSACTION_BATCH_BYTES_MAX,                  100000 ); if( randomize && BUGGIFY ) { COMMIT_TRANSACTION_BATCH_BYTES_MIN = COMMIT_TRANSACTION_BATCH_BYTES_MAX = 1000000; }
	init( COMMIT_TRANSACTION_BATCH_BYTES_SCALE_BASE,           100000 );
	init( COMMIT_TRANSACTION_BATCH_BYTES_SCALE_POWER,             0.0 );

	init( RESOLVER_COALESCE_TIME,                                1.0 );
	init( BUGGIFIED_ROW_LIMIT,                  APPLY_MUTATION_BYTES ); if( randomize && BUGGIFY ) BUGGIFIED_ROW_LIMIT = deterministicRandom()->randomInt(3, 30);
	init( PROXY_SPIN_DELAY,                                     0.01 );
	init( UPDATE_REMOTE_LOG_VERSION_INTERVAL,                    2.0 );
	init( MAX_TXS_POP_VERSION_HISTORY,                           1e5 );
	init( MIN_CONFIRM_INTERVAL,                                 0.05 );

	bool shortRecoveryDuration = randomize && BUGGIFY;
	init( ENFORCED_MIN_RECOVERY_DURATION,                       0.085 ); if( shortRecoveryDuration ) ENFORCED_MIN_RECOVERY_DURATION = 0.01;
	init( REQUIRED_MIN_RECOVERY_DURATION,                       0.080 ); if( shortRecoveryDuration ) REQUIRED_MIN_RECOVERY_DURATION = 0.01;
	init( ALWAYS_CAUSAL_READ_RISKY,                             false );
	init( MAX_COMMIT_UPDATES,                                    2000 ); if( randomize && BUGGIFY ) MAX_COMMIT_UPDATES = 1;
	init( MAX_PROXY_COMPUTE,                                      2.0 );
	init( MAX_COMPUTE_PER_OPERATION,                              0.1 );
	init( MAX_COMPUTE_DURATION_LOG_CUTOFF,                       0.05 );
	init( PROXY_COMPUTE_BUCKETS,                                20000 );
	init( PROXY_COMPUTE_GROWTH_RATE,                             0.01 );
	init( TXN_STATE_SEND_AMOUNT,                                    4 );
	init( REPORT_TRANSACTION_COST_ESTIMATION_DELAY,               0.1 );
	init( PROXY_REJECT_BATCH_QUEUED_TOO_LONG,                    true );

	bool buggfyUseResolverPrivateMutations = randomize && BUGGIFY && !ENABLE_VERSION_VECTOR_TLOG_UNICAST;
	init( PROXY_USE_RESOLVER_PRIVATE_MUTATIONS,                 false ); if( buggfyUseResolverPrivateMutations ) PROXY_USE_RESOLVER_PRIVATE_MUTATIONS = deterministicRandom()->coinflip();

	init( RESET_MASTER_BATCHES,                                   200 );
	init( RESET_RESOLVER_BATCHES,                                 200 );
	init( RESET_MASTER_DELAY,                                   300.0 );
	init( RESET_RESOLVER_DELAY,                                 300.0 );

	init( GLOBAL_CONFIG_MIGRATE_TIMEOUT,                          5.0 );
	init( GLOBAL_CONFIG_REFRESH_INTERVAL,                         1.0 ); if ( randomize && BUGGIFY ) GLOBAL_CONFIG_REFRESH_INTERVAL = 0.1;
	init( GLOBAL_CONFIG_REFRESH_TIMEOUT,                         10.0 ); if ( randomize && BUGGIFY ) GLOBAL_CONFIG_REFRESH_TIMEOUT = 1.0;

	// Master Server
	// masterCommitter() in the master server will allow lower priority tasks (e.g. DataDistibution)
	//  by delay()ing for this amount of time between accepted batches of TransactionRequests.
	bool fastBalancing = randomize && BUGGIFY;
	init( COMMIT_SLEEP_TIME,								  0.0001 ); if( randomize && BUGGIFY ) COMMIT_SLEEP_TIME = 0;
	init( KEY_BYTES_PER_SAMPLE,                                  2e4 ); if( fastBalancing ) KEY_BYTES_PER_SAMPLE = 1e3;
	init( MIN_BALANCE_TIME,                                      0.2 );
	init( MIN_BALANCE_DIFFERENCE,                                1e6 ); if( fastBalancing ) MIN_BALANCE_DIFFERENCE = 1e4;
	init( SECONDS_BEFORE_NO_FAILURE_DELAY,                  8 * 3600 );
	init( MAX_TXS_SEND_MEMORY,                                   1e7 ); if( randomize && BUGGIFY ) MAX_TXS_SEND_MEMORY = 1e5;
	init( MAX_RECOVERY_VERSIONS,           200 * VERSIONS_PER_SECOND );
	init( MAX_RECOVERY_TIME,                                    20.0 ); if( randomize && BUGGIFY ) MAX_RECOVERY_TIME = 1.0;
	init( PROVISIONAL_START_DELAY,                               1.0 );
	init( PROVISIONAL_MAX_DELAY,                                60.0 );
	init( PROVISIONAL_DELAY_GROWTH,                              1.5 );
	init( SECONDS_BEFORE_RECRUIT_BACKUP_WORKER,                  4.0 ); if( randomize && BUGGIFY ) SECONDS_BEFORE_RECRUIT_BACKUP_WORKER = deterministicRandom()->random01() * 8;
	init( CC_INTERFACE_TIMEOUT,                                 10.0 ); if( randomize && BUGGIFY ) CC_INTERFACE_TIMEOUT = 0.0;

	// Resolver
	init( SAMPLE_OFFSET_PER_KEY,                                 100 );
	init( SAMPLE_EXPIRATION_TIME,                                1.0 );
	init( SAMPLE_POLL_TIME,                                      0.1 );
	init( RESOLVER_STATE_MEMORY_LIMIT,                           1e6 );
	init( LAST_LIMITED_RATIO,                                    2.0 );

	// Backup Worker
	init( BACKUP_TIMEOUT,                                        0.4 );
	init( BACKUP_NOOP_POP_DELAY,                                 5.0 );
	init( BACKUP_FILE_BLOCK_BYTES,                       1024 * 1024 );
	init( BACKUP_LOCK_BYTES,                                     3e9 ); if(randomize && BUGGIFY) BACKUP_LOCK_BYTES = deterministicRandom()->randomInt(1024, 4096) * 15 * 1024;
	init( BACKUP_UPLOAD_DELAY,                                  10.0 ); if(randomize && BUGGIFY) BACKUP_UPLOAD_DELAY = deterministicRandom()->random01() * 60;

	//Cluster Controller
	init( CLUSTER_CONTROLLER_LOGGING_DELAY,                      5.0 );
	init( MASTER_FAILURE_REACTION_TIME,                          0.4 ); if( randomize && BUGGIFY ) MASTER_FAILURE_REACTION_TIME = 10.0;
	init( MASTER_FAILURE_SLOPE_DURING_RECOVERY,                  0.1 );
	init( WORKER_COORDINATION_PING_DELAY,                         60 );
	init( SIM_SHUTDOWN_TIMEOUT,                                   10 );
	init( SHUTDOWN_TIMEOUT,                                      600 ); if( randomize && BUGGIFY ) SHUTDOWN_TIMEOUT = 60.0;
	init( MASTER_SPIN_DELAY,                                     1.0 ); if( randomize && BUGGIFY ) MASTER_SPIN_DELAY = 10.0;
	init( CC_PRUNE_CLIENTS_INTERVAL,                            60.0 );
	init( CC_CHANGE_DELAY,                                       0.1 );
	init( CC_CLASS_DELAY,                                       0.01 );
	init( WAIT_FOR_GOOD_RECRUITMENT_DELAY,                       1.0 );
	init( WAIT_FOR_GOOD_REMOTE_RECRUITMENT_DELAY,                5.0 );
	init( ATTEMPT_RECRUITMENT_DELAY,                           0.035 );
	init( WAIT_FOR_DISTRIBUTOR_JOIN_DELAY,                       1.0 );
	init( WAIT_FOR_RATEKEEPER_JOIN_DELAY,                        1.0 );
	init( WAIT_FOR_CONSISTENCYSCAN_JOIN_DELAY,                   1.0 );
	init( WAIT_FOR_BLOB_MANAGER_JOIN_DELAY,                      1.0 );
	init( WAIT_FOR_ENCRYPT_KEY_PROXY_JOIN_DELAY,                 1.0 );
	init( WORKER_FAILURE_TIME,                                   1.0 ); if( randomize && BUGGIFY ) WORKER_FAILURE_TIME = 10.0;
	init( CHECK_OUTSTANDING_INTERVAL,                            0.5 ); if( randomize && BUGGIFY ) CHECK_OUTSTANDING_INTERVAL = 0.001;
	init( VERSION_LAG_METRIC_INTERVAL,                           0.5 ); if( randomize && BUGGIFY ) VERSION_LAG_METRIC_INTERVAL = 10.0;
	init( MAX_VERSION_DIFFERENCE,           20 * VERSIONS_PER_SECOND );
	init( INITIAL_UPDATE_CROSS_DC_INFO_DELAY,                    300 );
	init( CHECK_REMOTE_HEALTH_INTERVAL,                           60 );
	init( FORCE_RECOVERY_CHECK_DELAY,                            5.0 );
	init( RATEKEEPER_FAILURE_TIME,                               1.0 );
	init( CONSISTENCYSCAN_FAILURE_TIME,                          1.0 );
	init( BLOB_MANAGER_FAILURE_TIME,                             1.0 );
	init( BLOB_MIGRATOR_FAILURE_TIME,                            1.0 );
	init( REPLACE_INTERFACE_DELAY,                              60.0 );
	init( REPLACE_INTERFACE_CHECK_DELAY,                         5.0 );
	init( COORDINATOR_REGISTER_INTERVAL,                         5.0 );
	init( CLIENT_REGISTER_INTERVAL,                            600.0 );
	init( CC_ENABLE_WORKER_HEALTH_MONITOR,                     false );
	init( CC_WORKER_HEALTH_CHECKING_INTERVAL,                   60.0 );
	init( CC_DEGRADED_LINK_EXPIRATION_INTERVAL,                300.0 );
	init( CC_MIN_DEGRADATION_INTERVAL,                         120.0 );
	init( ENCRYPT_KEY_PROXY_FAILURE_TIME,                        0.1 );
	init( CC_DEGRADED_PEER_DEGREE_TO_EXCLUDE,                      3 );
	init( CC_MAX_EXCLUSION_DUE_TO_HEALTH,                          2 );
	init( CC_HEALTH_TRIGGER_RECOVERY,                          false );
	init( CC_TRACKING_HEALTH_RECOVERY_INTERVAL,               3600.0 );
	init( CC_MAX_HEALTH_RECOVERY_COUNT,                            5 );
	init( CC_HEALTH_TRIGGER_FAILOVER,                          false );
	init( CC_FAILOVER_DUE_TO_HEALTH_MIN_DEGRADATION,               5 );
	init( CC_FAILOVER_DUE_TO_HEALTH_MAX_DEGRADATION,              10 );
	init( CC_ENABLE_ENTIRE_SATELLITE_MONITORING,               false );
	init( CC_SATELLITE_DEGRADATION_MIN_COMPLAINER,                 3 );
	init( CC_SATELLITE_DEGRADATION_MIN_BAD_SERVER,                 3 );
	init( CC_THROTTLE_SINGLETON_RERECRUIT_INTERVAL,              0.5 );

	init( INCOMPATIBLE_PEERS_LOGGING_INTERVAL,                   600 ); if( randomize && BUGGIFY ) INCOMPATIBLE_PEERS_LOGGING_INTERVAL = 60.0;
	init( EXPECTED_MASTER_FITNESS,            ProcessClass::UnsetFit );
	init( EXPECTED_TLOG_FITNESS,              ProcessClass::UnsetFit );
	init( EXPECTED_LOG_ROUTER_FITNESS,        ProcessClass::UnsetFit );
	init( EXPECTED_COMMIT_PROXY_FITNESS,      ProcessClass::UnsetFit );
	init( EXPECTED_GRV_PROXY_FITNESS,         ProcessClass::UnsetFit );
	init( EXPECTED_RESOLVER_FITNESS,          ProcessClass::UnsetFit );
	init( RECRUITMENT_TIMEOUT,                                   600 ); if( randomize && BUGGIFY ) RECRUITMENT_TIMEOUT = deterministicRandom()->coinflip() ? 60.0 : 1.0;

	init( POLICY_RATING_TESTS,                                   200 ); if( randomize && BUGGIFY ) POLICY_RATING_TESTS = 20;
	init( POLICY_GENERATIONS,                                    100 ); if( randomize && BUGGIFY ) POLICY_GENERATIONS = 10;
	init( DBINFO_SEND_AMOUNT,                                      5 );
	init( DBINFO_BATCH_DELAY,                                    0.1 );
	init( SINGLETON_RECRUIT_BME_DELAY,                          10.0 );

	//Move Keys
	init( SHARD_READY_DELAY,                                    0.25 );
	init( SERVER_READY_QUORUM_INTERVAL,                         std::min(1.0, std::min(MAX_READ_TRANSACTION_LIFE_VERSIONS, MAX_WRITE_TRANSACTION_LIFE_VERSIONS)/(5.0*VERSIONS_PER_SECOND)) );
	init( SERVER_READY_QUORUM_TIMEOUT,                          15.0 ); if( randomize && BUGGIFY ) SERVER_READY_QUORUM_TIMEOUT = 1.0;
	init( REMOVE_RETRY_DELAY,                                    1.0 );
	init( MOVE_KEYS_KRM_LIMIT,                                  2000 ); if( randomize && BUGGIFY ) MOVE_KEYS_KRM_LIMIT = 2;
	init( MOVE_KEYS_KRM_LIMIT_BYTES,                             1e5 ); if( randomize && BUGGIFY ) MOVE_KEYS_KRM_LIMIT_BYTES = 5e4; //This must be sufficiently larger than CLIENT_KNOBS->KEY_SIZE_LIMIT (fdbclient/Knobs.h) to ensure that at least two entries will be returned from an attempt to read a key range map
	init( MOVE_SHARD_KRM_ROW_LIMIT,                            20000 );
 	init( MOVE_SHARD_KRM_BYTE_LIMIT,                             1e6 );
	init( MAX_SKIP_TAGS,                                           1 ); //The TLogs require tags to be densely packed to be memory efficient, so be careful increasing this knob
	init( MAX_ADDED_SOURCES_MULTIPLIER,                          2.0 );

	//FdbServer
	bool longReboots = randomize && BUGGIFY;
	init( MIN_REBOOT_TIME,                                       4.0 ); if( longReboots ) MIN_REBOOT_TIME = 10.0;
	init( MAX_REBOOT_TIME,                                       5.0 ); if( longReboots ) MAX_REBOOT_TIME = 20.0;
	init( LOG_DIRECTORY,                                          ".");  // Will be set to the command line flag.
	init( CONN_FILE,                                               "");  // Will be set to the command line flag.
	init( SERVER_MEM_LIMIT,                                8LL << 30 );
	init( SYSTEM_MONITOR_FREQUENCY,                              5.0 );

	//Ratekeeper
	bool slowRatekeeper = randomize && BUGGIFY;
	init( SMOOTHING_AMOUNT,                                      1.0 ); if( slowRatekeeper ) SMOOTHING_AMOUNT = 5.0;
	init( SLOW_SMOOTHING_AMOUNT,                                10.0 ); if( slowRatekeeper ) SLOW_SMOOTHING_AMOUNT = 50.0;
	init( METRIC_UPDATE_RATE,                                     .1 ); if( slowRatekeeper ) METRIC_UPDATE_RATE = 0.5;
	init( DETAILED_METRIC_UPDATE_RATE,                           5.0 );
	init( RATEKEEPER_DEFAULT_LIMIT,                              1e6 ); if( randomize && BUGGIFY ) RATEKEEPER_DEFAULT_LIMIT = 0;
	init( RATEKEEPER_LIMIT_REASON_SAMPLE_RATE,                   0.1 );
	init( RATEKEEPER_PRINT_LIMIT_REASON,                       false ); if( randomize && BUGGIFY ) RATEKEEPER_PRINT_LIMIT_REASON = true;
	init( RATEKEEPER_MIN_RATE,                                   0.0 );
	init( RATEKEEPER_MAX_RATE,                                   1e9 );
	init( RATEKEEPER_BATCH_MIN_RATE,                             0.0 );
	init( RATEKEEPER_BATCH_MAX_RATE,                             1e9 );

	bool smallStorageTarget = randomize && BUGGIFY;
	init( TARGET_BYTES_PER_STORAGE_SERVER,                    1000e6 ); if( smallStorageTarget ) TARGET_BYTES_PER_STORAGE_SERVER = 3000e3;
	init( SPRING_BYTES_STORAGE_SERVER,                         100e6 ); if( smallStorageTarget ) SPRING_BYTES_STORAGE_SERVER = 300e3;
	init( AUTO_TAG_THROTTLE_STORAGE_QUEUE_BYTES,               800e6 ); if( smallStorageTarget ) AUTO_TAG_THROTTLE_STORAGE_QUEUE_BYTES = 2500e3;
	init( TARGET_BYTES_PER_STORAGE_SERVER_BATCH,               750e6 ); if( smallStorageTarget ) TARGET_BYTES_PER_STORAGE_SERVER_BATCH = 1500e3;
	init( SPRING_BYTES_STORAGE_SERVER_BATCH,                   100e6 ); if( smallStorageTarget ) SPRING_BYTES_STORAGE_SERVER_BATCH = 150e3;
	init( STORAGE_HARD_LIMIT_BYTES,                           1500e6 ); if( smallStorageTarget ) STORAGE_HARD_LIMIT_BYTES = 4500e3;
	init( STORAGE_HARD_LIMIT_BYTES_OVERAGE,                   5000e3 ); if( smallStorageTarget ) STORAGE_HARD_LIMIT_BYTES_OVERAGE = 100e3; // byte+version overage ensures storage server makes enough progress on freeing up storage queue memory at hard limit by ensuring it advances desiredOldestVersion enough per commit cycle.
	init( STORAGE_HARD_LIMIT_BYTES_SPEED_UP_SIM, STORAGE_HARD_LIMIT_BYTES ); if( smallStorageTarget ) STORAGE_HARD_LIMIT_BYTES_SPEED_UP_SIM *= 10;
	init( STORAGE_HARD_LIMIT_BYTES_OVERAGE_SPEED_UP_SIM, STORAGE_HARD_LIMIT_BYTES_OVERAGE ); if( smallStorageTarget ) STORAGE_HARD_LIMIT_BYTES_OVERAGE_SPEED_UP_SIM *= 10;
	init( STORAGE_HARD_LIMIT_VERSION_OVERAGE, VERSIONS_PER_SECOND / 4.0 );
	init( STORAGE_DURABILITY_LAG_HARD_MAX,                    2000e6 ); if( smallStorageTarget ) STORAGE_DURABILITY_LAG_HARD_MAX = 100e6;
	init( STORAGE_DURABILITY_LAG_SOFT_MAX,                     250e6 ); if( smallStorageTarget ) STORAGE_DURABILITY_LAG_SOFT_MAX = 10e6;
	init( STORAGE_INCLUDE_FEED_STORAGE_QUEUE,                   true ); if ( randomize && BUGGIFY ) STORAGE_INCLUDE_FEED_STORAGE_QUEUE = false;

	//FIXME: Low priority reads are disabled by assigning very high knob values, reduce knobs for 7.0
	init( LOW_PRIORITY_STORAGE_QUEUE_BYTES,                    775e8 ); if( smallStorageTarget ) LOW_PRIORITY_STORAGE_QUEUE_BYTES = 1750e3;
	init( LOW_PRIORITY_DURABILITY_LAG,                         200e6 ); if( smallStorageTarget ) LOW_PRIORITY_DURABILITY_LAG = 15e6;

	bool smallTlogTarget = randomize && BUGGIFY;
	init( TARGET_BYTES_PER_TLOG,                              2400e6 ); if( smallTlogTarget ) TARGET_BYTES_PER_TLOG = 2000e3;
	init( SPRING_BYTES_TLOG,                                   400e6 ); if( smallTlogTarget ) SPRING_BYTES_TLOG = 200e3;
	init( TARGET_BYTES_PER_TLOG_BATCH,                        1400e6 ); if( smallTlogTarget ) TARGET_BYTES_PER_TLOG_BATCH = 1400e3;
	init( SPRING_BYTES_TLOG_BATCH,                             300e6 ); if( smallTlogTarget ) SPRING_BYTES_TLOG_BATCH = 150e3;
	init( TLOG_SPILL_THRESHOLD,                               1500e6 ); if( smallTlogTarget ) TLOG_SPILL_THRESHOLD = 1500e3; if( randomize && BUGGIFY ) TLOG_SPILL_THRESHOLD = 0;
	init( REFERENCE_SPILL_UPDATE_STORAGE_BYTE_LIMIT,            20e6 ); if( (randomize && BUGGIFY) || smallTlogTarget ) REFERENCE_SPILL_UPDATE_STORAGE_BYTE_LIMIT = 1e6;
	init( TLOG_HARD_LIMIT_BYTES,                              3000e6 ); if( smallTlogTarget ) TLOG_HARD_LIMIT_BYTES = 30e6;
	init( TLOG_RECOVER_MEMORY_LIMIT, TARGET_BYTES_PER_TLOG + SPRING_BYTES_TLOG );

	init( MAX_TRANSACTIONS_PER_BYTE,                            1000 );

	init( MIN_AVAILABLE_SPACE,                                   1e8 );
	init( MIN_AVAILABLE_SPACE_RATIO,                            0.05 );
	init( MIN_AVAILABLE_SPACE_RATIO_SAFETY_BUFFER,              0.01 );
	init( TARGET_AVAILABLE_SPACE_RATIO,                         0.30 );
	init( AVAILABLE_SPACE_UPDATE_DELAY,                          5.0 );

	init( MAX_TL_SS_VERSION_DIFFERENCE,                         1e99 ); // if( randomize && BUGGIFY ) MAX_TL_SS_VERSION_DIFFERENCE = std::max(1.0, 0.25 * VERSIONS_PER_SECOND); // spring starts at half this value //FIXME: this knob causes ratekeeper to clamp on idle cluster in simulation that have a large number of logs
	init( MAX_TL_SS_VERSION_DIFFERENCE_BATCH,                   1e99 );
	init( MAX_MACHINES_FALLING_BEHIND,                             1 );

	init( MAX_TPS_HISTORY_SAMPLES,                               600 );
	init( NEEDED_TPS_HISTORY_SAMPLES,                            200 );
	init( TARGET_DURABILITY_LAG_VERSIONS,                      350e6 ); // Should be larger than STORAGE_DURABILITY_LAG_SOFT_MAX
	init( AUTO_TAG_THROTTLE_DURABILITY_LAG_VERSIONS,           250e6 );
	init( TARGET_DURABILITY_LAG_VERSIONS_BATCH,                150e6 ); // Should be larger than STORAGE_DURABILITY_LAG_SOFT_MAX
	init( DURABILITY_LAG_UNLIMITED_THRESHOLD,                   50e6 );
	init( INITIAL_DURABILITY_LAG_MULTIPLIER,                    1.02 );
	init( DURABILITY_LAG_REDUCTION_RATE,                      0.9999 );
	init( DURABILITY_LAG_INCREASE_RATE,                        1.001 );
	init( STORAGE_SERVER_LIST_FETCH_TIMEOUT,                    20.0 );
	init( BW_THROTTLING_ENABLED,                                true );

	bool buggifySmallBWLag = randomize && BUGGIFY;
	init( TARGET_BW_LAG,                                       240.0 ); if(buggifySmallBWLag) TARGET_BW_LAG = 10.0;
	init( TARGET_BW_LAG_BATCH,                                 200.0 ); if(buggifySmallBWLag) TARGET_BW_LAG_BATCH = 4.0;
	init( TARGET_BW_LAG_UPDATE,                                  9.0 ); if(buggifySmallBWLag) TARGET_BW_LAG_UPDATE = 1.0;
	init( MIN_BW_HISTORY,                                         10 );
	init( BW_ESTIMATION_INTERVAL,                               10.0 ); if(buggifySmallBWLag) BW_ESTIMATION_INTERVAL = 2.0;
	init( BW_LAG_INCREASE_AMOUNT,                                1.1 );
	init( BW_LAG_DECREASE_AMOUNT,                                0.9 );
	init( BW_FETCH_WORKERS_INTERVAL,                             5.0 );
	init( BW_RW_LOGGING_INTERVAL,                                5.0 );
	init( BW_MAX_BLOCKED_INTERVAL,                              10.0 ); if(buggifySmallBWLag) BW_MAX_BLOCKED_INTERVAL = 2.0;
	init( BW_RK_SIM_QUIESCE_DELAY,                             150.0 );

	init( MAX_AUTO_THROTTLED_TRANSACTION_TAGS,                     5 ); if(randomize && BUGGIFY) MAX_AUTO_THROTTLED_TRANSACTION_TAGS = 1;
	init( MAX_MANUAL_THROTTLED_TRANSACTION_TAGS,                  40 ); if(randomize && BUGGIFY) MAX_MANUAL_THROTTLED_TRANSACTION_TAGS = 1;
	init( MIN_TAG_COST,                                          200 ); if(randomize && BUGGIFY) MIN_TAG_COST = 0.0;
	init( AUTO_THROTTLE_TARGET_TAG_BUSYNESS,                     0.1 ); if(randomize && BUGGIFY) AUTO_THROTTLE_TARGET_TAG_BUSYNESS = 0.0;
	init( AUTO_TAG_THROTTLE_RAMP_UP_TIME,                      120.0 ); if(randomize && BUGGIFY) AUTO_TAG_THROTTLE_RAMP_UP_TIME = 5.0;
	init( AUTO_TAG_THROTTLE_DURATION,                          240.0 ); if(randomize && BUGGIFY) AUTO_TAG_THROTTLE_DURATION = 20.0;
	init( TAG_THROTTLE_PUSH_INTERVAL,                            1.0 ); if(randomize && BUGGIFY) TAG_THROTTLE_PUSH_INTERVAL = 0.0;
	init( AUTO_TAG_THROTTLE_START_AGGREGATION_TIME,              5.0 ); if(randomize && BUGGIFY) AUTO_TAG_THROTTLE_START_AGGREGATION_TIME = 0.5;
	init( AUTO_TAG_THROTTLE_UPDATE_FREQUENCY,                   10.0 ); if(randomize && BUGGIFY) AUTO_TAG_THROTTLE_UPDATE_FREQUENCY = 0.5;
	init( TAG_THROTTLE_EXPIRED_CLEANUP_INTERVAL,                30.0 ); if(randomize && BUGGIFY) TAG_THROTTLE_EXPIRED_CLEANUP_INTERVAL = 1.0;
	init( AUTO_TAG_THROTTLING_ENABLED,                          true ); if(randomize && BUGGIFY) AUTO_TAG_THROTTLING_ENABLED = false;
	init( SS_THROTTLE_TAGS_TRACKED,                                1 ); if(randomize && BUGGIFY) SS_THROTTLE_TAGS_TRACKED = deterministicRandom()->randomInt(1, 10);
	init( GLOBAL_TAG_THROTTLING,                               false ); if(isSimulated) GLOBAL_TAG_THROTTLING = deterministicRandom()->coinflip();
	init( ENFORCE_TAG_THROTTLING_ON_PROXIES,   GLOBAL_TAG_THROTTLING );
	init( GLOBAL_TAG_THROTTLING_MIN_RATE,                        1.0 );
	// 60 seconds was chosen as a default value to ensure that
	// the global tag throttler does not react too drastically to
	// changes in workload. To make the global tag throttler more reactive,
	// lower this knob. To make global tag throttler more smooth, raise this knob.
	// Setting this knob lower than TAG_MEASUREMENT_INTERVAL can cause erratic
	// behaviour and is not recommended.
	init( GLOBAL_TAG_THROTTLING_FOLDING_TIME,                   60.0 );
	init( GLOBAL_TAG_THROTTLING_MAX_TAGS_TRACKED,                 10 );
	init( GLOBAL_TAG_THROTTLING_TAG_EXPIRE_AFTER,              240.0 );
	init( PROXY_MAX_TAG_THROTTLE_DURATION,          5.0 ); if( randomize && BUGGIFY ) PROXY_MAX_TAG_THROTTLE_DURATION = 0.5;
	init( GLOBAL_TAG_THROTTLING_PROXY_LOGGING_INTERVAL,         60.0 );

	//Storage Metrics
	init( STORAGE_METRICS_AVERAGE_INTERVAL,                    120.0 );
	init( STORAGE_METRICS_AVERAGE_INTERVAL_PER_KSECONDS,        1000.0 / STORAGE_METRICS_AVERAGE_INTERVAL );  // milliHz!
	init( SPLIT_JITTER_AMOUNT,                                  0.05 ); if( randomize && BUGGIFY ) SPLIT_JITTER_AMOUNT = 0.2;
	init( IOPS_UNITS_PER_SAMPLE,                                10000 * 1000 / STORAGE_METRICS_AVERAGE_INTERVAL_PER_KSECONDS / 100 );
	init( BANDWIDTH_UNITS_PER_SAMPLE,                           SHARD_MIN_BYTES_PER_KSEC / STORAGE_METRICS_AVERAGE_INTERVAL_PER_KSECONDS / 25 );
	init( BYTES_READ_UNITS_PER_SAMPLE,                          100000 ); // 100K bytes
	init( READ_HOT_SUB_RANGE_CHUNK_SIZE,                        10000000); // 10MB
	init( EMPTY_READ_PENALTY,                                   20 ); // 20 bytes
	init( DD_SHARD_COMPARE_LIMIT,                               1000 );
	init( READ_SAMPLING_ENABLED,                                false ); if ( randomize && BUGGIFY ) READ_SAMPLING_ENABLED = true;// enable/disable read sampling

	//Storage Server
	init( STORAGE_LOGGING_DELAY,                                 5.0 );
	init( STORAGE_SERVER_POLL_METRICS_DELAY,                     1.0 );
	init( FUTURE_VERSION_DELAY,                                  1.0 );
	init( STORAGE_LIMIT_BYTES,                                500000 );
	init( BUGGIFY_LIMIT_BYTES,                                  1000 );
	init( FETCH_USING_STREAMING,                               false ); if( randomize && isSimulated && BUGGIFY ) FETCH_USING_STREAMING = true; //Determines if fetch keys uses streaming reads
	init( FETCH_USING_BLOB,                                    false );
	init( FETCH_BLOCK_BYTES,                                     2e6 );
	init( FETCH_KEYS_PARALLELISM_BYTES,                          4e6 ); if( randomize && BUGGIFY ) FETCH_KEYS_PARALLELISM_BYTES = 3e6;
	init( FETCH_KEYS_PARALLELISM,                                  2 );
	init( FETCH_KEYS_PARALLELISM_FULL,                             6 );
	init( FETCH_KEYS_LOWER_PRIORITY,                               0 );
	init( SERVE_FETCH_CHECKPOINT_PARALLELISM,                      4 );
	init( SERVE_AUDIT_STORAGE_PARALLELISM,                         1 );
	init( BUGGIFY_BLOCK_BYTES,                                 10000 );
	init( STORAGE_RECOVERY_VERSION_LAG_LIMIT,				2 * MAX_READ_TRANSACTION_LIFE_VERSIONS );
	init( STORAGE_COMMIT_BYTES,                             10000000 ); if( randomize && BUGGIFY ) STORAGE_COMMIT_BYTES = 2000000;
	init( STORAGE_FETCH_BYTES,                               2500000 ); if( randomize && BUGGIFY ) STORAGE_FETCH_BYTES =  500000;
	init( STORAGE_DURABILITY_LAG_REJECT_THRESHOLD,              0.25 );
	init( STORAGE_DURABILITY_LAG_MIN_RATE,                       0.1 );
	init( STORAGE_COMMIT_INTERVAL,                               0.5 ); if( randomize && BUGGIFY ) STORAGE_COMMIT_INTERVAL = 2.0;
	init( BYTE_SAMPLING_FACTOR,                                  250 ); //cannot buggify because of differences in restarting tests
	init( BYTE_SAMPLING_OVERHEAD,                                100 );
	init( MAX_STORAGE_SERVER_WATCH_BYTES,                      100e6 ); if( randomize && BUGGIFY ) MAX_STORAGE_SERVER_WATCH_BYTES = 10e3;
	init( MAX_BYTE_SAMPLE_CLEAR_MAP_SIZE,                        1e9 ); if( randomize && BUGGIFY ) MAX_BYTE_SAMPLE_CLEAR_MAP_SIZE = 1e3;
	init( LONG_BYTE_SAMPLE_RECOVERY_DELAY,                      60.0 );
	init( BYTE_SAMPLE_LOAD_PARALLELISM,                            8 ); if( randomize && BUGGIFY ) BYTE_SAMPLE_LOAD_PARALLELISM = 1;
	init( BYTE_SAMPLE_LOAD_DELAY,                                0.0 ); if( randomize && BUGGIFY ) BYTE_SAMPLE_LOAD_DELAY = 0.1;
	init( BYTE_SAMPLE_START_DELAY,                               1.0 ); if( randomize && BUGGIFY ) BYTE_SAMPLE_START_DELAY = 0.0;
	init( BEHIND_CHECK_DELAY,                                    2.0 );
	init( BEHIND_CHECK_COUNT,                                      2 );
	init( BEHIND_CHECK_VERSIONS,             5 * VERSIONS_PER_SECOND );
	init( WAIT_METRICS_WRONG_SHARD_CHANCE,   isSimulated ? 1.0 : 0.1 );
	init( MIN_TAG_READ_PAGES_RATE,                             1.0e4 ); if( randomize && BUGGIFY ) MIN_TAG_READ_PAGES_RATE = 0;
	init( MIN_TAG_WRITE_PAGES_RATE,                             3200 ); if( randomize && BUGGIFY ) MIN_TAG_WRITE_PAGES_RATE = 0;
	init( TAG_MEASUREMENT_INTERVAL,                        30.0 ); if( randomize && BUGGIFY ) TAG_MEASUREMENT_INTERVAL = 1.0;
	init( PREFIX_COMPRESS_KVS_MEM_SNAPSHOTS,                    true ); if( randomize && BUGGIFY ) PREFIX_COMPRESS_KVS_MEM_SNAPSHOTS = false;
	init( REPORT_DD_METRICS,                                    true );
	init( DD_METRICS_REPORT_INTERVAL,                           30.0 );
	init( FETCH_KEYS_TOO_LONG_TIME_CRITERIA,                   300.0 );
	init( MAX_STORAGE_COMMIT_TIME,                             120.0 ); //The max fsync stall time on the storage server and tlog before marking a disk as failed
	init( RANGESTREAM_LIMIT_BYTES,                               2e6 ); if( randomize && BUGGIFY ) RANGESTREAM_LIMIT_BYTES = 1;
	init( CHANGEFEEDSTREAM_LIMIT_BYTES,                          1e6 ); if( randomize && BUGGIFY ) CHANGEFEEDSTREAM_LIMIT_BYTES = 1;
	init( BLOBWORKERSTATUSSTREAM_LIMIT_BYTES,                    1e4 ); if( randomize && BUGGIFY ) BLOBWORKERSTATUSSTREAM_LIMIT_BYTES = 1;
	init( ENABLE_CLEAR_RANGE_EAGER_READS,                       true ); if( randomize && BUGGIFY ) ENABLE_CLEAR_RANGE_EAGER_READS = deterministicRandom()->coinflip();
	init( CHECKPOINT_TRANSFER_BLOCK_BYTES,                      40e6 );
	init( QUICK_GET_VALUE_FALLBACK,                             true );
	init( QUICK_GET_KEY_VALUES_FALLBACK,                        true );
	init( STRICTLY_ENFORCE_BYTE_LIMIT,                          false); if( randomize && BUGGIFY ) STRICTLY_ENFORCE_BYTE_LIMIT = deterministicRandom()->coinflip();
	init( FRACTION_INDEX_BYTELIMIT_PREFETCH,                      0.2); if( randomize && BUGGIFY ) FRACTION_INDEX_BYTELIMIT_PREFETCH = 0.01 + deterministicRandom()->random01();
	init( MAX_PARALLEL_QUICK_GET_VALUE,                           10 ); if ( randomize && BUGGIFY ) MAX_PARALLEL_QUICK_GET_VALUE = deterministicRandom()->randomInt(1, 100);
	init( QUICK_GET_KEY_VALUES_LIMIT,                           2000 );
	init( QUICK_GET_KEY_VALUES_LIMIT_BYTES,                      1e7 );
	init( STORAGE_FEED_QUERY_HARD_LIMIT,                      100000 );
	// Read priority definitions in the form of a list of their relative concurrency share weights
	init( STORAGESERVER_READ_PRIORITIES,           "120,10,20,40,60" );
	// The total concurrency which will be shared by active priorities according to their relative weights
	init( STORAGE_SERVER_READ_CONCURRENCY,                        70 );
	// The priority number which each ReadType maps to in enumeration order
	// This exists for flexibility but assigning each ReadType to its own unique priority number makes the most sense
	// The enumeration is currently: eager, fetch, low, normal, high
	init( STORAGESERVER_READTYPE_PRIORITY_MAP,           "0,1,2,3,4" );

	//Wait Failure
	init( MAX_OUTSTANDING_WAIT_FAILURE_REQUESTS,                 250 ); if( randomize && BUGGIFY ) MAX_OUTSTANDING_WAIT_FAILURE_REQUESTS = 2;
	init( WAIT_FAILURE_DELAY_LIMIT,                              1.0 ); if( randomize && BUGGIFY ) WAIT_FAILURE_DELAY_LIMIT = 5.0;

	//Worker
	init( WORKER_LOGGING_INTERVAL,                               5.0 );
	init( HEAP_PROFILER_INTERVAL,                               30.0 );
	init( UNKNOWN_CC_TIMEOUT,                                  600.0 );
	init( DEGRADED_RESET_INTERVAL,                          24*60*60 ); // FIXME: short interval causes false positive degraded state to flap, e.g. when everyone tries and fails to connect to dead coordinator: if ( randomize && BUGGIFY ) DEGRADED_RESET_INTERVAL = 10;
	init( DEGRADED_WARNING_LIMIT,                                  1 );
	init( DEGRADED_WARNING_RESET_DELAY,                   7*24*60*60 );
	init( TRACE_LOG_FLUSH_FAILURE_CHECK_INTERVAL_SECONDS,         10 );
	init( TRACE_LOG_PING_TIMEOUT_SECONDS,                        5.0 );
	init( MIN_DELAY_CC_WORST_FIT_CANDIDACY_SECONDS,             10.0 );
	init( MAX_DELAY_CC_WORST_FIT_CANDIDACY_SECONDS,             30.0 );
	init( DBINFO_FAILED_DELAY,                                   1.0 );
	init( ENABLE_WORKER_HEALTH_MONITOR,                        false );
	init( WORKER_HEALTH_MONITOR_INTERVAL,                       60.0 );
	init( PEER_LATENCY_CHECK_MIN_POPULATION,                      30 );
	init( PEER_LATENCY_DEGRADATION_PERCENTILE,                  0.50 );
	init( PEER_LATENCY_DEGRADATION_THRESHOLD,                   0.05 );
	init( PEER_LATENCY_DEGRADATION_PERCENTILE_SATELLITE,        0.50 );
	init( PEER_LATENCY_DEGRADATION_THRESHOLD_SATELLITE,          0.1 );
	init( PEER_TIMEOUT_PERCENTAGE_DEGRADATION_THRESHOLD,         0.1 );
	init( PEER_DEGRADATION_CONNECTION_FAILURE_COUNT,               5 );
	init( WORKER_HEALTH_REPORT_RECENT_DESTROYED_PEER,           true );
	init( STORAGE_SERVER_REBOOT_ON_IO_TIMEOUT,                 false ); if ( randomize && BUGGIFY ) STORAGE_SERVER_REBOOT_ON_IO_TIMEOUT = true;

	// Test harness
	init( WORKER_POLL_DELAY,                                     1.0 );

	// Coordination
	init( COORDINATED_STATE_ONCONFLICT_POLL_INTERVAL,            1.0 ); if( randomize && BUGGIFY ) COORDINATED_STATE_ONCONFLICT_POLL_INTERVAL = 10.0;
	init( FORWARD_REQUEST_TOO_OLD,                        4*24*60*60 ); if( randomize && BUGGIFY ) FORWARD_REQUEST_TOO_OLD = 60.0;
	init( ENABLE_CROSS_CLUSTER_SUPPORT,                         true ); if( randomize && BUGGIFY ) ENABLE_CROSS_CLUSTER_SUPPORT = false;
	init( COORDINATOR_LEADER_CONNECTION_TIMEOUT,                20.0 );

	// Dynamic Knobs (implementation)
	init( COMPACTION_INTERVAL,             isSimulated ? 5.0 : 300.0 );
	init( BROADCASTER_SELF_UPDATE_DELAY,                         1.0 );
	init( GET_COMMITTED_VERSION_TIMEOUT,                         3.0 );
	init( GET_SNAPSHOT_AND_CHANGES_TIMEOUT,                      3.0 );
	init( FETCH_CHANGES_TIMEOUT,                                 3.0 );

	// Buggification
	init( BUGGIFIED_EVENTUAL_CONSISTENCY,                        1.0 );
	init( BUGGIFY_ALL_COORDINATION,                            false ); if( randomize && BUGGIFY ) BUGGIFY_ALL_COORDINATION = true;

	// Status
	init( STATUS_MIN_TIME_BETWEEN_REQUESTS,                      0.0 );
	init( MAX_STATUS_REQUESTS_PER_SECOND,                      256.0 );
	init( CONFIGURATION_ROWS_TO_FETCH,                         20000 );
	init( DISABLE_DUPLICATE_LOG_WARNING,                       false );
	init( HISTOGRAM_REPORT_INTERVAL,                           300.0 );

	// Timekeeper
	init( TIME_KEEPER_DELAY,                                      10 );
	init( TIME_KEEPER_MAX_ENTRIES,                3600 * 24 * 30 * 6 ); if( randomize && BUGGIFY ) { TIME_KEEPER_MAX_ENTRIES = 2; }

	// Fast Restore
	init( FASTRESTORE_FAILURE_TIMEOUT,                          3600 );
	init( FASTRESTORE_HEARTBEAT_INTERVAL,                         60 );
	init( FASTRESTORE_SAMPLING_PERCENT,                          100 ); if( randomize && BUGGIFY ) { FASTRESTORE_SAMPLING_PERCENT = deterministicRandom()->random01() * 100; }
	init( FASTRESTORE_NUM_LOADERS,                                 3 ); if( randomize && BUGGIFY ) { FASTRESTORE_NUM_LOADERS = deterministicRandom()->random01() * 10 + 1; }
	init( FASTRESTORE_NUM_APPLIERS,                                3 ); if( randomize && BUGGIFY ) { FASTRESTORE_NUM_APPLIERS = deterministicRandom()->random01() * 10 + 1; }
	init( FASTRESTORE_TXN_BATCH_MAX_BYTES,           1024.0 * 1024.0 ); if( randomize && BUGGIFY ) { FASTRESTORE_TXN_BATCH_MAX_BYTES = deterministicRandom()->random01() * 1024.0 * 1024.0 + 1.0; }
	init( FASTRESTORE_VERSIONBATCH_MAX_BYTES, 10.0 * 1024.0 * 1024.0 ); if( randomize && BUGGIFY ) { FASTRESTORE_VERSIONBATCH_MAX_BYTES = deterministicRandom()->random01() < 0.2 ? 50 * 1024 : deterministicRandom()->random01() < 0.4 ? 100 * 1024 * 1024 : deterministicRandom()->random01() * 1000.0 * 1024.0 * 1024.0; } // too small value may increase chance of TooManyFile error
	init( FASTRESTORE_VB_PARALLELISM,                              5 ); if( randomize && BUGGIFY ) { FASTRESTORE_VB_PARALLELISM = deterministicRandom()->random01() < 0.2 ? 2 : deterministicRandom()->random01() * 10 + 1; }
	init( FASTRESTORE_VB_MONITOR_DELAY,                           30 ); if( randomize && BUGGIFY ) { FASTRESTORE_VB_MONITOR_DELAY = deterministicRandom()->random01() * 20 + 1; }
	init( FASTRESTORE_VB_LAUNCH_DELAY,                           1.0 ); if( randomize && BUGGIFY ) { FASTRESTORE_VB_LAUNCH_DELAY = deterministicRandom()->random01() < 0.2 ? 0.1 : deterministicRandom()->random01() * 10.0 + 1; }
	init( FASTRESTORE_ROLE_LOGGING_DELAY,                          5 ); if( randomize && BUGGIFY ) { FASTRESTORE_ROLE_LOGGING_DELAY = deterministicRandom()->random01() * 60 + 1; }
	init( FASTRESTORE_UPDATE_PROCESS_STATS_INTERVAL,               5 ); if( randomize && BUGGIFY ) { FASTRESTORE_UPDATE_PROCESS_STATS_INTERVAL = deterministicRandom()->random01() * 60 + 1; }
	init( FASTRESTORE_ATOMICOP_WEIGHT,                             1 ); if( randomize && BUGGIFY ) { FASTRESTORE_ATOMICOP_WEIGHT = deterministicRandom()->random01() * 200 + 1; }
	init( FASTRESTORE_MONITOR_LEADER_DELAY,                        5 ); if( randomize && BUGGIFY ) { FASTRESTORE_MONITOR_LEADER_DELAY = deterministicRandom()->random01() * 100; }
	init( FASTRESTORE_STRAGGLER_THRESHOLD_SECONDS,                60 ); if( randomize && BUGGIFY ) { FASTRESTORE_STRAGGLER_THRESHOLD_SECONDS = deterministicRandom()->random01() * 240 + 10; }
	init( FASTRESTORE_TRACK_REQUEST_LATENCY,              	   false ); if( randomize && BUGGIFY ) { FASTRESTORE_TRACK_REQUEST_LATENCY = false; }
	init( FASTRESTORE_MEMORY_THRESHOLD_MB_SOFT,                 6144 ); if( randomize && BUGGIFY ) { FASTRESTORE_MEMORY_THRESHOLD_MB_SOFT = 1; }
	init( FASTRESTORE_WAIT_FOR_MEMORY_LATENCY,                    10 ); if( randomize && BUGGIFY ) { FASTRESTORE_WAIT_FOR_MEMORY_LATENCY = 60; }
	init( FASTRESTORE_HEARTBEAT_DELAY,                            10 ); if( randomize && BUGGIFY ) { FASTRESTORE_HEARTBEAT_DELAY = deterministicRandom()->random01() * 120 + 2; }
	init( FASTRESTORE_HEARTBEAT_MAX_DELAY,                        10 ); if( randomize && BUGGIFY ) { FASTRESTORE_HEARTBEAT_MAX_DELAY = FASTRESTORE_HEARTBEAT_DELAY * 10; }
	init( FASTRESTORE_APPLIER_FETCH_KEYS_SIZE,                   100 ); if( randomize && BUGGIFY ) { FASTRESTORE_APPLIER_FETCH_KEYS_SIZE = deterministicRandom()->random01() * 10240 + 1; }
	init( FASTRESTORE_LOADER_SEND_MUTATION_MSG_BYTES, 1.0 * 1024.0 * 1024.0 ); if( randomize && BUGGIFY ) { FASTRESTORE_LOADER_SEND_MUTATION_MSG_BYTES = deterministicRandom()->random01() < 0.2 ? 1024 : deterministicRandom()->random01() * 5.0 * 1024.0 * 1024.0 + 1; }
	init( FASTRESTORE_GET_RANGE_VERSIONS_EXPENSIVE,            false ); if( randomize && BUGGIFY ) { FASTRESTORE_GET_RANGE_VERSIONS_EXPENSIVE = deterministicRandom()->random01() < 0.5 ? true : false; }
	init( FASTRESTORE_REQBATCH_PARALLEL,                          50 ); if( randomize && BUGGIFY ) { FASTRESTORE_REQBATCH_PARALLEL = deterministicRandom()->random01() * 100 + 1; }
	init( FASTRESTORE_REQBATCH_LOG,                            false ); if( randomize && BUGGIFY ) { FASTRESTORE_REQBATCH_LOG = deterministicRandom()->random01() < 0.2 ? true : false; }
	init( FASTRESTORE_TXN_CLEAR_MAX,                             100 ); if( randomize && BUGGIFY ) { FASTRESTORE_TXN_CLEAR_MAX = deterministicRandom()->random01() * 100 + 1; }
	init( FASTRESTORE_TXN_RETRY_MAX,                              10 ); if( randomize && BUGGIFY ) { FASTRESTORE_TXN_RETRY_MAX = deterministicRandom()->random01() * 100 + 1; }
	init( FASTRESTORE_TXN_EXTRA_DELAY,                           0.0 ); if( randomize && BUGGIFY ) { FASTRESTORE_TXN_EXTRA_DELAY = deterministicRandom()->random01() * 1 + 0.001;}
	init( FASTRESTORE_NOT_WRITE_DB,                            false ); // Perf test only: set it to true will cause simulation failure
	init( FASTRESTORE_USE_RANGE_FILE,                           true ); // Perf test only: set it to false will cause simulation failure
	init( FASTRESTORE_USE_LOG_FILE,                             true ); // Perf test only: set it to false will cause simulation failure
	init( FASTRESTORE_SAMPLE_MSG_BYTES,                      1048576 ); if( randomize && BUGGIFY ) { FASTRESTORE_SAMPLE_MSG_BYTES = deterministicRandom()->random01() * 2048;}
	init( FASTRESTORE_SCHED_UPDATE_DELAY,                        0.1 ); if( randomize && BUGGIFY ) { FASTRESTORE_SCHED_UPDATE_DELAY = deterministicRandom()->random01() * 2;}
	init( FASTRESTORE_SCHED_TARGET_CPU_PERCENT,                   70 ); if( randomize && BUGGIFY ) { FASTRESTORE_SCHED_TARGET_CPU_PERCENT = deterministicRandom()->random01() * 100 + 50;} // simulate cpu usage can be larger than 100
	init( FASTRESTORE_SCHED_MAX_CPU_PERCENT,                      90 ); if( randomize && BUGGIFY ) { FASTRESTORE_SCHED_MAX_CPU_PERCENT = FASTRESTORE_SCHED_TARGET_CPU_PERCENT + deterministicRandom()->random01() * 100;}
	init( FASTRESTORE_SCHED_INFLIGHT_LOAD_REQS,                   50 ); if( randomize && BUGGIFY ) { FASTRESTORE_SCHED_INFLIGHT_LOAD_REQS = deterministicRandom()->random01() < 0.2 ? 1 : deterministicRandom()->random01() * 30 + 1;}
	init( FASTRESTORE_SCHED_INFLIGHT_SEND_REQS,                    3 ); if( randomize && BUGGIFY ) { FASTRESTORE_SCHED_INFLIGHT_SEND_REQS = deterministicRandom()->random01() < 0.2 ? 1 : deterministicRandom()->random01() * 10 + 1;}
	init( FASTRESTORE_SCHED_LOAD_REQ_BATCHSIZE,                    5 ); if( randomize && BUGGIFY ) { FASTRESTORE_SCHED_LOAD_REQ_BATCHSIZE = deterministicRandom()->random01() < 0.2 ? 1 : deterministicRandom()->random01() * 10 + 1;}
	init( FASTRESTORE_SCHED_INFLIGHT_SENDPARAM_THRESHOLD,         10 ); if( randomize && BUGGIFY ) { FASTRESTORE_SCHED_INFLIGHT_SENDPARAM_THRESHOLD = deterministicRandom()->random01() < 0.2 ? 1 : deterministicRandom()->random01() * 15 + 1;}
	init( FASTRESTORE_SCHED_SEND_FUTURE_VB_REQS_BATCH,             2 ); if( randomize && BUGGIFY ) { FASTRESTORE_SCHED_SEND_FUTURE_VB_REQS_BATCH = deterministicRandom()->random01() < 0.2 ? 1 : deterministicRandom()->random01() * 15 + 1;}
	init( FASTRESTORE_NUM_TRACE_EVENTS,                          100 ); if( randomize && BUGGIFY ) { FASTRESTORE_NUM_TRACE_EVENTS = deterministicRandom()->random01() < 0.2 ? 1 : deterministicRandom()->random01() * 500 + 1;}
	init( FASTRESTORE_EXPENSIVE_VALIDATION,                    false ); if( randomize && BUGGIFY ) { FASTRESTORE_EXPENSIVE_VALIDATION = deterministicRandom()->random01() < 0.5 ? true : false;}
	init( FASTRESTORE_WRITE_BW_MB,                                70 ); if( randomize && BUGGIFY ) { FASTRESTORE_WRITE_BW_MB = deterministicRandom()->random01() < 0.5 ? 2 : 100;}
	init( FASTRESTORE_RATE_UPDATE_SECONDS,                       1.0 ); if( randomize && BUGGIFY ) { FASTRESTORE_RATE_UPDATE_SECONDS = deterministicRandom()->random01() < 0.5 ? 0.1 : 2;}
	init( FASTRESTORE_DUMP_INSERT_RANGE_VERSION,               false );

	init( REDWOOD_DEFAULT_PAGE_SIZE,                            8192 );
	init( REDWOOD_DEFAULT_EXTENT_SIZE,              32 * 1024 * 1024 );
	init( REDWOOD_DEFAULT_EXTENT_READ_SIZE,              1024 * 1024 );
	init( REDWOOD_EXTENT_CONCURRENT_READS,                         4 );
	init( REDWOOD_KVSTORE_RANGE_PREFETCH,                       true );
	init( REDWOOD_PAGE_REBUILD_MAX_SLACK,                       0.33 );
	init( REDWOOD_LAZY_CLEAR_BATCH_SIZE_PAGES,                    10 );
	init( REDWOOD_LAZY_CLEAR_MIN_PAGES,                            0 );
	init( REDWOOD_LAZY_CLEAR_MAX_PAGES,                          1e6 );
	init( REDWOOD_REMAP_CLEANUP_WINDOW_BYTES, 4LL * 1024 * 1024 * 1024 );
	init( REDWOOD_REMAP_CLEANUP_TOLERANCE_RATIO,                0.05 );
	init( REDWOOD_PAGEFILE_GROWTH_SIZE_PAGES,                  20000 ); if( randomize && BUGGIFY ) { REDWOOD_PAGEFILE_GROWTH_SIZE_PAGES = deterministicRandom()->randomInt(200, 1000); }
	init( REDWOOD_METRICS_INTERVAL,                              5.0 );
	init( REDWOOD_HISTOGRAM_INTERVAL,                           30.0 );
	init( REDWOOD_EVICT_UPDATED_PAGES,                          true ); if( randomize && BUGGIFY ) { REDWOOD_EVICT_UPDATED_PAGES = false; }
	init( REDWOOD_DECODECACHE_REUSE_MIN_HEIGHT,                    2 ); if( randomize && BUGGIFY ) { REDWOOD_DECODECACHE_REUSE_MIN_HEIGHT = deterministicRandom()->randomInt(1, 7); }
	init( REDWOOD_IO_PRIORITIES,                       "32,32,32,32" );
	init( REDWOOD_SPLIT_ENCRYPTED_PAGES_BY_TENANT,             false );

	// Server request latency measurement
	init( LATENCY_SAMPLE_SIZE,                                100000 );
	init( FILE_LATENCY_SAMPLE_SIZE,                            10000 );
	init( LATENCY_METRICS_LOGGING_INTERVAL,                     60.0 );

	// Cluster recovery
	init ( CLUSTER_RECOVERY_EVENT_NAME_PREFIX,              "Master" );

	// Encryption
	init( ENABLE_ENCRYPTION,                                   false ); if ( randomize && BUGGIFY ) ENABLE_ENCRYPTION = !ENABLE_ENCRYPTION;
	init( ENCRYPTION_MODE,                             "AES-256-CTR" );
	init( SIM_KMS_MAX_KEYS,                                     4096 );
	init( ENCRYPT_PROXY_MAX_DBG_TRACE_LENGTH,                 100000 );
	init( ENABLE_TLOG_ENCRYPTION,                  ENABLE_ENCRYPTION ); if ( randomize && BUGGIFY && ENABLE_ENCRYPTION ) ENABLE_TLOG_ENCRYPTION = false;
	init( ENABLE_STORAGE_SERVER_ENCRYPTION,        ENABLE_ENCRYPTION ); if ( randomize && BUGGIFY && ENABLE_ENCRYPTION) ENABLE_STORAGE_SERVER_ENCRYPTION = false;
	init( ENABLE_BLOB_GRANULE_ENCRYPTION,          ENABLE_ENCRYPTION ); if ( randomize && BUGGIFY && ENABLE_ENCRYPTION) ENABLE_BLOB_GRANULE_ENCRYPTION = false;

	// encrypt key proxy
	init( ENABLE_BLOB_GRANULE_COMPRESSION,                     false ); if ( randomize && BUGGIFY ) { ENABLE_BLOB_GRANULE_COMPRESSION = deterministicRandom()->coinflip(); }
	init( BLOB_GRANULE_COMPRESSION_FILTER,                    "NONE" ); if ( randomize && BUGGIFY ) { BLOB_GRANULE_COMPRESSION_FILTER = CompressionUtils::toString(CompressionUtils::getRandomFilter()); }

	// KMS connector type
	init( KMS_CONNECTOR_TYPE,                     "RESTKmsConnector" );

	// Blob granlues
	init( BG_URL,        isSimulated ? "file://simfdb/fdbblob/" : "" ); // TODO: store in system key space or something, eventually	
	bool buggifyMediumGranules = simulationMediumShards || (randomize && BUGGIFY);
	// BlobGranuleVerify* simulation tests use "knobs", BlobGranuleCorrectness* use "tenant", default in real clusters is "knobs"
	init( BG_METADATA_SOURCE,                                "knobs" );
	init( BG_SNAPSHOT_FILE_TARGET_BYTES,                    10000000 ); if( buggifySmallShards ) BG_SNAPSHOT_FILE_TARGET_BYTES = 100000; else if (buggifyMediumGranules) BG_SNAPSHOT_FILE_TARGET_BYTES = 1000000;
	init( BG_SNAPSHOT_FILE_TARGET_CHUNK_BYTES,               64*1024 ); if ( randomize && BUGGIFY ) BG_SNAPSHOT_FILE_TARGET_CHUNK_BYTES = BG_SNAPSHOT_FILE_TARGET_BYTES / (1 << deterministicRandom()->randomInt(0, 8));
	init( BG_DELTA_BYTES_BEFORE_COMPACT, BG_SNAPSHOT_FILE_TARGET_BYTES/2 );
	init( BG_DELTA_FILE_TARGET_BYTES,   BG_DELTA_BYTES_BEFORE_COMPACT/10 );
	init( BG_DELTA_FILE_TARGET_CHUNK_BYTES,                  32*1024 ); if ( randomize && BUGGIFY ) BG_DELTA_FILE_TARGET_CHUNK_BYTES = BG_DELTA_FILE_TARGET_BYTES / (1 << deterministicRandom()->randomInt(0, 7));
	init( BG_MAX_SPLIT_FANOUT,                                    10 ); if( randomize && BUGGIFY ) BG_MAX_SPLIT_FANOUT = deterministicRandom()->randomInt(5, 15);
	init( BG_MAX_MERGE_FANIN,                                     10 ); if( randomize && BUGGIFY ) BG_MAX_MERGE_FANIN = deterministicRandom()->randomInt(2, 15);
	init( BG_HOT_SNAPSHOT_VERSIONS,                          5000000 );

	init( BG_CONSISTENCY_CHECK_ENABLED,                         true ); if (randomize && BUGGIFY) BG_CONSISTENCY_CHECK_ENABLED = false;
	init( BG_CONSISTENCY_CHECK_TARGET_SPEED_KB,                 1000 ); if (randomize && BUGGIFY) BG_CONSISTENCY_CHECK_TARGET_SPEED_KB *= (deterministicRandom()->randomInt(2, 50) / 10);
	init( BG_KEY_TUPLE_TRUNCATE_OFFSET,                            0 );
	init( BG_ENABLE_READ_DRIVEN_COMPACTION,                     true ); if (randomize && BUGGIFY) BG_ENABLE_READ_DRIVEN_COMPACTION = false;
	init( BG_RDC_BYTES_FACTOR,                                     2 ); if (randomize && BUGGIFY) BG_RDC_BYTES_FACTOR = deterministicRandom()->randomInt(1, 10);
	init( BG_RDC_READ_FACTOR,                                      3 ); if (randomize && BUGGIFY) BG_RDC_READ_FACTOR = deterministicRandom()->randomInt(1, 10);
	init( BG_WRITE_MULTIPART,                                  false ); if (randomize && BUGGIFY) BG_WRITE_MULTIPART = true;

	init( BG_ENABLE_MERGING,                                    true ); if (randomize && BUGGIFY) BG_ENABLE_MERGING = false;
	init( BG_MERGE_CANDIDATE_THRESHOLD_SECONDS, isSimulated ? 20.0 : 30 * 60 ); if (randomize && BUGGIFY) BG_MERGE_CANDIDATE_THRESHOLD_SECONDS = 5.0;
	init( BG_MERGE_CANDIDATE_DELAY_SECONDS, BG_MERGE_CANDIDATE_THRESHOLD_SECONDS / 10.0 );

	init( BLOB_WORKER_INITIAL_SNAPSHOT_PARALLELISM,                8 ); if( randomize && BUGGIFY ) BLOB_WORKER_INITIAL_SNAPSHOT_PARALLELISM = 1;
	init( BLOB_WORKER_RESNAPSHOT_PARALLELISM,                     40 ); if( randomize && BUGGIFY ) BLOB_WORKER_RESNAPSHOT_PARALLELISM = deterministicRandom()->randomInt(1, 10);
	init( BLOB_WORKER_DELTA_FILE_WRITE_PARALLELISM,             2000 ); if( randomize && BUGGIFY ) BLOB_WORKER_DELTA_FILE_WRITE_PARALLELISM = deterministicRandom()->randomInt(10, 100);
	init( BLOB_WORKER_RDC_PARALLELISM,                             2 ); if( randomize && BUGGIFY ) BLOB_WORKER_RDC_PARALLELISM = deterministicRandom()->randomInt(1, 6);

	init( BLOB_WORKER_TIMEOUT,                                  10.0 ); if( randomize && BUGGIFY ) BLOB_WORKER_TIMEOUT = 1.0;
	init( BLOB_WORKER_REQUEST_TIMEOUT,                           5.0 ); if( randomize && BUGGIFY ) BLOB_WORKER_REQUEST_TIMEOUT = 1.0;
	init( BLOB_WORKERLIST_FETCH_INTERVAL,                        1.0 );
	init( BLOB_WORKER_BATCH_GRV_INTERVAL,                        0.1 );
	init( BLOB_WORKER_DO_REJECT_WHEN_FULL,                      true ); if ( randomize && BUGGIFY ) BLOB_WORKER_DO_REJECT_WHEN_FULL = false;
	init( BLOB_WORKER_REJECT_WHEN_FULL_THRESHOLD,                0.9 );
	init( BLOB_WORKER_FORCE_FLUSH_CLEANUP_DELAY,                30.0 ); if ( randomize && BUGGIFY ) BLOB_WORKER_FORCE_FLUSH_CLEANUP_DELAY = deterministicRandom()->randomInt(0, 10) - 1;

	init( BLOB_MANAGER_STATUS_EXP_BACKOFF_MIN,                   0.1 );
	init( BLOB_MANAGER_STATUS_EXP_BACKOFF_MAX,                   5.0 );
	init( BLOB_MANAGER_STATUS_EXP_BACKOFF_EXPONENT,              1.5 );
	init( BLOB_MANAGER_CONCURRENT_MERGE_CHECKS,                   64 ); if( randomize && BUGGIFY ) BLOB_MANAGER_CONCURRENT_MERGE_CHECKS = 1 << deterministicRandom()->randomInt(0, 7);
	init( BLOB_MANIFEST_BACKUP,                                false );
	init( BLOB_MANIFEST_BACKUP_INTERVAL,   isSimulated ?  5.0 : 30.0 );
	init( BLOB_FULL_RESTORE_MODE,                              false );
	init( BLOB_MIGRATOR_CHECK_INTERVAL,      isSimulated ?  1.0 : 5.0);
	init( BLOB_MANIFEST_RW_ROWS,             isSimulated ?  10 : 1000);

	init( BGCC_TIMEOUT,                   isSimulated ? 10.0 : 120.0 );
	init( BGCC_MIN_INTERVAL,                isSimulated ? 1.0 : 10.0 );

	// Blob Metadata
	init( BLOB_METADATA_CACHE_TTL, isSimulated ? 120 : 24 * 60 * 60 );
	if ( randomize && BUGGIFY) { BLOB_METADATA_CACHE_TTL = deterministicRandom()->randomInt(50, 100); }

	// HTTP KMS Connector
	init( REST_KMS_CONNECTOR_KMS_DISCOVERY_URL_MODE,           "file");
	init( REST_KMS_CONNECTOR_VALIDATION_TOKEN_MODE,            "file");
	init( REST_KMS_CONNECTOR_VALIDATION_TOKEN_MAX_SIZE,          1024);
	init( REST_KMS_CONNECTOR_VALIDATION_TOKENS_MAX_PAYLOAD_SIZE, 10 * 1024);
	init( REST_KMS_CONNECTOR_REFRESH_KMS_URLS,                   true);
	init( REST_KMS_CONNECTOR_REFRESH_KMS_URLS_INTERVAL_SEC,       600);
	// Below KMS configurations are responsible for:
	// Discovering KMS URLs, fetch encryption keys endpoint and validation token details.
	// Configurations are expected to be passed as command-line arguments.
	// NOTE: Care must be taken when attempting to update below configurations for a up/running FDB cluster.
	init( REST_KMS_CONNECTOR_DISCOVER_KMS_URL_FILE,                "");
	init( REST_KMS_CONNECTOR_GET_ENCRYPTION_KEYS_ENDPOINT,         "");
	init( REST_KMS_CONNECTOR_GET_BLOB_METADATA_ENDPOINT,           "");
	// Details to fetch validation token from a localhost file
	// acceptable format: "<token_name1>#<absolute_file_path1>,<token_name2>#<absolute_file_path2>,.."
	// NOTE: 'token-name" can NOT contain '#' character
	init( REST_KMS_CONNECTOR_VALIDATION_TOKEN_DETAILS,             "");

	// Drop in-memory state associated with an idempotency id after this many seconds. Once dropped, this id cannot be
	// expired proactively, but will eventually get cleaned up by the idempotency id cleaner.
	init( IDEMPOTENCY_ID_IN_MEMORY_LIFETIME,                       10);

	// clang-format on

	if (clientKnobs) {
		clientKnobs->IS_ACCEPTABLE_DELAY =
		    clientKnobs->IS_ACCEPTABLE_DELAY *
		    std::min(MAX_READ_TRANSACTION_LIFE_VERSIONS, MAX_WRITE_TRANSACTION_LIFE_VERSIONS) /
		    (5.0 * VERSIONS_PER_SECOND);
		clientKnobs->INIT_MID_SHARD_BYTES = MIN_SHARD_BYTES;
	}
}
