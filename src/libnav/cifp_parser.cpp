/*
	This project is licensed under
	Creative Commons Attribution-NonCommercial-ShareAlike 4.0 International Public License (CC BY-NC-SA 4.0).

	A SUMMARY OF THIS LICENSE CAN BE FOUND HERE: https://creativecommons.org/licenses/by-nc-sa/4.0/

	Author: discord/bruh4096#4512

	This file contains definitions of member functions for Airport class.
*/


#include "libnav/cifp_parser.hpp"


namespace libnav
{
    // Functions for decoding some arinc data fields:

    ProcType str2proc_type(std::string s)
    {
        char tmp[ARINC_MAX_TP_LENGTH+1];
        size_t sz = 0;

        while (sz < s.length() && sz < ARINC_MAX_TP_LENGTH && s[sz] != ':')
        {
            tmp[sz] = s[sz];
            sz++;
        }
        tmp[sz] = 0;
        
        if(strcmp(tmp, "SID") == 0)
        {
            return ProcType::SID;
        }
        if(strcmp(tmp, "STAR") == 0)
        {
            return ProcType::STAR;
        }
        if(strcmp(tmp, "APPCH") == 0)
        {
            return ProcType::APPROACH;
        }
        if(strcmp(tmp, "PRDAT") == 0)
        {
            return ProcType::PRDAT;
        }
        if(strcmp(tmp, "RWY") == 0)
        {
            return ProcType::RWY;
        }
        
        return ProcType::NONE;
    }

    TurnDir char2dir(char c)
    {
        switch (c)
        {
        case 'L':
            return TurnDir::LEFT;
        case 'R':
            return TurnDir::RIGHT;
        default:
            return TurnDir::EITHER;
        }
    }

    float str2rnp(std::string s)
    {
        if(s.length() == 3)
        {
            float num = float(atof(s.substr(0, 2).c_str()));
            int exp = s[2] - '0';

            if(exp != 0)
            {
                num *= float(std::pow(0.1, exp));
            }

            return num;
        }
        return 0;
    }

    float str2outbd_crs(std::string s, bool* is_true)
    {
        *is_true = false;

        if(s.length() == 4)
        {
            if(s[3] == 'T')
            {
                *is_true = true;
                return float(atof(s.substr(0, 3).c_str()));
            }
            return float(atof(s.c_str()) * 0.1);
        }
        return 0;
    }

    float str2outbd_dist(std::string s, bool* as_time)
    {
        *as_time = false;

        if(s.length() == 4)
        {
            if(s[0] == 'T')
            {
                *as_time = true;
                return float(atof(s.substr(1, 3).c_str()) * 0.1);
            }
            return float(atof(s.c_str()) * 0.1);
        }
        return 0;
    }

    AltMode char2alt_mode(char c)
    {
        switch (c)
        {
        case '+':
            return AltMode::AT_OR_ABOVE;
        case '-':
            return AltMode::AT_OR_BELOW;
        case 'B':
            return AltMode::WITHIN;
        case 'C':
            return AltMode::SID_AT_OR_ABOVE;
        case 'G':
            return AltMode::GS_AT;
        case 'H':
            return AltMode::GS_AT_OR_ABOVE;
        case 'I':
            return AltMode::GS_INTC_AT;
        case 'J':
            return AltMode::GS_INTC_AT_OR_ABOVE;
        case 'V':
            return AltMode::ALT_STEPDOWN_AT_AT_OR_ABOVE;
        case 'X':
            return AltMode::ALT_STEPDOWN_AT_AT;
        case 'Y':
            return AltMode::ALT_STEPDOWN_AT_AT_OR_BELOW;
        default:
            return AltMode::AT;
        }
    }

    int str2alt(std::string s)
    {
        if(s.length() == 5)
        {
            if(s[0] == 'F' && s[1] == 'L')
            {
                std::string num_part = s.substr(2, 3);
                if(strutils::is_numeric(num_part))
                {
                    return stoi(num_part) * 100;
                }
            }
            else
            {
                if(strutils::is_numeric(s))
                {
                    return stoi(s);
                }
            }
        }

        return 0;
    }

