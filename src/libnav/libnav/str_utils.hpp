/*
        This project is licensed under
        Creative Commons Attribution-NonCommercial-ShareAlike 4.0 International
   Public License (CC BY-NC-SA 4.0).

        A SUMMARY OF THIS LICENSE CAN BE FOUND HERE:
   https://creativecommons.org/licenses/by-nc-sa/4.0/

        Author: discord/bruh4096#4512

        This file contains utulity functions for strings. These allow you to
   convert lat/lon to dms and vice versa and etc.
*/

#pragma once

#include <ctype.h>
#include <math.h>

#include <cstdint>
#include <iomanip>
#include <iostream>
#include <sstream>
#include <string>
#include <vector>

namespace strutils {

constexpr int N_LAT_STR_LENGTH = 9;
constexpr int N_LON_STR_LENGTH = 10;
constexpr char DEGREE_SYMBOL = '\370';

bool is_numeric(const std::string& s) noexcept;

std::string double_to_str(double num, uint8_t precision) noexcept;

double strtod(const std::string& s) noexcept;

/*
        Converts a double frequency to Boeing-style string representation
*/

std::string freq_to_str(double freq) noexcept;

/*
        Converts value in degrees to string Deg,Min,Sec notation
*/

std::string deg_to_str(double abs_deg, char deg_sbl = DEGREE_SYMBOL) noexcept;

/*
        Converts latitude value in degrees to string Deg,Min,Sec notation
*/

std::string lat_to_str(double lat_deg, char deg_sbl = DEGREE_SYMBOL) noexcept;

double str_to_lat(const std::string& s) noexcept;

/*
        Converts longitude value in degrees to string Deg,Min,Sec notation
*/

std::string lon_to_str(double lon_deg, char deg_sbl = DEGREE_SYMBOL) noexcept;

double str_to_lon(const std::string& s) noexcept;

/*
        Converts magnetic variation value in degrees to Boeing-style notation
*/

std::string mag_var_to_str(double mag_var_deg) noexcept;

/*
        Function: strip
        Description: removes a designated character from the start and end of
   the string
        @param in: input string
        @param sep: separator
        @Return: vector of strings
*/

std::string strip(const std::string& in, char sep = ' ') noexcept;

/*
        Function: str_split
        Description: splits the string by a designated character
        @param in: input string
        @param sep: separator
        @param n_split: maximum number of columns to separate
        @Return: vector of strings
*/

std::vector<std::string> str_split(const std::string& in, char sep = ' ',
                                   int n_split = INT32_MAX) noexcept;

int stoi_with_strip(const std::string& s, char s_char = ' ') noexcept;

float stof_with_strip(const std::string& s, char s_char = ' ') noexcept;

/*
        Function: normalize_rnw_id
        Description:
        Adds a leading 0 to runway IDs that need it. Runway IDs in some airports
   in e.g. US don't have leading 0s, however, in Boeing's data bases all runways
   have them. Param: id: target id Return: Returns a modified id.
*/

std::string normalize_rnw_id(const std::string& id) noexcept;

std::string get_rnw_id(const std::string& id, bool ignore_all = false) noexcept;

bool is_valid_airport_icao(const std::string& icao) noexcept;
};  // namespace strutils
