/*
 * extDB3
 * © 2016 Declan Ireland <https://bitbucket.org/torndeco/extdb3>
 */

#pragma once

#include <string>
#include <vector>

#include <mysql.h>


class MariaDBBinder
{
public:
	MariaDBBinder(MYSQL *mysql_ptr, std::size_t size);
	~MariaDBBinder();

	void bind(std::size_t pos);
	void bind(std::size_t pos, const std::string & str);

	void binder(std::size_t &pos, enum_field_types type, const void * buffer, int length);

	void clear();

private:
	MYSQL *mysql_ptr;
  MYSQL_BIND *mysql_binds = NULL;

	std::size_t size;

	std::vector<MYSQL_TIME*> mysql_times;
};