    SpeedMode char2spd_mode(char c)
    {
        switch(c)
        {
        case '+':
            return SpeedMode::AT_OR_ABOVE;
        case '-':
            return SpeedMode::AT_OR_BELOW;
        default:
            return SpeedMode::AT;
        }
    }

    TCHType char2tch_type(char c)
    {
        switch (c)
        {
        case 'I':
            return TCHType::ILS_MLS;
        case 'R':
            return TCHType::RNAV;
        case 'V':
            return TCHType::VGSI;
        case 'D':
            return TCHType::DEFAULT;
        default:
            return TCHType::NONE;
        }
    }

    LSCategory char2ls_category(char c)
    {
        switch(c)
        {
        case '0':
            return LSCategory::ILS_LOC_ONLY;
        case '1':
            return LSCategory::LS_CAT_I;
        case '2':
            return LSCategory::LS_CAT_II;
        case '3':
            return LSCategory::LS_CAT_III;
        case 'I':
            return LSCategory::IGS;
        case 'L':
            return LSCategory::LDA_GS;
        case 'A':
            return LSCategory::LDA_NO_GS;
        case 'S':
            return LSCategory::SDF_GS;
        case 'F':
            return LSCategory::SDF_NO_GS;
        default:
            return LSCategory::NONE;
        }
    }


    // arinc_fix_entry_t definitions:

    bool arinc_fix_entry_t::to_waypoint_t(std::string& area_code, 
        std::shared_ptr<ArptDB> arpt_db, std::shared_ptr<NavaidDB> navaid_db, 
        arinc_rwy_db_t& rwy_db, waypoint_t *out)
    {
        NavaidType lookup_type = NavaidType::NONE;
        std::string lookup_area = "ENRT";

        if(db_section == 'D')
        {
            if(db_subsection == 'B')
            {
                lookup_type = NavaidType::NDB;
            }
            else
            {
                lookup_type = NavaidType::VHF_NAVAID;
            }
            // Add section P
        }
        else if(db_section == 'P')
        {
            if(db_subsection == 'A')
            {
                airport_data_t apt_data;
                int ret = arpt_db->get_airport_data(area_code, &apt_data);
                if(ret)
                {
                    *out = {area_code, {NavaidType::APT, 0, apt_data.pos, 
                        area_code, country_code}};
                    return true;
                }
            }
            else if(db_subsection == 'G')
            {
                return get_rnw_wpt(rwy_db, fix_ident, area_code, country_code, out);
            }
            lookup_area = area_code;
        }
        else if(db_section != 'D' && db_section != 'E' && db_section != 'P')
        {
            return {};
        }
        
        std::vector<waypoint_entry_t> wpts;
        navaid_db->get_wpt_data(fix_ident, &wpts, lookup_area, country_code, lookup_type);

        //if(wpts.size() > 1)
        //{
        //    std::cout << fix_ident << " " << country_code << " " << 
        //        area_code << " " << db_section << " " << db_subsection << "\n";
        //}
        //assert(wpts.size() < 2);

        if(wpts.size())
        {
            *out = {fix_ident, wpts[0]};
            return true;
        }

        return false;
    }

    // arinc_leg_t definitions:

    double arinc_leg_t::get_mag_var_deg()
    {
        if(has_recd_navaid)
        {
            if(theta != 0)
            {
                return theta - tru_theta;
            }
            else if(recd_navaid.data.navaid)
            {
                return recd_navaid.data.navaid->mag_var;
            }
        }

        return 0;
    }

    void arinc_leg_t::set_main_fix(waypoint_t fix)
    {
        main_fix = fix;
        has_main_fix = true;
    }

    void arinc_leg_t::set_recd_fix(waypoint_t fix)
    {
        assert(fix.data.navaid != nullptr);
        recd_navaid = fix;
        has_recd_navaid = true;
    }

    void arinc_leg_t::set_ctr_fix(waypoint_t fix)
    {
        center_fix = fix;
        has_center_fix = true;
    }

    // arinc_str_t definitions:

