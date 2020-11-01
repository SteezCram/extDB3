/*
 * extDB3
 * © 2016 Declan Ireland <https://bitbucket.org/torndeco/extdb3>
 */

#include <string>

#include <boost/algorithm/string.hpp>

#include "ext.h"

#ifdef TEST_APP
	int main(int nNumberofArgs, char* pszArgs[])
	{
		int result_size = 80;
		char result[81] = {0};
		std::string input_str;

		Ext *extension;
		extension = new Ext(std::string(""));

		bool test = false;
		int test_counter = 0;
		for (;;)
		{
			result[0] = '\0';
			std::getline(std::cin, input_str);
			if (boost::algorithm::iequals(input_str, "Quit") == 1)
			{
				break;
			}
			else if (boost::algorithm::iequals(input_str, "Test") == 1)
			{
				test = true;
			}
			else
			{
				extension->callExtension(result, result_size, input_str.c_str());
				extension->console->info("extDB3: {0}", result);
			}
			while (test)
			{
				if (test_counter >= 10000)
				{
					test_counter = 0;
					test = false;
					break;
				}
				++test_counter;
				extension->callExtension(result, result_size, std::string("1:SQL:TEST1:testing").c_str());
				extension->callExtension(result, result_size, std::string("1:SQL:TEST2:testing").c_str());
				extension->callExtension(result, result_size, std::string("1:SQL:TEST3:testing").c_str());
				extension->callExtension(result, result_size, std::string("1:SQL:TEST4:testing").c_str());
				extension->callExtension(result, result_size, std::string("1:SQL:TEST5:testing").c_str());
				extension->console->info("extDB3: {0}", result);
			}
		}
		extension->stop();
		return 0;
	}
#endif
