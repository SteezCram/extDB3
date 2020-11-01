/*
 * extDB3
 * © 2016 Declan Ireland <https://bitbucket.org/torndeco/extdb3>
 */

#pragma once

#include "../abstract_ext.h"

class AbstractProtocol
{
public:
	AbstractProtocol(){};
	~AbstractProtocol(){};

	virtual bool init(AbstractExt *extension, const std::string &database_id, const std::string &init_str)=0;
	virtual bool callProtocol(std::string input_str, std::string &result, const bool async_method, const unsigned int unique_id=1)=0;

	AbstractExt *extension_ptr;
};
