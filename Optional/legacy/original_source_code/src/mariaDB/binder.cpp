/*
 * extDB3
 * © 2016 Declan Ireland <https://bitbucket.org/torndeco/extdb3>
 */

#include "binder.h"


MariaDBBinder::MariaDBBinder(MYSQL *mysql_ptr, std::size_t size) : mysql_ptr(mysql_ptr), size(size)
{
	mysql_binds = new MYSQL_BIND [size];
}


MariaDBBinder::~MariaDBBinder(void)
{
	delete[] mysql_binds;
}


void MariaDBBinder::clear()
{
	delete[] mysql_binds;
	mysql_binds = new MYSQL_BIND[size];
}


void MariaDBBinder::binder(std::size_t &pos, enum_field_types type, const void* buffer, int length)
{
	MYSQL_BIND mysql_bind = {0};

	mysql_bind.buffer_type   = type;
	mysql_bind.buffer  = const_cast<void*>(buffer);
	mysql_bind.buffer_length = length;
	mysql_bind.is_unsigned   = false;

	mysql_binds[pos] = (std::move(mysql_bind));
}

/*
void Binder::bind(std::size_t pos, const Time& val, Direction dir, const WhenNullCb& nullCb)
{
	poco_assert(dir == PD_IN);
	MYSQL_TIME mt = {0};

	mt.hour   = val.hour();
	mt.minute = val.minute();
	mt.second = val.second();

	mt.time_type = MYSQL_TIMESTAMP_TIME;

	_dates.push_back(new MYSQL_TIME(mt));

	realBind(pos, MYSQL_TYPE_TIME, _dates.back(), sizeof(MYSQL_TIME));
}
*/

/*
https://dev.mysql.com/doc/refman/5.7/en/c-api-prepared-statement-type-codes.html
MYSQL_TYPE_TINY
MYSQL_TYPE_SHORT
MYSQL_TYPE_LONG
MYSQL_TYPE_LONGLONG
MYSQL_TYPE_FLOAT
MYSQL_TYPE_DOUBLE
MYSQL_TYPE_TIME
MYSQL_TYPE_DATE
MYSQL_TYPE_DATETIME
MYSQL_TYPE_TIMESTAMP
MYSQL_TYPE_STRING
MYSQL_TYPE_BLOB
MYSQL_TYPE_NULL
*/


void MariaDBBinder::bind(std::size_t pos)
{
	binder(pos, MYSQL_TYPE_NULL, 0, 0);
}


void MariaDBBinder::bind(std::size_t pos, const std::string& str)
{
	binder(pos, MYSQL_TYPE_STRING, str.c_str(), str.size());
}
