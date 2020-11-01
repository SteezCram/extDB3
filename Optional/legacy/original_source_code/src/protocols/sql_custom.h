/*
 * extDB3
 * © 2016 Declan Ireland <https://bitbucket.org/torndeco/extdb3>
 */

#pragma once

#include <boost/filesystem.hpp>
#include <boost/property_tree/ini_parser.hpp>
#include <thread>
#include <unordered_map>


#include "abstract_protocol.h"
#include "../mariaDB/abstract.h"
#include "../mariaDB/session.h"

#define EXTDB_SQL_CUSTOM_REQUIRED_VERSION 1
#define EXTDB_SQL_CUSTOM_LATEST_VERSION 1


class SQL_CUSTOM: public AbstractProtocol
{
	public:
		struct sql_struct
		{
			std::string sql;
			std::vector<sql_option> input_options;
			std::vector<sql_option> output_options;
		};

		struct call_struct
		{
			bool preparedStatement = false;
			bool returnInsertID = false;
			bool returnInsertIDString = false;

			std::string strip_chars;
			int strip_chars_mode = 0;
			bool input_sqf_parser = false;
			
			int highest_input_value = 0;
			int num_of_retrys = 0;
			std::vector<sql_struct> sql;
		};
		
		bool init(AbstractExt *extension, const std::string &database_id, const std::string &options_str);
		bool callProtocol(std::string input_str, std::string &result, const bool async_method, const unsigned int unique_id=1);

	private:
		MariaDBPool *database_pool;
		boost::property_tree::ptree ptree;

		std::unordered_map<std::string, call_struct> calls;

		bool query(std::string &input_str, std::string &result, std::vector<std::vector<std::string>> &result_vec, std::vector<std::string> &tokens, MariaDBSession &session, std::string &insertID, std::unordered_map<std::string, call_struct>::iterator &calls_itr);
		bool preparedStatementPrepare(std::string &input_str, std::string &result, std::vector<std::vector<std::string>> &result_vec, MariaDBSession &session, MariaDBStatement *session_statement_itr, std::string callname, std::unordered_map<std::string, call_struct>::iterator &calls_itr);
		bool preparedStatementExecute(std::string &input_str, std::string &result, std::vector<std::vector<std::string>> &result_vec, MariaDBSession &session, MariaDBStatement *session_statement_itr, std::string callname, std::unordered_map<std::string, call_struct>::iterator &calls_itr, std::vector<std::string> &tokens, std::string &insertID);
		bool loadConfig(boost::filesystem::path &config_path);
};
