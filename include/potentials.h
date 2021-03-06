#ifndef AFINES_POTENTIALS_H
#define AFINES_POTENTIALS_H

#include "globals.h"
#include "vec.h"

struct stretch_fene {
    double kl, l0, max_ext, eps_ext;

    stretch_fene(double kl, double l0, double max_ext_ratio);
    double tension(double len);
    double energy(double len);
};

struct bend_result_type {
    double energy;
    vec_type force1;
    vec_type force2;
};

bend_result_type bend_angle(vec_type delr1, vec_type delr2);

double angle(vec_type delr1, vec_type delr2);

bend_result_type bend_harmonic(
        double kb, double theta0,
        vec_type delr1, vec_type delr2);

double bend_harmonic_energy(
        double kb, double theta0,
        vec_type delr1, vec_type delr2);

#endif
