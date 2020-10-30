/*
 * extDB3
 * © 2016 Declan Ireland <https://bitbucket.org/torndeco/extdb3>
 */

#include "pool.h"

#include <boost/bind.hpp>
#include <mariadb/mysql.h>

#include "connector.h"



MariaDBPool::MariaDBPool()
{
}


MariaDBPool::~MariaDBPool(void)
{
}


void MariaDBPool::init(std::string &host, unsigned int &port, std::string &user, std::string &password, std::string &db)
{
	login_data.host = host;
	login_data.port = port;
	login_data.user = user;
	login_data.password = password;
	login_data.db = db;

	putBack(get()); // Force Start MySQL Connection + Return MySQL Connection Back to Idle
}


std::unique_ptr<MariaDBPool::mariadb_session_struct> MariaDBPool::get()
{
	std::unique_ptr<mariadb_session_struct> mariadb_session;
	{
		std::lock_guard<std::mutex> lock(mariadb_session_pool_mutex);
		if (mariadb_session_pool.size() > 0)
		{
			mariadb_session = std::move(mariadb_session_pool.front());
			mariadb_session_pool.pop_front();
		} else {
			mariadb_session.reset(new mariadb_session_struct());
			mariadb_session->connector.init(login_data.host, login_data.port, login_data.user, login_data.password, login_data.db);
			mariadb_session->connector.connect();
			mariadb_session->query.init(mariadb_session->connector);
		}
	}
	return mariadb_session;
}


void MariaDBPool::putBack(std::unique_ptr<mariadb_session_struct> mariadb_session)
{
	mariadb_session->last_used = boost::posix_time::second_clock::local_time();
	{
		std::lock_guard<std::mutex> lock(mariadb_session_pool_mutex);
		mariadb_session_pool.push_back(std::move(mariadb_session));
	}
}


void MariaDBPool::idleCleanup()
{
	auto tick = boost::posix_time::second_clock::local_time();
	boost::posix_time::time_duration diff;
	std::lock_guard<std::mutex> lock(mariadb_session_pool_mutex);
	{
		// Remove old connections (10 Minutes)
		for(auto session_itr = mariadb_session_pool.end(); session_itr != mariadb_session_pool.begin(); --session_itr) {
			diff = tick - (*session_itr)->last_used;
			if (diff.total_seconds() > 600)
			{
				mariadb_session_pool.erase(mariadb_session_pool.begin(), session_itr);
				break;
			};
		}
		// Ping any connections still alive, Reconnect & Wipe Statements if Required
		for(auto session_itr = mariadb_session_pool.begin(); session_itr != mariadb_session_pool.end(); ++session_itr) {
			auto original_thread_id = mysql_thread_id((*session_itr)->connector.mysql_ptr);
			if (mysql_ping((*session_itr)->connector.mysql_ptr))
			{
				if ((original_thread_id) != mysql_thread_id((*session_itr)->connector.mysql_ptr))
				{
					(*session_itr)->statements.clear();
					mysql_reset_connection((*session_itr)->connector.mysql_ptr);
				};
			};
		}
	}
}
