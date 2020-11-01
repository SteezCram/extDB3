/*
 * extDB3
 * © 2016 Declan Ireland <https://bitbucket.org/torndeco/extdb3>
 */
 
 
 #include "sqfparser.h"


inline bool sqf_extract_string(std::string &input_str, std::string::size_type &pos, char quotation, std::vector<std::string> &output_vec)
{
	pos++;
	if (pos > input_str.size())
	{
		return false;
	}

	std::string output_str;
	for(;pos < input_str.size(); ++pos)
	{
		if (input_str[pos] == quotation)
		{
			if ((input_str[pos+1]) == quotation)
			{
				output_str += quotation;
				pos++;
			}	else {
				break;
			}
		} else {
			output_str += input_str[pos];
		}
	}
	output_vec.push_back(std::move(output_str));
	return true;
}


inline bool sqf_skip_string(std::string &input_str, std::string::size_type &pos, char quotation)
{
	pos++;
	if (pos > input_str.size())
	{
		return false;
	}

	for(;pos < input_str.size(); ++pos)
	{
		if (input_str[pos] == quotation)
		{
			if ((input_str[pos+1]) == quotation)
			{
				pos++;
			}	else {
				break;
			}
		}
	}
	return true;
}


inline bool sqf_extract_number(std::string &input_str, std::string::size_type &pos, std::vector<std::string> &output_vec)
{
	bool loop = true;

	std::string temp_str;
	for (; pos < input_str.size(); ++pos)
	{
		switch (input_str[pos])
		{
			case 'e':
			case '+':
			case '-':
			case '0':
			case '1':
			case '2':
			case '3':
			case '4':
			case '5':
			case '6':
			case '7':
			case '8':
			case '9':
			case '.':
				temp_str += input_str[pos];
				break;
			default:
				--pos;
				loop = false;
		}
		if (!loop)
		{
			break;
		}
	}

	bool status = false;
	if ((!loop) || ((pos == input_str.size()) - 1))
	{
		output_vec.push_back(temp_str);
		status = true;
	}
	return status;
}


inline bool sqf_skip_number(std::string &input_str, std::string::size_type &pos)
{
	bool loop = true;

	for (; pos < input_str.size(); ++pos)
	{
		switch (input_str[pos])
		{
			case 'e':
			case '+':
			case '-':
			case '0':
			case '1':
			case '2':
			case '3':
			case '4':
			case '5':
			case '6':
			case '7':
			case '8':
			case '9':
			case '.':
				break;
			default:
				--pos;
				loop = false;
		}
		if (!loop)
		{
			break;
		}
	}

	bool status = false;
	if ((!loop) || ((pos == input_str.size()) - 1))
	{
		status = true;
	}
	return status;
}


inline bool sqf_extract_special(std::string &input_str, std::string::size_type &pos, std::vector<std::string> &output_vec)
{
	pos++;
	bool status = false;
	std::string output_str;
	output_str += '<';

	for (; pos < input_str.size(); ++pos)
	{
		if (input_str[pos] == '>')
		{
			status = true;
			break;
		} else {
			output_str += input_str[pos];
		}
	}
	output_str += '>';

	if (status)
	{
		output_vec.push_back(std::move(output_str));
		return true;
	}
	else
	{
		return false;
	}
}


inline bool sqf_skip_special(std::string &input_str, std::string::size_type &pos)
{
	pos++;
	bool status = false;

	for (; pos < input_str.size(); ++pos)
	{
		if (input_str[pos] == '>')
		{
			status = true;
			break;
		}
	}
	return status;
}


inline bool sqf_skip_array(std::string &input_str, std::string::size_type &pos)
{
	bool loop = true;
	bool status = true;

	for (; pos < input_str.size(); ++pos)
	{
		switch (input_str[pos])
		{
			case '<':
				status = sqf_skip_special(input_str, pos);
				break;
			case '"':
				status = sqf_skip_string(input_str, pos, '\"');
				break;
			case '\'':
				status = sqf_skip_string(input_str, pos, '\'');
				break;
			case ',':  // Ignore
			case ' ':  // Ignore
				break;
			case ']':
				loop = false;
				break;
			case '[':
				sqf_skip_array(input_str, pos);
				break;
			case '-':
			case '0':
			case '1':
			case '2':
			case '3':
			case '4':
			case '5':
			case '6':
			case '7':
			case '8':
			case '9':
			case '.':
				status =  sqf_skip_number(input_str, pos);
				break;
			default:
				status = false;
		}
		if ((!status) || (!loop))
		{
			break;
		}
	}
	if ((status) && (!loop))
	{
		return true;
	}
	else {
		return false;
	}
}


inline bool sqf_extract_array(std::string &input_str, std::vector<std::string> &output_vec)
{
	std::string::size_type pos = 1;
	bool loop = true;
	bool status = true;

	for (; pos < input_str.size(); ++pos)
	{
		switch (input_str[pos])
		{
		case '<':
			status = sqf_extract_special(input_str, pos, output_vec);
			break;
		case '"':
			status = sqf_extract_string(input_str, pos, '\"', output_vec);
			break;
		case '\'':
			status = sqf_extract_string(input_str, pos, '\'', output_vec);
			break;
		case ',':  // Ignore
		case ' ':  // Ignore
			break;
		case ']':
			loop = false;
			break;
		case '[':
			sqf_skip_array(input_str, pos);
			break;
		case '-':
		case '0':
		case '1':
		case '2':
		case '3':
		case '4':
		case '5':
		case '6':
		case '7':
		case '8':
		case '9':
		case '.':
			status = sqf_extract_number(input_str, pos, output_vec);
			break;
		default:
			status = false;
		}
		if ((!status) || (!loop))
		{
			break;
		}
	}
	if ((status) && (!loop))
	{
		return true;
	}
	else {
		return false;
	}
}


namespace sqf
{
	bool parser(std::string &input_str, std::vector<std::string> &output_vec)
	{
		bool status = false;
		if (!(input_str.empty()))
		{
			if ((input_str.front() == '[') && (input_str.back() == ']'))
			{
				status = sqf_extract_array(input_str, output_vec);
			}
		}
		return status;
	}
}
