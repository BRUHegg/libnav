#include <iostream>
#include <memory>
#include <string>
#include "lib/libnav/nav_db.hpp"
#include "lib/libnav/awy_db.hpp"


double AC_LAT_DEF = 45.588670483;
double AC_LON_DEF = -122.598150383;


namespace dbg
{
    class Avionics
    {
    public:
        double ac_lat;
        double ac_lon;

        std::shared_ptr<libnav::ArptDB> arpt_db_ptr;
        std::shared_ptr<libnav::NavaidDB> navaid_db_ptr;

        libnav::AwyDB* awy_db;
        libnav::NavDB* db;

        std::unordered_map<std::string, std::string> env_vars;


        Avionics(std::string apt_dat, std::string custom_apt, std::string custom_rnw,
            std::string fix_data, std::string navaid_data, std::string awy_data, 
            double def_lat=AC_LAT_DEF, double def_lon=AC_LON_DEF)
        {
            env_vars["ac_lat"] = strutils::double_to_str(def_lat, 8);
            env_vars["ac_lon"] = strutils::double_to_str(def_lon, 8);

            ac_lat = def_lat;
            ac_lon = def_lon;

            std::shared_ptr<libnav::ArptDB> arpt_db_ptr = 
                std::make_shared<libnav::ArptDB>(apt_dat, custom_apt, custom_rnw);
	        std::shared_ptr<libnav::NavaidDB> navaid_db_ptr = 
                std::make_shared<libnav::NavaidDB>(fix_data, navaid_data);

            awy_db = new libnav::AwyDB(awy_data);
            db = new libnav::NavDB(arpt_db_ptr, navaid_db_ptr);

            libnav::DbErr err_arpt = db->is_arpt_loaded();
            libnav::DbErr err_nav = db->is_navaid_loaded();

            if(err_arpt != libnav::DbErr::SUCCESS)
            {
                std::cout << "Unable to load airport database\n";
            }
            if(err_nav != libnav::DbErr::SUCCESS)
            {
                std::cout << "Unable to load navaid database\n";
            }
        }

        void update()
        {
            update_pos();
        }

        ~Avionics()
        {
            delete awy_db;
            delete db;
            navaid_db_ptr.reset();
            navaid_db_ptr.reset();
            arpt_db_ptr.reset();
        }

    private:
        void update_pos()
        {
            bool lat_valid = strutils::is_numeric(env_vars["ac_lat"]);
            bool lon_valid = strutils::is_numeric(env_vars["ac_lon"]);
            
            if(lon_valid && lat_valid)
            {
                ac_lat = std::stod(env_vars["ac_lat"]);
                ac_lon = std::stod(env_vars["ac_lon"]);
            }
        }
    };

    typedef void (*cmd)(Avionics*, std::vector<std::string>&);


    inline void set_var(Avionics* av, std::vector<std::string>& in)
    {
        if(in.size() != 2)
        {
            std::cout << "Command expects 2 arguments\n";
            return;
        }

        av->env_vars[in[0]] = in[1];
    }

    inline void display_poi_info(Avionics* av, std::vector<std::string>& in)
    {
        if(in.size() < 1)
        {
            std::cout << "Too few arguments provided\n";
            return;
        }

        std::string poi_id = in[0];

        libnav::airport_data_t found_arpt;
        std::vector<libnav::waypoint_entry_t> found_wpts;

        libnav::NavDB* db = av->db;

        size_t n_arpts_found = db->get_airport_data(poi_id, &found_arpt);
        size_t n_wpts_found = db->get_wpt_data(poi_id, &found_wpts);

        if (n_arpts_found)
        {
            std::string lat_str = strutils::lat_to_str(found_arpt.pos.lat_deg);
            std::string lon_str = strutils::lon_to_str(found_arpt.pos.lon_deg);
            std::cout << poi_id << " " << lat_str << " " << lon_str << "\n";
        }
        else if (n_wpts_found)
        {
            libnav::sort_wpt_entry_by_dist(&found_wpts, { av->ac_lat, av->ac_lon });

            for (size_t i = 0; i < n_wpts_found; i++)
            {
                std::string lat_str = strutils::lat_to_str(found_wpts[i].pos.lat_deg);
                std::string lon_str = strutils::lon_to_str(found_wpts[i].pos.lon_deg);
                int wpt_type = found_wpts[i].type;
                std::string type_str = libnav::navaid_to_str(wpt_type);
                if (wpt_type == libnav::NAV_WAYPOINT)
                {
                    std::cout << poi_id << " " << lat_str << " " << lon_str << " " << 
                        type_str << "\n";
                }
                else
                {
                    libnav::navaid_entry_t* navaid_data = found_wpts[i].navaid;
                    double freq = navaid_data->freq;
                    std::string lat_dms = strutils::lat_to_str(found_wpts[i].pos.lat_deg);
                    std::string lon_dms = strutils::lon_to_str(found_wpts[i].pos.lon_deg);
                    std::cout << poi_id << " " << lat_dms << " " << lon_dms << " " << type_str << " " << 
                        strutils::freq_to_str(freq) << "\n";
                }
            }
        }
        else
        {
            std::cout << "Not in data base\n";
        }
    }

    inline void get_path(Avionics* av, std::vector<std::string>& in)
    {
        if(in.size() != 3)
        {
            std::cout << "Command expects 3 arguments: <airway name> <entry point> <exit point>\n";
            return;
        }

        std::vector<libnav::awy_point_t> tmp;
        av->awy_db->get_path(in[0], in[1], in[2], &tmp);
        for(int i = 0; i < int(tmp.size()); i++)
        {
            std::cout << tmp[i].id << " " << tmp[i].alt_restr.lower 
                << " " << tmp[i].alt_restr.upper << "\n";
        }
    }

    inline void quit(Avionics* av, std::vector<std::string>& in)
    {
        (void)av;

        if(in.size())
        {
            std::cout << "Too many arguments provided\n";
            return;
        }
        std::exit(0);
    }


    std::unordered_map<std::string, cmd> cmd_map = {
        {"set", set_var},
        {"poinfo", display_poi_info}, 
        {"get_path", get_path},
        {"quit", quit}
        };
}