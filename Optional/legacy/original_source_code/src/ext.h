/*
 * extDB3
 * © 2016 Declan Ireland <https://bitbucket.org/torndeco/extdb3>
 */

#pragma once

#include <chrono>
#include <thread>
#include <unordered_map>

#include <boost/asio.hpp>
#include <boost/filesystem.hpp>
#include <boost/property_tree/ptree.hpp>
#include <boost/random/random_device.hpp>
#include <boost/random/uniform_int_distribution.hpp>
#include <boost/thread/thread.hpp>
#include <boost/date_time/posix_time/posix_time.hpp>

#include "abstract_ext.h"

#include "protocols/abstract_protocol.h"


class Ext: public AbstractExt
{
public:
	Ext(std::string shared_libary_path);
	~Ext();
	void reset();
	void stop();
	void idleCleanup(const boost::system::error_code& ec);
	void callExtension(char *output, const int &output_size, const char *function);

	struct protocol_struct
	{
		std::string													name;
		std::unique_ptr<AbstractProtocol>		protocol;
	};


private:
	// Config File
	boost::property_tree::ptree ptree;

	// Input
	std::string::size_type input_str_length;

	// Main ASIO Thread Queue
	std::unique_ptr<boost::asio::io_service::work> io_work_ptr;
	boost::asio::io_service io_service;
	boost::thread_group threads;

	std::mutex mutex_mariadb_idle_cleanup_timer;
	std::unique_ptr<boost::asio::deadline_timer> mariadb_idle_cleanup_timer;

	// Protocols
	std::vector<protocol_struct> vec_protocols;
	std::mutex mutex_vec_protocols;

	// Unique ID
	std::string::size_type call_extension_input_str_length;
	unsigned long unique_id_counter = 100; // Can't be value 1

	// Results
	std::unordered_map<unsigned long, resultData> stored_results;
	std::mutex mutex_results;  // Using Same Lock for Unique ID aswell

	// UPTimer
	std::chrono::time_point<std::chrono::steady_clock> uptime_start;
	std::chrono::time_point<std::chrono::steady_clock> uptime_current;
	
	// Clock
	boost::posix_time::ptime ptime;
	boost::posix_time::time_facet *facet;
	
	boost::posix_time::time_facet *facet_localtime;

	void search(boost::filesystem::path &extDB_config_path, bool &conf_found, bool &conf_randomized);

	void connectDatabase(char *output, const std::string &database_conf, const std::string &database_id);

	// Protocols
	void addProtocol(char *output, const std::string &database_id, const std::string &protocol, const std::string &protocol_name, const std::string &init_data);
	void getSinglePartResult_mutexlock(char *output, const int &output_size, const unsigned long &unique_id);
	void getMultiPartResult_mutexlock(char *output, const int &output_size, const unsigned long &unique_id);
	void syncCallProtocol(char *output, const int &output_size, std::string &input_str);
	void onewayCallProtocol(std::string &input_str);
	void asyncCallProtocol(const int &output_size, const std::string &protocol_name, const std::string &data, const unsigned long unique_id);

	const unsigned long saveResult_mutexlock(const resultData &result_data);
	void saveResult_mutexlock(const unsigned long &unique_id, const resultData &result_data);
	void saveResult_mutexlock(std::vector<unsigned long> &unique_ids, const resultData &result_data);

	void getUPTime(std::string &token, std::string &result);
	void getUPTime2(std::string &token, std::string &result);
	void getLocalTime(std::string &result);
	void getLocalTime(std::string &input_str, std::string &result);
	void getUTCTime(std::string &result);
	void getUTCTime(std::string &input_str, std::string &result);
	
	void getDateAdd(std::string &token, std::string &token2, std::string &result);

};
