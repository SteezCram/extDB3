/*
 * extDB3
 * © 2016 Declan Ireland <https://bitbucket.org/torndeco/extdb3>
 */

#pragma once

#include <string>
#include <vector>

#include <boost/date_time/posix_time/posix_time.hpp>
#include <mariadb/mysql.h>

#include "abstract.h"
#include "connector.h"


class MariaDBQuery
{
public:
	MariaDBQuery();
	~MariaDBQuery();

	void init(MariaDBConnector &connector);
	void send(std::string &sql_query);
	void get(int &check_dataType_string, bool &check_dataType_null, std::string &insertID, std::vector<std::vector<std::string>> &result_vec);
	void get(std::vector<sql_option> &output_options, std::string &strip_chars, int &strip_chars_mode, std::string &insertID, std::vector<std::vector<std::string>> &result_vec);

private:
	MariaDBConnector *connector_ptr;
	std::locale loc_date;
	std::locale loc_datetime;
	std::locale loc_time;
	boost::posix_time::time_facet* facet;
};
