/*------------------------------------------------------------------
 filament_ensemble.cpp : container class for filaments
 
 Copyright (C) 2016 
 Created by: Simon Freedman, Shiladitya Banerjee, Glen Hocky, Aaron Dinner
 Contact: dinner@uchicago.edu

 This program is free software: you can redistribute it and/or modify
 it under the terms of the GNU General Public License as published by
 the Free Software Foundation, either version 3 of the License, or
 (at your option) any later version. See ../LICENSE for details. 
-------------------------------------------------------------------*/

#include "globals.h"
//#include "Link.h"
#include "filament_ensemble.h"

filament_ensemble::~filament_ensemble(){ 
    cout<<"DELETING FILAMENT_ENSEMBLE\n";
    
    int s = network.size();
    
    for (int x = 0; x < nq[0]; x++){
        for (int y = 0; y < nq[1]; y++){
            delete springs_per_quad[x]->at(y);
        }
        delete springs_per_quad[x];
        //delete n_springs_per_quad[x];
    }
    
    for (int i = 0; i < s; i++){
        delete network[i];
    }
}


vector<filament *>* filament_ensemble::get_network()
{
    return &network;
}


filament * filament_ensemble::get_filament(int index)
{
    return network[index];
}


void filament_ensemble::turn_quads_off()
{
    quad_off_flag = true;
}


void filament_ensemble::nlist_init_serial()
{
    for (int x = 0; x < nq[0]; x++){
        springs_per_quad.push_back(new vector< vector<array<int, 2> >* >(nq[1]));   
        for (int y = 0; y < nq[1]; y++){
            springs_per_quad[x]->at(y) = new vector<array<int, 2> >();
        }
    }
    for (int f = 0; f < int(network.size()); f++) {
        for (int l = 0; l < network[f]->get_nsprings(); l++) {
            all_springs.push_back({f, l});
        }
    }
}

void filament_ensemble::quad_update_serial()
{
    string BC = bc->get_BC();
    array<double, 2> fov = bc->get_fov();
    double delrx = bc->get_delrx();
    //initialize all quadrants to have no springs
    for (int x = 0; x < nq[0]; x++){
        for (int y = 0; y < nq[1]; y++){
            springs_per_quad[x]->at(y)->clear();
        }
    }

    for (int f = 0; f < int(network.size()); f++) {
        for (int l = 0; l < network[f]->get_nsprings(); l++) {
            // quadrant numbers crossed by the bead in x-direction

            spring *s = network[f]->get_spring(l);
            array<double, 2> hx = s->get_hx();
            array<double, 2> hy = s->get_hy();
            array<double, 2> disp = s->get_disp();

            if (BC != "PERIODIC" && BC != "LEES-EDWARDS") {

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
                        springs_per_quad[i]->at(j)->push_back({f, l});

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

                        springs_per_quad[i]->at(j)->push_back({f, l});
                    }
                }
            }
        }
    }

    if (check_dup_in_quad) {
        for (int x = 0; x < nq[0]; x++) {
            for (int y = 0; y < nq[1]; y++) {
                vector<array<int, 2>> *quad = springs_per_quad[x]->at(y);
                set<array<int, 2>> s(quad->begin(), quad->end());
                if (s.size() != quad->size()) {
                    cout << "Quadrant (" << x << ", " << y << ") contains duplicates!" << endl;
                }
            }
        }
    }
}

// return list of possible attachment points for motor head at (x, y)
vector<array<int, 2>> *filament_ensemble::get_attach_list(double x, double y)
{
    array<double, 2> fov = bc->get_fov();
    double delrx = bc->get_delrx();
    if (quad_off_flag) {
        return &all_springs;
    } else {
        int iy = round(nq[1] * (y / fov[1] + 0.5));
        while (iy < 0) {
            iy += nq[1];
            x += delrx;
        }
        while (iy >= nq[1]) {
            iy -= nq[1];
            x -= delrx;
        }
        int ix = round(nq[0] * (x / fov[0] + 0.5));
        while (ix < 0) ix += nq[0];
        while (ix >= nq[0]) ix -= nq[0];
        assert(0 <= ix && ix < nq[0]);
        assert(0 <= iy && iy < nq[1]);
        return springs_per_quad[ix]->at(iy);
    }
}

