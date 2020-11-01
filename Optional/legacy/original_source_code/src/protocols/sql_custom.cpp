/*
 * extDB3
 * © 2016 Declan Ireland <https://bitbucket.org/torndeco/extdb3>
 */

#include "sql_custom.h"

#include <algorithm>
#include <thread>

#include <boost/algorithm/string.hpp>
#include <boost/algorithm/string/erase.hpp>
#include <boost/filesystem.hpp>
#include <boost/optional/optional.hpp>
#include <boost/property_tree/ini_parser.hpp>

#include "../mariaDB/exceptions.h"
#include "../md5/md5.h"

#include "../sqfparser.h"


bool SQL_CUSTOM::init(AbstractExt *extension, const std::string &database_id, const std::string &options_str)
{
	extension_ptr = extension;

	if (extension_ptr->mariadb_databases.count(database_id) == 0)
	{
		#ifdef DEBUG_TESTING
			extension_ptr->console->warn("extDB3: SQL_CUSTOM: No Database Connection: {0}", database_id);
		#endif
		extension_ptr->logger->warn("extDB3: SQL_CUSTOM: No Database Connection: {0}", database_id);
		return false;
	}
	database_pool = &extension->mariadb_databases[database_id];

	if (options_str.empty())
	{
		#ifdef DEBUG_TESTING
			extension_ptr->console->warn("extDB3: SQL_CUSTOM: Missing Config Parameter");
		#endif
		extension_ptr->logger->warn("extDB3: SQL_CUSTOM: Missing Config Parameter");
		return false;
	}

	try
	{
		boost::filesystem::path custom_ini_path(extension_ptr->ext_info.path);
		custom_ini_path /= "sql_custom";
		boost::filesystem::create_directories(custom_ini_path); // Create Directory if Missing
		custom_ini_path /= options_str;
		if (boost::filesystem::exists(custom_ini_path))
		{
			if (boost::filesystem::is_regular_file(custom_ini_path))
			{
				return loadConfig(custom_ini_path);
			} else {
				#ifdef DEBUG_TESTING
					extension_ptr->console->info("extDB3: SQL_CUSTOM: Loading Template Error: Not Regular File: {0}", custom_ini_path.string());
				#endif
				extension_ptr->logger->info("extDB3: SQL_CUSTOM: Loading Template Error: Not Regular File: {0}", custom_ini_path.string());
				return false;
			}
		} else {
			#ifdef DEBUG_TESTING
				extension_ptr->console->info("extDB3: SQL_CUSTOM: {0} doesn't exist", custom_ini_path.string());
			#endif
			extension_ptr->logger->info("extDB3: SQL_CUSTOM: {0} doesn't exist", custom_ini_path.string());
			return false;
		}
	}
	catch (boost::filesystem::filesystem_error &e)
	{
		#ifdef DEBUG_TESTING
			extension_ptr->console->info("extDB3: SQL_CUSTOM: filesystem_error: {0}", e.what());
		#endif
		extension_ptr->logger->info("extDB3: SQL_CUSTOM: filesystem_error: {0}", e.what());
		return false;
	}
}


