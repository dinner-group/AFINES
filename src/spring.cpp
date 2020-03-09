/*------------------------------------------------------------------
 spring.cpp : object describing a hookean spring that connects beads
            in a worm-like chain. 
 
 Copyright (C) 2016 
 Created by: Simon Freedman, Shiladitya Banerjee, Glen Hocky, Aaron Dinner
 Contact: dinner@uchicago.edu

 This program is free software: you can redistribute it and/or modify
 it under the terms of the GNU General Public License as published by
 the Free Software Foundation, either version 3 of the License, or
 (at your option) any later version. See ../LICENSE for details. 
-------------------------------------------------------------------*/

#include "spring.h"
#include "globals.h"
#include "filament.h"

spring::spring(){ }

spring::spring(double len, double stretching_stiffness, double max_ext_ratio, filament* f, 
        array<int, 2> myaindex, array<double, 2> myfov, array<int, 2> mynq)
{
    kl      = stretching_stiffness;
    l0      = len;
    fil     = f;
    aindex  = myaindex;
    fov     = myfov;
    nq      = mynq;
    half_nq = {{nq[0]/2, nq[1]/2}};

    max_ext = max_ext_ratio * l0;
    eps_ext = 0.01*max_ext;
    
    hx = {{0,0}};
    hy = {{0,0}};

    force = {{0,0}};
    intpoint = {{0,0}};
    llensq = l0*l0;
    llen = l0;
}
spring::~spring(){ 
};

array<double,2> spring::get_hx(){
    return hx;
}

array<double,2> spring::get_hy(){
    return hy;
}

// stepping kinetics

void spring::step(string bc, double shear_dist)
{
    hx[0] = fil->get_bead(aindex[0])->get_xcm();
    hx[1] = fil->get_bead(aindex[1])->get_xcm();
    hy[0] = fil->get_bead(aindex[0])->get_ycm();
    hy[1] = fil->get_bead(aindex[1])->get_ycm();

    disp   = rij_bc(bc, hx[1]-hx[0], hy[1]-hy[0], fov[0], fov[1], shear_dist); 
    llensq = disp[0]*disp[0] + disp[1]*disp[1];
    llen   = sqrt(llensq);

    if (llen != 0)
        direc = {{disp[0]/llen, disp[1]/llen}};
    else
        direc = {{0, 0}};

}

void spring::update_force(string bc, double shear_dist)
{
    double kf = kl*(llen-l0);
    force = {{kf*direc[0], kf*direc[1]}};
}

/* Taken from hsieh, jain, larson, jcp 2006; eqn (5)
 * Adapted by placing a cutoff, similar to how it's done in LAMMPS src/bond_fene.cpp*/
void spring::update_force_fraenkel_fene(string bc, double shear_dist)
{
    double ext = abs(l0 - llen);
    double scaled_ext, klp;
    if (max_ext - ext > eps_ext ){
        scaled_ext = ext/max_ext;
    }
    else{
        scaled_ext = (max_ext - eps_ext)/max_ext;
    }

    klp = kl/(1-scaled_ext*scaled_ext)*(llen-l0);
    force = {{klp*direc[0], klp*direc[1]}};

}

void spring::update_force_marko_siggia(string bc, double shear_dist, double kToverLp)
{
    double xrat = llen/l0, yrat = llen/l0;
    if (xrat != xrat || xrat == 1) xrat = 0;
    if (yrat != yrat || yrat == 1) yrat = 0;
    force = {{kToverLp*(0.25/((1-xrat)*(1-xrat))-0.25+xrat), kToverLp*(0.25/((1-yrat)*(1-yrat))-0.25+yrat)}};  
}

array<double,2> spring::get_force()
{
    return force;
}

array<double,2> spring::get_disp()
{
    return disp;
}

array<double,2> spring::get_neg_disp()
{
    return {{-disp[0], -disp[1]}};
}

void spring::filament_update()
{
    fil->update_forces(aindex[0],  force[0],  force[1]);
    fil->update_forces(aindex[1], -force[0], -force[1]);

}

double spring::get_kl(){
    return kl;
}

double spring::get_l0(){
    return l0;
}

double spring::get_fene_ext(){
    return max_ext/l0;
}

double spring::get_length(){
    return llen;
}

double spring::get_length_sq(){
    return llensq;
}

std::string spring::write(string bc, double shear_dist){
    return "\n" + std::to_string(hx[0]) + "\t" + std::to_string(hy[0]) + "\t" + std::to_string(disp[0]) + "\t" 
        + std::to_string(disp[1]);
}

std::string spring::to_string(){
    
    char buffer [100];
    sprintf(buffer, "aindex[0] = %d;\t aindex[1] = %d;\t kl = %f;\t l0 = %f\nfilament : \n",
                        aindex[0], aindex[1], kl, l0);

    return buffer + fil->to_string();

}

bool spring::operator==(const spring& that) 
{
    /*Note: you can't compare the filament objects because that will lead to infinite recursion;
     * this function requires the filament poiner to be identical to evaluate to true*/
    return (this->aindex[0] == that.aindex[0] && this->aindex[1] == that.aindex[1] &&
            this->kl == that.kl && 
            this->l0 == that.l0 && this->fil == that.fil);
}

bool spring::is_similar(const spring& that) 
{
    
    /* Same as ==; but doesn't compare the filament pointer*/

    return (this->aindex[0] == that.aindex[0] && this->aindex[1] == that.aindex[1] &&
            this->kl == that.kl &&
            this->l0 == that.l0);
}

