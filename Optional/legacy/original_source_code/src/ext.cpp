/*
 * extDB3
 * © 2016 Declan Ireland <https://bitbucket.org/torndeco/extdb3>
 */

#include "ext.h"

#include <cstdlib>
#include <cstring>
#include <iostream>
#include <mutex>
#include <regex>
#include <stdlib.h>
#include <thread>

#include <boost/algorithm/string.hpp>
#include <boost/asio.hpp>
#include <boost/bind.hpp>
#include <boost/date_time/posix_time/posix_time.hpp>
#include <boost/filesystem.hpp>
#include <boost/property_tree/ini_parser.hpp>

#include <boost/random/random_device.hpp>
#include <boost/random/uniform_int_distribution.hpp>

#include "abstract_ext.h"
#include "mariaDB/exceptions.h"
#include "md5/md5.h"

#include "protocols/abstract_protocol.h"
#include "protocols/sql.h"
#include "protocols/sql_custom.h"
#include "protocols/log.h"


Ext::Ext(std::string shared_library_path)
{
	uptime_start = std::chrono::steady_clock::now();
	std::setlocale(LC_ALL, "");
	std::locale::global(std::locale(""));
	std::setlocale(LC_NUMERIC, "C");
	mysql_library_init(0, NULL, NULL);

	try
	{
		bool conf_found = false;
		bool conf_randomized = false;

		boost::filesystem::path config_path;

		// extDB3 Shared Library Location
		config_path = shared_library_path;
		config_path = config_path.parent_path();
		config_path /= "extdb3-conf.ini";
		if (boost::filesystem::is_regular_file(config_path))
		{
			conf_found = true;
			ext_info.path = config_path.parent_path().string();
		}	else {
			// Search for Randomize Config File -- Legacy Security Support For Arma2Servers

			config_path = config_path.parent_path();
			// CHECK DLL PATH FOR CONFIG)
			if (!config_path.string().empty())
			{
				search(config_path, conf_found, conf_randomized);
			}
		}

		if (conf_found)
		{
			boost::property_tree::ini_parser::read_ini(config_path.string(), ptree);
			ext_info.logger_flush = ptree.get("Log.Flush",true);

			// Search for Randomize Config File -- Legacy Security Support For Arma2Servers

			if ((ptree.get("Main.Randomize Config File",false)) && (!conf_randomized))
			// Only Gonna Randomize Once, Keeps things Simple
			{
				std::string chars("ABCDEFGHIJKLMNOPQRSTUVWXYZ"
									"1234567890");
				// Skipping Lowercase, this function only for arma2 + extensions only available on windows.
				boost::random::random_device rng;
				boost::random::uniform_int_distribution<> index_dist(0, chars.size() - 1);

				std::string randomized_filename = "extdb3-conf-";
				for (int i = 0; i < 8; ++i)
				{
					randomized_filename += chars[index_dist(rng)];
				}
				randomized_filename += ".ini";

				boost::filesystem::path randomize_configfile_path = config_path.parent_path() /= randomized_filename;
				boost::filesystem::rename(config_path, randomize_configfile_path);
			}
		}

		// Initialize Loggers
		//		Console Logger
		//size_t q_size = 1048576; //queue size must be power of 2
		//spdlog::set_async_mode(q_size);

		#ifdef DEBUG_TESTING
			spdlog::drop("extDB3 Console logger");
			auto console_temp = spdlog::stdout_logger_mt("extDB3 Console logger");
			console.swap(console_temp);
		#elif TEST_APP
			spdlog::drop("extDB3 Console logger");{
			auto console_temp = spdlog::stdout_logger_mt("extDB3 Console logger");
			console.swap(console_temp);
		#endif

		//		File Logger
		std::time_t t = std::time(nullptr);
		std::tm tm = *std::localtime(&t); //Not Threadsafe

		boost::filesystem::path log_relative_path;
		log_relative_path = boost::filesystem::path(ext_info.path);
		log_relative_path /= "logs";
		log_relative_path /= std::to_string((tm.tm_year + 1900));
		log_relative_path /= std::to_string((tm.tm_mon + 1));
		log_relative_path /= std::to_string(tm.tm_mday);
		ext_info.log_path = log_relative_path.make_preferred().string();
		boost::filesystem::create_directories(log_relative_path);
		log_relative_path /= std::to_string(tm.tm_hour) + "-" + std::to_string(tm.tm_min) + "-" + std::to_string(tm.tm_sec);

		spdlog::drop("extDB3 File logger");
		logger = spdlog::rotating_logger_mt("extDB3 File Logger", log_relative_path.make_preferred().string(), 1048576 * 100, 3);
		
		if (ext_info.logger_flush)
			logger->flush_on(spdlog::level::info);
		spdlog::set_level(spdlog::level::info);
		spdlog::set_pattern("%v");

		logger->info("");
		logger->info("extDB3: https://bitbucket.org/torndeco/extdb3/wiki/Home");
		logger->info("extDB3: Version: {0}", EXTDB_VERSION);
		#ifdef __GNUC__
			#ifndef DEBUG_TESTING
				logger->info("extDB3: Linux Version");
			#else
				logger->info("extDB3: Linux Debug Version");
			#endif
		#endif

		#ifdef _MSC_VER
			#ifndef DEBUG_LOGGING
				logger->info("extDB3: Windows Version");
			#else
				logger->info("extDB3: Windows Debug Version");
				logger->info("");
			#endif
		#endif

		#ifdef TEST_APP
			console->info("Welcome to extDB3 Test Application");
			console->info("OutputSize is set to 80 for Test Application, just so it is readable");
			console->info("OutputSize for Arma3 is more like 10k in size ");
			console->info("");
			console->info("Typing test will spam 1:SQL:TEST<1-5>:testing");
			console->info("This is used for poor man stress testing");
			console->info("");
			console->info("Type 'test' for spam test");
			console->info("Type 'quit' to exit");
		#else
			logger->info("Message: All development for extDB3 is done on a Linux Dedicated Server");
			logger->info("Message: If you would like to Donate to extDB3 Development");
			logger->info("Message: https://www.paypal.com/cgi-bin/webscr?cmd=_s-xclick&hosted_button_id=2SUEFTGABTAM2");
			logger->info("Message: Also leave a message if there is any particular feature you would like to see added.");
			logger->info("Message: Thanks for all the people that have donated.");
			logger->info("Message: Torndeco: 18/05/15");
			logger->info("");
			logger->info("");
		#endif

		if (!conf_found)
		{
			std::cout << "extDB3: Unable to find extdb3-conf.ini" << std::endl;
			logger->critical("extDB3: Unable to find extdb3-conf.ini");
			// Kill Server no config file found -- Evil
			std::exit(EXIT_SUCCESS);
		}
		else
		{
			#ifdef DEBUG_TESTING
				console->info("extDB3: Found extdb3-conf.ini");
			#endif
			logger->info("extDB3: Found extdb3-conf.ini");

			if ((ptree.get("Main.Version",0)) != EXTDB_CONF_VERSION)
			{
				#ifdef DEBUG_TESTING
					console->critical("extDB3: Incompatiable Config Version: {0},  Required Version: 1", (ptree.get("Main.Version",0)), EXTDB_CONF_VERSION);
				#endif
				logger->critical("extDB3: Incompatiable Config Version: {0},  Required Version: 1", (ptree.get("Main.Version",0)), EXTDB_CONF_VERSION);
				// Kill Server if wrong config version -- Evil
				std::exit(EXIT_SUCCESS);
			}

			//
			ext_info.allow_reset = ptree.get("Main.Allow Reset",false);

			// Start Threads + ASIO
			ext_info.max_threads = ptree.get("Main.Threads",0);
			int detected_cpu_cores = boost::thread::hardware_concurrency();
			if (ext_info.max_threads <= 0)
			{
				// Auto-Detect
				if (detected_cpu_cores > 6)
				{
					#ifdef DEBUG_TESTING
						console->info("extDB3: Detected {0} Cores, Setting up {1} Worker Threads", detected_cpu_cores, 6);
					#endif
					logger->info("extDB3: Detected {0} Cores, Setting up {1} Worker Threads", detected_cpu_cores, 6);
					ext_info.max_threads = 6;
				}
				else if (detected_cpu_cores <= 2)
				{
					#ifdef DEBUG_TESTING
						console->info("extDB3: Detected {0} Cores, Setting up {1} Worker Threads", detected_cpu_cores, 2);
					#endif
					logger->info("extDB3: Detected {0} Cores, Setting up {1} Worker Threads", detected_cpu_cores, 2);
					ext_info.max_threads = 2;
				}	else {
					ext_info.max_threads = detected_cpu_cores;
					#ifdef DEBUG_TESTING
						console->info("extDB3: Detected {0} Cores, Setting up {1} Worker Threads", detected_cpu_cores, ext_info.max_threads);
					#endif
					logger->info("extDB3: Detected {0} Cores, Setting up {1} Worker Threads", detected_cpu_cores, ext_info.max_threads);
				}
			}
			else if (ext_info.max_threads > 8)  // Sanity Check
			{
				// Manual Config
				#ifdef DEBUG_TESTING
					console->info("extDB3: Sanity Check, Setting up {0} Worker Threads (config settings {1})", 8, ext_info.max_threads);
				#endif
				logger->info("extDB3: Sanity Check, Setting up {0} Worker Threads (config settings {1})", 8, ext_info.max_threads);
				ext_info.max_threads = 8;
			}	else {
				// Manual Config
				#ifdef DEBUG_TESTING
					console->info("extDB3: Detected {0} Cores, Setting up {1} Worker Threads (config settings)", detected_cpu_cores, ext_info.max_threads);
				#endif
				logger->info("extDB3: Detected {0} Cores, Setting up {1} Worker Threads (config settings)", detected_cpu_cores, ext_info.max_threads);
			}

			// Setup ASIO Worker Pool
			io_work_ptr.reset(new boost::asio::io_service::work(io_service));
			for (int i = 0; i < ext_info.max_threads; ++i)
			{
				logger->info("extDB3: ...");
				threads.create_thread(boost::bind(&boost::asio::io_service::run, &io_service));
			}

			logger->info("");
			logger->info("");

			#ifdef _WIN32
				spdlog::set_pattern("[%H:%M:%S:%f %z] [Thread %t] %v");
			#else
				spdlog::set_pattern("[%H:%M:%S %z] [Thread %t] %v");
			#endif
		}
	}
	catch (boost::property_tree::ini_parser::ini_parser_error const &e)
	{
		std::cout << "BOOST INI PARSER: " << e.what() << std::endl;
		std::exit(EXIT_FAILURE);
	}
	catch(boost::filesystem::filesystem_error const &e)
	{
		std::cout << "BOOST FILESYSTEM ERROR: " << e.what() << std::endl;
		std::exit(EXIT_FAILURE);
	}
	catch (spdlog::spdlog_ex& e)
	{
		std::cout << "SPDLOG ERROR: " <<  e.what() << std::endl;
		std::exit(EXIT_FAILURE);
	}
}


Ext::~Ext(void)
{
	#ifdef DEBUG_TESTING
		console->info("extDB3: Closing ...");
	#endif
	logger->info("extDB3: Closing ...");
	stop();
	mysql_library_end();
	spdlog::drop_all();
}


void Ext::reset()
{
	stop();
	std::lock_guard<std::mutex> lock(mutex_vec_protocols);
	{
		vec_protocols.clear();
	}
	mariadb_databases.clear();

	// Setup ASIO Worker Pool
	io_service.reset();
	io_work_ptr.reset(new boost::asio::io_service::work(io_service));
	for (int i = 0; i < ext_info.max_threads; ++i)
	{
		threads.create_thread(boost::bind(&boost::asio::io_service::run, &io_service));
	}
	mariadb_idle_cleanup_timer.reset(new boost::asio::deadline_timer(io_service));
	mariadb_idle_cleanup_timer->expires_at(mariadb_idle_cleanup_timer->expires_at() + boost::posix_time::seconds(600));
	mariadb_idle_cleanup_timer->async_wait(boost::bind(&Ext::idleCleanup, this, _1));
}


void Ext::stop()
{
	std::lock_guard<std::mutex> lock(mutex_mariadb_idle_cleanup_timer);
	{
		if (mariadb_idle_cleanup_timer)
		{
			mariadb_idle_cleanup_timer->cancel();
			mariadb_idle_cleanup_timer.reset(nullptr);
		}
	}
	io_work_ptr.reset(nullptr);
	threads.join_all();
	io_service.stop();
}


void Ext::idleCleanup(const boost::system::error_code& ec)
{
	if (!ec)
	{
		for(auto &dbpool : mariadb_databases)
		{
				dbpool.second.idleCleanup();
		}
		std::lock_guard<std::mutex> lock(mutex_mariadb_idle_cleanup_timer);
		{
			mariadb_idle_cleanup_timer->expires_at(mariadb_idle_cleanup_timer->expires_at() + boost::posix_time::seconds(300));
			mariadb_idle_cleanup_timer->async_wait(boost::bind(&Ext::idleCleanup, this, _1));
		}
	}
}


void Ext::search(boost::filesystem::path &config_path, bool &conf_found, bool &conf_randomized)
{
	std::regex expression("extdb3-conf.*ini");
	for (boost::filesystem::directory_iterator it(config_path); it != boost::filesystem::directory_iterator(); ++it)
	{
		if (boost::filesystem::is_regular_file(it->path()))
		{
			if(std::regex_search(it->path().string(), expression))
			{
				conf_found = true;
				conf_randomized = true;
				config_path = boost::filesystem::path(it->path().string());
				ext_info.path = config_path.parent_path().string();
				break;
			}
		}
	}
}


void Ext::connectDatabase(char *output, const std::string &database_conf, const std::string &database_id)
// Connection to Database, database_id used when connecting to multiple different database.
{
	if (mariadb_databases.count(database_id) > 0)
	{
		#ifdef DEBUG_TESTING
			console->warn("extDB3: Already Connected to Database");
		#endif
		logger->warn("extDB3: Already Connected to a Database");
		std::strcpy(output, "[0,\"Already Connected to Database\"]");
	} else {
		try
		{
			std::string ip = ptree.get<std::string>(database_conf + ".IP");
			unsigned int port = ptree.get<unsigned int>(database_conf + ".Port");
			std::string username = ptree.get<std::string>(database_conf + ".Username");
			std::string password = ptree.get<std::string>(database_conf + ".Password");
			std::string database = ptree.get<std::string>(database_conf + ".Database");

			MariaDBPool *database_pool = &mariadb_databases[database_id];
			database_pool->init(ip, port, username, password, database);

			if (!mariadb_idle_cleanup_timer)
			{
				mariadb_idle_cleanup_timer.reset(new boost::asio::deadline_timer(io_service));
				mariadb_idle_cleanup_timer->expires_at(mariadb_idle_cleanup_timer->expires_at() + boost::posix_time::seconds(600));
				mariadb_idle_cleanup_timer->async_wait(boost::bind(&Ext::idleCleanup, this, _1));
			}
			std::strcpy(output, "[1]");
		}
		catch (boost::property_tree::ptree_bad_path &e)
		{
			std::strcpy(output, "[0,\"Database Config Error\"]");
			mariadb_databases.erase(database_id);
			#ifdef DEBUG_TESTING
				console->info("extDB3: Config Error: {0}: {1}", database_conf, e.what());
			#endif
			logger->info("extDB3: Config Error: {0}: {1}", database_conf, e.what());
		}
		catch (MariaDBConnectorException &e)
		{
			std::strcpy(output, "[0,\"Database Connection Error\"]");
			mariadb_databases.erase(database_id);
			#ifdef DEBUG_TESTING
				console->info("extDB3: MariaDBConnectorException: {0}: {1}", database_conf, e.what());
			#endif
			logger->info("extDB3: MariaDBConnectorException: {0}: {1}", database_conf, e.what());
		}
	}
}


void Ext::addProtocol(char *output, const std::string &database_id, const std::string &protocol, const std::string &protocol_name, const std::string &init_data)
{
	std::lock_guard<std::mutex> lock(mutex_vec_protocols);
	auto foo = (std::find_if(vec_protocols.begin(), vec_protocols.end(), [=](const protocol_struct& elem) { return protocol_name == elem.name; }));
	if (foo  != vec_protocols.end())
	{
		std::strcpy(output, "[0,\"Error Protocol Name Already Taken\"]");
		logger->warn("extDB3: Error Protocol Name Already Taken: {0}", protocol_name);
	}
	else
	{
		bool status = true;
		protocol_struct protocol_data;
		protocol_data.name = protocol_name;
		if (database_id.empty())
		{
			if (boost::algorithm::iequals(protocol, std::string("LOG")) == 1)
			{
				protocol_data.protocol.reset(new LOG());
			}	else {
				status = false;
				std::strcpy(output, "[0,\"Error Unknown Protocol\"]");
				logger->warn("extDB3: Failed to Load Unknown Protocol: {0}", protocol);
			}
		}
		else
		{
			if (boost::algorithm::iequals(protocol, std::string("SQL")) == 1)
			{
				protocol_data.protocol.reset(new SQL());
			}
			else if (boost::algorithm::iequals(protocol, std::string("SQL_CUSTOM")) == 1)
			{
				protocol_data.protocol.reset(new SQL_CUSTOM());
			}	else {
				status = false;
				std::strcpy(output, "[0,\"Error Unknown Protocol\"]");
				logger->warn("extDB3: Failed to Load Unknown Protocol: {0}", protocol);
			}
		}

		if (status)
		{
			if (protocol_data.protocol->init(this, database_id, init_data))
			{
				vec_protocols.push_back(std::move(protocol_data));
				std::strcpy(output, "[1]");
			}	else {
				std::strcpy(output, "[0,\"Failed to Load Protocol\"]");
				logger->warn("extDB3: Failed to Load Protocol: {0}", protocol);
			}
		}
	}
}


void Ext::getSinglePartResult_mutexlock(char *output, const int &output_size, const unsigned long &unique_id)
// Gets Result String from unordered map array -- Result Formt == Single-Message
//   If <=, then sends output to arma, and removes entry from unordered map array
//   If >, sends [5] to indicate MultiPartResult
{
	std::lock_guard<std::mutex> lock(mutex_results);

	auto const_itr = stored_results.find(unique_id);
	if (const_itr == stored_results.end()) // NO UNIQUE ID
	{
		std::strcpy(output, "");
	}
	else // SEND MSG (Part)
	{
		if (const_itr->second.wait) // WAIT
		{
			std::strcpy(output, "[3]");
		}
		else if (const_itr->second.message.length() > output_size)
		{
			std::strcpy(output, "[5]");
		}
		else
		{
			std::strcpy(output, const_itr->second.message.c_str());
			stored_results.erase(const_itr);
		}
	}
}


void Ext::getMultiPartResult_mutexlock(char *output, const int &output_size, const unsigned long &unique_id)
// Gets Result String from unordered map array  -- Result Format = Multi-Message
//   If length of String = 0, sends arma "", and removes entry from unordered map array
//   If <=, then sends output to arma
//   If >, then sends 1 part to arma + stores rest.
{
	std::lock_guard<std::mutex> lock(mutex_results);

	auto const_itr = stored_results.find(unique_id);
	if (const_itr == stored_results.end()) // NO UNIQUE ID or WAIT
	{
		std::strcpy(output, "");
	}
	else if (const_itr->second.wait)
	{
		std::strcpy(output, "[3]");
	}
	else if (const_itr->second.message.empty()) // END of MSG
	{
		stored_results.erase(const_itr);
		std::strcpy(output, "");
	}
	else // SEND MSG (Part)
	{
		if (const_itr->second.message.length() > output_size)
		{
			std::strcpy(output, const_itr->second.message.substr(0, output_size).c_str());
			const_itr->second.message = const_itr->second.message.substr(output_size);
		}
		else
		{
			std::strcpy(output, const_itr->second.message.c_str());
			const_itr->second.message.clear();
		}
	}
}


const unsigned long Ext::saveResult_mutexlock(const resultData &result_data)
// Stores Result String and returns Unique ID, used by SYNC Calls where message > outputsize
{
	std::lock_guard<std::mutex> lock(mutex_results);
	const unsigned long unique_id = unique_id_counter++;
	stored_results[unique_id] = std::move(result_data);
	stored_results[unique_id].wait = false;
	return unique_id;
}


void Ext::saveResult_mutexlock(const unsigned long &unique_id, const resultData &result_data)
// Stores Result String for Unique ID
{
	std::lock_guard<std::mutex> lock(mutex_results);
	stored_results[unique_id] = std::move(result_data);
	stored_results[unique_id].wait = false;
}


void Ext::saveResult_mutexlock(std::vector<unsigned long> &unique_ids, const resultData &result_data)
// Stores Result for multiple Unique IDs (used by Rcon Backend)
{
	std::lock_guard<std::mutex> lock(mutex_results);
	for (auto &unique_id : unique_ids)
	{
		stored_results[unique_id] = result_data;
		stored_results[unique_id].wait = false;
	}
}


void Ext::syncCallProtocol(char *output, const int &output_size, std::string &input_str)
// Sync callPlugin
{
	const std::string::size_type found = input_str.find(":",2);

	if ((found==std::string::npos) || (found == (call_extension_input_str_length - 1)))
	{
		std::strcpy(output, "[0,\"Error Invalid Format\"]");
		logger->error("extDB3: Invalid Format: {0}", input_str);
	}
	else
	{
		std::string protocol_name = input_str.substr(2, (found - 2));
		auto const_itr = (std::find_if(vec_protocols.begin(), vec_protocols.end(), [=](const protocol_struct& elem) { return protocol_name == elem.name; }));
		if (const_itr == vec_protocols.end())
		{
			std::strcpy(output, "[0,\"Error Unknown Protocol\"]");
		}
		else
		{
			resultData result_data;
			result_data.message.reserve(output_size);

			const_itr->protocol->callProtocol(input_str.substr(found+1), result_data.message, false);
			if (result_data.message.length() <= output_size)
			{
				std::strcpy(output, result_data.message.c_str());
			}
			else
			{
				const unsigned long unique_id = saveResult_mutexlock(result_data);
				std::strcpy(output, ("[2,\"" + std::to_string(unique_id) + "\"]").c_str());
			}
		}
	}
}


void Ext::onewayCallProtocol(std::string &input_str)
// ASync callProtocol
{
	const std::string::size_type found = input_str.find(":",2);
	if ((found==std::string::npos) || (found == (input_str.size() - 1)))
	{
		logger->error("extDB3: Invalid Format: {0}", input_str);
	}
	else
	{
		std::string protocol_name = input_str.substr(2, (found - 2));
		auto const_itr = (std::find_if(vec_protocols.begin(), vec_protocols.end(), [=](const protocol_struct& elem) { return protocol_name == elem.name; }));
		if (const_itr != vec_protocols.end())
		{
			resultData result_data;
			const_itr->protocol->callProtocol(input_str.substr(found+1), result_data.message, true);
		}
	}
}


void Ext::asyncCallProtocol(const int &output_size, const std::string &protocol_name, const std::string &data, const unsigned long unique_id)
// ASync + Save callProtocol
// We check if Protocol exists here, since its a thread (less time spent blocking arma) and it shouldnt happen anyways
{
	resultData result_data;
	result_data.message.reserve(output_size);
	auto const_itr = (std::find_if(vec_protocols.begin(), vec_protocols.end(), [=](const protocol_struct& elem) { return protocol_name == elem.name; }));
	if (const_itr->protocol->callProtocol(data, result_data.message, true, unique_id))
	{
		saveResult_mutexlock(unique_id, result_data);
	}
}


void Ext::getUPTime(std::string &token, std::string &result)
{
	uptime_current = std::chrono::steady_clock::now();
	auto uptime_diff = uptime_current - uptime_start;
	if (token == "MINUTES")
	{
		result = std::to_string(std::chrono::duration_cast<std::chrono::minutes>(uptime_diff).count());
	}
	else if (token == "HOURS")
	{
		result = std::to_string(std::chrono::duration_cast<std::chrono::hours>(uptime_diff).count());
	}
	else if (token == "SECONDS")
	{
		result = std::to_string(std::chrono::duration_cast<std::chrono::seconds>(uptime_diff).count());
	} else {
		result = "[0,\"Error Invalid Format\"]";
		#ifdef DEBUG_TESTING
			console->info("extDB3: getUPTime: invalid input: {0}", token);
		#endif
		logger->info("extDB3: getUPTime: invalid input: {0}", token);
	}
}


void Ext::getUPTime2(std::string &token, std::string &result)
{
	uptime_current = std::chrono::steady_clock::now();
	auto uptime_diff = uptime_current - uptime_start;
	if (token == "MINUTES")
	{
		result = "[1," + std::to_string(std::chrono::duration_cast<std::chrono::minutes>(uptime_diff).count()) + "]";
	}
	else if (token == "HOURS")
	{
		result = "[1," + std::to_string(std::chrono::duration_cast<std::chrono::hours>(uptime_diff).count()) + "]";
	}
	else if (token == "SECONDS")
	{
		result = result = "[1," + std::to_string(std::chrono::duration_cast<std::chrono::seconds>(uptime_diff).count()) + "]";
	} else {
		result = "[0,\"Error Invalid Format\"]";
		#ifdef DEBUG_TESTING
			console->info("extDB3: getUPTime: invalid input: {0}", token);
		#endif
		logger->info("extDB3: getUPTime: invalid input: {0}", token);
	}
}


void Ext::getDateAdd(std::string &token, std::string &token2, std::string &result)
{
	try
	{
		if (token.length() > 2)
		{
			token.erase(0, 1);
			token.pop_back();
			std::vector<std::string> vec;
			boost::split(vec, token, boost::is_any_of(","));

			if (vec.size() == 6)
			{
				int year = std::stoi(vec[0], nullptr);
				year = year - 1900;
				int month = std::stoi(vec[1], nullptr);
				month = month - 1;
				int day = std::stoi(vec[2], nullptr);
				int hour = std::stoi(vec[3], nullptr);
				int minute = std::stoi(vec[4], nullptr);
				int second = std::stoi(vec[5], nullptr);
				
				struct tm ptime_tm = { second,minute,hour,day,month,year };
				ptime = boost::posix_time::ptime_from_tm(ptime_tm);

				if (token2.length() > 2)
				{
					token2.erase(0, 1);
					token2.pop_back();
					std::vector<std::string> vec;
					boost::split(vec, token2, boost::is_any_of(","));
					if (vec.size() == 4)
					{
						int hours = std::stoi(vec[0], nullptr) * 24;
						hours = hours + std::stoi(vec[1], nullptr);
						int minutes = std::stoi(vec[2], nullptr);
						int seconds = std::stoi(vec[3], nullptr);

						boost::posix_time::time_duration diff(hours, minutes, seconds, 0);
						ptime = ptime + diff;

						facet = new boost::posix_time::time_facet();
						facet->format("[1,[%Y,%m,%d,%H,%M,%S]]");
						std::stringstream ss2;
						ss2.imbue(std::locale(std::locale::classic(), facet));
						ss2 << ptime;
						result = ss2.str();
					}
					else {
						result = "[0,\"Error Invalid Format\"]";
						#ifdef DEBUG_TESTING
							console->info("extDB3: addDate: invalid input: {0}", token2);
						#endif
						logger->info("extDB3: addDate: invalid input: {0}", token2);
					}
				}
				else {
					result = "[0,\"Error Invalid Format\"]";
					#ifdef DEBUG_TESTING
						console->info("extDB3: addDate: invalid input2: {0}", token2);
					#endif
					logger->info("extDB3: addDate: invalid input2: {0}", token2);
				}
			}
			else {
				result = "[0,\"Error Invalid Format\"]";
				#ifdef DEBUG_TESTING
					console->info("extDB3: addDate: invalid input3: {0}", token);
				#endif
				logger->info("extDB3: addDate: invalid input3: {0}", token);
			}

		}
	}
	catch(std::exception const &e)
	{
		result = "[0,\"Error Invalid Format\"]";
		#ifdef DEBUG_TESTING
			console->info("extDB3: addDate: stdException: {0}", e.what());
		#endif
		logger->info("extDB3: addDate: stdException: {0}", e.what());
	}
}


void Ext::getLocalTime(std::string &result)
{
	ptime = boost::posix_time::second_clock::local_time();
	std::stringstream stream;
	facet = new boost::posix_time::time_facet();
	facet->format("[1,[%Y,%m,%d,%H,%M,%S]]");
	stream.imbue(std::locale(std::locale::classic(), facet));
	stream << ptime;
	result = stream.str();
}


void Ext::getLocalTime(std::string &input_str, std::string &result)
{
	try
	{
		ptime = boost::posix_time::second_clock::local_time();

		if (!(input_str.empty()))
		{
			int offset;
			if (input_str[0] == '[')
			{
				input_str.erase(0,1);
				input_str.pop_back();

				std::vector<std::string> tokens;
				boost::split(tokens, input_str, boost::is_any_of(","));
				for (unsigned int i = 0; i < tokens.size(); i++)
				{
					offset = std::stoi(tokens[i]);
					switch (i)
					{
						case 0:
							break;
						case 1:
							break;
						case 2:
							ptime += boost::posix_time::hours(offset * 24);
						case 3:
							ptime += boost::posix_time::hours(offset);
						case 4:
							ptime += boost::posix_time::minutes(offset);
						case 5:
							ptime += boost::posix_time::seconds(offset);
						default:
							break;
					}
				}
			} else {
				offset = std::stoi(input_str);
				ptime += boost::posix_time::hours(offset);
			}
		}

		std::stringstream stream;
		facet = new boost::posix_time::time_facet();
		facet->format("[1,[%Y,%m,%d,%H,%M,%S]]");
		stream.imbue(std::locale(std::locale::classic(), facet));
		stream << ptime;
		result = stream.str();
	}
	catch(std::exception& e)
	{
		result = "[0,\"ERROR\"]";
	}
}


void Ext::getUTCTime(std::string &result)
{
	try
	{
		ptime = boost::posix_time::second_clock::universal_time();

		std::stringstream stream;
		facet = new boost::posix_time::time_facet();
		facet->format("[1,[%Y,%m,%d,%H,%M,%S]]");
		stream.imbue(std::locale(std::locale::classic(), facet));
		stream << ptime;
		result = stream.str();
	}
	catch(std::exception& e)
	{
		result = "[0,\"ERROR\"]";
	}
}


void Ext::getUTCTime(std::string &input_str, std::string &result)
{
	try
	{
		ptime = boost::posix_time::second_clock::universal_time();

		if (!(input_str.empty()))
		{
			int offset;
			if (input_str[0] == '[')
			{
				input_str.erase(0,1);
				input_str.pop_back();

				std::vector<std::string> tokens;
				boost::split(tokens, input_str, boost::is_any_of(","));
				for (unsigned int i = 0; i < tokens.size(); i++)
				{
					offset = std::stoi(tokens[i]);
					switch (i)
					{
						case 0:
							break;
						case 1:
							break;
						case 2:
							ptime += boost::posix_time::hours(offset*24);
						case 3:
							ptime += boost::posix_time::hours(offset);
						case 4:
							ptime += boost::posix_time::minutes(offset);
						case 5:
							ptime += boost::posix_time::seconds(offset);
						default:
							break;
					}
				}
			} else {
				offset = std::stoi(input_str);
				ptime += boost::posix_time::hours(offset);
			}
		}

		std::stringstream stream;
		facet = new boost::posix_time::time_facet();
		facet->format("[1,[%Y,%m,%d,%H,%M,%S]]");
		stream.imbue(std::locale(std::locale::classic(), facet));
		stream << ptime;
		result = stream.str();
	}
	catch(std::exception& e)
	{
		result = "[0,\"ERROR\"]";
	}
}


void Ext::callExtension(char *output, const int &output_size, const char *function)
{
	try
	{
		#ifdef DEBUG_LOGGING
			logger->info("extDB3: Input from Server: {0}", std::string(function));
		#endif

		std::string input_str(function);
		call_extension_input_str_length = input_str.length();

		if (call_extension_input_str_length <= 2)
		{
			std::strcpy(output, "[0,\"Error Invalid Message\"]");
			logger->info("extDB3: Invalid Message: {0}", input_str);
		}
		else
		{
			// Async / Sync
			switch (input_str[0])
			{
				case '1': //ASYNC
				{
					io_service.post(boost::bind(&Ext::onewayCallProtocol, this, std::move(input_str)));
					break;
				}
				case '2': //ASYNC + SAVE
				{
					// Protocol
					const std::string::size_type found = input_str.find(":", 2);
					if ((found==std::string::npos) || (found == (call_extension_input_str_length - 1)))
					{
						std::strcpy(output, "[0,\"Error Invalid Format\"]");
						logger->error("extDB3: Error Invalid Format: {0}", input_str);
					}	else {
						// Check for Protocol Name Exists...
						// Do this so if someone manages to get server, the error message wont get stored in the result unordered map
						const std::string protocol_name = input_str.substr(2,(found-2));
						if ((std::find_if(vec_protocols.begin(), vec_protocols.end(), [=](const protocol_struct& elem) { return protocol_name == elem.name; })) != vec_protocols.end()) //TODO Change to ITER
						{
							unsigned long unique_id;
							{
								std::lock_guard<std::mutex> lock(mutex_results);
								unique_id = unique_id_counter++;
								stored_results[unique_id].wait = true;
							}
							io_service.post(boost::bind(&Ext::asyncCallProtocol, this, output_size, std::move(protocol_name), input_str.substr(found+1), std::move(unique_id)));
							std::strcpy(output, ("[2,\"" + std::to_string(unique_id) + "\"]").c_str());
						}	else {
							std::strcpy(output, "[0,\"Error Unknown Protocol\"]");
							logger->error("extDB3: Error Unknown Protocol: {0}  Input String: {1}", protocol_name, input_str);
						}
					}
					break;
				}
				case '4': // GET -- Single-Part Message Format
				{
					//const unsigned long unique_id = std::stoul(input_str.substr(2));
					const unsigned long unique_id = strtoul (input_str.substr(2).c_str(), NULL, 0);
					getSinglePartResult_mutexlock(output, output_size, unique_id);
					break;
				}
				case '5': // GET -- Multi-Part Message Format
				{
					//const unsigned long unique_id = std::stoul(input_str.substr(2));
					const unsigned long unique_id = strtoul (input_str.substr(2).c_str(), NULL, 0);
					getMultiPartResult_mutexlock(output, output_size, unique_id);
					break;
				}
				case '0': //SYNC
				{
					syncCallProtocol(output, output_size, input_str);
					break;
				}
				case '9': // SYSTEM CALLS / SETUP
				{
					std::vector<std::string> tokens;
					boost::split(tokens, input_str, boost::is_any_of(":"));

					if (ext_info.extDB_lock)
					{
						switch (tokens.size())
						{
							case 4:
								if (tokens[1] == "DATEADD")
								{
									std::string result;
									getDateAdd(tokens[2],tokens[3],result);
									std::strcpy(output, result.c_str());
								}	else {
									std::strcpy(output, "[0,\"Error Invalid Format\"]");
									logger->error("extDB3: Error Invalid Format: {0}", input_str);
								}
								break;
							case 3:
								if (tokens[1] == "UPTIME2")
								{
									std::string result;
									getUPTime2(tokens[2], result);
									std::strcpy(output, result.c_str());
								}
								else if (tokens[1] == "UPTIME")
								{
									std::string result;
									getUPTime(tokens[2], result);
									std::strcpy(output, result.c_str());
								}
								else if (tokens[1] == "LOCAL_TIME")
								{
									std::string result;
									getLocalTime(tokens[2], result);
									std::strcpy(output, result.c_str());
								}
								else if (tokens[1] == "UTC_TIME")
								{
									std::string result;
									getUTCTime(tokens[2], result);
									std::strcpy(output, result.c_str());
								}
								else if (tokens[1] == "UNLOCK")
								{
									std::strcpy(output, ("[0]"));
									if (!(ext_info.extDB_lockCode.empty()))
									{
										if (tokens[2] == ext_info.extDB_lockCode)
										{
											std::strcpy(output, ("[1]"));
											logger->info("extDB3: UnLocked");
											ext_info.extDB_lockCode.clear();
											ext_info.extDB_lock = false;
										}
									}
								}	else {
									std::strcpy(output, "[0,\"Error Invalid Format\"]");
									logger->error("extDB3: Error Invalid Format: {0}", input_str);
								}
								break;
							case 2:
								if (tokens[1] == "LOCAL_TIME")
								{
									std::string result;
									getLocalTime(result);
									std::strcpy(output, result.c_str());
								}
								else if (tokens[1] == "UTC_TIME")
								{
									std::string result;
									getUTCTime(result);
									std::strcpy(output, result.c_str());
								}
								else if (tokens[1] == "UNLOCK")
								{
									std::strcpy(output, ("[0]"));
								}
								else if (tokens[1] == "LOCK_STATUS")
								{
									std::strcpy(output, "[1]");
								}
								else if (tokens[1] == "VERSION")
								{
									std::strcpy(output, EXTDB_VERSION);
								}
								else
								{
									std::strcpy(output, "[0,\"Error Invalid Format\"]");
									logger->error("extDB3: Error Invalid Format: {0}", input_str);
								}
								break;
							default:
								// Invalid Format
								std::strcpy(output, "[0,\"Error Invalid Format\"]");
								logger->error("extDB3: Error Invalid Format: {0}", input_str);
						}
					}
					else
					{
						switch (tokens.size())
						{
							case 2:
								if (tokens[1] == "LOCAL_TIME")
								{
									std::string result;
									getLocalTime(result);
									std::strcpy(output, result.c_str());
								}
								else if (tokens[1] == "UTC_TIME")
								{
									std::string result;
									getUTCTime(result);
									std::strcpy(output, result.c_str());
								}
								else if (tokens[1] == "LOCK")
								{
									ext_info.extDB_lock = true;
									std::strcpy(output, ("[1]"));
									logger->info("extDB3: Locked");
								}
								else if (tokens[1] == "LOCK_STATUS")
								{
									std::strcpy(output, "[0]");
								}
								else if (tokens[1] == "UNLOCK")
								{
									std::strcpy(output, "[1]");
								}
								else if (tokens[1] == "RESET")
								{
									if (ext_info.allow_reset)
									{
										reset();
										std::strcpy(output, "[1]");
									} else {
										std::strcpy(output, "[0]");
									}
								}
								else if (tokens[1] == "VERSION")
								{
									std::strcpy(output, EXTDB_VERSION);
								}
								else if (tokens[1] == "OUTPUTSIZE")
								{
									std::string outputsize_str(std::to_string(output_size));
									std::strcpy(output, outputsize_str.c_str());
									logger->info("extDB3: Output Size: {0}", outputsize_str);
								}
								else
								{
									std::strcpy(output, "[0,\"Error Invalid Format\"]");
									logger->error("extDB3: Error Invalid Format: {0}", input_str);
								}
								break;
							case 3:
								if (tokens[1] == "UPTIME")
								{
									std::string result;
									getUPTime(tokens[2], result);
									std::strcpy(output, result.c_str());
								}
								else if (tokens[1] == "LOCAL_TIME")
								{
									std::string result;
									getLocalTime(tokens[2], result);
									std::strcpy(output, result.c_str());
								}
								else if (tokens[1] == "UTC_TIME")
								{
									std::string result;
									getUTCTime(tokens[2], result);
									std::strcpy(output, result.c_str());
								}
								// DATABASE
								else if (tokens[1] == "ADD_DATABASE")
								{
									connectDatabase(output, tokens[2], tokens[2]);
								}
								else if (tokens[1] == "LOCK")
								{
									ext_info.extDB_lock = true;
									ext_info.extDB_lockCode = tokens[2];
									std::strcpy(output, ("[1]"));
									logger->info("extDB3: Locked");
								}
								else
								{
									// Invalid Format
									std::strcpy(output, "[0,\"Error Invalid Format\"]");
									logger->error("extDB3: Error Invalid Format: {0}", input_str);
								}
								break;
							case 4:
								if (tokens[1] == "ADD_DATABASE")
								{
									connectDatabase(output, tokens[2], tokens[3]);
								}
								else if (tokens[1] == "ADD_PROTOCOL")
								{
									addProtocol(output, "", tokens[2], tokens[3], "");
								}
								else if (tokens[1] == "DATEADD")
								{
									std::string result;
									getDateAdd(tokens[2],tokens[3],result);
									std::strcpy(output, result.c_str());
								}
								else
								{
									// Invalid Format
									std::strcpy(output, "[0,\"Error Invalid Format\"]");
									logger->error("extDB3: Error Invalid Format: {0}", input_str);
								}
								break;
							case 5:
								if (tokens[1] == "ADD_PROTOCOL")
								{
									addProtocol(output, "", tokens[2], tokens[3], tokens[4]); // ADD + Init Options
								}
								else if (tokens[1] == "ADD_DATABASE_PROTOCOL")
								{
									addProtocol(output, tokens[2], tokens[3], tokens[4], ""); // ADD Database Protocol + No Options
								}
								else
								{
									// Invalid Format
									std::strcpy(output, "[0,\"Error Invalid Format\"]");
									logger->error("extDB3: Error Invalid Format: {0}", input_str);
								}
								break;
							case 6:
								if (tokens[1] == "ADD_DATABASE_PROTOCOL")
								{
									addProtocol(output, tokens[2], tokens[3], tokens[4], tokens[5]); // ADD Database Protocol + Options
								}
								else
								{
									// Invalid Format
									std::strcpy(output, "[0,\"Error Invalid Format\"]");
									logger->error("extDB3: Error Invalid Format: {0}", input_str);
								}
								break;
							default:
								{
									// Invalid Format
									std::strcpy(output, "[0,\"Error Invalid Format\"]");
									logger->error("extDB3: Error Invalid Format: {0}", input_str);
								}
						}
					}
					break;
				}
				default:
				{
					std::strcpy(output, "[0,\"Error Invalid Message\"]");
					logger->error("extDB3: Error Invalid Message: {0}", input_str);
				}
			}
		}
		#ifdef DEBUG_LOGGING
			logger->info("extDB3: Output to Server: {0}", output);
		#endif
	}
	catch (spdlog::spdlog_ex& e)
	{
		std::strcpy(output, "[0,\"Error LOGGER\"]");
		std::cout << "SPDLOG ERROR: " <<  e.what() << std::endl;
	}
}
