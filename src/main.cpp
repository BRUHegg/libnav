﻿#include "main_helpers.hpp"
#include <libnav/str_utils.hpp>


int main()
{
	std::string airac_pref = "test_data/xp12/";
	dbg::Avionics avncs(airac_pref+"apt.dat", airac_pref+"777_arpt.dat", 
		airac_pref+"777_rnw.dat", airac_pref+"earth_fix.dat", 
		airac_pref+"earth_nav.dat", airac_pref+"earth_awy.dat", 
		airac_pref+"earth_hold.dat", "test_data/CIFP");
	std::cout << "Avionics loaded\n";
	
	while(1)
	{
		std::string in_raw;
		std::getline(std::cin, in_raw);
		
		std::string in_proc = strutils::strip(in_raw, ' ');

		std::vector<std::string> line_split = strutils::str_split(in_proc, ' ');
		std::string cmd_name = line_split[0];
		std::vector<std::string> args = std::vector<std::string>(line_split.begin() + 1, 
			line_split.end());

		if(dbg::cmd_map.find(cmd_name) != dbg::cmd_map.end())
		{
			dbg::cmd_map[cmd_name](&avncs, args);
		}
		else
		{
			std::cout << "Invalid command name\n";
		}


		avncs.update();
	}
	
	return 0;
}
