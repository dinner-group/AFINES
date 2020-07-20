/*
 *  spring.h
 *
 *  Created by Simon Freedman on 9/10/2014
 *  Copyright 2014 University of Chicago. All rights reserved.
 *
 */

#ifndef AFINES_SPRING_H
#define AFINES_SPRING_H

class filament;

#include "globals.h"
#include "box.h"
#include "motor.h"

class spring {
    public:
        spring();
        spring(double len, double stiffness, double max_ext, filament* f, array<int, 2> aindex);
        virtual ~spring();

        // dynamics

        void step();

        void update_force();
        void update_force_fraenkel_fene();

        void filament_update();

        // state

        vec_type get_h0();
        vec_type get_h1();

        double get_length();
        double get_length_sq();
        vec_type get_disp();
        vec_type get_direction();

        vec_type get_force();

        array<int,2> get_aindex();
        void set_aindex(array<int,2> idx);

        void inc_aindex();

        double get_l0();
        void set_l0(double myl0);

        void add_mot(motor * mot, int hd);
        void remove_mot(motor * mot);
        map<motor *, int> & get_mots();

        // parameters

        double get_kl();
        double get_fene_ext();
        double get_max_ext();

        // thermo

        double get_stretching_energy();
        double get_stretching_energy_fene();

        virial_type get_virial();

        // output

        string to_string();
        vector<double> output();
        string write();

        // misc

        bool operator==(const spring& that);
        bool is_similar(const spring& that);

        vec_type intpoint(vec_type pos);
        bool get_line_intersect(spring *l2); 

    protected:

        // state
        vec_type h0, h1;
        map<motor *, int> mots;
        array<int, 2> aindex;  // bead indices

        // derived
        double llen, llensq;
        vec_type disp, direc;
        vec_type force;

        // parameters
        double kl, l0;
        double max_ext, eps_ext;  // fene
        box *bc;
        filament *fil;
};

#endif