    arinc_str_t::arinc_str_t(std::vector<std::string>& in_split)
    {
        rt_type = in_split[1][0];
        

        main_fix.fix_ident = in_split[4];
        main_fix.country_code = in_split[5];
        main_fix.db_section = in_split[6][0];
        main_fix.db_subsection = in_split[7][0];
        wpt_desc = in_split[8];

        turn_dir = in_split[9][0];
        rnp = strutils::strip(in_split[10], ' ');
        leg_type = strutils::strip(in_split[11], ' ');
        tdv = in_split[12][0];

        recd_navaid.fix_ident = strutils::strip(in_split[13], ' ');
        recd_navaid.country_code = strutils::strip(in_split[14], ' ');
        recd_navaid.db_section = in_split[15][0];
        recd_navaid.db_subsection = in_split[16][0];

        arc_radius = double(strutils::stof_with_strip(in_split[17]));
        theta = double(strutils::stof_with_strip(in_split[18]));
        rho = double(strutils::stof_with_strip(in_split[19]));
        outbd_mag_crs = strutils::strip(in_split[20], ' ');
        outbd_dist_time = strutils::strip(in_split[21], ' ');

        alt_desc = in_split[22][0];
        alt1 = strutils::strip(in_split[23], ' ');
        alt2 = strutils::strip(in_split[24], ' ');
        trans_alt = strutils::stoi_with_strip(in_split[25]);

        speed_desc = in_split[26][0];
        spd_lim = strutils::stoi_with_strip(in_split[27]);
        vert_angle = double(strutils::stof_with_strip(in_split[28]) * 0.01);
        vert_scale = strutils::stoi_with_strip(in_split[29]);

        center_fix.fix_ident = strutils::strip(in_split[30], ' ');
        center_fix.country_code = strutils::strip(in_split[31], ' ');
        center_fix.db_section = in_split[32][0];
        center_fix.db_subsection = in_split[33][0];

        multi_cod = in_split[34][0];
        gnss_ind = in_split[35][0];
        rt_qual1 = in_split[36][0];
        rt_qual2 = in_split[37][0];
    }

    // arinc_leg_t definitions:

    arinc_leg_t arinc_str_t::get_leg(std::string& area_code, airport_data_t& apt_data, 
        std::shared_ptr<ArptDB> arpt_db, std::shared_ptr<NavaidDB> navaid_db, 
        arinc_rwy_db_t& rwy_db)
    {
        arinc_leg_t out;
        out.rt_type = rt_type;

        out.has_main_fix = main_fix.to_waypoint_t(area_code, arpt_db, navaid_db, 
            rwy_db, &out.main_fix);

        out.wpt_desc = wpt_desc;
        out.turn_dir = char2dir(turn_dir);
        out.rnp = str2rnp(rnp);
        out.leg_type = leg_type;
        out.is_ovfy = tdv == 'Y';

        out.has_recd_navaid = recd_navaid.to_waypoint_t(area_code, arpt_db, navaid_db, 
            rwy_db, &out.recd_navaid);
        out.arc_radius = arc_radius;
        out.theta = theta * 0.1;
        if(theta != 0 && out.has_recd_navaid && out.has_main_fix)
        {
            out.tru_theta = out.recd_navaid.data.pos.get_gc_bearing_rad(
                out.main_fix.data.pos) * geo::RAD_TO_DEG;
            
            if(out.tru_theta < 0)
            {
                out.tru_theta += 360;
            }
        }
        out.rho = rho;
        out.outbd_crs_deg = str2outbd_crs(outbd_mag_crs, &out.outbd_crs_true);
        out.outbd_dist_time = str2outbd_dist(outbd_dist_time, &out.outbd_dist_as_time);

        out.alt_desc = char2alt_mode(alt_desc);
        out.alt1_ft = str2alt(alt1);
        out.alt2_ft = str2alt(alt2);
        out.trans_alt = trans_alt;
        if(trans_alt == 0)
            out.trans_alt = int(apt_data.transition_alt_ft);

        out.speed_desc = char2spd_mode(speed_desc);
        out.spd_lim_kias = spd_lim;
        out.vert_angle_deg = vert_angle;
        out.vert_scale_ft = vert_scale;

        out.has_center_fix = center_fix.to_waypoint_t(area_code, arpt_db, navaid_db, 
            rwy_db, &out.center_fix);

        out.multi_cod = multi_cod;
        out.gnss_ind = gnss_ind;
        out.rt_qual1 = rt_qual1;
        out.rt_qual2 = rt_qual2;

        return out;
    }

