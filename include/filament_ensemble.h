/*
 *  filament_ensemble.h
 *  
 *
 *  Authors : Shiladitya Banerjee, Simon Freedman
 *  Copyright 2013 University of Chicago. All rights reserved.
 *
 */

//=====================================
//include guard
#ifndef __FILAMENT_ENSEMBLE_H_INCLUDED__
#define __FILAMENT_ENSEMBLE_H_INCLUDED__

//=====================================
// forward declared dependencies

//=====================================
//included dependences
#include "string"
#include "vector"
#include "map"
#include "unordered_map"
#include "filament.h"
#include <boost/functional/hash.hpp>
#include <boost/scoped_array.hpp>
//=====================================
//filament network class
template <class filament_type>
class filament_ensemble
{
    public:
        filament_ensemble();

        ~filament_ensemble();
        
        void nlist_init();
        
        void nlist_init_serial();
        
        void quad_update();
        
        void quad_update_serial();
        
        void consolidate_quads();

        void update_quads_per_filament(int);

        void reset_n_links(int);

        void update_dist_map(map<array<int,2>, double>& t_map, const array<int, 2>& mquad, double x, double y);
        
        vector<filament_type *> * get_network();

        filament_type * get_filament(int index);

        map<array<int,2>, double> get_dist(double x, double y);
        
        map<array<int,2>, double> get_dist_all(double x, double y);
        
        array<double,2> get_direction(int fil, int link);

        array<double,2> get_intpoints(int fil, int link, double xp, double yp);

        array<double,2> get_start(int fil, int link);
        
        array<double,2> get_end(int fil, int link);
        
        array<double,2> get_force(int fil, int actin);
        
        double get_int_direction(int fil, int link, double xp, double yp);

        double get_xcm(int fil, int link);
       
        double get_ycm(int fil, int link);

        double get_angle(int fil, int link);

        double get_llength(int fil, int link);
       
        double get_actin_friction();
        
        double get_delrx();
        
        double get_stretching_energy();
        
        double get_bending_energy();
        
        int get_nactins();
        
        int get_nlinks();
        
        int get_nfilaments();

        vector<vector<double> > link_link_intersections(double cllen, double prob);

        void update_shear();
        
        void update_d_strain(double);
        
        void update_delrx(double);
        
        void update_stretching();
        
        void update_filament_stretching(int);
        
        void update_bending();
        
        void update_int_forces();

        void update_positions();

        void update_positions_range(int lo, int hi);
        
        void update_forces(int fil, int actin, double f2, double f3);

        void write_actins(ofstream& fout);
        
        void write_links(ofstream& fout);
        
        void write_thermo(ofstream& fout);
        
        void set_straight_filaments(bool is_straight);

        void set_y_thresh(double);
        
        void set_fene_dist_pct(double);
        
        void set_shear_rate(double);
        
        void set_shear_stop(double);

        void set_shear_dt(double);
        
        bool is_polymer_start(int f, int a);

        void set_fov(double x, double y);

        void set_nq(double x, double y);

        void set_visc(double v);

        vector<int> get_broken();

        void clear_broken();
        
        void print_filament_thermo();

        void print_network_thermo();

        void print_filament_lengths();
        
        void update();
        
        void update_energies();
        
    protected:

        double t, dt, temperature, link_ld, visc, min_time;
        double gamma, shear_stop, shear_dt, shear_speed, delrx;
        double max_links_per_quad_per_filament, max_links_per_quad; 
        bool straight_filaments = false;
        double pe_stretch, pe_bend, ke;

        array<double,2> fov, view;
        array<int, 2> nq, half_nq;
        vector<int> broken_filaments, empty_vector;
        
        //links_per_quad[{x,y}] => {{f_1, l_1}, {f_1, l_2},...,{f_k, l_j},...} means that link l_j on filament f_k is located at quadrant {x,y}
        //n_links_per_quad[{x,y}] => kmax means that kmax links are indexed to {x,y} 
        vector< vector < vector< array<int, 2 > >* > * > links_per_quad;
        vector< vector < int >* > n_links_per_quad;
        
//        boost::scoped_array< boost::scoped_array < boost::scoped_array < array<int, 2 > >* > * > links_per_quad;
//        boost::scoped_array< boost::scoped_array < int >* > n_links_per_quad;

        /* per_quad_per_filament does the same thing as above, but for a specific filament. useful for parallelization,
         * if implemented.
        vector<map<array<int, 2>, vector<int>*> * > links_per_quad_per_filament; 
        vector<map<array<int, 2>, int> * > n_links_per_quad_per_filament; 
        */
        
        vector<array<int, 2>* > all_quads;
        vector<filament_type *> network;
};

class ATfilament_ensemble:
    public filament_ensemble<filament>
{

    public:
        
        ATfilament_ensemble(double density, array<double,2> myfov, array<int, 2> mynq, double delta_t, double temp, 
                double len, double vis, int nactin,
                double link_len, vector<array<double, 3> > pos_sets, double stretching, double ext, double bending, double frac_force, 
                string bc, double seed);
        
        ATfilament_ensemble(vector< vector<double> > actins, array<double,2> myfov, array<int,2> mynq, double delta_t, double temp,
                double vis, double link_len, double stretching, double ext, double bending, double frac_force, string bc); 
        
};

class baoab_filament_ensemble:
    public filament_ensemble<baoab_filament>
{
    public:

        baoab_filament_ensemble(double density, array<double,2> myfov, array<int, 2> mynq, double delta_t, double temp, 
                double len, double vis, int nactin,
                double link_len, vector<array<double, 3> > pos_sets, double stretching, double bending, double frac_force, 
                string bc, double seed);

        void update_velocities_B();

        void update_velocities_O();

        void update();

};

class lammps_filament_ensemble:
    public filament_ensemble<lammps_filament>
{
    public:

        lammps_filament_ensemble(double density, array<double,2> myfov, array<int, 2> mynq, double delta_t, double temp, 
                double len, double vis, int nactin,
                double link_len, vector<array<double, 3> > pos_sets, double stretching, double bending, double frac_force, 
                string bc, double seed);

        void set_mass(double m);

        void update_drag();

        void update_brownian();

        void update();
};

class langevin_leapfrog_filament_ensemble:
    public filament_ensemble<langevin_leapfrog_filament>
{
    public:

        langevin_leapfrog_filament_ensemble(double density, array<double,2> myfov, array<int, 2> mynq, double delta_t, double temp, 
                double len, double vis, int nactin,
                double link_len, vector<array<double, 3> > pos_sets, double stretching, double bending, double frac_force, 
                string bc, double seed);

        void set_mass(double m);

        void update_velocities();

        void update_positions();

        void update();
    
};
#endif
