#pragma once

#include <string>
#include <fstream>
#include <sstream>
#include <unordered_map>
#include <unordered_set>
#include <queue>
#include <vector>
#include "str_utils.hpp"
#include "navaid_db.hpp"


namespace libnav
{
    constexpr int N_AWY_LINES_IGNORE = 3;
    constexpr int N_AWY_COL_NORML = 11;
    constexpr char AWY_NAME_SEP = '-';
    constexpr char AWY_RESTR_FWD = 'F';
    constexpr char AWY_RESTR_BWD = 'B';
    constexpr char AWY_RESTR_NONE = 'N';


    struct alt_restr_t
    {
        uint32_t lower, upper;
    };

    struct awy_entry_t
    {
        std::string xp_type, reg_code;  // Region code of navaid/fix
    };

    struct awy_point_t
    {
        std::string id;
        awy_entry_t data;
        alt_restr_t alt_restr;

        awy_point_t(std::string nm="", std::string tp="", 
            std::string r_c="", uint32_t lower=0, uint32_t upper=0);

        std::string get_uid();
    };

    struct awy_line_t  // This is used to store the contents of 1 line of awy.dat
    {
        earth_data_line_t data;

        awy_point_t p1, p2;
        char path_restr;

        uint32_t lower_fl, upper_fl;
        std::string awy_names;

        awy_line_t(std::string& s);
    };


    class AwyDB
    {
        typedef std::unordered_map<std::string, std::unordered_map<std::string, alt_restr_t>> graph_t;
        typedef std::unordered_map<std::string, graph_t> awy_db_t;
        typedef std::unordered_map<std::string, awy_entry_t> awy_data_db_t;

    public:

        AwyDB(std::string awy_path);

        DbErr get_err();

        int get_airac();

        bool is_in_awy(std::string awy, std::string point);

        int get_path(std::string awy, std::string start, 
            std::string end, std::vector<awy_point_t>* out);

        // You aren't supposed to call this function.
        // It's public to allow for the concurrent loading
        DbErr load_airways(std::string awy_path);

        ~AwyDB();

    private:
        int airac_cycle;
        awy_db_t awy_db;
        std::unordered_map<std::string, awy_data_db_t> awy_data;
        std::future<DbErr> db_loaded;

        void add_to_awy_db(awy_point_t p1, awy_point_t p2, std::string awy_nm, char restr);
    };    
}; // namespace libnav
