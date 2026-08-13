/*
	This project is licensed under
	Creative Commons Attribution-NonCommercial-ShareAlike 4.0 International Public License (CC BY-NC-SA 4.0).

	A SUMMARY OF THIS LICENSE CAN BE FOUND HERE: https://creativecommons.org/licenses/by-nc-sa/4.0/

	Author: discord/bruh4096#4512

	This file contains definitions of member functions for ArptDB class. ArptDB is an interface which allows
	to create a custom airport data base using x-plane's apt.dat. The class also allows you to access the data
	base and perform some searches on it.
*/

#include "libnav/arpt_db.hpp"


namespace libnav
{
	// Public member functions

	ArptDB::ArptDB(std::string sim_arpt_path, std::string custom_arpt_path,
		std::string custom_rnw_path, double min_rwy_l_m)
	{
		err_code = DbErr::ERR_NONE;

		db_version = 0;
		min_rwy_length_m = min_rwy_l_m;

		sim_arpt_db_path = sim_arpt_path;
		custom_arpt_db_path = custom_arpt_path;
		custom_rnw_db_path = custom_rnw_path;

		
		if (!does_db_exist(custom_arpt_db_path, custom_arpt_db_sign) || 
			!does_db_exist(custom_rnw_db_path, custom_rnw_db_sign))
		{
			if(does_file_exist(sim_arpt_db_path))
			{
				write_arpt_db.store(true, std::memory_order_seq_cst);
				sim_db_loaded = std::async(std::launch::async, [](ArptDB* ptr) -> int { return ptr->load_from_sim_db(); }, this);
				if (!does_db_exist(custom_arpt_db_path, custom_arpt_db_sign))
				{
					apt_db_created = true;
					arpt_db_task = std::async(std::launch::async, [](ArptDB* ptr) {ptr->write_to_arpt_db(); }, this);
				}
				if (!does_db_exist(custom_rnw_db_path, custom_rnw_db_sign))
				{
					rnw_db_created = true;
					rnw_db_task = std::async(std::launch::async, [](ArptDB* ptr) {ptr->write_to_rnw_db(); }, this);
				}
			}
			else
			{
				err_code = DbErr::FILE_NOT_FOUND;
			}
		}
		else
		{
			arpt_db_task = std::async(std::launch::async, [](ArptDB* ptr) {ptr->load_from_custom_arpt(); }, this);
			rnw_db_task = std::async(std::launch::async, [](ArptDB* ptr) {ptr->load_from_custom_rnw(); }, this);
		}
		
	}

	DbErr ArptDB::get_err()
	{
		if(err_code == DbErr::ERR_NONE)
		{
			// Wait until all of the threads finish
			arpt_db_task.get();
			rnw_db_task.get();
			if (apt_db_created || rnw_db_created)
			{
				if(bool(sim_db_loaded.get()))
				{
					err_code = DbErr::SUCCESS;
				}
				else
				{
					err_code = DbErr::DATA_BASE_ERROR;
				}
			}
			else
			{
				err_code = DbErr::SUCCESS;
			}
		}
		
		return err_code;
	}

	const airport_db_t& ArptDB::get_arpt_db()
	{
		return arpt_db;
	}

	const rnw_db_t& ArptDB::get_rnw_db()
	{
		return rnw_db;
	}

	// These functions need to be public because they're used in 
	// other threads when ArptDB object is constructed.

	/*
		Function: load_from_sim_db
		Description:
		Function that parses airport data from x-plane's apt.dat and adds all of the neccessary data to 
		arpt_db and rnw_db. The function also creates 2 .dat files for caching all of the neccessary data. 
		All airports with maximum runway length below MIN_RWY_LENGTH_M are rejected.
		Param:
		-----
		Return:
		Returns 1 if x-plane's airport data base has been loaded successfully. Otherwise, returns 0.
	*/