//given a motor position, and a quadrant
//update the map of {{f, l}} -- > dist
void filament_ensemble::update_dist_map(set<pair<double, array<int,2>>>& t_map, const array<int, 2>& mq, double x, double y){
    
    array<int, 2> fl;
    double dist_sq;
    
    for (int i = 0; i < int(springs_per_quad[mq[0]]->at(mq[1])->size()); i++){

        fl = springs_per_quad[mq[0]]->at(mq[1])->at(i); //fl  = {{filament_index, spring_index}}

        if (fls.find(fl) == fls.end()){
            network[fl[0]]->get_spring(fl[1])->calc_intpoint(x, y); //calculate the point on the spring closest to (x,y)
            dist_sq = network[fl[0]]->get_spring(fl[1])->get_distance_sq(x, y); //store the distance to that point
            //cout<<"\nDEBUG : dist = "<<dist;

            t_map.insert(pair<double, array<int, 2> >(dist_sq, fl));
            fls.insert(fl);
        }
    }

}

//given motor head position, return a map between  
//  the INDICES (i.e., {i, j} for the j'th spring of the i'th filament)
//  and their corresponding DISTANCES to the spring at that distance 

set<pair<double, array<int, 2>>> filament_ensemble::get_dist(double x, double y)
{
    array<double, 2> fov = bc->get_fov();
    double delrx = bc->get_delrx();
    fls.clear();
    set<pair<double, array<int, 2>>> t_map;

    int iy = round(nq[1] * (y / fov[1] + 0.5));
    while (iy < 0) {
        iy += nq[1];
        x += delrx;
    }
    while (iy >= nq[1]) {
        iy -= nq[1];
        x -= delrx;
    }
    int ix = round(nq[0] * (x / fov[0] + 0.5));
    while (ix < 0) ix += nq[0];
    while (ix >= nq[0]) ix -= nq[0];
    assert(0 <= ix && ix < nq[0]);
    assert(0 <= iy && iy < nq[1]);

    update_dist_map(t_map, {ix, iy}, x, y);
    return t_map;
}


set<pair<double, array<int,2>>> filament_ensemble::get_dist_all(double x, double y)
{
    set<pair<double, array<int,2>>> t_map;
    double dist_sq=0;
    for (int f = 0; f < int(network.size()); f++){
        for (int l=0; l < network[f]->get_nsprings(); l++){
                network[f]->get_spring(l)->calc_intpoint(x, y); //calculate the point on the spring closest to (x,y)
                dist_sq = network[f]->get_spring(l)->get_distance_sq(x, y); //store the distance to that point
                // t_map[dist] = {f,l}; 
                t_map.insert(pair<double, array<int, 2>>(dist_sq, {{f, l}}));
        }
    }
    
    return t_map;
}

double filament_ensemble::get_llength(int fil, int spring)
{
    return network[fil]->get_spring(spring)->get_length();
}


array<double,2> filament_ensemble::get_start(int fil, int spring)
{
    return {{network[fil]->get_spring(spring)->get_hx()[0] , network[fil]->get_spring(spring)->get_hy()[0]}};
}


array<double,2> filament_ensemble::get_end(int fil, int spring)
{
    return {{network[fil]->get_spring(spring)->get_hx()[1] , network[fil]->get_spring(spring)->get_hy()[1]}};
}


array<double,2> filament_ensemble::get_force(int fil, int bead)
{
    return network[fil]->get_bead(bead)->get_force();
}


array<double,2> filament_ensemble::get_direction(int fil, int spring)
{
    return network[fil]->get_spring(spring)->get_direction();
}

 
void filament_ensemble::set_straight_filaments(bool is_straight)
{
    straight_filaments = is_straight;
}

 
void filament_ensemble::update_positions()
{
    int net_sz = int(network.size());
    for (int f = 0; f < net_sz; f++)
    {
        //if (f==0) cout<<"\nDEBUG: update_positions: using "<<omp_get_num_threads()<<" cores";  
        network[f]->update_positions();
    }

}

 
void filament_ensemble::update_positions_range(int lo, int hi)
{
    for (unsigned int f = 0; f < network.size(); f++)
    {
        network[f]->update_positions_range(lo, hi);
    }

}

vector<vector<double>> filament_ensemble::output_beads()
{
    vector<vector<double>> out;
    for (size_t i = 0; i < network.size(); i++) {
        vector<vector<double>> tmp = network[i]->output_beads(i);
        out.insert(out.end(), tmp.begin(), tmp.end());
    }
    return out;
}

