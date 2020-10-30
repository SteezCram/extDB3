/*
 * extDB3
 * © 2016 Declan Ireland <https://bitbucket.org/torndeco/extdb3>
 */

#include "log.h"

#include <boost/filesystem.hpp>


bool LOG::init(AbstractExt *extension, const std::string &database_id, const std::string &init_str)
{
	bool status = false;
	extension_ptr = extension;

	if (!(init_str.empty()))
	{
		try
		{
			boost::filesystem::path customlog(extension_ptr->ext_info.log_path);
			customlog /= init_str;
			if (customlog.parent_path().make_preferred().string() == extension_ptr->ext_info.log_path)
			{
				logger = spdlog::rotating_logger_mt(init_str, customlog.make_preferred().string(), 1048576 * 100, 3);
				if (extension_ptr->ext_info.logger_flush)
					logger->flush_on(spdlog::level::info);
				status = true;
			}
		}
		catch (spdlog::spdlog_ex& e)
		{
			#ifdef DEBUG_TESTING
				extension_ptr->console->warn("extDB3: LOG: Error: {0}", e.what());
			#endif
			extension_ptr->logger->warn("extDB3: LOG: Error: {0}", e.what());
			status = false;
		}
	}
	else
	{
		logger = extension_ptr->logger;
		status = true;
	}
	return status;
}


bool LOG::callProtocol(std::string input_str, std::string &result, const bool async_method, const unsigned int unique_id)
{
	logger->info(input_str.c_str());
	result = "[1]";
	return true;
}