bool SQL_CUSTOM::loadConfig(boost::filesystem::path &config_path)
{
	bool status = true;
	try
	{
		boost::property_tree::ini_parser::read_ini(config_path.string(), ptree);
		std::string strip_chars = ptree.get("Default.Strip Chars", "");
		int strip_chars_mode = ptree.get("Default.Strip Chars Mode", 0);
		int num_of_retrys = ptree.get("Default.Number of Retrys", 1);
		if ((num_of_retrys) <= 0)
		{
			num_of_retrys = 0;
		}
		bool input_sqf_parser = ptree.get("Default.Input SQF Parser", false);


		ptree.get_child("Default").erase("Strip Chars");
		ptree.get_child("Default").erase("Strip Chars Mode");
		ptree.get_child("Default").erase("Version");
		ptree.get_child("Default").erase("Input SQF Parser");
		ptree.get_child("Default").erase("Number of Retrys");

		for (auto& value : ptree.get_child("Default")) {
			#ifdef DEBUG_TESTING
				extension_ptr->console->info("extDB3: SQL_CUSTOM Config Error: Section Default Unknown Setting: {0}", value.first);
			#endif
			extension_ptr->logger->info("extDB3: SQL_CUSTOM Config Error: Section Default Unknown Setting: {0}", value.first);
			status = false;
		}

		ptree.erase("Default");
		for (auto& section : ptree) {

			int num_line = 1;
			int num_line_part = 1;
			int highest_input_value = 0;
			std::string path;

			while(true)
			{
				std::string sql;

				// Check For SQLx_1
				path = section.first + ".SQL" + std::to_string(num_line) + "_" + std::to_string(num_line_part);
				auto child = ptree.get_child_optional(path);
				if (!child) break;

				// Fetch SQLx_x
				while(true)
				{
					path = section.first + ".SQL" + std::to_string(num_line) + "_" + std::to_string(num_line_part);
					auto child = ptree.get_child_optional(path);
					if (!child)
					{
						ptree.get_child(section.first).erase("SQL" + std::to_string(num_line) + "_" + std::to_string(num_line_part));
						break;
					}
					sql += ptree.get<std::string>(path) + " ";
					ptree.get_child(section.first).erase("SQL" + std::to_string(num_line) + "_" + std::to_string(num_line_part));
					++num_line_part;
				}


				// Fetch Custom
					// Fetch SQL INPUT OPTION
				path = section.first + ".SQL" + std::to_string(num_line) + "_INPUTS";
				std::string input_options_str = ptree.get<std::string>(path, "");
				ptree.get_child(section.first).erase("SQL" + std::to_string(num_line) + "_INPUTS");

					// Fetch SQL OUTPUT OPTION
				path = section.first + ".OUTPUT";
				std::string output_options_str = ptree.get<std::string>(path, "");

				// Parse SQL INPUT OPTIONS
				std::vector<std::string> tokens;
				std::vector<std::string> sub_tokens;

				//calls[section.first].sql[num_line - 1] = sql_struct{};
				if (num_line > calls[section.first].sql.size())
				{
					calls[section.first].sql.resize(num_line);
				}
				if (!(input_options_str.empty()))
				{
					boost::split(tokens, input_options_str, boost::is_any_of(","));
					for (auto &token : tokens)
					{
						sub_tokens.clear();
						boost::trim(token);
						boost::split(sub_tokens, token, boost::is_any_of("-"));
						sql_option option;
						for (auto &sub_token : sub_tokens)
						{
							if (boost::algorithm::iequals(sub_token, std::string("beguid")) == 1)
							{
								option.beguidConvert = true;
							}
							else if	(boost::algorithm::iequals(sub_token, std::string("bool")) == 1)
							{
								option.boolConvert = true;
							}
							else if	(boost::algorithm::iequals(sub_token, std::string("null")) == 1)
							{
								option.nullConvert = true;
							}
							else if (boost::algorithm::iequals(sub_token, std::string("time")) == 1)
							{
								option.timeConvert = true;
							}
							else if	(boost::algorithm::iequals(sub_token, std::string("string")) == 1)
							{
								option.stringify = true;
							}
							else if	(boost::algorithm::iequals(sub_token, std::string("string2")) == 1)
							{
								option.stringify2 = true;
							}
							else if	(boost::algorithm::iequals(sub_token, std::string("add_escape_quotes")) == 1)
							{
								option.string_add_escape_quotes = true;
							}
							else if	(boost::algorithm::iequals(sub_token, std::string("remove_escape_quotes")) == 1)
							{
								option.string_remove_escape_quotes = true;
							}
							else if	(boost::algorithm::iequals(sub_token, std::string("remove_quotes")) == 1)
							{
								option.string_remove_quotes = true;
							}
							else if	(boost::algorithm::iequals(sub_token, std::string("strip")) == 1)
							{
								option.strip = true;
							}
							else if	(boost::algorithm::iequals(sub_token, std::string("mysql_escape")) == 1)
							{
								option.mysql_escape = true;
							}
							else
							{
								try
								{
									option.value_number = std::stoi(sub_token,nullptr);
								}
								catch(std::exception const &e)
								{
									#ifdef DEBUG_TESTING
										extension_ptr->console->info("extDB3: SQL_CUSTOM Config Error: Section: {0} Unknown INPUT Option: {1} in {2}", section.first, sub_token, token);
									#endif
									extension_ptr->logger->info("extDB3: SQL_CUSTOM Config Error: Section: {0} Unknown INPUT Option: {1} in {2}", section.first, sub_token, token);
									status = false;
								}
							}
						}
						if (option.value_number > 0)
						{
							if (option.value_number > highest_input_value)
							{
								highest_input_value = option.value_number;
							}
							calls[section.first].sql[(num_line - 1)].input_options.push_back(option);
						}
					}
					calls[section.first].highest_input_value = std::move(highest_input_value);
				}

				// Parse SQL OUTPUT OPTIONS
				tokens.clear();
				if (!(output_options_str.empty()))
				{
					boost::split(tokens, output_options_str, boost::is_any_of(","));
					for (auto &token : tokens)
					{
						sub_tokens.clear();
						boost::trim(token);
						boost::split(sub_tokens, token, boost::is_any_of("-"));
						sql_option option;
						for (auto &sub_token : sub_tokens)
						{
							if (boost::algorithm::iequals(sub_token, std::string("beguid")) == 1)
							{
								option.beguidConvert = true;
							}
							else if	(boost::algorithm::iequals(sub_token, std::string("bool")) == 1)
							{
								option.boolConvert = true;
							}
							else if	(boost::algorithm::iequals(sub_token, std::string("null")) == 1)
							{
								option.nullConvert = true;
							}
							else if	(boost::algorithm::iequals(sub_token, std::string("string")) == 1)
							{
								option.stringify = true;
							}
							else if	(boost::algorithm::iequals(sub_token, std::string("string2")) == 1)
							{
								option.stringify2 = true;
							}
							else if	(boost::algorithm::iequals(sub_token, std::string("add_escape_quotes")) == 1)
							{
								option.string_add_escape_quotes = true;
							}
							else if	(boost::algorithm::iequals(sub_token, std::string("remove_escape_quotes")) == 1)
							{
								option.string_remove_escape_quotes = true;
							}
							else if	(boost::algorithm::iequals(sub_token, std::string("remove_quotes")) == 1)
							{
								option.string_remove_quotes = true;
							}
							else if	(boost::algorithm::iequals(sub_token, std::string("strip")) == 1)
							{
								option.strip = true;
							}
							else
							{
								try
								{
									option.value_number = std::stoi(sub_token,nullptr);
								}
								catch(std::exception const &e)
								{
									#ifdef DEBUG_TESTING
										extension_ptr->console->info("extDB3: SQL_CUSTOM Config Error: Section: {0} Unknown OUTPUT Option: {1} in {2}", section.first, sub_token, token);

									#endif
									extension_ptr->logger->info("extDB3: SQL_CUSTOM Config Error: Section: {0} Unknown OUTPUT Option: {1} in {2}", section.first, sub_token, token);
									extension_ptr->logger->info("extDB3: SQL_CUSTOM Config Error: Debug: {0}", output_options_str);
									status = false;
								}
							}
						}
						calls[section.first].sql[(num_line - 1)].output_options.push_back(std::move(option));
					}
				}

				if (!sql.empty())
				{
					sql.pop_back();
				}
				calls[section.first].sql[(num_line - 1)].sql = sql;

				// Foo
				++num_line;
				num_line_part = 1;
			}
			ptree.get_child(section.first).erase("OUTPUT");

			path = section.first + ".Prepared Statement";
			calls[section.first].preparedStatement = ptree.get(path, true);
			ptree.get_child(section.first).erase("Prepared Statement");

			path = section.first + ".Return InsertID";
			calls[section.first].returnInsertID = ptree.get(path, false);
			ptree.get_child(section.first).erase("Return InsertID");
			
			path = section.first + ".Return InsertID String";
			calls[section.first].returnInsertIDString = ptree.get(path, false);
			ptree.get_child(section.first).erase("Return InsertID String");

			path = section.first + ".Strip Chars";
			calls[section.first].strip_chars = ptree.get(path, strip_chars);
			ptree.get_child(section.first).erase("Strip Chars");

			path = section.first + ".Strip Chars Mode";
			calls[section.first].strip_chars_mode = ptree.get(path, strip_chars_mode);
			ptree.get_child(section.first).erase("Strip Chars Mode");

			path = section.first + ".Input SQF Parser";
			calls[section.first].input_sqf_parser = ptree.get(path, input_sqf_parser);
			ptree.get_child(section.first).erase("Input SQF Parser");

			path = section.first + ".Number of Retrys";
			calls[section.first].num_of_retrys = ptree.get(path, num_of_retrys);
			if (calls[section.first].num_of_retrys < 0)
			{
				calls[section.first].num_of_retrys = 0;
			}
			ptree.get_child(section.first).erase("Number of Retrys");

			for (auto& value : section.second) {
				#ifdef DEBUG_TESTING
					extension_ptr->console->info("extDB3: SQL_CUSTOM Config Error: Section: {0} Unknown Setting: {1}", section.first, value.first);
				#endif
				extension_ptr->logger->info("extDB3: SQL_CUSTOM Config Error: Section: {0} Unknown Setting: {1}", section.first, value.first);
				status = false;
			}
		}
		return status;
	}
	catch (boost::property_tree::ini_parser::ini_parser_error const& e)
	{
		#ifdef DEBUG_TESTING
			extension_ptr->console->info("extDB3: SQL_CUSTOM Config Error: {0}", e.what());
		#endif
		extension_ptr->logger->info("extDB3: SQL_CUSTOM Config Error: {0}", e.what());
		return false;
	}
	catch (boost::property_tree::ptree_bad_path const &e)
	{
		#ifdef DEBUG_TESTING
			extension_ptr->console->info("extDB3: SQL_CUSTOM Config Error: {0}", e.what());
		#endif
		extension_ptr->logger->info("extDB3: SQL_CUSTOM Config Error: {0}", e.what());
		return false;
	}
}