	int ArptDB::load_from_sim_db()
	{
		std::ifstream file(sim_arpt_db_path, std::ifstream::in);
		if (file.is_open())
		{
			std::string line;
			int i = 0;
			int limit = N_ARPT_LINES_IGNORE;
			airport_t tmp_arpt = { "", {{0, 0}, 0, 0, 0} };
			rnw_data_t tmp_rnw = { "", {} };
			double max_rnw_length_m = 0;

			while (getline(file, line))
			{
				if (i >= limit && line != "")
				{
					int row_code;
					std::string junk;
					std::stringstream s(line);
					s >> row_code;

					if (tmp_arpt.icao != "" && tmp_rnw.icao != "" && 
						(row_code == static_cast<int>(XPLMArptRowCode::LAND_ARPT)
						 || row_code == static_cast<int>(XPLMArptRowCode::DB_EOF)))
					{
						// Offload airport data
						double threshold = min_rwy_length_m;

						if (max_rnw_length_m >= threshold && tmp_arpt.data.transition_alt_ft + 
							tmp_arpt.data.transition_level > 0)
						{
							std::unordered_map<std::string, runway_entry_t> apt_runways;
							size_t n_runways = tmp_rnw.runways.size();

							for (size_t i = 0; i < n_runways; i++)
							{
								runway_t rnw = tmp_rnw.runways.at(i);
								std::pair<std::string, runway_entry_t> tmp = std::make_pair(rnw.id, rnw.data);
								apt_runways.insert(tmp);
								tmp_arpt.data.pos.lat_rad += rnw.data.start.lat_rad;
								tmp_arpt.data.pos.lon_rad += rnw.data.start.lon_rad;
							}

							tmp_arpt.data.pos.lat_rad /= double(n_runways);
							tmp_arpt.data.pos.lon_rad /= double(n_runways);

							// Update queues

							add_to_arpt_queue(tmp_arpt);
							add_to_rnw_queue(tmp_rnw);

							// Update internal data

							str_arpt_data_t apt = std::make_pair(tmp_arpt.icao, 
								tmp_arpt.data);
							str_rnw_t rnw_pair = std::make_pair(tmp_arpt.icao, 
								apt_runways);
							arpt_db.insert(apt);
							rnw_db.insert(rnw_pair);
						}

						tmp_arpt.icao = "";
						tmp_rnw.icao = "";
						tmp_arpt.data.pos = { 0, 0 };
						tmp_arpt.data.transition_alt_ft = 0;
						tmp_arpt.data.transition_level = 0;
						tmp_rnw.runways.clear();
						max_rnw_length_m = 0;
					}

					// Parse data

					if (row_code == static_cast<int>(XPLMArptRowCode::LAND_ARPT))
					{
						s >> tmp_arpt.data.elevation_ft;
					}
					else if (row_code == static_cast<int>(XPLMArptRowCode::MISC_DATA))
					{
						std::string var_name;
						s >> var_name;
						if (var_name == "icao_code")
						{
							std::string icao_code;
							s >> icao_code;
							tmp_arpt.icao = icao_code;
							tmp_rnw.icao = icao_code;
						}
						else if (var_name == "transition_alt")
						{
							s >> tmp_arpt.data.transition_alt_ft;
						}
						else if (var_name == "transition_level")
						{
							s >> tmp_arpt.data.transition_level;
						}
					}
					else if (row_code == static_cast<int>(XPLMArptRowCode::LAND_RUNWAY)
						 && tmp_arpt.icao != "")
					{
						double tmp = parse_runway(line, &tmp_rnw.runways);
						if (tmp > max_rnw_length_m)
						{
							max_rnw_length_m = tmp;
						}
					}
					else if (row_code == static_cast<int>(XPLMArptRowCode::DB_EOF))
					{
						break;
					}
				}
				else
				{
					int tmp = get_db_version(line);
					if(tmp)
						db_version = tmp;
				}
				i++;
			}
			file.close();
			write_arpt_db.store(false, std::memory_order_seq_cst);
			return 1;
		}
		file.close();
		return 0;
	}

	/*
		Function: write_to_arpt_db
		Description:
		Creates and populates a .dat file with all of the useful information about each airport.
		This includes icao code, latitude, longitude, elevation AMSL in feet, transition altitude 
		and transition level.
		Param:
		-----
		Return:
		------
	*/

