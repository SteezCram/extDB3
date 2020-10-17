/*
	File: fn_strip.sqf
	Author: Declan Ireland

	Description:
		Strips : from String
			Needed since extDB3 uses : as seperator character
			Basically you need to strip user input, incase user add : i.e playernames
			Except if you are using the Input SQF Parser Option in SQL_CUSTOM read
			   https://bitbucket.org/torndeco/extdb3/wiki/extDB3%20-%20sql_custom.ini
			
*/

private _string = (_this select 0);
private _array = toArray _string;
{
	if (_x == 58) then
	{
		_array set[_forEachIndex, -1];
	};
} foreach _array;
_array = _array - [-1];
_string = toString _array;
_string