bool SQL_CUSTOM::query(std::string &input_str, std::string &result, std::vector<std::vector<std::string>> &result_vec, std::vector<std::string> &tokens, MariaDBSession &session, std::string &insertID, std::unordered_map<std::string, call_struct>::iterator &calls_itr)
{
	// -------------------
	// Raw SQL
	// -------------------
	for (auto &sql : calls_itr->second.sql)
	{
		std::string sql_str = sql.sql;
		std::string tmp_str;
		for (int i = 0; i < sql.input_options.size(); ++i)
		{
			int value_number = sql.input_options[i].value_number;
			tmp_str = tokens[value_number];
			if (sql.input_options[i].strip)
			{
				std::string stripped_str = tmp_str;
				for (auto &strip_char : calls_itr->second.strip_chars)
				{
					boost::erase_all(stripped_str, std::string(1, strip_char));
				}
				if (stripped_str != tmp_str)
				{
					switch (calls_itr->second.strip_chars_mode)
					{
						case 2: // Log + Error
							extension_ptr->logger->warn("extDB3: SQL_CUSTOM: Error Bad Char Detected: Input: {0} Token: {1}", input_str, tmp_str);
							result = "[0,\"Error Strip Char Found\"]";
							return false;
						case 1: // Log
							extension_ptr->logger->warn("extDB3: SQL_CUSTOM: Error Bad Char Detected: Input: {0} Token: {1}", input_str, tmp_str);
					}
					tmp_str = std::move(stripped_str);
				}
			}
			if (sql.input_options[i].beguidConvert)
			{
				std::string beguid_str;
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
					beguid_str = md5(bestring.str());
				}
				catch(std::exception const &e)
				{
					beguid_str = e.what();
				}
				tmp_str = beguid_str;
			}
			if (sql.input_options[i].boolConvert)
			{
				if (boost::algorithm::iequals(tmp_str, std::string("1")) == 1)
				{
					tmp_str = "true";
				} else {
					tmp_str = "false";
				}
			}
			if (sql.input_options[i].nullConvert)
			{
				if (tmp_str.empty())
				{
					tmp_str = "objNull";
				}
			}
			if (sql.input_options[i].string_remove_escape_quotes)
			{
				boost::replace_all(tmp_str, "\"\"", "\"");
			}
			if (sql.input_options[i].string_add_escape_quotes)
			{
				boost::replace_all(tmp_str, "\"", "\"\"");
			}
			if (sql.input_options[i].string_remove_quotes)
			{
				boost::replace_all(tmp_str, "\"", "");
				boost::replace_all(tmp_str, "\'", "");
			}
			if (sql.input_options[i].stringify)
			{
				tmp_str = "\"" + tmp_str + "\"";
			}
			if (sql.input_options[i].stringify2)
			{
				tmp_str = "'" + tmp_str + "'";
			}
			if (sql.input_options[i].mysql_escape)
			{
				std::string tmp_escaped_str(" ", (tmp_str.length() * 2) + 1);
				mysql_real_escape_string(session.data->connector.mysql_ptr, &tmp_escaped_str[0], tmp_str.c_str(), tmp_str.length());
				tmp_str = std::move(tmp_escaped_str);
			}
			boost::replace_all(sql_str, ("$CUSTOM_" + std::to_string(i + 1) + "$"), tmp_str.c_str());  //TODO Improve this
		}
		try
		{
			auto &session_query_itr = session.data->query;
			session.data->query.send(sql_str);
			//session.data->query.get(insertID, result_vec); // TODO: OUTPUT OPTIONS SUPPORT
			session.data->query.get(sql.output_options, calls_itr->second.strip_chars, calls_itr->second.strip_chars_mode, insertID, result_vec);
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
			return false;
		}
	}
	return true;
}

