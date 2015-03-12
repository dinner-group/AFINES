/*
 * motor.cpp
 *  
 *
 *  Created by Shiladitya Banerjee on 9/3/13.
 *  Copyright 2013 University of Chicago. All rights reserved.
 *
 */

#include "globals.h"
#include "motor.h"
#include "motor_ensemble.h"
#include "filament_ensemble.h"

//motor_ensemble class
template <class filament_ensemble_type>
motor_ensemble<filament_ensemble_type>::motor_ensemble(double mdensity, double fovx, double fovy, double delta_t, double temp, 
        double mlen, filament_ensemble_type * network, double v0, double stiffness, double ron, double roff, double rend, 
        double actin_len, double vis, std::vector<double *> positions) {
    
    fov[0]=fovx;
    fov[1]=fovy;
    mrho=mdensity;
    mld=mlen;
    nm=int(ceil(mrho*fov[0]*fov[1]));
    std::cout<<"\nDEBUG: Number of motors:"<<nm<<"\n";
    f_network=network;
    alpha=0.8;
    gamma = 0;

    if (v0 == 0){
        color = "0.1"; //purple
    }
    else
        color = "0.5";//"green"; 
    
    for (int i=0; i< nm; i++) {
        
        if ((unsigned int)i < positions.size()){
            motorx = positions[i][0];
            motory = positions[i][1];
            mang   = positions[i][2];
        }else{
            motorx = rng(-0.5*(fovx*alpha-mld),0.5*(fovx*alpha-mld));
            motory = rng(-0.5*(fovy*alpha-mld),0.5*(fovy*alpha-mld));
            mang   = rng(0,2*pi);
        }

        n_motors.push_back(new motor<filament_ensemble_type>(motorx,motory,mang,mld,f_network,0,0,-1,-1,-1,-1,fov[0],fov[1],delta_t, temp, 
                    v0,stiffness,ron,roff,rend,actin_len,vis,color));
    }
}

template <class filament_ensemble_type>
motor_ensemble<filament_ensemble_type>::~motor_ensemble( ){ 
    std::cout<<"DELETING MOTOR ENSEMBLE\n";
    int s = n_motors.size();
    for (int i = 0; i < s; i++){
        delete n_motors[i];
    }
    n_motors.clear();
};

template <class filament_ensemble_type>
void motor_ensemble<filament_ensemble_type>::motor_walk(double t)
{

    for (unsigned int i=0; i<n_motors.size(); i++) {

        s[0]=n_motors[i]->get_states()[0];
        s[1]=n_motors[i]->get_states()[1];


        if (s[0]==0 && s[1]==0) {
            n_motors[i]->attach(0);
            n_motors[i]->attach(1);
            n_motors[i]->brownian(t, gamma);
        }
        else if (s[0]==0 && s[1]==1) {
            n_motors[i]->attach(0);
            n_motors[i]->brownian(t, gamma);
            n_motors[i]->step_onehead(1);
        }
        else if (s[0]==1 && s[1]==0) {
            n_motors[i]->attach(1);
            n_motors[i]->brownian(t, gamma);
            n_motors[i]->step_onehead(0);
        }
        else {
            n_motors[i]->step_twoheads();
        }

        n_motors[i]->actin_update();
    }

}

template <class filament_ensemble_type>
void motor_ensemble<filament_ensemble_type>::reshape()
{
    for (unsigned int i=0; i<n_motors.size(); i++) {
        n_motors[i]->update_shape();
    }
}



template <class filament_ensemble_type>
void motor_ensemble<filament_ensemble_type>::motor_write(std::ofstream& fout)
{
    for (unsigned int i=0; i<n_motors.size(); i++) {
        //double stretch=dis_points(n_motors[i]->get_hx()[0],n_motors[i]->get_hy()[0],n_motors[i]->get_hx()[1],n_motors[i]->get_hy()[1])-mld;
        /*   if (stretch>3*0.25) {
             continue;
             }
             else{
             */   
        fout<<n_motors[i]->get_hx()[0]<<"\t"<<n_motors[i]->get_hy()[0]<<"\t"<<n_motors[i]->get_hx()[1]-n_motors[i]->get_hx()[0]<<"\t"<<n_motors[i]->get_hy()[1]-n_motors[i]->get_hy()[0]<<"\t"<<n_motors[i]->get_color()<<"\n";
        //}
    } 
}

template <class filament_ensemble_type>
void motor_ensemble<filament_ensemble_type>::motor_tension(std::ofstream& fout)
{
    for (unsigned int i=0; i<n_motors.size(); i++) {
        fout<<n_motors[i]->tension()<<"\n";
    }
}

template <class filament_ensemble_type>
void motor_ensemble<filament_ensemble_type>::add_motor(motor<filament_ensemble_type> * m)
{
    n_motors.push_back(m);
}

template <class filament_ensemble_type>
void motor_ensemble<filament_ensemble_type>::set_shear(double g)
{
    gamma = g;
}

template class motor_ensemble<DLfilament_ensemble>;
template class motor_ensemble<NFfilament_ensemble>;