// Updates all derived quantities of a monomer
void spring::quad_update(string bc, double delrx)
{
    // quadrant numbers crossed by the bead in x-direction
    quad.clear();

    if (bc != "PERIODIC" && bc != "LEES-EDWARDS") {

        double xlo = hx[0], xhi = hx[1];
        if (disp[0] < 0) std::swap(xlo, xhi);

        double ylo = hy[0], yhi = hy[1];
        if (disp[1] < 0) std::swap(ylo, yhi);

        int xlower = floor(nq[0] * (xlo / fov[0] + 0.5));
        int xupper = ceil(nq[0] * (xhi / fov[0] + 0.5));
        if (xlower < 0) {
            cout << "Warning: x-index of quadrant < 0." << endl;
            xlower = 0;
        }
        if (xupper > nq[0]) {
            cout << "Warning: x-index of quadrant > nq[0]." << endl;
            xupper = nq[0];
        }
        assert(xlower <= xupper);

        int ylower = floor(nq[1] * (ylo / fov[1] + 0.5));
        int yupper = ceil(nq[1] * (yhi / fov[1] + 0.5));
        if (ylower < 0) {
            cout << "Warning: y-index of quadrant < 0." << endl;
            ylower = 0;
        }
        if (yupper > nq[1]) {
            cout << "Warning: y-index of quadrant > nq[1]." << endl;
            yupper = nq[1];
        }
        assert(ylower <= yupper);

        for (int i = xlower; i <= xupper; i++)
            for (int j = ylower; j <= yupper; j++)
                quad.push_back({i, j});

    } else {

        double xlo, xhi;
        double ylo, yhi;
        if (disp[1] >= 0) {
            ylo = hy[0];
            yhi = hy[0] + disp[1];
            if (disp[0] >= 0) {
                xlo = hx[0];
                xhi = hx[0] + disp[0];
            } else {
                xlo = hx[0] + disp[0];
                xhi = hx[0];
            }
        } else {
            ylo = hy[1];
            yhi = hy[1] - disp[1];
            if (disp[0] >= 0) {
                xlo = hx[1] - disp[0];
                xhi = hx[1];
            } else {
                xlo = hx[1];
                xhi = hx[1] - disp[0];
            }
        }
        assert(xlo <= xhi);
        assert(ylo <= yhi);

        int ylower = floor(nq[1] * (ylo / fov[1] + 0.5));
        int yupper =  ceil(nq[1] * (yhi / fov[1] + 0.5));
        assert(ylower <= yupper);

        for (int jj = ylower; jj <= yupper; jj++) {
            int j = jj;

            double xlo_new = xlo;
            double xhi_new = xhi;
            while (j < 0) {
                j += nq[1];
                xlo_new += delrx;
                xhi_new += delrx;
            }
            while (j >= nq[1]) {
                j -= nq[1];
                xlo_new -= delrx;
                xhi_new -= delrx;
            }
            assert(0 <= j && j < nq[1]);

            int xlower = floor(nq[0] * (xlo_new / fov[0] + 0.5));
            int xupper =  ceil(nq[0] * (xhi_new / fov[0] + 0.5));
            assert(xlower <= xupper);

            for (int ii = xlower; ii <= xupper; ii++) {
                int i = ii;

                while (i < 0) i += nq[0];
                while (i >= nq[0]) i -= nq[0];
                assert(0 <= i && i < nq[0]);

                quad.push_back({i, j});
            }
        }
    }
}


//shortest(perpendicular) distance between an arbitrary point and the spring
//SO : 849211
double spring::get_distance_sq(string bc, double delrx, double xp, double yp)
{
    array<double, 2> dr = rij_bc(bc, intpoint[0]-xp, intpoint[1]-yp, fov[0], fov[1], delrx);
    return dr[0]*dr[0] + dr[1]*dr[1];
}

array<double,2> spring::get_intpoint()
{
    return intpoint;
}

void spring::calc_intpoint(string bc, double delrx, double xp, double yp)
{
    double l2 = disp[0]*disp[0]+disp[1]*disp[1];
    if (l2==0){
        intpoint = {{hx[0], hy[0]}};
    }else{
        //Consider the line extending the spring, parameterized as h0 + tp ( h1 - h0 )
        //tp = projection of (xp, yp) onto the line
        double tp=dot_bc(bc, xp-hx[0], yp-hy[0], hx[1]-hx[0], hy[1]-hy[0], fov[0], fov[1], delrx)/l2;
        if (tp<0){ 
            intpoint = {{hx[0], hy[0]}};
        }
        else if(tp>1.0){
            intpoint = {{hx[1], hy[1]}};
        }
        else{
            array<double, 2> proj   = {{hx[0] + tp*disp[0], hy[0] + tp*disp[1]}};
            intpoint                = pos_bc(bc, delrx, 0, fov, {{0,0}}, proj); //velocity and dt are 0 since not relevant
        }
    }
}

vector<array<int, 2> > spring::get_quadrants()
{
    return quad;
}

array<double, 2> spring::get_direction()
{
    return direc;
}

double spring::get_stretching_energy(){
    return (force[0]*force[0]+force[1]*force[1])/(2*kl);
}

array<array<double, 2>, 2> spring::get_virial() {
    double k = kl*(llen-l0)/llen;
    return {
        array<double, 2>{k * disp[0] * disp[0], k * disp[0] * disp[1]},
        array<double, 2>{k * disp[0] * disp[1], k * disp[1] * disp[1]}
    };
}

double spring::get_stretching_energy_fene(string bc, double shear_dist)
{
    double ext = abs(l0 - llen);
    
    if (max_ext - ext > eps_ext )
        return -0.5*kl*max_ext*max_ext*log(1-(ext/max_ext)*(ext/max_ext));
    else
        return 0.25*kl*ext*ext*(max_ext/eps_ext);
    
}