bool SQL_CUSTOM::preparedStatementPrepare(std::string &input_str, std::string &result, std::vector<std::vector<std::string>> &result_vec, MariaDBSession &session, MariaDBStatement *session_statement_itr, std::string callname, std::unordered_map<std::string, call_struct>::iterator &calls_itr)
{
	try
	{
		if (session.data->statements.count(callname) == 0)
		{
			session.data->statements[callname].resize(calls_itr->second.sql.size());

			for (int sql_index = 0; sql_index < calls_itr->second.sql.size(); ++sql_index)
			{
				session_statement_itr = &session.data->statements[callname][sql_index];
				session_statement_itr->init(session.data->connector);
				session_statement_itr->create();
				session_statement_itr->prepare(calls_itr->second.sql[sql_index].sql);
			}
		}
	}
	catch (MariaDBStatementException0 &e)
	{
		#ifdef DEBUG_TESTING
			extension_ptr->console->error("extDB3: SQL: Error MariaDBStatementException0: {0}", e.what());
			extension_ptr->console->error("extDB3: SQL: Error MariaDBStatementException0: Input: {0}", input_str);
		#endif
		extension_ptr->logger->error("extDB3: SQL: Error MariaDBStatementException0: {0}", e.what());
		extension_ptr->logger->error("extDB3: SQL: Error MariaDBStatementException0: Input: {0}", input_str);
		result = "[0,\"Error MariaDBStatementException0 Exception\"]";
		session.resetSession();
		return false;
	}
	catch (MariaDBStatementException1 &e)
	{
		#ifdef DEBUG_TESTING
			extension_ptr->console->error("extDB3: SQL: Error MariaDBStatementException1: {0}", e.what());
			extension_ptr->console->error("extDB3: SQL: Error MariaDBStatementException1: Input: {0}", input_str);
		#endif
		extension_ptr->logger->error("extDB3: SQL: Error MariaDBStatementException1: {0}", e.what());
		extension_ptr->logger->error("extDB3: SQL: Error MariaDBStatementException1: Input: {0}", input_str);
		result = "[0,\"Error MariaDBStatementException1 Exception\"]";
		session.resetSession();
		return false;
	}
	catch (extDB3Exception &e)
	{
		#ifdef DEBUG_TESTING
			extension_ptr->console->error("extDB3: SQL: Error extDB3Exception: {0}", e.what());
			extension_ptr->console->error("extDB3: SQL: Error extDB3Exception: Input: {0}", input_str);
		#endif
		extension_ptr->logger->error("extDB3: SQL: Error extDB3Exception: {0}", e.what());
		extension_ptr->logger->error("extDB3: SQL: Error extDB3Exception: Input: {0}", input_str);
		result = "[0,\"Error extDB3Exception Exception\"]";
		session.resetSession();
		return false;
	}
	return true;
}