vector<vector<double>> filament_ensemble::output_springs()
{
    vector<vector<double>> out;
    for (size_t i = 0; i < network.size(); i++) {
        vector<vector<double>> tmp = network[i]->output_springs(i);
        out.insert(out.end(), tmp.begin(), tmp.end());
    }
    return out;
}

vector<vector<double>> filament_ensemble::output_thermo()
{
    vector<vector<double>> out;
    for (size_t i = 0; i < network.size(); i++) {
        out.push_back(network[i]->output_thermo(i));
    }
    return out;
}

void filament_ensemble::write_beads(ofstream& fout)
{
    for (unsigned int i=0; i<network.size(); i++) {
        fout<<network[i]->write_beads(i);
    } 
}

 
void filament_ensemble::write_springs(ofstream& fout)
{
    for (unsigned int i=0; i<network.size(); i++) {
        fout<<network[i]->write_springs(i);
    } 
}

 
void filament_ensemble::write_thermo(ofstream& fout){
    for (unsigned int f = 0; f < network.size(); f++)
        fout<<network[f]->write_thermo(f);
    
}

box *filament_ensemble::get_box()
{
    return bc;
}

void filament_ensemble::set_shear_rate(double g)
{
    array<double, 2> fov = bc->get_fov();
    if (network.size() > 0)
        if (network[0]->get_nbeads() > 0)
            shear_speed = g*fov[1] / (2*network[0]->get_bead(0)->get_friction());

    for (unsigned int f = 0; f < network.size(); f++)
    {
        network[f]->set_shear(g);
    }
}

 
void filament_ensemble::set_y_thresh(double y)
{
    for (unsigned int f = 0; f < network.size(); f++) network[f]->set_y_thresh(y);
}

void filament_ensemble::update_d_strain(double g)
{
    bc->update_d_strain(g);
    for (unsigned int f = 0; f < network.size(); f++)
    {
        network[f]->update_d_strain(g);
    }
}

 
void filament_ensemble::update_shear()
{
    //cout<<"\nDEBUG: SHEARING"; 
    for (unsigned int f = 0; f < network.size(); f++)
    {
        network[f]->update_shear(t);
    }
}

 
void filament_ensemble::print_filament_thermo(){
    
    for (unsigned int f = 0; f < network.size(); f++)
    {
        cout<<"\nF"<<f<<"\t:";
        network[f]->print_thermo();
    }

}

 
void filament_ensemble::update_energies(){
    pe_stretch = 0;
    pe_bend = 0;
    ke = 0;
    vir_stretch[0][0] = vir_stretch[0][1] = 0.0;
    vir_stretch[1][0] = vir_stretch[1][1] = 0.0;
    vir_bend[0][0] = vir_bend[0][1] = 0.0;
    vir_bend[1][0] = vir_bend[1][1] = 0.0;
    for (unsigned int f = 0; f < network.size(); f++) {
        ke += network[f]->get_kinetic_energy();
        pe_bend += network[f]->get_bending_energy();
        pe_stretch += network[f]->get_stretching_energy();  
        array<array<double, 2>, 2> vs = network[f]->get_stretching_virial();
        array<array<double, 2>, 2> vb = network[f]->get_bending_virial();
        vir_stretch[0][0] += vs[0][0]; vir_stretch[0][1] += vs[0][1];
        vir_stretch[1][0] += vs[1][0]; vir_stretch[1][1] += vs[1][1];
        vir_bend[0][0] += vb[0][0]; vir_bend[0][1] += vb[0][1];
        vir_bend[1][0] += vb[1][0]; vir_bend[1][1] += vb[1][1];
    }
}

 
double filament_ensemble::get_stretching_energy(){
    return pe_stretch;
}

 
double filament_ensemble::get_bending_energy(){
    return pe_bend;
}

array<array<double, 2>, 2> filament_ensemble::get_stretching_virial() {
    return vir_stretch;
}

array<array<double, 2>, 2> filament_ensemble::get_bending_virial() {
    return vir_bend;
}