    // arinc_rwy_full_t definitions:

    int arinc_rwy_full_t::get_pos_from_db(std::string& area_code, 
        std::shared_ptr<ArptDB> arpt_db)
    {
        runway_entry_t rnw;
        int ret = arpt_db->get_rnw_data(area_code, id, &rnw);

        if(ret)
        {
            data.pos = rnw.start;
            data.thresh_displ_ft = int(double(rnw.displ_threshold_m) * geo::M_TO_FT);
        }

        return ret;
    }

    void arinc_rwy_full_t::get_rwy_coords(std::string& s, std::string& area_code, 
        std::shared_ptr<ArptDB> arpt_db)
    {
        std::vector<std::string> second_part_splt = strutils::str_split(
            s, ARINC_FIELD_SEP);
        
        if(second_part_splt.size() == N_ARINC_RWY_COL_SECOND)
        {
            std::string lat_stripped = strutils::strip(second_part_splt[0], ' ');
            std::string lon_stripped = strutils::strip(second_part_splt[1], ' ');
            data.thresh_displ_ft = strutils::stoi_with_strip(second_part_splt[2]);

            data.pos.lat_rad = strutils::str_to_lat(lat_stripped) * geo::DEG_TO_RAD;
            data.pos.lon_rad = strutils::str_to_lon(lon_stripped) * geo::DEG_TO_RAD;

            if(data.pos.lat_rad == 0 || data.pos.lon_rad == 0)
            {
                if(!get_pos_from_db(area_code, arpt_db))
                {
                    err = DbErr::DATA_BASE_ERROR;
                }
            }
        }
        else
        {
            if(!get_pos_from_db(area_code, arpt_db))
            {
                err = DbErr::DATA_BASE_ERROR;
            }
        }
    }

    arinc_rwy_full_t::arinc_rwy_full_t(std::string& s, std::string& area_code, 
        std::shared_ptr<ArptDB> arpt_db)
    {
        /*
            Runway data is grouped into 2 parts separated by ';'. 
            The second(threshold location) part can be missing(e.g. HUEN).
            In that case we have to hope it's present in apt.dat.
        */
        err = DbErr::SUCCESS;

        std::vector<std::string> main_parts = strutils::str_split(s, ';');
        std::vector<std::string> first_part_splt = strutils::str_split(
            main_parts[0], ARINC_FIELD_SEP);
        std::vector<std::string> name_part = strutils::str_split(
            first_part_splt[0], ':');
        
        if(name_part.size() > 1 && first_part_splt.size() == N_ARINC_RWY_COL_FIRST)
        {
            std::string curr_id = strutils::strip(name_part[1], ' ');
            size_t id_length = curr_id.length();
            if(id_length > 2 && curr_id[0] == 'R' && 
                curr_id[1] == 'W' && isdigit(curr_id[2]))
            {
                id = strutils::normalize_rnw_id(curr_id.substr(2, id_length-2));
            }
            else
            {
                err = DbErr::DATA_BASE_ERROR;
                return;
            }

            data.grad_deg = float(strutils::stof_with_strip(first_part_splt[1]) * 0.001);
            data.ellips_height_m = float(strutils::stof_with_strip(first_part_splt[2]) * 0.1);
            data.thresh_elev_msl_ft = strutils::stoi_with_strip(first_part_splt[3]);

            data.tch_tp = char2tch_type(first_part_splt[4][0]);
            data.ls_ident = strutils::strip(first_part_splt[5], ' ');
            data.ls_cat = char2ls_category(first_part_splt[6][0]);
            data.tch_ft = strutils::stoi_with_strip(first_part_splt[7]);
        }
        else
        {
            err = DbErr::DATA_BASE_ERROR;
            return;
        }

        std::string pos_str = "";
        if(main_parts.size() == 2)
        {
            pos_str = main_parts[1];
        }

        get_rwy_coords(pos_str, area_code, arpt_db);
    }


