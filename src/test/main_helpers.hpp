#include <iostream>
#include <memory>
#include <string>
#include <libnav/awy_db.hpp>
#include <libnav/hold_db.hpp>
#include <libnav/cifp_parser.hpp>
#include <libnav/geo_utils.hpp>

#define UNUSED(x) (void)(x)


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

        std::shared_ptr<libnav::AwyDB> awy_db;
        std::shared_ptr<libnav::HoldDB> hold_db;

        std::unordered_map<std::string, std::string> env_vars;

        std::string cifp_dir_path;


        Avionics(std::string apt_dat, std::string custom_apt, std::string custom_rnw,
            std::string fix_data, std::string navaid_data, std::string awy_data,
            std::string hold_data, 
            std::string cifp_path, double def_lat=AC_LAT_DEF, 
            double def_lon=AC_LON_DEF)
        {
            env_vars["ac_lat"] = strutils::double_to_str(def_lat, 8);
            env_vars["ac_lon"] = strutils::double_to_str(def_lon, 8);

            cifp_dir_path = cifp_path;

            ac_lat = def_lat;
            ac_lon = def_lon;

            arpt_db_ptr = 
                std::make_shared<libnav::ArptDB>(apt_dat, custom_apt, custom_rnw);
	        navaid_db_ptr = 
                std::make_shared<libnav::NavaidDB>(fix_data, navaid_data);
            awy_db = std::make_shared<libnav::AwyDB>(awy_data);
            hold_db = std::make_shared<libnav::HoldDB>(hold_data);

            libnav::DbErr err_arpt = arpt_db_ptr->get_err();
            libnav::DbErr err_wpt = navaid_db_ptr->get_wpt_err();
            libnav::DbErr err_nav = navaid_db_ptr->get_navaid_err();
            libnav::DbErr err_awy = awy_db->get_err();
            libnav::DbErr err_hold = hold_db->get_err();

            std::cout << navaid_db_ptr->get_wpt_cycle() << " " <<
                navaid_db_ptr->get_navaid_cycle() << " " << 
                awy_db->get_airac() << " " << hold_db->get_airac() << "\n";

            std::cout << "Fix data base version: " << 
                navaid_db_ptr->get_wpt_version() << "\n";
            std::cout << "Navaid data base version: " << 
                navaid_db_ptr->get_navaid_version() << "\n";

            if(err_arpt != libnav::DbErr::SUCCESS)
            {
                std::cout << "Unable to load airport database\n";
            }
            if(err_wpt != libnav::DbErr::SUCCESS)
            {
                std::cout << "Unable to load waypoint database\n";
            }
            if(err_nav != libnav::DbErr::SUCCESS)
            {
                std::cout << "Unable to load navaid database\n";
            }
            if(err_awy != libnav::DbErr::SUCCESS)
            {
                std::cout << "Unable to load airway database\n";
            }
            if(err_hold != libnav::DbErr::SUCCESS)
            {
                std::cout << "Unable to load hold database\n";
            }

            auto apt_db = arpt_db_ptr->get_arpt_db();
            auto navaid_db = navaid_db_ptr->get_db();

            
            geo::point a = {0.88620446987426393, -0.032602314035019953};
            geo::point b = {0.88933687994841504, -0.023475648495750793};
            double brng = b.get_gc_bearing_rad(a);
            std::cout << brng * geo::RAD_TO_DEG + 360 << "\n";
        }

        void update()
        {
            update_pos();
        }

        ~Avionics()
        {
            hold_db.reset();
            awy_db.reset();
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

    struct awy_filter_data_t
    {
        std::string s;
        Avionics* ptr;
    };

    typedef void (*cmd)(Avionics*, std::vector<std::string>&);


    inline void set_var(Avionics* av, std::vector<std::string>& in)
    {
        if(in.size() != 2)
        {
            std::cout << "Command expects 2 arguments: <variable name>, <value>\n";
            return;
        }

        av->env_vars[in[0]] = in[1];
    }

    inline void print(Avionics* av, std::vector<std::string>& in)
    {
        if(in.size() != 1)
        {
            std::cout << "Command expects 1 argument: <variable name>\n";
            return;
        }

        if(av->env_vars.find(in[0]) != av->env_vars.end())
        {
            std::cout << av->env_vars[in[0]] << "\n";
        }
        else
        {
            std::cout << "Variable not found\n";
        }
    }

    inline void name(Avionics* av, std::vector<std::string>& in)
    {
        if(in.size() != 1)
        {
            std::cout << "Command expects 1 argument: <fix name>\n";
            return;
        }
        
        std::string poi_id = in[0];

        std::vector<libnav::waypoint_entry_t> found_wpts;

        size_t n_wpts_found = av->navaid_db_ptr->get_wpt_data(poi_id, &found_wpts);

        for(size_t i = 0; i < n_wpts_found; i++)
        {
            libnav::waypoint_t wpt = {poi_id, found_wpts[i]};

            std::cout << av->navaid_db_ptr->get_fix_desc(wpt) << "\n";
        }
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

        size_t n_arpts_found = size_t(av->arpt_db_ptr->get_airport_data(poi_id, &found_arpt));
        size_t n_wpts_found = av->navaid_db_ptr->get_wpt_data(poi_id, &found_wpts);

        if (n_arpts_found)
        {
            std::string lat_str = strutils::lat_to_str(found_arpt.pos.lat_rad 
                * geo::RAD_TO_DEG);
            std::string lon_str = strutils::lon_to_str(found_arpt.pos.lon_rad 
                * geo::RAD_TO_DEG);
            std::cout << poi_id << " " << lat_str << " " << lon_str << "\n";
        }
        else if (n_wpts_found)
        {
            libnav::sort_wpt_entry_by_dist(&found_wpts, { av->ac_lat 
                * geo::DEG_TO_RAD, av->ac_lon * geo::DEG_TO_RAD });

            for (size_t i = 0; i < n_wpts_found; i++)
            {
                std::string lat_str = strutils::lat_to_str(found_wpts[i].pos.lat_rad 
                    * geo::RAD_TO_DEG);
                std::string lon_str = strutils::lon_to_str(found_wpts[i].pos.lon_rad 
                    * geo::RAD_TO_DEG);
                libnav::NavaidType wpt_type = found_wpts[i].type;
                std::string type_str = libnav::navaid_to_str(wpt_type);
                if (wpt_type == libnav::NavaidType::WAYPOINT)
                {
                    std::cout << poi_id << " " << lat_str << " " << lon_str << " " << 
                        type_str << "\n";
                }
                else
                {
                    libnav::navaid_entry_t* navaid_data = found_wpts[i].navaid;
                    double freq = navaid_data->freq;
                    std::string lat_dms = strutils::lat_to_str(found_wpts[i].pos.lat_rad
                        * geo::RAD_TO_DEG);
                    std::string lon_dms = strutils::lon_to_str(found_wpts[i].pos.lon_rad
                        * geo::RAD_TO_DEG);
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

        std::vector<libnav::waypoint_entry_t> entry_wpts;
        std::vector<libnav::waypoint_entry_t> exit_wpts;

        awy_filter_data_t filter_data = {in[0], av};
        av->navaid_db_ptr->get_wpt_data(in[1], &entry_wpts, "", "", 
            libnav::NavaidType::NAVAID, 
            [](libnav::waypoint_t wpt, void* ref) -> bool {
                awy_filter_data_t *data = reinterpret_cast<awy_filter_data_t*>(ref);
                return data->ptr->awy_db->is_in_awy(data->s, wpt.get_awy_id());
            }, &filter_data);

        av->navaid_db_ptr->get_wpt_data(in[2], &exit_wpts, "", "", 
            libnav::NavaidType::NAVAID, 
            [](libnav::waypoint_t wpt, void* ref) -> bool {
                awy_filter_data_t *data = reinterpret_cast<awy_filter_data_t*>(ref);
                return data->ptr->awy_db->is_in_awy(data->s, wpt.get_awy_id());
            }, &filter_data);

        
        if(entry_wpts.size() && exit_wpts.size())
        {
            libnav::waypoint_t entry_wpt = {in[1], entry_wpts[0]};
            libnav::waypoint_t exit_wpt = {in[2], exit_wpts[0]};
            std::vector<libnav::awy_point_t> tmp;
            av->awy_db->get_ww_path(in[0], entry_wpt.get_awy_id(), exit_wpt.get_awy_id(), &tmp);
            for(size_t i = 0; i < tmp.size(); i++)
            {
                std::cout << tmp[i].id << " " << tmp[i].alt_restr.lower 
                    << " " << tmp[i].alt_restr.upper << "\n";
            }
        }
        
    }

    inline void get_aa_path(Avionics* av, std::vector<std::string>& in)
    {
        if(in.size() != 3)
        {
            std::cout << "Command expects 3 arguments: <airway name> <entry point> <next airway>\n";
            return;
        }

        std::vector<libnav::waypoint_entry_t> entry_wpts;

        awy_filter_data_t filter_data = {in[0], av};
        av->navaid_db_ptr->get_wpt_data(in[1], &entry_wpts, "", "", 
            libnav::NavaidType::NAVAID, 
            [](libnav::waypoint_t wpt, void* ref) -> bool {
                awy_filter_data_t *data = reinterpret_cast<awy_filter_data_t*>(ref);
                return data->ptr->awy_db->is_in_awy(data->s, wpt.get_awy_id());
            }, &filter_data);

        if(entry_wpts.size())
        {
            libnav::waypoint_t entry_wpt = {in[1], entry_wpts[0]};
            std::vector<libnav::awy_point_t> tmp;
            av->awy_db->get_aa_path(in[0], entry_wpt.get_awy_id(), in[2], &tmp);
            for(size_t i = 0; i < tmp.size(); i++)
            {
                std::cout << tmp[i].id << " " << tmp[i].alt_restr.lower 
                    << " " << tmp[i].alt_restr.upper << "\n";
            }
        }
    }

    inline libnav::waypoint_entry_t select_desired(std::string& name,
            std::vector<libnav::waypoint_entry_t>& wpts)
    {
        if(wpts.size() == 0)
        {
            return {};
        }
        if(wpts.size() == 1)
        {
            return wpts[0];
        }
        std::cout << "Select desired " << name << "\n";
        for(size_t i = 0; i < wpts.size(); i++)
        {
            std::cout << i+1 << ". " << strutils::lat_to_str(wpts[i].pos.lat_rad 
                * geo::RAD_TO_DEG) 
                << " " << strutils::lat_to_str(wpts[i].pos.lon_rad
                * geo::RAD_TO_DEG) << "\n";
        }
        while(1)
        {
            std::string tmp;
            std::getline(std::cin, tmp);

            size_t num = size_t(strutils::stoi_with_strip(tmp));
            if(num != 0 && num < wpts.size() + 1)
            {
                return wpts[num-1];
            }
        }
    }

    inline void hold_info(Avionics* av, std::vector<std::string>& in)
    {
        if(in.size() != 1)
        {
            std::cout << "Command expects 1 argument: <name of waypoint>\n";
            return;
        }

        std::vector<libnav::waypoint_entry_t> wpts;
        av->navaid_db_ptr->get_wpt_data(in[0], &wpts);
        libnav::waypoint_entry_t tgt_data = select_desired(in[0], wpts);
        libnav::waypoint_t tgt_wpt = {in[0], tgt_data};

        std::string wpt_hold_id = tgt_wpt.get_hold_id();

        std::vector<libnav::hold_data_t> hld_data = 
            av->hold_db->get_hold_data(wpt_hold_id);

        if(hld_data.size())
        {
            for(size_t i = 0; i < hld_data.size(); i++)
            {
                std::cout << "Inbound magnetic course(degrees): " << hld_data[i].inbd_crs_mag
                    << "\n" << "Leg time(minutes): " << hld_data[i].leg_time_min << "\n";
                std::cout << "DME leg length(nm): " << hld_data[i].dme_leg_dist_nm << "\n";
                if(hld_data[i].turn_dir == libnav::HoldTurnDir::LEFT)
                {
                    std::cout << "Turn direction: Left\n";
                }
                else
                {
                    std::cout << "Turn direction: Right\n";
                }
                std::cout << "Minimum altitude(feet): " << hld_data[i].min_alt_ft << "\n";
                std::cout << "Maximum altitude(feet): " << hld_data[i].max_alt_ft << "\n";
                std::cout << "Speed restriction(knots): " << hld_data[i].spd_kts << "\n";
                std::cout << "\n";
            }
            
        }
        else
        {
            std::cout << "No hold found at selected fix\n";
        }
    }

    inline void quit(Avionics* av, std::vector<std::string>& in)
    {
        UNUSED(av);

        if(in.size())
        {
            std::cout << "Too many arguments provided\n";
            return;
        }
        std::exit(0);
    }

    inline void allrwy(Avionics* av, std::vector<std::string>& in)
    {
        if(in.size() != 1)
        {
            std::cout << "Invalid arguments provided\n";
            return;
        }
        
        libnav::Airport apt(in[0], av->arpt_db_ptr.get(), 
            av->navaid_db_ptr.get(), av->cifp_dir_path);

        std::vector<std::string> rwys = apt.get_rwys();
        for(auto i: rwys)
        {
            std::cout << i << "\n";
        }
    }

    inline void allsid(Avionics* av, std::vector<std::string>& in)
    {
        if(in.size() != 1)
        {
            std::cout << "Invalid arguments provided\n";
            return;
        }
        
        libnav::Airport apt(in[0], av->arpt_db_ptr.get(), av->navaid_db_ptr.get(), 
            av->cifp_dir_path);
        libnav::Airport apt1(apt);

        auto sids = apt1.get_all_sids();

        for(auto i: sids)
        {
            std::cout << "SID: " << i.first << "\n";
            for(auto j: i.second)
            {
                std::cout << "Transition " << j << "\n";
            }
        }
    }

    inline void allapch(Avionics* av, std::vector<std::string>& in)
    {
        if(in.size() != 1)
        {
            std::cout << "Invalid arguments provided\n";
            return;
        }
        
        libnav::Airport apt(in[0], av->arpt_db_ptr.get(), av->navaid_db_ptr.get(), 
            av->cifp_dir_path);

        auto appr = apt.get_all_appch();

        for(auto i: appr)
        {
            std::cout << "Approach: " << i.first << "\n";
            for(auto j: i.second)
            {
                std::cout << "Transition " << j << "\n";
            }
        }
    }

    inline void getsid(Avionics* av, std::vector<std::string>& in)
    {
        if(in.size() != 3)
        {
            std::cout << "Invalid arguments provided\n";
            return;
        }
        
        libnav::Airport apt(in[0], av->arpt_db_ptr.get(), av->navaid_db_ptr.get(), 
            av->cifp_dir_path);
        libnav::Airport apt1(apt);

        libnav::arinc_leg_seq_t sid_legs = apt1.get_sid(in[1], in[2]);
        for(auto i: sid_legs)
        {
            std::cout << i.main_fix.id << " " << i.leg_type << "\n";
        }
    }

    inline void getstar(Avionics* av, std::vector<std::string>& in)
    {
        if(in.size() != 3)
        {
            std::cout << "Command expected 3 arguments: <airport icao> <SID name> <transition>\n";
            return;
        }
        
        libnav::Airport apt(in[0], av->arpt_db_ptr.get(), av->navaid_db_ptr.get(), 
            av->cifp_dir_path);

        if(apt.get_err() != libnav::DbErr::SUCCESS &&
            apt.get_err() != libnav::DbErr::PARTIAL_LOAD)
        {
            std::cout << "Invalid airport icao\n";
            return;
        }

        libnav::arinc_leg_seq_t star_legs = apt.get_star(in[1], in[2]);
        for(auto i: star_legs)
        {
            std::cout << i.main_fix.id << " " << i.leg_type << "\n";
        }
    }

    inline void getappch(Avionics* av, std::vector<std::string>& in)
    {
        if(in.size() != 3)
        {
            std::cout << "Command expected 3 arguments: <airport icao> <approach name> <transition>\n";
            return;
        }
        
        libnav::Airport apt(in[0], av->arpt_db_ptr.get(), av->navaid_db_ptr.get(), 
            av->cifp_dir_path);

        if(apt.get_err() != libnav::DbErr::SUCCESS &&
            apt.get_err() != libnav::DbErr::PARTIAL_LOAD)
        {
            std::cout << "Invalid airport icao\n";
            return;
        }

        libnav::arinc_leg_seq_t appch_legs = apt.get_appch(in[1], in[2]);
        for(auto i: appch_legs)
        {
            std::cout << i.main_fix.id << " " << i.leg_type << "\n";
        }
    }

    inline void lssid(Avionics* av, std::vector<std::string>& in)
    {
        if(in.size() != 2)
        {
            std::cout << "Command expected 2 arguments: <airport icao> <runway id>\n";
            return;
        }

        libnav::Airport apt(in[0], av->arpt_db_ptr.get(), av->navaid_db_ptr.get(), 
            av->cifp_dir_path);

        if(apt.get_err() != libnav::DbErr::SUCCESS &&
            apt.get_err() != libnav::DbErr::PARTIAL_LOAD)
        {
            std::cout << "Invalid airport icao\n";
            return;
        }

        std::set<std::string> sids = apt.get_sid_by_rwy(in[1]);

        if(!sids.size())
        {
            std::cout << "Invalid runway\n";
        }
        else
        {
            for(auto i: sids)
            {
                std::cout << i << "\n";
            }
        }
    }

    inline void lsstar(Avionics* av, std::vector<std::string>& in)
    {
        if(in.size() != 2)
        {
            std::cout << "Command expected 2 arguments: <airport icao> <runway id>\n";
            return;
        }

        libnav::Airport apt(in[0], av->arpt_db_ptr.get(), av->navaid_db_ptr.get(), 
            av->cifp_dir_path);

        if(apt.get_err() != libnav::DbErr::SUCCESS &&
            apt.get_err() != libnav::DbErr::PARTIAL_LOAD)
        {
            std::cout << "Invalid airport icao\n";
            return;
        }

        std::set<std::string> stars = apt.get_star_by_rwy(in[1]);

        if(!stars.size())
        {
            std::cout << "Invalid runway\n";
        }
        else
        {
            for(auto i: stars)
            {
                std::cout << i << "\n";
            }
        }
    }

    inline void lssidtrans(Avionics* av, std::vector<std::string>& in)
    {
        if(in.size() != 2)
        {
            std::cout << "Invalid arguments provided\n";
            return;
        }

        libnav::Airport apt(in[0], av->arpt_db_ptr.get(), av->navaid_db_ptr.get(), 
            av->cifp_dir_path);

        if(apt.get_err() != libnav::DbErr::SUCCESS &&
            apt.get_err() != libnav::DbErr::PARTIAL_LOAD)
        {
            std::cout << "Invalid airport icao\n";
            return;
        }

        std::set<std::string> trans = apt.get_trans_by_sid(in[1]);

        for(auto i: trans)
        {
            std::cout << i << "\n";
        }
    }

    inline void lsstartrans(Avionics* av, std::vector<std::string>& in)
    {
        if(in.size() != 2)
        {
            std::cout << "Invalid arguments provided\n";
            return;
        }

        libnav::Airport apt(in[0], av->arpt_db_ptr.get(), av->navaid_db_ptr.get(), 
            av->cifp_dir_path);

        if(apt.get_err() != libnav::DbErr::SUCCESS &&
            apt.get_err() != libnav::DbErr::PARTIAL_LOAD)
        {
            std::cout << "Invalid airport icao\n";
            return;
        }

        std::set<std::string> trans = apt.get_trans_by_star(in[1]);

        for(auto i: trans)
        {
            std::cout << i << "\n";
        }
    }

    std::unordered_map<std::string, cmd> cmd_map = {
        {"set", set_var},
        {"print", print},
        {"p", print},
        {"name", name}, 
        {"poinfo", display_poi_info}, 
        {"get_path", get_path},
        {"get_aa_path", get_aa_path},
        {"holdinfo", hold_info},
        {"quit", quit},
        {"q", quit},
        {"allrwy", allrwy},
        {"allsid", allsid},
        {"allapch", allapch},
        {"getsid", getsid},
        {"getstar", getstar},
        {"getappch", getappch},
        {"lssid", lssid},
        {"lsstar", lsstar},
        {"lssidtrans", lssidtrans},
        {"lsstartrans", lsstartrans}
        };
}