	void ArptDB::write_to_arpt_db()
	{
		std::ofstream out(custom_arpt_db_path, std::ofstream::out);
		out << custom_arpt_db_sign << " " << std::to_string(DB_VERSION) << "\n";
		while (arpt_queue.size() || write_arpt_db.load(std::memory_order_seq_cst))
		{
			if (arpt_queue.size())
			{
				std::lock_guard<std::mutex> lock(arpt_queue_mutex);
				uint8_t precision = N_DOUBLE_OUT_PRECISION;
				airport_t data = arpt_queue[0];
				arpt_queue.erase(arpt_queue.begin());

				std::string arpt_lat = strutils::double_to_str(data.data.pos.lat_rad 
					* geo::RAD_TO_DEG, precision);
				std::string arpt_lon = strutils::double_to_str(data.data.pos.lon_rad 
					* geo::RAD_TO_DEG, precision);
				std::string arpt_icao_pos = data.icao + " " + arpt_lat + " " + arpt_lon;

				out << arpt_icao_pos << " " << data.data.elevation_ft << " " << data.data.transition_alt_ft << " " << data.data.transition_level << "\n";
			}
		}
		out.close();
	}

	/*
		Function: write_to_rnw_db
		Description:
		Creates and populates a .dat file with all of the useful information about each airport's runway.
		This includes id of the runway(e.g. 08L), latitude and longitude for strat poind and end point and
		the length of the displaced threshold in meters.
		Param:
		-----
		Return:
		------
	*/

	void ArptDB::write_to_rnw_db()
	{
		std::ofstream out(custom_rnw_db_path, std::ofstream::out);
		out << custom_rnw_db_sign << " " << std::to_string(DB_VERSION) << "\n";
		while (rnw_queue.size() || write_arpt_db.load(std::memory_order_seq_cst))
		{
			if (rnw_queue.size())
			{
				std::lock_guard<std::mutex> lock(rnw_queue_mutex);
				uint8_t precision = N_DOUBLE_OUT_PRECISION;
				rnw_data_t data = rnw_queue[0];
				rnw_queue.erase(rnw_queue.begin());
				for (size_t i = 0; i < data.runways.size(); i++)
				{
					std::string rnw_start_lat = strutils::double_to_str(
						data.runways[i].data.start.lat_rad * geo::RAD_TO_DEG, precision);

					std::string rnw_start_lon = strutils::double_to_str(
						data.runways[i].data.start.lon_rad * geo::RAD_TO_DEG, precision);

					std::string rnw_end_lat = strutils::double_to_str(
						data.runways[i].data.end.lat_rad * geo::RAD_TO_DEG, precision);

					std::string rnw_end_lon = strutils::double_to_str(
						data.runways[i].data.end.lon_rad * geo::RAD_TO_DEG, precision);


					std::string rnw_start = rnw_start_lat + " " + rnw_start_lon;
					std::string rnw_end = rnw_end_lat + " " + rnw_end_lon;

					std::string rnw_icao_pos = data.icao + " " + data.runways[i].id + " " + rnw_start + " " + rnw_end;

					out << rnw_icao_pos << " " << data.runways[i].data.displ_threshold_m << "\n";
				}
			}
		}
		out.close();
	}

	/*
		Function: load_from_custom_arpt
		Description:
		Loads data from .dat file created by write_to_arpt_db into arpt_db.
		Param:
		-----
		Return:
		------
	*/

	void ArptDB::load_from_custom_arpt()
	{
		std::ifstream file(custom_arpt_db_path, std::ifstream::in);
		if (file.is_open())
		{
			std::string line;
			while (getline(file, line))
			{
				if(line.length() == 0 || line[0] == DEFAULT_COMMENT_CHAR)
				{
					continue;
				}
				if (line != custom_arpt_db_sign)
				{
					std::string icao;
					airport_data_t tmp;
					std::stringstream s(line);
					s >> icao >> tmp.pos.lat_rad >> tmp.pos.lon_rad >> tmp.elevation_ft >> tmp.transition_alt_ft >> tmp.transition_level;
					tmp.pos.lat_rad *= geo::DEG_TO_RAD;
					tmp.pos.lon_rad *= geo::DEG_TO_RAD;
					std::pair<std::string, airport_data_t> tmp_pair = std::make_pair(icao, tmp);
					arpt_db.insert(tmp_pair);
				}
			}
			file.close();
		}
		file.close();
	}