    bool get_rnw_wpt(arinc_rwy_db_t& rwy_db, std::string& id, std::string& area_cd, 
        std::string& country_cd, waypoint_t *out)
    {
        std::string tmp = id;
        size_t tmp_length = tmp.length();
        if(tmp_length > 1 && tmp[0] == 'R' && tmp[1] == 'W')
        {
            tmp = tmp.substr(2, tmp_length-2);
        }

        tmp = strutils::normalize_rnw_id(tmp);
        
        if(rwy_db.find(tmp) != rwy_db.end())
        {
            out->data.pos = rwy_db[tmp].pos;
            out->id = tmp;
            out->data.type = NavaidType::RWY;
            out->data.area_code = area_cd;
            out->data.country_code = country_cd;

            return true;
        }
        return false;
    }

    std::vector<std::string> get_all_rwys_by_mask(std::string mask, 
        arinc_rwy_db_t& rwy_db)
    {
        if(mask == "ALL")
        {
            std::vector<std::string> out;

            for(auto i: rwy_db)
            {
                out.push_back(i.first);
            }

            return out;
        }
        if(rwy_db.find(mask) != rwy_db.end())
        {
            return {mask};
        }
        else if(mask.back() == 'B')
        {
            std::string rnw_id = mask.substr(0, mask.length()-1);
            std::vector<std::string> out;

            for(auto i: rwy_db)
            {
                std::string curr_id = i.first.substr(0, i.first.length()-1);
                if(curr_id == rnw_id)
                {
                    out.push_back(i.first);
                }
            }

            return out;
        }
        return {};
    }

    std::string get_full_appr_nm(std::string nm, appr_pref_db_t& pref_db)
    {
        if(nm.size() > 1 && nm[1] >= '0' && nm[1] <= '9' && pref_db.find(nm[0]) != pref_db.end())
        {
            std::string nm_sub = nm.substr(1, nm.size()-1);
            return pref_db[nm[0]] + nm_sub;
        }
        else if(nm.size() > 1 && (nm[1] < '0' || nm[1] > '9'))
        {
            return nm;
        }
        return "";
    }

    // Airport class definitions

    // public member functions:

    Airport::Airport(std::string icao, std::shared_ptr<ArptDB> arpt_db, 
        std::shared_ptr<NavaidDB> navaid_db, std::string cifp_path,
        std::string postfix, bool use_pr, appr_pref_db_t pr_db, arinc_leg_t* leg_ptr)
    {
        use_appch_prefix = use_pr;
        appch_prefix_db = pr_db;

        icao_code = icao;
        err_code = DbErr::ERR_NONE;

        arpt_db->get_airport_data(icao_code, &apt_data);

        self_alloc = false;
        if(leg_ptr == nullptr)
        {
            arinc_legs = new arinc_leg_t[N_FLT_LEG_CACHE_SZ];
            self_alloc = true;
        }
        else
        {
            arinc_legs = leg_ptr;
        }
        
        n_arinc_legs_used = 0;

        if(arinc_legs == nullptr)
        {
            err_code = DbErr::BAD_ALLOC;
        }
        else
        {
            err_code = load_db(arpt_db, navaid_db, cifp_path, postfix);
        }
    }

    Airport::Airport(Airport& copy, arinc_leg_t* leg_ptr): appch_prefix_db(), rwy_db(), 
        sid_db(), star_db(), appch_db(), sid_per_rwy(), star_per_rwy()
    {
        assert(flt_leg_strings.size() == 0); // Make sure the other airport isn't being updated
        
        err_code = copy.err_code;
        icao_code = copy.icao_code;

        apt_data = copy.apt_data;

        use_appch_prefix = copy.use_appch_prefix;
        appch_prefix_db = copy.appch_prefix_db;
        rwy_db = copy.rwy_db;
        n_arinc_legs_used = copy.n_arinc_legs_used;

        self_alloc = false;
        if(leg_ptr == nullptr)
        {
            arinc_legs = new arinc_leg_t[N_FLT_LEG_CACHE_SZ];
            self_alloc = true;
        }
        else
        {
            arinc_legs = leg_ptr;
        }

        for(int i = 0; i < n_arinc_legs_used; i++)
        {
            arinc_legs[i] = copy.arinc_legs[i];
        }

        sid_db = copy.sid_db;
        star_db = copy.star_db;
        appch_db = copy.appch_db;

        sid_per_rwy = copy.sid_per_rwy;
        star_per_rwy = copy.star_per_rwy;
    }

