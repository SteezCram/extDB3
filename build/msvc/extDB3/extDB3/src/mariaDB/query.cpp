/*
 * extDB3
 * © 2016 Declan Ireland <https://bitbucket.org/torndeco/extdb3>
 */

#include "query.h"

#include <iomanip>
#include <iostream>
#include <sstream>

#include <mariadb/errmsg.h>

#include "exceptions.h"
#include "../md5/md5.h"


MariaDBQuery::MariaDBQuery()
{
	loc_date = std::locale(std::locale::classic(), new boost::posix_time::time_input_facet("%Y-%m-%d"));
	loc_datetime = std::locale(std::locale::classic(), new boost::posix_time::time_input_facet("%Y-%m-%d %H:%M:%S"));
	loc_time = std::locale(std::locale::classic(), new boost::posix_time::time_input_facet("%H:%M:%S"));
}


MariaDBQuery::~MariaDBQuery(void)
{
}


void MariaDBQuery::init(MariaDBConnector &connector)
{
	connector_ptr = &connector;
}


void MariaDBQuery::get(std::vector<sql_option> &output_options, std::string &strip_chars, int &strip_chars_mode, std::string &insertID, std::vector<std::vector<std::string>> &result_vec)
{
	result_vec.clear();
	do {
		MYSQL_RES *result = (mysql_store_result(connector_ptr->mysql_ptr));  // Returns NULL for Errors & No Result
		insertID = std::to_string(mysql_insert_id(connector_ptr->mysql_ptr));
		if (!result)
		{
			std::string error_msg(mysql_error(connector_ptr->mysql_ptr));
			mysql_free_result(result);
			if (!error_msg.empty())
			{
				throw MariaDBQueryException(connector_ptr->mysql_ptr);
			}
		} else {
			unsigned int num_fields = mysql_num_fields(result);
			if (num_fields > 0)
			{
				MYSQL_ROW row;
				MYSQL_FIELD *fields;
				fields = mysql_fetch_fields(result);
				output_options.resize(num_fields);

				while ((row = mysql_fetch_row(result)) != NULL)
				{
					std::vector<std::string> field_row;
					for (unsigned int i = 0; i < num_fields; i++)
					{
						switch (fields[i].type)
						{
							case MYSQL_TYPE_DATE:
							{
								try
								{
									std::istringstream is(row[i]);
									is.imbue(loc_date);
									boost::posix_time::ptime ptime;
									is >> ptime;

									std::stringstream stream;
									facet = new boost::posix_time::time_facet();
									facet->format("[%Y,%m,%d]");
									stream.imbue(std::locale(std::locale::classic(), facet));
									stream << ptime;
									std::string tmp_str = stream.str();
									if (tmp_str != "not-a-date-time")
									{
										field_row.push_back(std::move(tmp_str));
									} else {
										field_row.emplace_back("[]");
									}
								}
								catch(std::exception& e)
								{
									field_row.emplace_back("[]");
								}
								break;
							}
							case MYSQL_TYPE_DATETIME:
							{
								try
								{
									std::istringstream is(row[i]);
									is.imbue(loc_datetime);
									boost::posix_time::ptime ptime;
									is >> ptime;

									std::stringstream stream;
									facet = new boost::posix_time::time_facet();
									facet->format("[%Y,%m,%d,%H,%M,%S]");
									stream.imbue(std::locale(std::locale::classic(), facet));
									stream << ptime;
									std::string tmp_str = stream.str();
									if (tmp_str != "not-a-date-time")
									{
										field_row.push_back(std::move(tmp_str));
									} else {
										field_row.emplace_back("[]");
									}
								}
								catch(std::exception& e)
								{
									field_row.emplace_back("[]");
								}
								break;
							}
							case MYSQL_TYPE_TIME:
							{
								try
								{
									std::istringstream is(row[i]);
									is.imbue(loc_time);
									boost::posix_time::ptime ptime;
									is >> ptime;

									std::stringstream stream;
									facet = new boost::posix_time::time_facet();
									facet->format("[%H,%M,%S]");
									stream.imbue(std::locale(std::locale::classic(), facet));
									stream << ptime;
									std::string tmp_str = stream.str();
									if (tmp_str != "not-a-date-time")
									{
										field_row.push_back(std::move(tmp_str));
									} else {
										field_row.emplace_back("[]");
									}
								}
								catch(std::exception& e)
								{
									field_row.emplace_back("[]");
								}
								break;
							}
							case MYSQL_TYPE_NULL:
							{
								if (output_options[i].nullConvert)
								{
									field_row.emplace_back("objNull");
								} else {
									field_row.emplace_back("\"\"");
								}
								break;
							}
							default:
							{
								std::string tmp_str(row[i]);

								if (output_options[i].strip)
								{
									std::string stripped_str(tmp_str);
									for (auto &strip_char : strip_chars)
									{
										boost::erase_all(stripped_str, std::string(1, strip_char));
									}
									if (stripped_str != tmp_str)
									{
										switch (strip_chars_mode)
										{
											case 2: // Log + Error
												throw extDB3Exception("Bad Character detected from database query");
											//case 1: // Log
												//logger->warn("extDB3: SQL_CUSTOM: Error Bad Char Detected: Input: {0} Token: {1}", input_str, processed_inputs[i].buffer);
										}
										tmp_str = std::move(stripped_str);
									}
								}
								if (output_options[i].beguidConvert)
								{
									try
									{
										int64_t steamID = std::stoll(tmp_str, nullptr);
										std::stringstream bestring;
										int8_t i = 0, parts[8] = { 0 };
										do parts[i++] = steamID & 0xFF;
										while (steamID >>= 8);
										bestring << "BE";
										for (int i = 0; i < sizeof(parts); i++) {
											bestring << char(parts[i]);
										}
										tmp_str = md5(bestring.str());
									}
									catch(std::exception const & e)
									{
										tmp_str = "ERROR";
									}
								}
								if (output_options[i].boolConvert)
								{
									if (tmp_str == "1")
									{
										tmp_str = "true";
									} else {
										tmp_str = "false";
									}
								}
								if (output_options[i].string_remove_escape_quotes)
								{
									boost::replace_all(tmp_str, "\"\"", "\"");
								}
								if (output_options[i].string_add_escape_quotes)
								{
									boost::replace_all(tmp_str, "\"", "\"\"");
								}
								if (output_options[i].stringify)
								{
									tmp_str = "\"" + tmp_str + "\"";
								}
								if (output_options[i].stringify2)
								{
									tmp_str = "'" + tmp_str + "'";
								}
								field_row.push_back(std::move(tmp_str));
							}
						}
					}
					result_vec.push_back(std::move(field_row));
				}
			}
			mysql_free_result(result);
		}
	} while ((mysql_next_result(connector_ptr->mysql_ptr)) == 0);
}


