#pragma once

#include <string>
#include <vector>
#include <sstream>
#include <iomanip>
#include <ctype.h>
#include <math.h>


namespace strutils
{
	inline bool is_numeric(std::string& s)
	{
		for(auto i: s)
        {
            if(std::isalpha(i))
            {
                return false;
            }
        }

		return true;
	}

	inline std::string double_to_str(double num, uint8_t precision)
	{
		std::stringstream s;
		s << std::fixed << std::setprecision(precision) << num;
		return s.str();
	}

	/*
		Converts a double frequency to Boeing-style string representation
	*/

	inline std::string freq_to_str(double freq)
	{
		uint64_t freq_str_length = 0;
		for (double i = 1; i <= freq; i *= 10)
		{
			freq_str_length++;
		}
		if (freq > 9999)
		{
			freq /= 100;
			freq_str_length++;
		}
		else
		{
			freq_str_length += 2;
		}
		return std::to_string(freq).substr(0, freq_str_length);
	}

	/*
		Converts value in degrees to string Deg,Min,Sec notation
	*/

	inline std::string deg_to_str(double abs_deg)
	{
		std::string s;
		std::vector<int> repr;

		if (abs_deg < 10)
		{
			s.append("0");
		}

		for (int i = 0; i < 3; i++)
		{
			int v = int(abs_deg);
			repr.push_back(v);
			abs_deg -= v;
			abs_deg *= 60;
		}

		s.append(std::to_string(repr[0]));
		s.append("°");
		s.append(std::to_string(repr[1]));
		s.append(".");
		s.append(std::to_string(round(double(repr[2]) / 10)).substr(0, 1));

		return s;
	}

	/*
		Converts latitude value in degrees to string Deg,Min,Sec notation
	*/

	inline std::string lat_to_str(double lat_deg)
	{
		std::string s;
		double abs_lat = abs(lat_deg);

		if (lat_deg < 0)
		{
			s.append("S");
		}
		else
		{
			s.append("N");
		}

		s.append(deg_to_str(abs_lat));

		return s;
	}

	/*
		Converts longitude value in degrees to string Deg,Min,Sec notation
	*/

	inline std::string lon_to_str(double lon_deg)
	{
		std::string s;
		double abs_lon = abs(lon_deg);
		if (lon_deg < 0)
		{
			s.append("W");
		}
		else
		{
			s.append("E");
		}

		if (abs_lon < 100)
		{
			s.append("0");
		}

		s.append(deg_to_str(abs_lon));

		return s;
	}

	/*
		Converts magnetic variation value in degrees to Boeing-style notation
	*/

	inline std::string mag_var_to_str(double mag_var_deg)
	{
		int mag_var_rnd = int(round(mag_var_deg));
		int mag_var = ((mag_var_rnd) + 360) % 360;
		std::string str_mag_var = std::to_string(abs(mag_var_rnd));
		
		if (mag_var < 180)
		{
			return "E" + str_mag_var;
		}
		return "W" + str_mag_var;
	}

	/*
		Function: str_split
		Description: splits the string by a designated character
		@param in: input string
		@param sep: separator
		@Return: vector of strings
	*/

	inline std::vector<std::string> str_split(std::string in, char sep)
	{
		std::stringstream s(in);
		std::string tmp;
		std::vector<std::string> out;

		while(std::getline(s, tmp, sep))
		{
			out.push_back(tmp);
		}

		return out;
	}

	/*
		Function: strip
		Description: removes a designated character from the start and end of the string
		@param in: input string
		@param sep: separator
		@Return: vector of strings
	*/

	inline std::string strip(std::string& in, char tgt)
    {
        size_t i_first = 0;
        size_t i_last = in.length()-1;

        while(in[i_first] == tgt)
        {
            i_first++;
        }
        while(in[i_last] == tgt)
        {
            i_last--;
        }

        return in.substr(i_first, i_last - i_first + 1);
    }
}