    std::vector<std::string> Airport::get_rwys()
    {
        std::vector<std::string> out;
        for(auto i: rwy_db)
        {
            out.push_back(i.first);
        }
        return out;
    }

    const arinc_rwy_db_t& Airport::get_rwy_db()
    {
        return rwy_db;
    }

    str_umap_t Airport::get_all_sids()
    {
        return get_all_proc(sid_db);
    }

    str_umap_t Airport::get_all_stars()
    {
        return get_all_proc(star_db);
    }

    str_umap_t Airport::get_all_appch()
    {
        return get_all_proc(appch_db);
    }

    arinc_leg_seq_t Airport::get_sid(std::string& proc_name, std::string& trans)
    {
        return get_proc(proc_name, trans, sid_db);
    }

    arinc_leg_seq_t Airport::get_star(std::string& proc_name, std::string& trans)
    {
        return get_proc(proc_name, trans, star_db);
    }

    arinc_leg_seq_t Airport::get_appch(std::string& proc_name, std::string& trans)
    {
        return get_proc(proc_name, trans, appch_db);
    }

    str_set_t Airport::get_sid_by_rwy(std::string& rwy_id)
    {
        return get_proc_by_rwy(rwy_id, sid_per_rwy);
    }

    str_set_t Airport::get_star_by_rwy(std::string& rwy_id)
    {
        return get_proc_by_rwy(rwy_id, star_per_rwy);
    }

    str_set_t Airport::get_rwy_by_sid(std::string& sid)
    {
        return get_trans_by_proc(sid, sid_db, true);
    }

    str_set_t Airport::get_rwy_by_star(std::string& star)
    {
        return get_trans_by_proc(star, star_db, true);
    }

    str_set_t Airport::get_trans_by_sid(std::string& sid)
    {
        return get_trans_by_proc(sid, sid_db);
    }

    str_set_t Airport::get_trans_by_star(std::string& star)
    {
        return get_trans_by_proc(star, star_db);
    }

    str_set_t Airport::get_trans_by_appch(std::string& appch)
    {
        return get_trans_by_proc(appch, appch_db);
    }

    Airport::~Airport()
    {
        if(arinc_legs != nullptr && self_alloc)
        {
            delete[] arinc_legs;
        }
    }

    // private member functions:

    str_umap_t Airport::get_all_proc(proc_db_t& db)
    {
        str_umap_t out;
        for(auto i: db)
        {
            for(auto j: i.second)
            {
                out[i.first].insert(j.first);
            }
        }

        return out;
    }

    arinc_leg_seq_t Airport::get_proc(std::string& proc_name, std::string& trans, 
        proc_db_t& db)
    {
        if(db.find(proc_name) != db.end())
        {
            if(db[proc_name].find(trans) != db[proc_name].end())
            {
                arinc_leg_seq_t proc_legs;

                for(size_t i = 0; i < db[proc_name][trans].size(); i++)
                {
                    int leg_idx = db[proc_name][trans][i];
                    proc_legs.push_back(arinc_legs[leg_idx]);
                }

                return proc_legs;
            }
        }
        return {};
    }

    str_set_t Airport::get_proc_by_rwy(std::string& rwy_id, 
        str_umap_t& umap)
    {
        if(umap.find(rwy_id) != umap.end())
        {
            // Case: runway was found
            return umap[rwy_id];
        }

        return {};
    }

