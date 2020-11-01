/*
 * extDB3
 * © 2016 Declan Ireland <https://bitbucket.org/torndeco/extdb3>
 */

#include "session.h"


MariaDBSession::MariaDBSession(MariaDBPool *database_pool)
{
	database_pool_ptr = database_pool;
	data = database_pool->get();
}

MariaDBSession::~MariaDBSession(void)
{
	database_pool_ptr->putBack(std::move(data));
}

void MariaDBSession::resetSession()
{
	data->statements.clear();
	mysql_reset_connection(data->connector.mysql_ptr);
}