void filament_ensemble::print_network_thermo(){
    cout<<"\nAll Fs\t:\tKE = "<<ke<<"\tPEs = "<<pe_stretch<<"\tPEb = "<<pe_bend<<"\tTE = "<<(ke+pe_stretch+pe_bend);
}

 
void filament_ensemble::print_filament_lengths(){
    for (unsigned int f = 0; f < network.size(); f++)
    {
        cout<<"\nF"<<f<<" : "<<network[f]->get_end2end()<<" um";
    }
}


 
bool filament_ensemble::is_polymer_start(int fil, int bead){

    return !(bead);

}

void filament_ensemble::set_nq(double nqx, double nqy){
    nq[0] = nqx;
    nq[1] = nqy;
}

 
void filament_ensemble::set_visc(double nu){
    visc = nu;
}

 
void filament_ensemble::update_forces(int f_index, int a_index, double f1, double f2){
    network[f_index]->update_forces(a_index, f1,f2);
}

 
vector<int> filament_ensemble::get_broken(){
    return broken_filaments;
}

 
void filament_ensemble::clear_broken(){
    broken_filaments.clear();
}

 
int filament_ensemble::get_nbeads(){
    int tot = 0;
    for (unsigned int f = 0; f < network.size(); f++)
        tot += network[f]->get_nbeads();
    return tot;
}

 
int filament_ensemble::get_nsprings(){
    return this->get_nbeads() - network.size();
}

 
int filament_ensemble::get_nfilaments(){
    return network.size();
}

array<int, 2> filament_ensemble::get_nq() {
    return nq;
}
 
double filament_ensemble::get_bead_friction(){
    
    if (network.size() > 0)
        if (network[0]->get_nbeads() > 0)
            return network[0]->get_bead(0)->get_friction();
    
    return 0;
}

// Update bending forces between monomers

void filament_ensemble::update_bending()
{
    int net_sz = int(network.size());
    
    for (int f = 0; f < net_sz; f++)
    {
        //if (f==0) cout<<"\nDEBUG: update_bending: using "<<omp_get_num_threads()<<" cores";  
        network[f]->update_bending(t);
    }
}


void filament_ensemble::update_stretching(){
    
//    vector<filament *> newfilaments;
    int s = network.size(); //keep it to one fracture per filament per timestep, or things get messy
    for (int f = 0; f < s; f++)
    {
        //if (f==0) cout<<"\nDEBUG: update_stretching: using "<<omp_get_num_threads()<<" cores";  
        this->update_filament_stretching(f);
    }
}


void filament_ensemble::update_filament_stretching(int f){
    vector<filament *> newfilaments = network[f]->update_stretching(t);

    if (newfilaments.size() > 0){ //fracture event occured

        cout<<"\n\tDEBUG: fracturing filament : "<<f;
        filament * broken = network[f];     //store a pointer to the broken filament to delete it with
        network[f] = newfilaments[0];       //replace that pointer with one of the new filaments

        if (newfilaments.size() == 2) network.push_back(newfilaments[1]); //add the second filament to the top of the stack

        broken_filaments.push_back(f);      // record the index, for automatic motor detachment
        delete broken;                      // delete the old filament

    }
}


void filament_ensemble::set_shear_stop(double stopT){
    shear_stop = stopT; 
}


void filament_ensemble::set_shear_dt(double delT){
    shear_dt = delT; 
}


void filament_ensemble::update_int_forces()
{
    this->update_stretching();
    this->update_bending();
}

/* Overdamped Langevin Dynamics Integrator (Leimkuhler, 2013) */

void filament_ensemble::update()
{
    for (int f = 0; f < int(network.size()); f++) {
        update_filament_stretching(f);
        network[f]->update_bending(t);
        if (external_force_flag) {
            for (int i = 0; i < network[f]->get_nbeads(); i++) {
                array<double, 2> pos = network[f]->get_bead_position(i);
                array<double, 2> force = external_force(pos);
                update_forces(f, i, force[0], force[1]);
            }
        }
        network[f]->update_positions();
    }
    update_energies();
    t += dt;
}

array<double, 2> filament_ensemble::external_force(array<double, 2> pos)
{
    if (external_force_flag == CIRCLE) {
        double x = pos[0];
        double y = pos[1];
        double rsq = x * x + y * y;
        if (rsq < circle_wall_radius * circle_wall_radius) {
            return {0, 0};
        }
        double r = sqrt(rsq);
        double k = -circle_wall_spring_constant * (1.0 - circle_wall_radius / r);
        return {k * x, k * y};
    } else {
        throw std::logic_error("External force flag not recognized.");
    }
}