    str_set_t Airport::get_trans_by_proc(std::string& proc_name, 
        proc_db_t db, bool rwy)
    {
        str_set_t out;

        if(db.find(proc_name) != db.end())
        {
            for(auto i: db[proc_name])
            {
                if(rwy_db.find(i.first) != rwy_db.end() && rwy)
                {
                    out.insert(i.first);
                }
                else if(rwy_db.find(i.first) == rwy_db.end() && !rwy)
                {
                    out.insert(i.first);
                }
            }
        }

        return out;
    }

    DbErr Airport::parse_flt_legs(std::shared_ptr<ArptDB> arpt_db, 
        std::shared_ptr<NavaidDB> navaid_db)
    {
        DbErr out = DbErr::SUCCESS;
        while(flt_leg_strings.size())
        {
            proc_typed_str_t curr = flt_leg_strings.front();
            flt_leg_strings.pop();

            if(curr.second != ProcType::PRDAT)
            {
                std::vector<std::string> s_split = strutils::str_split(curr.first, 
                    ARINC_FIELD_SEP);

                if(s_split.size() == N_ARINC_FLT_PROC_COL)
                {
                    std::string proc_name = strutils::strip(s_split[2], ' ');
                    std::string trans_name = strutils::strip(s_split[3], ' ');

                    if(trans_name == "")
                        trans_name = "NONE";

                    arinc_str_t arnc_str(s_split);
                    arinc_leg_t leg = arnc_str.get_leg(icao_code, apt_data, arpt_db, 
                        navaid_db, rwy_db);

                    if(n_arinc_legs_used == N_FLT_LEG_CACHE_SZ)
                    {
                        return DbErr::BAD_ALLOC;
                    }
                    arinc_legs[n_arinc_legs_used] = leg;

                    std::string rnw_trans = strutils::get_rnw_id(trans_name);
                    std::vector<std::string> rwys = get_all_rwys_by_mask(
                        rnw_trans, rwy_db);
                    bool is_rwy = true;

                    if(rwys.size() == 0)
                    {
                        rwys.push_back(trans_name);
                        is_rwy = false;
                    }
                    
                    for(auto i: rwys)
                    {
                        if(curr.second == ProcType::SID)
                        {
                            if(is_rwy)
                            {
                                sid_per_rwy[i].insert(proc_name);
                            }
                            sid_db[proc_name][i].push_back(
                                n_arinc_legs_used);
                        }
                        else if(curr.second == ProcType::STAR)
                        {
                            if(is_rwy)
                            {
                                star_per_rwy[i].insert(proc_name);
                            }
                            star_db[proc_name][i].push_back(
                                n_arinc_legs_used);
                        }
                        else
                        {
                            std::string appr_nm = proc_name;
                            if(use_appch_prefix)
                            {
                                appr_nm = get_full_appr_nm(appr_nm, appch_prefix_db);
                            }
                            if(appr_nm != "")
                            {
                                appch_db[appr_nm][i].push_back(
                                    n_arinc_legs_used);
                            }
                        }   
                    }

                    n_arinc_legs_used++;
                }
                else
                {
                    out = DbErr::PARTIAL_LOAD;
                }
            }
        }

        return out;
    }

    DbErr Airport::load_db(std::shared_ptr<ArptDB> arpt_db, 
        std::shared_ptr<NavaidDB> navaid_db, std::string& path,
        std::string& postfix)
    {
        std::string full_path = path + "/" + icao_code + postfix;

        std::ifstream file(full_path);
		if (file.is_open())
        {
            // Let the fun begin
            std::string line;
			while (getline(file, line))
            {
                ProcType curr_tp = str2proc_type(line);

                if(curr_tp != ProcType::RWY)
                {
                    proc_typed_str_t tmp = std::make_pair(line, curr_tp);
                    flt_leg_strings.push(tmp);
                }
                else
                {
                    arinc_rwy_full_t rwy(line, icao_code, arpt_db);

                    if(rwy.err != DbErr::SUCCESS)
                    {
                        return DbErr::DATA_BASE_ERROR;
                    }
                    rwy_db[rwy.id] = rwy.data;
                }
            }
            file.close();
            return parse_flt_legs(arpt_db, navaid_db);
        }
        else
        {
            return DbErr::FILE_NOT_FOUND;
        }
    }
}; // namespace libnav
