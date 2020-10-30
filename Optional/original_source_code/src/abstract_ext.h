/*
 * extDB3
 * © 2016 Declan Ireland <https://bitbucket.org/torndeco/extdb3>
 */

#pragma once

#include <thread>
#include <unordered_map>

#include <spdlog/spdlog.h>

#include "mariaDB/pool.h"


#define EXTDB_VERSION "1.032"
#define EXTDB_CONF_VERSION 1


class AbstractExt
{
public:
	struct resultData
	{
		bool wait = true;
		std::string message;
	};

	std::unordered_map<std::string, MariaDBPool> mariadb_databases;

	// extInfo
	struct extInfo
	{
		int max_threads;
		bool allow_reset = false;

		bool logger_flush = true;

		bool extDB_lock = false;
		std::string extDB_lockCode;

		std::string path;
		std::string log_path;
	};
	extInfo ext_info;


	#ifdef DEBUG_TESTING
		std::shared_ptr<spdlog::logger> console;
	#elif TEST_APP
		std::shared_ptr<spdlog::logger> console;
	#endif
	std::shared_ptr<spdlog::logger> logger;
};