bool SQL_CUSTOM::preparedStatementExecute(std::string &input_str, std::string &result, std::vector<std::vector<std::string>> &result_vec, MariaDBSession &session, MariaDBStatement *session_statement_itr, std::string callname, std::unordered_map<std::string, call_struct>::iterator &calls_itr, std::vector<std::string> &tokens, std::string &insertID)
{
	for (int sql_index = 0; sql_index < calls_itr->second.sql.size(); ++sql_index)
	{
		std::vector<MariaDBStatement::mysql_bind_param> processed_inputs;
		processed_inputs.resize(calls_itr->second.sql[sql_index].input_options.size());
		for (int i = 0; i < processed_inputs.size(); ++i)
		{
			processed_inputs[i].type = MYSQL_TYPE_VARCHAR;
			processed_inputs[i].buffer = tokens[calls_itr->second.sql[sql_index].input_options[i].value_number];
			processed_inputs[i].length = processed_inputs[i].buffer.size();
			if (calls_itr->second.sql[sql_index].input_options[i].strip)
			{
				std::string stripped_str = processed_inputs[i].buffer;
				for (auto &strip_char : calls_itr->second.strip_chars)
				{
					boost::erase_all(stripped_str, std::string(1, strip_char));
				}
				if (stripped_str != processed_inputs[i].buffer)
				{
					switch (calls_itr->second.strip_chars_mode)
					{
						case 2: // Log + Error
							extension_ptr->logger->warn("extDB3: SQL_CUSTOM: Error Bad Char Detected: Input: {0} Token: {1}", input_str, processed_inputs[i].buffer);
							result = "[0,\"Error Strip Char Found\"]";
							return true;
						case 1: // Log
							extension_ptr->logger->warn("extDB3: SQL_CUSTOM: Error Bad Char Detected: Input: {0} Token: {1}", input_str, processed_inputs[i].buffer);
					}
					processed_inputs[i].buffer = std::move(stripped_str);
					processed_inputs[i].length = processed_inputs[i].buffer.size();
				}
			}
			if (calls_itr->second.sql[sql_index].input_options[i].beguidConvert)
			{
				std::string beguid_str;
				try
				{
					int64_t steamID = std::stoll(processed_inputs[i].buffer, nullptr);
					std::stringstream bestring;
					int8_t i = 0, parts[8] = { 0 };
					do parts[i++] = steamID & 0xFF;
					while (steamID >>= 8);
					bestring << "BE";
					for (int i = 0; i < sizeof(parts); i++) {
						bestring << char(parts[i]);
					}
					beguid_str = std::move(md5(bestring.str()));
				}
				catch(std::exception const &e)
				{
					beguid_str = "ERROR";
				}
				processed_inputs[i].buffer = beguid_str;
				processed_inputs[i].length = beguid_str.size();
			}
			if (calls_itr->second.sql[sql_index].input_options[i].boolConvert)
			{
				if (boost::algorithm::iequals(processed_inputs[i].buffer, std::string("true")) == 1)
				{
					processed_inputs[i].buffer = "1";
				} else {
					processed_inputs[i].buffer = "0";
				}
				processed_inputs[i].length = processed_inputs[i].buffer.size();
			}
			if (calls_itr->second.sql[sql_index].input_options[i].nullConvert)
			{
				if (processed_inputs[i].buffer.empty())
				{
					processed_inputs[i].type = MYSQL_TYPE_NULL;
				}
			}
			if (calls_itr->second.sql[sql_index].input_options[i].string_remove_escape_quotes)
			{
					boost::replace_all(processed_inputs[i].buffer, "\"\"", "\"");
					processed_inputs[i].length = processed_inputs[i].buffer.size();
			}
			if (calls_itr->second.sql[sql_index].input_options[i].string_add_escape_quotes)
			{
					boost::replace_all(processed_inputs[i].buffer, "\"", "\"\"");
					processed_inputs[i].length = processed_inputs[i].buffer.size();
			}
			if (calls_itr->second.sql[sql_index].input_options[i].string_remove_quotes)
			{
					boost::replace_all(processed_inputs[i].buffer, "\"", "");
					boost::replace_all(processed_inputs[i].buffer, "\'", "");
					processed_inputs[i].length = processed_inputs[i].buffer.size();
			}
			if (calls_itr->second.sql[sql_index].input_options[i].stringify)
			{
					processed_inputs[i].buffer = "\"" + processed_inputs[i].buffer + "\"";
					processed_inputs[i].length = processed_inputs[i].buffer.size();
			}
			if (calls_itr->second.sql[sql_index].input_options[i].stringify2)
			{
					processed_inputs[i].buffer = "'" + processed_inputs[i].buffer + "'";
					processed_inputs[i].length = processed_inputs[i].buffer.size();
			}
			if (calls_itr->second.sql[sql_index].input_options[i].timeConvert)
			{
				processed_inputs[i].type = MYSQL_TYPE_DATETIME;
			}
		}
		try
		{
			session_statement_itr = &session.data->statements[callname][sql_index];
			session_statement_itr->bindParams(processed_inputs);
			session_statement_itr->execute(calls_itr->second.sql[sql_index].output_options, calls_itr->second.strip_chars, calls_itr->second.strip_chars_mode, insertID, result_vec);
		}
		catch (MariaDBStatementException0 &e)
		{
			#ifdef DEBUG_TESTING
				extension_ptr->console->error("extDB3: SQL: Error MariaDBStatementException0: {0}", e.what());
				extension_ptr->console->error("extDB3: SQL: Error MariaDBStatementException0: Input: {0}", input_str);
			#endif
			extension_ptr->logger->error("extDB3: SQL: Error MariaDBStatementException0: {0}", e.what());
			extension_ptr->logger->error("extDB3: SQL: Error MariaDBStatementException0: Input: {0}", input_str);
			result = "[0,\"Error MariaDBStatementException0 Exception\"]";
			session.resetSession();
			return false;
		}
		catch (MariaDBStatementException1 &e)
		{
			#ifdef DEBUG_TESTING
				extension_ptr->console->error("extDB3: SQL: Error MariaDBStatementException1: {0}", e.what());
				extension_ptr->console->error("extDB3: SQL: Error MariaDBStatementException1: Input: {0}", input_str);
			#endif
			extension_ptr->logger->error("extDB3: SQL: Error MariaDBStatementException1: {0}", e.what());
			extension_ptr->logger->error("extDB3: SQL: Error MariaDBStatementException1: Input: {0}", input_str);
			result = "[0,\"Error MariaDBStatementException1 Exception\"]";
			session.resetSession();
			return false;
		}
		catch (extDB3Exception &e)
		{
			#ifdef DEBUG_TESTING
				extension_ptr->console->error("extDB3: SQL: Error extDB3Exception: {0}", e.what());
				extension_ptr->console->error("extDB3: SQL: Error extDB3Exception: Input: {0}", input_str);
			#endif
			extension_ptr->logger->error("extDB3: SQL: Error extDB3Exception: {0}", e.what());
			extension_ptr->logger->error("extDB3: SQL: Error extDB3Exception: Input: {0}", input_str);
			result = "[0,\"Error extDB3Exception Exception\"]";
			session.resetSession();
			return false;
		}
	}
	return true;
}