void filament_ensemble::set_circle_wall(double radius, double spring_constant)
{
    external_force_flag = CIRCLE;
    circle_wall_radius = radius;
    circle_wall_spring_constant = spring_constant;
}

vector<vector<double> > filament_ensemble::spring_spring_intersections(double len, double prob){
    vector< vector<double> > itrs;
    array<double, 2> r1, r2, s1, s2, direc;
    pair<double, double> mmx1, mmy1, mmx2, mmy2;
    for (unsigned int f1 = 0; f1 < network.size(); f1++){
        
        for (int l1 = 0; l1 < network[f1]->get_nsprings(); l1++){

            r1 = {{network[f1]->get_spring(l1)->get_hx()[0], network[f1]->get_spring(l1)->get_hy()[0]}};
            r2 = {{network[f1]->get_spring(l1)->get_hx()[1], network[f1]->get_spring(l1)->get_hy()[1]}};
            for (unsigned int f2 = f1+1; f2 < network.size(); f2++){
                
                for (int l2 = 0; l2 < network[f2]->get_nsprings(); l2++){

                    if (f1 == f2 && fabs(double(l1) - double(l2)) < 2){ //springs should be at least two away to get crosslinked
                        continue;
                    }

                    s1 = {{network[f2]->get_spring(l2)->get_hx()[0], network[f2]->get_spring(l2)->get_hy()[0]}};
                    s2 = {{network[f2]->get_spring(l2)->get_hx()[1], network[f2]->get_spring(l2)->get_hy()[1]}};

                    boost::optional<array<double, 2>> inter = seg_seg_intersection_bc(bc, r1, r2, s1, s2);

                    if (inter && rng(0,1) <= prob){
                        direc = network[f2]->get_spring(l2)->get_direction();
                        itrs.push_back({inter->at(0), inter->at(1), len*direc[0], len*direc[1], 
                                double(f1), double(f2), double(l1), double(l2)}); 
                    }
                }
            }
        }
    }
    return itrs;
}
////////////////////////////////////////
///SPECIFIC FILAMENT IMPLEMENTATIONS////
////////////////////////////////////////

filament_ensemble::filament_ensemble(box *bc_, vector<vector<double> > beads, array<int,2> mynq, double delta_t, double temp,
        double vis, double spring_len, double stretching, double ext, double bending, double frac_force, bool check_dup_in_quad_)
{
    external_force_flag = 0;
    check_dup_in_quad = check_dup_in_quad_;
    bc = bc_;

    visc=vis;
    spring_rest_len = spring_len;
    dt = delta_t;
    temperature = temp;
    t = 0;

    view[0] = 1;
    view[1] = 1;

    int s = beads.size(), sa, j;
    int fil_idx = 0;
    vector<bead *> avec;
    
    nq = mynq;
    
    for (int i=0; i < s; i++){
        
        if (beads[i][3] != fil_idx && avec.size() > 0){
            
	  network.push_back( new filament(this, avec, spring_rest_len, stretching, ext, bending, delta_t, temp, frac_force, 0) );
            
            sa = avec.size();
            for (j = 0; j < sa; j++) delete avec[j];
            avec.clear();
            
            fil_idx = beads[i][3];
        }
        avec.push_back(new bead(beads[i][0], beads[i][1], beads[i][2], vis));
    }

    sa = avec.size();
    if (sa > 0)
      network.push_back( new filament(this, avec, spring_rest_len, stretching, ext, bending, delta_t, temp, frac_force, 0) );
    
    for (j = 0; j < sa; j++) delete avec[j];
    avec.clear();
   
    quad_off_flag = false;
    max_springs_per_quad              = beads.size();
    max_springs_per_quad_per_filament = int(ceil(beads.size() / (fil_idx + 1)))- 1;
    //this->nlist_init();
    this->nlist_init_serial();
    this->update_energies();

    vir_stretch[0][0] = vir_stretch[0][1] = 0.0;
    vir_stretch[1][0] = vir_stretch[1][1] = 0.0;
    vir_bend[0][0] = vir_bend[0][1] = 0.0;
    vir_bend[1][0] = vir_bend[1][1] = 0.0;

    fls = { };
} 
