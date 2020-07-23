/*-------------------------------------------------------------------
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
#include "filament_ensemble.h"

filament_ensemble::filament_ensemble(box *bc_, vector<vector<double> > beads, array<int,2> mynq, double delta_t, double temp,
        double vis, double spring_len, double stretching, double ext, double bending, double frac_force, double RMAX, double A)
{
    external_force_flag = external_force_type::none;
    bc = bc_;

    bc->add_callback([this](double g) { this->update_d_strain(g); });

    exv = nullptr;
    dt = delta_t;

    int fil_idx = 0;
    vector<vector<double>> avec;

    nsprings_per_fil_max = 0;
    for (int i=0; i < int(beads.size()); i++){

        if (beads[i][3] != fil_idx && avec.size() > 0){

            network.push_back(new filament(this, avec, spring_len, stretching, ext, bending, delta_t, temp, frac_force));
            nsprings_per_fil_max = max(nsprings_per_fil_max, int(avec.size() - 1));
            avec.clear();
            fil_idx = beads[i][3];
        }
        avec.push_back({beads[i][0], beads[i][1], beads[i][2], vis});
    }

    if (avec.size() > 0)
      network.push_back(new filament(this, avec, spring_len, stretching, ext, bending, delta_t, temp, frac_force));

    avec.clear();

    if (A > 0) {
        exv = new excluded_volume(bc, RMAX, A);
    }

    quads = new quadrants(bc, mynq);
    this->update_energies();

    pe_stretch = 0;
    pe_bend = 0;
    pe_exv = 0;
    ke_vir = 0;
}

filament_ensemble::~filament_ensemble()
{
    delete quads;
    if (!exv) {
        delete exv;
    }
    for (filament *f : network) delete f;
}

// begin [quadrants]

quadrants *filament_ensemble::get_quads()
{
    return quads;
}

void filament_ensemble::quad_update_serial()
{
    quads->clear();
    for (int f = 0; f < int(network.size()); f++) {
        for (int l = 0; l < network[f]->get_nsprings(); l++) {
            spring *s = network[f]->get_spring(l);
            quads->add_spring(s, {f, l});
        }
    }
}

vector<array<int, 2>> *filament_ensemble::get_attach_list(vec_type pos)
{
    return quads->get_attach_list(pos);
}

// end [quadrants]

vector<filament *>* filament_ensemble::get_network()
{
    return &network;
}

filament * filament_ensemble::get_filament(int index)
{
    return network[index];
}

double filament_ensemble::get_llength(int fil, int spring)
{
    return network[fil]->get_spring(spring)->get_length();
}

vec_type filament_ensemble::get_start(int fil, int spring)
{
    return network[fil]->get_spring(spring)->get_h0();
}

vec_type filament_ensemble::get_end(int fil, int spring)
{
    return network[fil]->get_spring(spring)->get_h1();
}

vec_type filament_ensemble::get_direction(int fil, int spring)
{
    return network[fil]->get_spring(spring)->get_direction();
}

vec_type filament_ensemble::get_force(int fil, int spring)
{
    return network[fil]->get_force(spring);
}

box *filament_ensemble::get_box()
{
    return bc;
}

bool filament_ensemble::is_polymer_start(int fil, int bead){

    return !(bead);

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

// begin [output]

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

void filament_ensemble::print_filament_thermo()
{
    for (size_t i = 0; i < network.size(); i++) {
        fmt::print("\nF{}\t:", i);
        network[i]->print_thermo();
    }
}

void filament_ensemble::print_network_thermo()
{
    fmt::print(
            "\nAll Fs\t:\t"
            "KEvel = {}\t"
            "KEvir = {}\t"
            "PEs = {}\t"
            "PEb = {}\t"
            "PEexv = {}\t"
            "TE = {}",
            ke_vel, ke_vir,
            pe_stretch, pe_bend, pe_exv,
            ke_vir + pe_stretch + pe_bend + pe_exv);
}

void filament_ensemble::print_filament_lengths()
{
    for (size_t f = 0; f < network.size(); f++) {
        fmt::print("\nF{} : {} um", f, network[f]->get_end2end());
    }
}

// end [output]

// begin [thermo]

void filament_ensemble::update_energies()
{
    pe_stretch = 0.0;
    pe_bend = 0.0;
    ke_vel = 0.0;
    ke_vir = 0.0;
    vir_stretch.zero();
    vir_bend.zero();
    for (filament *f : network) {
        ke_vel += f->get_kinetic_energy_vel();
        ke_vir += f->get_kinetic_energy_vir();
        pe_bend += f->get_bending_energy();
        pe_stretch += f->get_stretching_energy();
        vir_stretch += f->get_stretching_virial();
        vir_bend += f->get_bending_virial();
    }
}

double filament_ensemble::get_stretching_energy(){
    return pe_stretch;
}

double filament_ensemble::get_bending_energy(){
    return pe_bend;
}

virial_type filament_ensemble::get_stretching_virial() {
    return vir_stretch;
}

virial_type filament_ensemble::get_bending_virial() {
    return vir_bend;
}

double filament_ensemble::get_kinetic_energy_vir(){
    return ke_vir;
}

double filament_ensemble::get_exv_energy()
{
    return pe_exv;
}

// end [thermo]

// begin [monte carlo]

void filament_ensemble::set_growing(double kgrow, double lgrow, double l0min, double l0max, int nsprings_max)
{
    nsprings_per_fil_max = nsprings_max;
    for (int i = 0; i < int(network.size()); i++){
        network[i]->set_kgrow(kgrow);
        network[i]->set_lgrow(lgrow);
        network[i]->set_l0_min(l0min);
        network[i]->set_l0_max(l0max);
        network[i]->set_nsprings_max(nsprings_max);
    }
}

void filament_ensemble::try_grow()
{
    for (filament *f : network) {
        f->update_length();
    }
}

void filament_ensemble::try_fracture() {
    // Note: network.size() is not a constant
    // fractured filaments are added to the end of network,
    // so they can be fractured again
    for (size_t i = 0; i < network.size(); i++) {
        vector<filament *> newfilaments = network[i]->try_fracture();
        if (newfilaments.size() > 0) {
            filament *broken = network[i];
            network[i] = newfilaments[0];
            for (size_t i = 1; i < newfilaments.size(); i++) {
                network.push_back(newfilaments[i]);
            }
            broken->detach_all_motors();
            delete broken;
        }
    }
}

void filament_ensemble::montecarlo()
{
    for (filament *f : network) f->update_length();
    try_fracture();
}

// end [monte carlo]

// begin [dynamics]

// Overdamped Langevin Dynamics Integrator (Leimkuhler, 2013)
void filament_ensemble::integrate()
{
    for (filament *f : network) {
        f->update_positions();
    }
}

void filament_ensemble::update_d_strain(double g)
{
    for (filament *f : network) {
        f->update_d_strain(g);
    }
}

// end [dynamics]

// begin [forces]

void filament_ensemble::compute_forces()
{
    update_excluded_volume();
    update_stretching();
    update_bending();
}

void filament_ensemble::update_bending()
{
    for (filament *f : network) {
        f->update_bending();
    }
}

void filament_ensemble::update_stretching()
{
    for (filament *f : network) {
        f->update_stretching();
    }
}

void filament_ensemble::update_forces(int f_index, int a_index, vec_type f)
{
    network[f_index]->update_forces(a_index, f);
}

void filament_ensemble::update_excluded_volume()
{
    pe_exv = 0.0;
    if (exv) {
        exv->update_spring_forces_from_quads(quads, network, nsprings_per_fil_max);
        pe_exv = exv->get_pe_exv();
    }
}

// end [forces]

// begin [external force]

void filament_ensemble::update_external()
{
    for (size_t f = 0; f < network.size(); f++) {
        if (external_force_flag != external_force_type::none) {
            for (int i = 0; i < network[f]->get_nbeads(); i++) {
                vec_type pos = network[f]->get_bead_position(i);
                vec_type force = external_force(pos);
                update_forces(f, i, force);
            }
        }
    }
}

vec_type filament_ensemble::external_force(vec_type pos)
{
    if (external_force_flag == external_force_type::circle) {
        double rsq = abs2(pos);
        if (rsq < circle_wall_radius * circle_wall_radius) {
            return {0, 0};
        }
        double r = sqrt(rsq);
        double k = -circle_wall_spring_constant * (1.0 - circle_wall_radius / r);
        return k * pos;
    } else {
        throw std::logic_error("External force flag not recognized.");
    }
}

void filament_ensemble::set_circle_wall(double radius, double spring_constant)
{
    external_force_flag = external_force_type::circle;
    circle_wall_radius = radius;
    circle_wall_spring_constant = spring_constant;
}

// end [external force]
