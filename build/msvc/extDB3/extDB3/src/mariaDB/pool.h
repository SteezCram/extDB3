/*
 * extDB3
 * © 2016 Declan Ireland <https://bitbucket.org/torndeco/extdb3>
 */

#pragma once

#include <list>
#include <memory>
#include <mutex>
#include <unordered_map>

#include <boost/asio.hpp>
#include <mariadb/mysql.h>

#include "connector.h"
#include "query.h"
#include "statement.h"


class MariaDBPool
{
public:
	MariaDBPool();
	~MariaDBPool();

	struct mariadb_session_struct
	{
		boost::posix_time::ptime last_used;
		MariaDBConnector connector;
		MariaDBQuery     query;
		std::unordered_map<std::string, std::vector<MariaDBStatement> > statements;
	};

	void init(std::string &host, unsigned int &port, std::string &user, std::string &password, std::string &db);
	std::unique_ptr<mariadb_session_struct> get();
	void putBack(std::unique_ptr<mariadb_session_struct> mariadb_session);
	void idleCleanup();

private:
	struct login_data_struct
	{
		std::string host;
		std::string user;
		std::string password;
		std::string db;
		unsigned int port;
	};
	login_data_struct login_data;

	std::list<std::unique_ptr<mariadb_session_struct>> mariadb_session_pool;
	std::mutex mariadb_session_pool_mutex;
};