	/*
		Function: load_from_custom_rnw
		Description:
		Loads data from .dat file created by write_to_rnw_db into rnw_db.
		Param:
		Param pam pam
		Return:
		------
	*/

	void ArptDB::load_from_custom_rnw()
	{
		std::ifstream file(custom_rnw_db_path, std::ifstream::in);
		if (file.is_open())
		{
			std::string line;
			std::string curr_icao = "";
			std::unordered_map<std::string, runway_entry_t> runways = {};
			while (getline(file, line))
			{
				if(line.length() == 0 || line[0] == DEFAULT_COMMENT_CHAR)
				{
					continue;
				}
				if (line != custom_rnw_db_sign)
				{
					std::string icao, rnw_id;
					runway_entry_t tmp;
					std::stringstream s(line);
					s >> icao;
					if (icao != curr_icao)
					{
						if (curr_icao != "")
						{
							std::pair<std::string, std::unordered_map<std::string, runway_entry_t>> icao_runways = std::make_pair(curr_icao, runways);
							rnw_db.insert(icao_runways);
						}
						curr_icao = icao;
						runways.clear();
					}
					s >> rnw_id >> tmp.start.lat_rad >> tmp.start.lon_rad >> tmp.end.lat_rad >> tmp.end.lon_rad >> tmp.displ_threshold_m;
					tmp.start.lat_rad *= geo::DEG_TO_RAD;
					tmp.start.lon_rad *= geo::DEG_TO_RAD;
					tmp.end.lat_rad *= geo::DEG_TO_RAD;
					tmp.end.lon_rad *= geo::DEG_TO_RAD;
					std::pair<std::string, runway_entry_t> str_rnw_entry = std::make_pair(rnw_id, tmp);
					runways.insert(str_rnw_entry);
				}
			}
			file.close();
		}
		file.close();
	}

	// Normal user interface functions:

	/*
		Function: load_from_custom_rnw
		Description:
		Checks if the ICAO code belongs to an airport in the data base.
		Param:
		icao_code: ICAO code that we want to check
		Return:
		true if if there's an airport in the data base with such ICAO code. Otherwise, returns false.
	*/

	bool ArptDB::is_airport(std::string icao_code) const
	{
		std::lock_guard<std::mutex> lock(arpt_db_mutex);
		return arpt_db.find(icao_code) != arpt_db.end();
	}

	/*
		Function: get_airport_data
		Description:
		Gets data of an airport and returns it into an airport_data structure.
		Param:
		icao_code: ICAO code of target airport.
		out: pointer to the airport_data structure, where the output will be written.
		Return:
		Returns 1 if any data has been written to out. Otherwise, returns 0.
	*/

	bool ArptDB::get_airport_data(std::string icao_code, airport_data_t* out) const
	{
		if (is_airport(icao_code))
		{
			std::lock_guard<std::mutex> lock(arpt_db_mutex);
			*out = arpt_db.at(icao_code);
			return 1;
		}
		return 0;
	}

	/*
		Function: get_apt_rwys
		Description:
		Gets data of all runways of an airport and returns it into an runway_data structure.
		Param:
		icao_code: ICAO code of target airport.
		out: pointer to the runway_data structure, where the output will be written.
		Return:
		Returns number of runways of an airport if any data has been written to out. Otherwise, returns 0.
	*/

	int ArptDB::get_apt_rwys(std::string icao_code, runway_data* out)
	{
		if (is_airport(icao_code))
		{
			std::lock_guard<std::mutex> lock(rnw_db_mutex);
			int n_runways = 0;
			runway_data tmp = rnw_db.at(icao_code);
			for (auto& it : tmp)
			{
				out->insert(it);
				n_runways++;
			}
			return n_runways;
		}
		return 0;
	}