void MariaDBQuery::get(int &check_dataType_string, bool &check_dataType_null, std::string &insertID, std::vector<std::vector<std::string>> &result_vec)
{
	result_vec.clear();
	do {
		MYSQL_RES *result = (mysql_store_result(connector_ptr->mysql_ptr));  // Returns NULL for Errors & No Result
		insertID = std::to_string(mysql_insert_id(connector_ptr->mysql_ptr));
		if (!result)
		{
			std::string error_msg(mysql_error(connector_ptr->mysql_ptr));
			mysql_free_result(result);
			if (!error_msg.empty())
			{
				throw MariaDBQueryException(connector_ptr->mysql_ptr);
			}
		} else {
			unsigned int num_fields = mysql_num_fields(result);
			if (num_fields > 0)
			{
				MYSQL_ROW row;
				MYSQL_FIELD *fields;
				fields = mysql_fetch_fields(result);
				while ((row = mysql_fetch_row(result)) != NULL)
				{
					std::vector<std::string> field_row;
					for (unsigned int i = 0; i < num_fields; i++)
					{
						if (!(row[i]))
						{
							if (check_dataType_null)
							{
								field_row.emplace_back("objNull");
							} else {
								field_row.emplace_back("\"\"");
							}
						} else {
							switch(fields[i].type)
							{
								case MYSQL_TYPE_VAR_STRING:
								{
									std::string tmp_str(row[i]);
									if (tmp_str.empty())
									{
										if (check_dataType_null)
										{
											field_row.emplace_back("objNull");
										} else {
											field_row.emplace_back("\"\"");
										}
									} else {
										switch (check_dataType_string)
										{
											case 1:
												field_row.emplace_back(('"' + std::move(tmp_str) + '"'));
												break;
											case 2:
												field_row.emplace_back(('\'' + std::move(tmp_str) + '\''));
												break;
											default:
												field_row.push_back(std::move(tmp_str));
										}
									}
									break;
								};
								case MYSQL_TYPE_TINY_BLOB:
								case MYSQL_TYPE_MEDIUM_BLOB:
								case MYSQL_TYPE_BLOB:
								{
									std::string tmp_str(row[i]);
									if (tmp_str.empty())
									{
										if (check_dataType_null)
										{
											field_row.emplace_back("objNull");
										} else {
											field_row.emplace_back("\"\"");
										}
									} else {
										field_row.push_back(std::move(tmp_str));
									}
									break;
								}
								case MYSQL_TYPE_DATE:
								{
									try
									{
										std::istringstream is(row[i]);
										is.imbue(loc_date);
										boost::posix_time::ptime ptime;
										is >> ptime;

										std::stringstream stream;
										facet = new boost::posix_time::time_facet();
										facet->format("[%Y,%m,%d]");
										stream.imbue(std::locale(std::locale::classic(), facet));
										stream << ptime;
										std::string tmp_str = stream.str();
										if (tmp_str != "not-a-date-time")
										{
											field_row.push_back(std::move(tmp_str));
										} else {
											field_row.emplace_back("[]");
										}
									}
									catch(std::exception& e)
									{
										field_row.emplace_back("[]");
									}
									break;
								}
								case MYSQL_TYPE_TIMESTAMP:
								case MYSQL_TYPE_DATETIME:
								{
									try
									{
										std::istringstream is(row[i]);
										is.imbue(loc_datetime);
										boost::posix_time::ptime ptime;
										is >> ptime;

										std::stringstream stream;
										facet = new boost::posix_time::time_facet();
										facet->format("[%Y,%m,%d,%H,%M,%S]");
										stream.imbue(std::locale(std::locale::classic(), facet));
										stream << ptime;
										std::string tmp_str = stream.str();
										if (tmp_str != "not-a-date-time")
										{
											field_row.push_back(std::move(tmp_str));
										} else {
											field_row.emplace_back("[]");
										}
									}
									catch(std::exception& e)
									{
										field_row.emplace_back("[]");
									}
									break;
								}
								case MYSQL_TYPE_TIME:
								{
									try
									{
										std::istringstream is(row[i]);
										is.imbue(loc_time);
										boost::posix_time::ptime ptime;
										is >> ptime;

										std::stringstream stream;
										facet = new boost::posix_time::time_facet();
										facet->format("[%H,%M,%S]");
										stream.imbue(std::locale(std::locale::classic(), facet));
										stream << ptime;
										std::string tmp_str = stream.str();
										if (tmp_str != "not-a-date-time")
										{
											field_row.push_back(std::move(tmp_str));
										} else {
											field_row.emplace_back("[]");
										}
									}
									catch(std::exception& e)
									{
										field_row.emplace_back("[]");
									}
									break;
								}
								case MYSQL_TYPE_NULL:
								{
									if (check_dataType_null)
									{
										field_row.emplace_back("objNull");
									} else {
										field_row.emplace_back("\"\"");
									}
									break;
								}
								default:
								{
									field_row.emplace_back(row[i]);
								}
							}
						}
					}
					result_vec.push_back(std::move(field_row));
				}
			}
			mysql_free_result(result);
		}
	} while ((mysql_next_result(connector_ptr->mysql_ptr)) == 0);
}


void MariaDBQuery::send(std::string &sql_query)
{
	unsigned long len = sql_query.length();
	int return_code = mysql_real_query(connector_ptr->mysql_ptr, sql_query.c_str(), len);
	if (return_code != 0)
	{
		int error_code = mysql_errno(connector_ptr->mysql_ptr);
		if ((error_code == CR_SERVER_GONE_ERROR) || (error_code == CR_SERVER_LOST))
		{
			return_code = mysql_real_query(connector_ptr->mysql_ptr, sql_query.c_str(), len);
		}
	}
	if (return_code != 0) throw MariaDBQueryException(connector_ptr->mysql_ptr);
}
