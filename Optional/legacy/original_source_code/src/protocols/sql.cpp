/*
 * extDB3
 * © 2016 Declan Ireland <https://bitbucket.org/torndeco/extdb3>
 */

#include "sql.h"

#include "../mariaDB/exceptions.h"
#include "../mariaDB/session.h"

#include <boost/algorithm/string.hpp>


bool SQL::init(AbstractExt *extension, const std::string &database_id, const std::string &options_str)
{
	extension_ptr = extension;

	if (extension_ptr->mariadb_databases.count(database_id) == 0)
	{
		#ifdef DEBUG_TESTING
			extension_ptr->console->warn("extDB3: SQL: No Database Connection: {0}", database_id);
		#endif
		extension_ptr->logger->warn("extDB3: SQL: No Database Connection: {0}", database_id);
		return false;
	}
	database_pool = &extension->mariadb_databases[database_id];

	std::vector<std::string> tokens;
	boost::split(tokens, options_str, boost::is_any_of("-"));
	for(auto& token: tokens)
	{
		if (boost::algorithm::iequals(token, std::string("TEXT")))
		{
			check_dataType_string = 1;
		}
		if (boost::algorithm::iequals(token, std::string("TEXT2")))
		{
			check_dataType_string = 2;
		}
		else if (boost::algorithm::iequals(token, std::string("NULL")))
		{
			check_dataType_null = true;
		}
	}

	#ifdef DEBUG_TESTING
		if (check_dataType_string > 0)
		{
			extension_ptr->console->info("extDB3: SQL: Initialized: Add Quotes around TEXT Datatypes mode: {0}", check_dataType_string);
		}
		if (check_dataType_null)
		{
			extension_ptr->console->info("extDB3: SQL: Initialized: NULL = objNull");
		}
		else
		{
			extension_ptr->console->info("extDB3: SQL: Initialized: NULL = \"\"");
		}
	#endif

	if (check_dataType_string > 0)
	{
		extension_ptr->logger->info("extDB3: SQL: Initialized: Add Quotes around TEXT Datatypes mode: {0}", check_dataType_string);
	}
	if (check_dataType_null)
	{
		extension_ptr->logger->info("extDB3: SQL: Initialized: NULL = objNull");
	}
	else
	{
		extension_ptr->logger->info("extDB3: SQL: Initialized: NULL = \"\"");
	}

	return true;
}


bool SQL::callProtocol(std::string input_str, std::string &result, const bool async_method, const unsigned int unique_id)
{
	#ifdef DEBUG_TESTING
		extension_ptr->console->info("extDB3: SQL: Trace: Input: {0}", input_str);
	#endif
	#ifdef DEBUG_LOGGING
		extension_ptr->logger->info("extDB3: SQL: Trace: Input: {0}", input_str);
	#endif
	try
	{
		std::string insertID = "0";
		MariaDBSession session(database_pool);
		session.data->query.send(input_str);

		std::vector<std::vector<std::string>> result_vec;
		session.data->query.get(check_dataType_string, check_dataType_null, insertID, result_vec);

		result = "[1,[";
		if (result_vec.size() > 0)
		{
			for(auto &row: result_vec)
			{
				result += "[";
				if (row.size() > 0)
				{
					for(auto &field: row)
					{
						if (field.empty())
						{
							result += "\"\"";
						} else {
							result += field;
						}
						result += ",";
					}
					result.pop_back();
				}
				result += "],";
			}
			result.pop_back();
		}
		result += "]]";

		#ifdef DEBUG_TESTING
			extension_ptr->console->info("extDB3: SQL: Trace: Result: {0}", result);
		#endif
		#ifdef DEBUG_LOGGING
			extension_ptr->logger->info("extDB3: SQL: Trace: Result: {0}", result);
		#endif
	}
	catch (MariaDBQueryException &e)
	{
		#ifdef DEBUG_TESTING
			extension_ptr->console->error("extDB3: SQL: Error MariaDBQueryException: {0}", e.what());
			extension_ptr->console->error("extDB3: SQL: Error MariaDBQueryException: Input: {0}", input_str);
		#endif
		extension_ptr->logger->error("extDB3: SQL: Error MariaDBQueryException: {0}", e.what());
		extension_ptr->logger->error("extDB3: SQL: Error MariaDBQueryException: Input: {0}", input_str);
		result = "[0,\"Error MariaDBQueryException Exception\"]";
	}
	catch (MariaDBConnectorException &e)
	{
		#ifdef DEBUG_TESTING
			extension_ptr->console->error("extDB3: SQL: Error MariaDBConnectorException: {0}", e.what());
			extension_ptr->console->error("extDB3: SQL: Error MariaDBConnectorException: Input: {0}", input_str);
		#endif
		extension_ptr->logger->error("extDB3: SQL: Error MariaDBConnectorException: {0}", e.what());
		extension_ptr->logger->error("extDB3: SQL: Error MariaDBConnectorException: Input: {0}", input_str);
		result = "[0,\"Error MariaDBConnectorException Exception\"]";
	}
	return true;
}