	/*
		Function: get_rnw_data
		Description:
		Gets data of a specific runway of an airport and returns it into an runway_entry structure.
		Param:
		apt_icao: ICAO code of target airport.
		rnw_id: id of a runway that we're looking for
		out: pointer to the runway_entry structure, where the output will be written.
		Return:
		Returns 1 if runway data was found and written to out. Otherwise, returns 0.
	*/

	int ArptDB::get_rnw_data(std::string apt_icao, std::string rnw_id, runway_entry_t* out)
	{
		if (is_airport(apt_icao))
		{
			std::lock_guard<std::mutex> lock(rnw_db_mutex);
			runway_data tmp = rnw_db.at(apt_icao);

			if (tmp.find(rnw_id) != tmp.end())
			{
				*out = tmp.at(rnw_id);
				return 1;
			}
		}
		return 0;
	}

	// Private member functions:

	bool ArptDB::does_db_exist(std::string path, std::string sign)
	{
		std::ifstream file(path, std::ifstream::in);
		if (file.is_open())
		{
			std::string line, tmp;
			double ver = 0;
			getline(file, line);
			std::stringstream s(line);
			
			s >> tmp >> ver;

			if (tmp == sign && ver == DB_VERSION)
			{
				file.close();
				return true;
			}
		}
		file.close();
		return false;
	}

	int ArptDB::get_db_version(std::string& line)
	{
		std::vector<std::string> s_split = strutils::str_split(line);

		if(int(s_split.size()) >= N_HEADER_STR_WORDS && 
			s_split[1] == "Generated" && s_split[2] == "by" && 
			s_split[3] == "WorldEditor")
		{
			return strutils::stoi_with_strip(s_split[0]);
		}
		return 0;
	}

	double ArptDB::parse_runway(std::string line, std::vector<runway_t>* rnw)
	{
		std::stringstream s(line);
		int limit_1 = N_RNW_ITEMS_IGNORE_BEGINNING;

		int limit_2 = N_RNW_ITEMS_IGNORE_END;
		std::string junk;
		runway_t rnw_1;
		runway_t rnw_2;
		for (int i = 0; i < limit_1; i++)
		{
			s >> junk;
		}
		s >> rnw_1.id >> rnw_1.data.start.lat_rad >> rnw_1.data.start.lon_rad >> rnw_1.data.displ_threshold_m;
		for (int i = 0; i < limit_2; i++)
		{
			s >> junk;
		}
		s >> rnw_2.id >> rnw_1.data.end.lat_rad >> rnw_1.data.end.lon_rad >> rnw_2.data.displ_threshold_m;
		
		rnw_1.data.start.lat_rad *= geo::DEG_TO_RAD;
		rnw_1.data.start.lon_rad *= geo::DEG_TO_RAD;
		rnw_1.data.end.lat_rad *= geo::DEG_TO_RAD;
		rnw_1.data.end.lon_rad *= geo::DEG_TO_RAD;
		
		rnw_2.data.start.lat_rad = rnw_1.data.end.lat_rad;
		rnw_2.data.start.lon_rad = rnw_1.data.end.lon_rad;
		rnw_2.data.end.lat_rad = rnw_1.data.start.lat_rad;
		rnw_2.data.end.lon_rad = rnw_1.data.start.lon_rad;

		rnw_1.id = strutils::normalize_rnw_id(rnw_1.id);
		rnw_2.id = strutils::normalize_rnw_id(rnw_2.id);

		rnw->push_back(rnw_1);
		rnw->push_back(rnw_2);

		return rnw_1.data.get_impl_length_m();
	}

	void ArptDB::add_to_arpt_queue(airport_t arpt)
	{
		std::lock_guard<std::mutex> lock(arpt_queue_mutex);
		arpt_queue.push_back(arpt);
	}

	void ArptDB::add_to_rnw_queue(rnw_data_t rnw)
	{
		std::lock_guard<std::mutex> lock(rnw_queue_mutex);
		rnw_queue.push_back(rnw);
	}
}; // namespace libnav