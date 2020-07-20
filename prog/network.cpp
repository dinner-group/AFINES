#include "filament_ensemble.h"
#include "motor_ensemble.h"
#include "globals.h"
#include "generate.h"

#include <iostream>
#include <fstream>
#include <iterator>
#include <array>
#include <boost/program_options.hpp>
#include <boost/any.hpp>
#include <typeinfo>

namespace po = boost::program_options;

template<class T>
ostream& operator<<(ostream& os, const vector<T>& v)
{
    copy(v.begin(), v.end(), ostream_iterator<T>(os, " "));
    return os;
}

ostream &operator<<(ostream &os, const virial_type &a)
{
    os << a.xx << "\t" << a.xy << "\t" << a.yx << "\t" << a.yy;
    return os;
}

int main(int argc, char **argv)
{
    // BEGIN PROGRAM OPTIONS

    // options allowed only on the command line
    string config_file;
    po::options_description generic("Command Line Only Options");
    generic.add_options()
        ("version, v", "print version string")
        ("help, h", "produce help message")
        ("config, c", po::value<string>(&config_file)->default_value("config/network.cfg"), "name of a configuration file")
        ;

    // environment

    string bnd_cnd;
    double xrange, yrange;

    double dt, tinit, tfinal;
    int nframes, nmsgs;

    double viscosity, temperature;

    string dir;
    int myseed;

    bool restart;
    double restart_time;
    double restart_strain;
    bool steven_continuation_flag;
    int continuation_fr;

    double grid_factor;
    bool quad_off_flag;
    int quad_update_period;
    bool check_dup_in_quad;

    bool use_attach_opt;

    bool circle_flag; double circle_radius, circle_spring_constant;

    po::options_description config_environment("Environment Options");
    config_environment.add_options()
        ("bnd_cnd,bc", po::value<string>(&bnd_cnd)->default_value("PERIODIC"), "boundary conditions")
        ("xrange", po::value<double>(&xrange)->default_value(10), "size of cell in horizontal direction (um)")
        ("yrange", po::value<double>(&yrange)->default_value(10), "size of cell in vertical direction (um)")

        ("dt", po::value<double>(&dt)->default_value(0.0001), "length of individual timestep in seconds")
        ("tinit", po::value<double>(&tinit)->default_value(0), "time that recording of simulation starts")
        ("tfinal", po::value<double>(&tfinal)->default_value(0.01), "length of simulation in seconds")
        ("nframes", po::value<int>(&nframes)->default_value(1000), "number of times between actin/link/motor positions to are printed to file")
        ("nmsgs", po::value<int>(&nmsgs)->default_value(10000), "number of times simulation progress is printed to stdout")

        ("viscosity", po::value<double>(&viscosity)->default_value(0.001), "Dynamic viscosity to determine friction [mg / (um*s)]. At 20 C, is 0.001 for water")
        ("temperature,temp", po::value<double>(&temperature)->default_value(0.004), "Temp in kT [pN-um] that effects magnituded of Brownian component of simulation")

        ("dir", po::value<string>(&dir)->default_value("."), "output directory")
        ("myseed", po::value<int>(&myseed)->default_value(time(NULL)), "Random number generator myseed")

        // restarts
        ("restart", po::value<bool>(&restart)->default_value(false), "if true, will restart simulation from last timestep recorded")
        ("restart_time", po::value<double>(&restart_time)->default_value(-1), "time to restart simulation from")
        ("restart_strain", po::value<double>(&restart_strain)->default_value(0),"the starting strain for restarting simulation")
        ("steven_continuation_flag", po::value<bool>(&steven_continuation_flag)->default_value(false), "flag to continue from last strain")
        ("continuation_fr", po::value<int>(&continuation_fr)->default_value(0),"the last saved frame from the simulation to be continued")

        // quadrants
        ("grid_factor", po::value<double>(&grid_factor)->default_value(2), "number of grid boxes per um^2")
        ("quad_off_flag", po::value<bool>(&quad_off_flag)->default_value(false), "flag to turn off neighbor list updating")
        ("quad_update_period", po::value<int>(&quad_update_period)->default_value(1), "number of timesteps between actin/link/motor position updates to update quadrants")
        ("check_dup_in_quad", po::value<bool>(&check_dup_in_quad)->default_value(true), "flag to check for duplicates in quadrants")

        ("use_attach_opt", po::value<bool>(&use_attach_opt)->default_value(false), "flag to use optimized attachment point search")

        // circular confinement
        ("circle_flag", po::value<bool>(&circle_flag)->default_value(false), "flag to add a circular wall")
        ("circle_radius", po::value<double>(&circle_radius)->default_value(INFINITY), "radius of circular wall")
        ("circle_spring_constant", po::value<double>(&circle_spring_constant)->default_value(0.0), "spring constant of circular wall")
        ;

    // filaments

    string actin_in;
    int npolymer, nmonomer, nmonomer_extra; double extra_bead_prob;

    double actin_length;
    string actin_pos_str;

    double link_length, polymer_bending_modulus, link_stretching_stiffness, fene_pct, fracture_force;
    double rmax, kexv;
    double kgrow, lgrow, l0min, l0max; int nlink_max;

    po::options_description config_actin("Filament Options");
    config_actin.add_options()
        ("actin_in", po::value<string>(&actin_in)->default_value(""), "input actin positions file")
        ("npolymer", po::value<int>(&npolymer)->default_value(3), "number of polymers in the network")
        ("nmonomer", po::value<int>(&nmonomer)->default_value(11), "number of monomers per filament (if extra_bead_prob != 0, then minimum #)")
        ("nmonomer_extra", po::value<int>(&nmonomer_extra)->default_value(0), "max # of monomers per filament")
        ("extra_bead_prob", po::value<double>(&extra_bead_prob)->default_value(0.5), "probability of adding an extra bead from nmonomer...nmonomer_extra")

        ("actin_length", po::value<double>(&actin_length)->default_value(0.5), "Length of a single actin monomer")
        ("actin_pos_str", po::value<string> (&actin_pos_str)->default_value(""), "Starting positions of actin polymers, commas delimit coordinates; semicolons delimit positions")

        ("link_length", po::value<double>(&link_length)->default_value(1), "Length of links connecting monomers")
        ("polymer_bending_modulus", po::value<double>(&polymer_bending_modulus)->default_value(0.068), "Bending modulus of a filament")
        ("fracture_force", po::value<double>(&fracture_force)->default_value(100000000), "pN-- filament breaking point")
        ("link_stretching_stiffness,ks", po::value<double>(&link_stretching_stiffness)->default_value(1), "stiffness of link, pN/um")
        ("fene_pct", po::value<double>(&fene_pct)->default_value(0.5), "pct of rest length of filament to allow outstretched until fene blowup")

        // excluded volume
        ("rmax", po::value<double>(&rmax)->default_value(0.25), "cutoff distance for interactions between actins beads and filaments")
        ("kexv", po::value<double>(&kexv)->default_value(1.0), "parameter of exv force calculation")

        // filament growth
        ("kgrow", po::value<double>(&kgrow)->default_value(0), "rate of filament growth")
        ("lgrow", po::value<double>(&lgrow)->default_value(0), "additional length of filament upon growth")
        ("l0min", po::value<double>(&l0min)->default_value(0), "minimum length a link can shrink to before disappearing")
        ("l0max", po::value<double>(&l0max)->default_value(2), "maximum length a link can grow to before breaking into two links")
        ("nlink_max", po::value<int>(&nlink_max)->default_value(25), "maximum number of links allowed on filament")
        ;

    // motors

    string a_motor_in;
    double a_motor_density;
    string a_motor_pos_str;

    bool motor_intersect_flag;
    double a_linkage_prob;

    double a_m_kon, a_m_kend, a_m_koff, a_m_cut;
    double a_motor_length, a_motor_stiffness;
    double a_motor_v, a_m_stall;

    bool dead_head_flag; int dead_head;

    po::options_description config_motors("Motor Options");
    config_motors.add_options()
        ("a_motor_in", po::value<string>(&a_motor_in)->default_value(""), "input motor positions file")
        ("a_motor_density", po::value<double>(&a_motor_density)->default_value(0.05), "number of active motors / um^2")
        ("a_motor_pos_str", po::value<string> (&a_motor_pos_str)->default_value(""), "Starting positions of motors, commas delimit coordinates; semicolons delimit positions")

        ("motor_intersect_flag", po::value<bool>(&motor_intersect_flag)->default_value(false), "flag to put a motor at all filament intersections")
        ("a_linkage_prob", po::value<double>(&a_linkage_prob)->default_value(1), "If motor_intersect_flag, probability that two filaments that intersect will be motor-d")

        // binding/unbinding
        ("a_m_kon", po::value<double>(&a_m_kon)->default_value(1),"active motor on rate")
        ("a_m_koff", po::value<double>(&a_m_koff)->default_value(0.1),"active motor off rate")
        ("a_m_kend", po::value<double>(&a_m_kend)->default_value(0.1),"active motor off rate at filament end")
        ("a_m_cut", po::value<double>(&a_m_cut)->default_value(0.063),"cutoff distance for binding (um)")

        ("a_motor_length", po::value<double>(&a_motor_length)->default_value(0.4),"active motor rest length (um)")
        ("a_motor_stiffness", po::value<double>(&a_motor_stiffness)->default_value(1),"active motor spring stiffness (pN/um)")

        // walking
        ("a_motor_v", po::value<double>(&a_motor_v)->default_value(1),"active motor velocity (um/s)")
        ("a_m_stall", po::value<double>(&a_m_stall)->default_value(0.5),"force beyond which motors don't walk (pN)")

        ("dead_head_flag", po::value<bool>(&dead_head_flag)->default_value(false), "flag to kill head <dead_head> of all motors")
        ("dead_head", po::value<int>(&dead_head)->default_value(0), "index of head to kill")
        ;

    // crosslinkers

    string p_motor_in;
    double p_motor_density;
    string p_motor_pos_str;

    bool link_intersect_flag; double p_linkage_prob;

    double p_m_kon, p_m_kend, p_m_koff, p_m_cut;
    double p_motor_length, p_motor_stiffness;
    double p_motor_v, p_m_stall;

    bool p_dead_head_flag; int p_dead_head;
    bool static_cl_flag;

    po::options_description config_crosslinks("Crosslinker Options");
    config_crosslinks.add_options()
        ("p_motor_in", po::value<string>(&p_motor_in)->default_value(""), "input crosslinker positions file")
        ("p_motor_density", po::value<double>(&p_motor_density)->default_value(0.05), "number of passive motors / um^2")
        ("p_motor_pos_str", po::value<string> (&p_motor_pos_str)->default_value(""), "Starting positions of crosslinks, commas delimit coordinates; semicolons delimit positions")

        ("link_intersect_flag", po::value<bool>(&link_intersect_flag)->default_value(false), "flag to put a cross link at all filament intersections")
        ("p_linkage_prob", po::value<double>(&p_linkage_prob)->default_value(1), "If link_intersect_flag, probability that two filaments that intersect will be linked")

        // binding/unbinding
        ("p_m_kon", po::value<double>(&p_m_kon)->default_value(1),"passive motor on rate")
        ("p_m_koff", po::value<double>(&p_m_koff)->default_value(0.1),"passive motor off rate")
        ("p_m_kend", po::value<double>(&p_m_kend)->default_value(0.1),"passive motor off rate at filament end")
        ("p_m_cut", po::value<double>(&p_m_cut)->default_value(0.063),"cutoff distance for binding (um)")

        ("p_motor_length", po::value<double>(&p_motor_length)->default_value(0.150),"passive motor rest length (um) (default: filamin)")
        ("p_motor_stiffness", po::value<double>(&p_motor_stiffness)->default_value(1),"passive motor spring stiffness (pN/um)")

        // walking
        ("p_motor_v", po::value<double>(&p_motor_v)->default_value(0),"passive motor velocity (um/s)")
        ("p_m_stall", po::value<double>(&p_m_stall)->default_value(0),"force beyond which xlinks don't walk (pN)")

        ("p_dead_head_flag", po::value<bool>(&p_dead_head_flag)->default_value(false), "flag to kill head <dead_head> of all crosslinks")
        ("p_dead_head", po::value<int>(&p_dead_head)->default_value(0), "index of head to kill")

        ("static_cl_flag", po::value<bool>(&static_cl_flag)->default_value(false), "flag to indicate compeletely static xlinks; i.e, no walking, no detachment")
        ;

    int n_bw_shear;

    double d_strain_freq, d_strain_pct;
    double time_of_dstrain, time_of_dstrain2;

    bool diff_strain_flag, osc_strain_flag;
    bool stress_flag; double stress1, stress_rate1, stress2, stress_rate2;

    bool shear_motor_flag;

    po::options_description config_shear("Shear Options");
    config_shear.add_options()
        ("n_bw_shear", po::value<int>(&n_bw_shear)->default_value(1000000000), "number of timesteps between subsequent shears")

        ("d_strain_freq", po::value<double>(&d_strain_freq)->default_value(1), "differential strain frequency")
        ("d_strain_pct", po::value<double>(&d_strain_pct)->default_value(0), "differential strain amplitude")
        ("time_of_dstrain", po::value<double>(&time_of_dstrain)->default_value(10000), "time when differential strain starts")
        ("time_of_dstrain2", po::value<double>(&time_of_dstrain2)->default_value(10000), "time when second differential strain starts")

        ("diff_strain_flag", po::value<bool>(&diff_strain_flag)->default_value(false), "flag to turn on linear differential strain")
        ("osc_strain_flag", po::value<bool>(&osc_strain_flag)->default_value(false), "flag to turn on oscillatory differential strain")

        // stress
        ("stress_flag", po::value<bool>(&stress_flag)->default_value(false), "flag to turn on constant stress")
        ("stress", po::value<double>(&stress1)->default_value(0.0), "value of constant stress")
        ("stress_rate", po::value<double>(&stress_rate1)->default_value(0.0), "decay rate to specified value of stress, in weird units")
        ("stress2", po::value<double>(&stress2)->default_value(0.0), "second value of constant stress")
        ("stress_rate2", po::value<double>(&stress_rate2)->default_value(0.0), "second decay rate to specified value of stress, in weird units")

        ("shear_motor_flag", po::value<bool>(&shear_motor_flag)->default_value(false), "flag to turn on shearing for motors")
        ;

    po::options_description config;
    config.add(config_environment).add(config_actin).add(config_motors).add(config_crosslinks).add(config_shear);

    po::options_description cmdline_options;
    cmdline_options.add(generic).add(config);

    po::variables_map vm;
    po::store(po::parse_command_line(argc, argv, cmdline_options), vm);
    po::notify(vm);

    if (vm.count("help")) {
        std::cout << generic << "\n";
        std::cout << config << "\n";
        return 1;
    }

    ifstream ifs(config_file);
    if (!ifs){
        cout<<"can not open config file: "<<config_file<<"\n";
        return 0;
    } else {
        po::store(po::parse_config_file(ifs, config), vm);
        po::notify(vm);
    }

    // Write the full configuration file
    ofstream o_file(dir + "/data/config_full.cfg");
    for (auto const &it : vm) {
        if (it.first == "config") continue;
        boost::any val = it.second.value();

        if(typeid(bool) == val.type())
            o_file << it.first <<"="<< boost::any_cast<bool>(val) <<endl;
        else if(typeid(int) == val.type())
            o_file << it.first <<"="<< boost::any_cast<int>(val) <<endl;
        else if(typeid(double) == val.type())
            o_file << it.first <<"="<< boost::any_cast<double>(val) <<endl;
        else if(typeid(string) == val.type())
            o_file << it.first <<"="<< boost::any_cast<string>(val) <<endl;
    }

    // END PROGRAM OPTIONS

    int n_bw_stdout = max(int((tfinal)/(dt*double(nmsgs))),1);
    int n_bw_print  = max(int((tfinal)/(dt*double(nframes))),1);
    int unprinted_count = int(double(tinit)/dt);

    string tdir   = dir  + "/txt_stack";
    string ddir   = dir  + "/data";
    fs::path dir1(tdir), dir2(ddir);

    string afile  = tdir + "/actins.txt";
    string lfile  = tdir + "/links.txt";
    string amfile = tdir + "/amotors.txt";
    string pmfile = tdir + "/pmotors.txt";
    string thfile = ddir + "/filament_e.txt";
    string pefile = ddir + "/pe.txt";
    string kefile = ddir + "/ke.txt";

    if(fs::create_directory(dir1)) cerr<< "Directory Created: "<<afile<<std::endl;
    if(fs::create_directory(dir2)) cerr<< "Directory Created: "<<thfile<<std::endl;

    vector<vector<double>> actin_pos_vec;
    vector<vector<double>> a_motor_pos_vec, p_motor_pos_vec;

    // BEGIN READ CONFIGURATIONS

    // To get positions from input files:
    if (actin_in.size() > 0)
        actin_pos_vec   = file2vecvec(actin_in, "\t");
    if (a_motor_in.size() > 0)
        a_motor_pos_vec = file2vecvec(a_motor_in, "\t");
    if (p_motor_in.size() > 0)
        p_motor_pos_vec = file2vecvec(p_motor_in, "\t");

    // To restart a whole trajectory from it's last full timestep :
    if (restart){

        double tf_prev  = min(last_full_timestep(afile), last_full_timestep(lfile));
        if (a_motor_density > 0)
            tf_prev = min(tf_prev, last_full_timestep(amfile));
        if (p_motor_density > 0)
            tf_prev = min(tf_prev, last_full_timestep(pmfile));

        if (restart_time == -1 || restart_time > tf_prev)
            restart_time = tf_prev;

        cout<<"\nRestarting from t = "<<restart_time<<endl;

        double nprinted = restart_time / (dt*n_bw_print);

        actin_pos_vec   = traj2vecvec(afile, "\t ", restart_time);
        a_motor_pos_vec = traj2vecvec(amfile, "\t ", restart_time);
        p_motor_pos_vec = traj2vecvec(pmfile, "\t ", restart_time);

        // for actins, links, amotors, pmotors:
        // do:
        //   copy whole file into temp
        //   while hasn't reached tf in temp file:
        //      write from copy into afile
        write_first_tsteps(afile,  restart_time);
        write_first_tsteps(lfile,  restart_time);
        write_first_tsteps(amfile, restart_time);
        write_first_tsteps(pmfile, restart_time);

        write_first_tsteps(thfile, restart_time);
        write_first_nlines(pefile, (int) nprinted);
        write_first_nlines(kefile, (int) nprinted);

        // additional outputs below

        tinit = restart_time;

        if (steven_continuation_flag) {
            restart_strain = get_restart_strain(pefile, continuation_fr);
        }

    }

    // END READ CONFIGURATIONS

    // compute derived quantities

    if (polymer_bending_modulus < 0){ //This is a flag for using the temperature for the bending modulus
        polymer_bending_modulus = 10*temperature; // 10um * kT
    }

    double actin_density = double(npolymer*nmonomer)/(xrange*yrange);//0.65;
    cout<<"\nDEBUG: actin_density = "<<actin_density;
    double link_bending_stiffness    = polymer_bending_modulus / link_length;

    // set number of quadrants to 1 if there are no crosslinkers/motors
    int xgrid, ygrid;
    if(a_motor_density == 0 && a_motor_pos_vec.size() == 0 &&
            p_motor_density==0 && p_motor_pos_vec.size() == 0 &&
            !link_intersect_flag && !motor_intersect_flag){
        xgrid = 1;
        ygrid = 1;
    }
    else{
        xgrid  = (int) round(grid_factor*xrange);
        ygrid  = (int) round(grid_factor*yrange);
    }
    double d_strain_amp = d_strain_pct * xrange;

    box *bc = new box(bnd_cnd, xrange, yrange, restart_strain);

    set_seed(myseed);

    // BEGIN GENERATE CONFIGURATIONS

    if (actin_pos_vec.size() == 0 && actin_in.size() == 0) {
        vector<array<double, 3>> actin_position_arrs;
        if (actin_pos_str.size() > 0) {
            // read positions from input strings in config file
            actin_position_arrs = str2arrvec(actin_pos_str, ":", ",");
        }
        actin_pos_vec = generate_filament_ensemble(
                bc, npolymer, nmonomer, nmonomer_extra, extra_bead_prob,
                dt, temperature, actin_length, link_length,
                actin_position_arrs, link_bending_stiffness, myseed);
    }

    if (link_intersect_flag)
        p_motor_pos_vec = spring_spring_intersections(bc, actin_pos_vec, p_motor_length, p_linkage_prob);

    if (motor_intersect_flag)
        a_motor_pos_vec = spring_spring_intersections(bc, actin_pos_vec, a_motor_length, a_linkage_prob);

    if (a_motor_pos_vec.size() == 0 && a_motor_in.size() == 0) {
        vector<array<double, 3>> a_motor_position_arrs;
        if (a_motor_pos_str.size() > 0) {
            a_motor_position_arrs = str2arrvec(a_motor_pos_str, ":", ",");
        }
        a_motor_pos_vec = generate_motor_ensemble(bc, a_motor_density, a_motor_length, a_motor_position_arrs);
    }

    if (p_motor_pos_vec.size() == 0 && p_motor_in.size() == 0) {
        vector<array<double, 3>> p_motor_position_arrs;
        if (p_motor_pos_str.size() > 0) {
            p_motor_position_arrs = str2arrvec(p_motor_pos_str, ":", ",");
        }
        p_motor_pos_vec = generate_motor_ensemble(bc, p_motor_density, p_motor_length, p_motor_position_arrs);
    }

    // END GENERATE CONFIGURATIONS

    // BEGIN CREATE NETWORK OBJECTS

    cout<<"\nCreating actin network..";
    filament_ensemble *net = new filament_ensemble(
            bc, actin_pos_vec, {xgrid, ygrid}, dt,
            temperature, viscosity, link_length,
            link_stretching_stiffness, fene_pct, link_bending_stiffness,
            fracture_force, rmax, kexv);

    // additional options
    net->set_growing(kgrow, lgrow, l0min, l0max, nlink_max);
    if (circle_flag) net->set_circle_wall(circle_radius, circle_spring_constant);
    if (quad_off_flag) net->get_quads()->use_quad(false);

    cout<<"\nAdding active motors...";
    xlink_ensemble *myosins = new xlink_ensemble(
            a_motor_pos_vec, dt, temperature,
            a_motor_length, net, a_motor_v, a_motor_stiffness, fene_pct, a_m_kon, a_m_koff,
            a_m_kend, a_m_stall, a_m_cut, viscosity);

    if (dead_head_flag) myosins->kill_heads(dead_head);
    if (shear_motor_flag) myosins->use_shear(true);
    if (use_attach_opt) myosins->use_attach_opt(true);

    cout<<"Adding passive motors (crosslinkers) ...\n";
    xlink_ensemble *crosslks = new xlink_ensemble(
            p_motor_pos_vec, dt, temperature,
            p_motor_length, net, p_motor_v, p_motor_stiffness, fene_pct, p_m_kon, p_m_koff,
            p_m_kend, p_m_stall, p_m_cut, viscosity);

    if (p_dead_head_flag) crosslks->kill_heads(p_dead_head);
    if (shear_motor_flag) crosslks->use_shear(true);
    if (use_attach_opt) crosslks->use_attach_opt(true);
    if (static_cl_flag) crosslks->use_static(true);

    // END CREATE NETWORK OBJECTS

    // run simulation

    cout<<"\nUpdating motors, filaments and crosslinks in the network..";
    string time_str;

    double prev_d_strain = restart_strain;

    // open output files
    // for restarts, append instead of writing from the start
    // files are automatically closed by RAII
    ios_base::openmode write_mode = (restart) ? ios_base::app : ios_base::out;
    ofstream file_a(afile, write_mode);
    ofstream file_l(lfile, write_mode);
    ofstream file_am(amfile, write_mode);
    ofstream file_pm(pmfile, write_mode);
    ofstream file_th(thfile, write_mode);
    ofstream file_pe(pefile, write_mode);
    ofstream file_ke(kefile, write_mode);

    int count; double t;
    for (count = 0, t = tinit; t <= tfinal; count++, tinit += dt) {

        /*
        //print to file
        if (t+dt/100 >= tinit && (count-unprinted_count)%n_bw_print==0) {

            if (t>tinit) time_str ="\n";
            time_str += "t = "+to_string(t);

            file_a << time_str<<"\tN = "<<to_string(net->get_nbeads());
            net->write_beads(file_a);

            file_l << time_str<<"\tN = "<<to_string(net->get_nsprings());
            net->write_springs(file_l);

            file_am << time_str<<"\tN = "<<to_string(myosins->get_nmotors());
            myosins->motor_write(file_am);

            file_pm << time_str<<"\tN = "<<to_string(crosslks->get_nmotors());
            crosslks->motor_write(file_pm);

            file_th << time_str<<"\tN = "<<to_string(net->get_nfilaments());
            net->write_thermo(file_th);

            file_pe <<net->get_stretching_energy()<<"\t"<<net->get_bending_energy()<<"\t"<<net->get_exv_energy()<<"\t"<<myosins->get_potential_energy()<<"\t"<<crosslks->get_potential_energy()<<endl;

            file_ke <<net->get_kinetic_energy_vir()<<"\t"<<myosins->get_kinetic_energy()<<"\t"<<crosslks->get_kinetic_energy()<<endl;

            file_a<<std::flush;
            file_l<<std::flush;
            file_am<<std::flush;
            file_pm<<std::flush;
            file_th<<std::flush;
            file_pe<<std::flush;
            file_ke<<std::flush;

        }

        //print time count
        if (time_of_strain!=0 && close(t, time_of_strain, dt/(10*time_of_strain))){
            //Perform the shear here
            cout<<"\nDEBUG: t = "<<t<<"; adding pre_strain of "<<pre_strain<<" um here";
            net->update_delrx( pre_strain );
            net->update_shear();
        }
        */

        if (t >= time_of_dstrain && count % n_bw_shear == 0) {
            double d_strain = 0.0;
            if (stress_flag) {
                double stress, stress_rate;
                if (t < time_of_dstrain2) {
                    stress = stress1;
                    stress_rate = stress_rate1;
                } else {
                    stress = stress2;
                    stress_rate = stress_rate2;
                }

                virial_type total_virial, virial;
                total_virial.zero();

                cout << "\nDEBUG: t = " << t;

                virial = net->get_stretching_virial();
                cout << "\nfilament stretch virial:\t" << virial;
                total_virial += virial;

                virial = net->get_bending_virial();
                cout << "\nfilament bend virial:\t" << virial;
                total_virial += virial;

                virial = myosins->get_virial();
                cout << "\nmotor stretch virial:\t" << virial;
                total_virial += virial;

                virial = crosslks->get_virial();
                cout << "\ncrosslinker stretch virial:\t" << virial;
                total_virial += virial;

                array<double, 2> fov = net->get_box()->get_fov();
                double area = fov[0] * fov[1];
                array<array<double, 2>, 2> stress_tensor;
                stress_tensor[0][0] = total_virial.xx / area;
                stress_tensor[0][1] = total_virial.xy / area;
                stress_tensor[1][0] = total_virial.yx / area;
                stress_tensor[1][1] = total_virial.yy / area;
                d_strain += prev_d_strain + stress_rate * (stress - stress_tensor[1][0]) * fov[1] * dt;
            }
            if (osc_strain_flag) {
                //d_strain += d_strain_amp * sin(2*pi*d_strain_freq * ( t - time_of_dstrain) );
                d_strain += restart_strain + d_strain_amp*4*d_strain_freq*((t-time_of_dstrain) - 1/(d_strain_freq*2)*floor(2*(t-time_of_dstrain)*d_strain_freq + 0.5))*pow(-1,floor(2*(t-time_of_dstrain)*d_strain_freq + 0.5));
            }
            if (diff_strain_flag) {
                d_strain += restart_strain + d_strain_amp*d_strain_freq*(t - time_of_dstrain);
            }
            bc->update_d_strain(d_strain - prev_d_strain);
            cout << "\nDEBUG: t = " << t << "; adding d_strain of " << (d_strain - prev_d_strain) << " um here; total strain = " << d_strain;
            prev_d_strain = d_strain;
        }

        if (count%n_bw_stdout==0) {
            cout<<"\nTime counts: "<<count;
            //net->print_filament_thermo();
            net->print_network_thermo();
            crosslks->print_ensemble_thermo();
            myosins->print_ensemble_thermo();
        }

        //print to file
        if (t+dt/100 >= tinit && (count-unprinted_count)%n_bw_print==0) {

            if (t>tinit) time_str ="\n";
            time_str += "t = "+to_string(t);

            file_a << time_str<<"\tN = "<<to_string(net->get_nbeads());
            net->write_beads(file_a);

            file_l << time_str<<"\tN = "<<to_string(net->get_nsprings());
            net->write_springs(file_l);

            file_am << time_str<<"\tN = "<<to_string(myosins->get_nmotors());
            myosins->motor_write(file_am);

            file_pm << time_str<<"\tN = "<<to_string(crosslks->get_nmotors());
            crosslks->motor_write(file_pm);

            file_th << time_str<<"\tN = "<<to_string(net->get_nfilaments());
            net->write_thermo(file_th);

            file_pe
                << net->get_stretching_energy() << "\t"
                << net->get_bending_energy() << "\t"
                << myosins->get_potential_energy() << "\t"
                << crosslks->get_potential_energy() << "\t"
                << prev_d_strain << "\t"
                << net->get_stretching_virial() << "\t"
                << net->get_bending_virial() << "\t"
                << myosins->get_virial() << "\t"
                << crosslks->get_virial() << endl;

            file_a<<std::flush;
            file_l<<std::flush;
            file_am<<std::flush;
            file_pm<<std::flush;
            file_th<<std::flush;
            file_pe<<std::flush;
        }

        //update network
        net->update();//updates all forces, velocities and positions of filaments

        if (quad_off_flag) {
            // we want results that are correct regardless of other settings when quadrants are off
            // this just builds a list of all springs, which are then handed to attachment/etc
            net->quad_update_serial();

        } else if (count % quad_update_period == 0) {
            // when quadrants are on, this actually builds quadrants
            net->quad_update_serial();

            if (check_dup_in_quad) {
                net->get_quads()->check_duplicates();
            }

        }

        //update cross linkers
        crosslks->motor_update();

        //update motors
        myosins->motor_update();

        //clear the vector of fractured filaments
        net->clear_broken();
    }

    file_a << "\n";
    file_l << "\n";
    file_am << "\n";
    file_pm << "\n";
    file_th << "\n";

    //Delete all objects created
    cout<<"\nHere's where I think I delete things\n";

    delete myosins;
    delete crosslks;
    delete net;
    delete bc;

    cout<<"\nTime counts: "<<count;
    cout<<"\nExecuted";
    cout<<"\n Done\n";

    return 0;
}