bool SQL_CUSTOM::callProtocol (std::string input_str, std::string &result, const bool async_method, const unsigned int unique_id)
{
	#ifdef DEBUG_TESTING
		extension_ptr->console->info("extDB3: SQL_CUSTOM: Trace: UniqueID: {0} Input: {1}", unique_id, input_str);
	#endif
	#ifdef DEBUG_LOGGING
		extension_ptr->logger->info("extDB3: SQL_CUSTOM: Trace: UniqueID: {0} Input: {1}", unique_id, input_str);
	#endif

	std::string callname;
	//std::string tokens_str;
	std::string insertID = "0";
	const std::string::size_type found = input_str.find(":");
	if (found != std::string::npos)
	{
		callname = input_str.substr(0, found);
		//tokens_str = input_str.substr(found+1);
	}	else {
		callname = input_str;
	}
	std::unordered_map<std::string, SQL_CUSTOM::call_struct>::iterator calls_itr = calls.find(callname);
	if (calls_itr == calls.end())
	{
		// NO CALLNAME FOUND IN PROTOCOL
		result = "[0,\"Error No Custom Call Not Found\"]";
		extension_ptr->logger->warn("extDB3: SQL_CUSTOM: Error No Custom Call Not Found: Input String {0}", input_str);
		extension_ptr->logger->warn("extDB3: SQL_CUSTOM: Error No Custom Call Not Found: Callname {0}", callname);
		#ifdef DEBUG_TESTING
			extension_ptr->console->warn("extDB3: SQL_CUSTOM: Error No Custom Call Not Found: Input String {0}", input_str);
			extension_ptr->console->warn("extDB3: SQL_CUSTOM: Error No Custom Call Not Found: Callname {0}", callname);
		#endif
		return true;
	}

	std::vector<std::vector<std::string>> result_vec;
	try
	{
		MariaDBSession session(database_pool);

		std::vector<std::string> tokens;
		if (calls_itr->second.input_sqf_parser)
		{
			if (found != std::string::npos)
			{
				tokens.push_back(callname);
				std::string tokens_str = input_str.substr(found+1);
				sqf::parser(tokens_str, tokens);
			}
		} else {
			boost::split(tokens, input_str, boost::is_any_of(":"));
		}

		if ((tokens.size()-1) != calls_itr->second.highest_input_value)
		{
			throw extDB3Exception("Config Invalid Number Number of Inputs Got " + std::to_string(tokens.size()-1) + " Expected " + std::to_string(calls_itr->second.highest_input_value));
		}

		bool success = false;
		if (!calls_itr->second.preparedStatement)
		{
			for (int i = 0; i <= calls_itr->second.num_of_retrys; ++i)
			{
				if (!query(input_str, result, result_vec, tokens, session, insertID, calls_itr))
				{
					// DO NOTHING
				} else {
					success = true;
					break;
				}
			}

		} else {
			// -------------------
			// Prepared Statement
			// -------------------
			for (int i = 0; i <= calls_itr->second.num_of_retrys; ++i)
			{
				MariaDBStatement *session_statement_itr = nullptr;
				if (!preparedStatementPrepare(input_str, result, result_vec, session, session_statement_itr, callname, calls_itr))
				{
					// DO NOTHING
				} else {
					if (!preparedStatementExecute(input_str, result, result_vec, session, session_statement_itr, callname, calls_itr, tokens, insertID))
					{
						// DO NOTHING
					} else {
						success = true;
						break;
					}
				}
			}
		}
		if (!success)
		{
			#ifdef DEBUG_TESTING
				extension_ptr->console->error("extDB3: SQL: Error Max Retrys Reached");
				extension_ptr->console->error("extDB3: SQL: Error Max Retrys Reached");
			#endif
			extension_ptr->logger->error("extDB3: SQL: Error Max Retrys Reached");
			extension_ptr->logger->error("extDB3: SQL: Error Max Retrys Reached");
			return true;
		}
		result = "[1,[";
		if (calls_itr->second.returnInsertID)
		{
			result += std::move(insertID) + ",[";
		} else if (calls_itr->second.returnInsertIDString)
		{
			result += "\"" + std::move(insertID) + "\",[";
		}
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
		if ((calls_itr->second.returnInsertID) || (calls_itr->second.returnInsertIDString))
		{
			result += "]";
		}
		#ifdef DEBUG_TESTING
			extension_ptr->console->info("extDB3: SQL_CUSTOM: Trace: Result: {0}", result);
		#endif
		#ifdef DEBUG_LOGGING
			extension_ptr->logger->info("extDB3: SQL_CUSTOM: Trace: Result: {0}", result);
		#endif
	}
	catch (extDB3Exception &e) // Make new exception & renamed it
	{
		#ifdef DEBUG_TESTING
			extension_ptr->console->error("extDB3: SQL: Error extDB3Exception: {0}", e.what());
			extension_ptr->console->error("extDB3: SQL: Error extDB3Exception: Input: {0}", input_str);
		#endif
		extension_ptr->logger->error("extDB3: SQL: Error extDB3Exception: {0}", e.what());
		extension_ptr->logger->error("extDB3: SQL: Error extDB3Exception: Input: {0}", input_str);
		result = "[0,\"Error extDB3Exception Exception\"]";
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
