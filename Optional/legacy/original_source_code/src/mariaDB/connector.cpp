/*
 * extDB3
 * © 2016 Declan Ireland <https://bitbucket.org/torndeco/extdb3>
 */

#include "connector.h"

#include <iostream>

#include "exceptions.h"


MariaDBConnector::MariaDBConnector()
{
}


MariaDBConnector::~MariaDBConnector(void)
{
	if (connected)
	{
		mysql_close(mysql_ptr);
	}
}


void MariaDBConnector::init(std::string &host, unsigned int &port, std::string &user, std::string &password, std::string &db)
{
	login_data.host = host;
	login_data.port = port;
	login_data.user = user;
	login_data.password = password;
	login_data.db = db;
}


void MariaDBConnector::connect()
{
	if (connected)
	{
		mysql_close(mysql_ptr);
	}
	mysql_ptr = mysql_init(mysql_ptr);
	//mysql_optionsv(mysql_ptr, MYSQL_OPT_COMPRESS, NULL); // Compression
	//mysql_optionsv(mysql_ptr, MYSQL_OPT_CONNECT_TIMEOUT, (const char *)5);
	mysql_optionsv(mysql_ptr, MYSQL_OPT_RECONNECT, (void *)"1");
	mysql_optionsv(mysql_ptr, MYSQL_SET_CHARSET_NAME, (void *)"utf8");
	if (!(mysql_real_connect(mysql_ptr, login_data.host.c_str(), login_data.user.c_str(), login_data.password.c_str(), login_data.db.c_str(), login_data.port, 0, 0)))
	{
		throw MariaDBConnectorException(mysql_ptr);
	}
	connected = true;
}


unsigned long long MariaDBConnector::getInsertId()
{
	return (mysql_insert_id(mysql_ptr));
}

int MariaDBConnector::ping()
{
	return (mysql_ping(mysql_ptr));
}


std::string MariaDBConnector::escapeString(std::string &input_str)
{
	char *output_c_str = new char[(input_str.size() * 2) + 1];
	mysql_real_escape_string(mysql_ptr, output_c_str, input_str.c_str(), input_str.size());
	std::string output_str(output_c_str);
	delete[] output_c_str;
	return output_str;
}
