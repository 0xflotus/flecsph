/*~--------------------------------------------------------------------------~*
 * Copyright (c) 2017 Triad National Security, LLC
 * All rights reserved.
 *~--------------------------------------------------------------------------~*/


#include <iostream>
#include <algorithm>
#include <cassert>
#include <math.h>

#include "user.h"
#include "sodtube.h"
#include "params.h"
#include "lattice.h"
#include "kernels.h"
#include "io.h"

using namespace io;
//
// help message
//
void print_usage() {
  clog_one(warn)
      << "Initial data generator for KH test in"
      << gdimension << "D" << std::endl
      << "Usage: ./KD_XD_generator <parameter-file.par>" << std::endl;
}

//
// derived parameters
//
static int64_t nparticlesproc;        // number of particles per proc
static double rho_1, rho_2;           // densities
static double vx_1, vx_2;             // velocities
static double pressure_1, pressure_2; // pressures
static std::string initial_data_file; // = initial_data_prefix + ".h5part"

// geometric extents of the three regions: top, middle and bottom
static point_t tbox_min, tbox_max;
static point_t mbox_min, mbox_max;
static point_t bbox_min, bbox_max;

static int64_t np_middle = 0; // number of particles in the middle block
static int64_t np_top = 0;    // number of particles in the top block
static int64_t np_bottom = 0; // number of particles in the bottom block
static double sph_sep_tb = 0; // particle separation in top or bottom blocks
static double pmass = 0;      // particle mass in the middle block
static double pmass_tb = 0;   // particle mass in top or bottom blocks

void set_derived_params() {
  using namespace std;
  using namespace param;

  // boundary tolerance factor
  const double b_tol = 1e-8;

  mbox_max[0] = bbox_max[0] = tbox_max[0] = box_length/2.;
  mbox_min[0] = bbox_min[0] = tbox_min[0] =-box_length/2.;

  mbox_max[1] =  box_width/4.;
  mbox_min[1] = -box_width/4.;
  bbox_min[1] = -box_width/2.;
  bbox_max[1] = -box_width/4.;
  tbox_min[1] =  box_width/4.;
  tbox_max[1] =  box_width/2.;

  if(gdimension == 3){
    mbox_max[2] = bbox_max[2] = tbox_max[2] = box_height/2.;
    mbox_min[2] = bbox_min[2] = tbox_min[2] =-box_height/2.;
  }


  // set physical parameters
  // --  in the top and bottom boxes (tbox and bbox):
  rho_2      =  rho_initial;
  pressure_2 =  pressure_initial;
  vx_2       = -flow_velocity/2.0;
  // -- in the middle box
  rho_1 = rho_2 * KH_density_ratio; // 2.0 by default
  pressure_1 = pressure_2;          // pressures must be equal in KH test
  vx_1       =  flow_velocity/2.0;

  // file to be generated
  std::ostringstream oss;
  oss << initial_data_prefix << ".h5part";
  initial_data_file = oss.str();

  std::cout<<"Boxes: " << std::endl << "up="
    <<tbox_min<<"-"<<tbox_max<<std::endl
    <<" middle="
    <<mbox_min<<"-"<<mbox_max<<std::endl
    <<" bottom="
    <<bbox_min<<"-"<<bbox_max<< std::endl;


  // select particle lattice and kernel function
  particle_lattice::select();
  kernels::select();

  // particle mass and spacing
  SET_PARAM(sph_separation, (box_length*(1.0-b_tol)/(double)(lattice_nx-1)));
  pmass = rho_1*sph_separation*sph_separation;
  if (lattice_type == 1 or lattice_type == 2)
    pmass *= sqrt(3)/2;

  // adjust width of the middle block for symmetry
  double dy = sph_separation;
  if (lattice_type == 1 or lattice_type == 2)
    dy *= sqrt(3)/2;
  double dy_tb = dy; // lattice step in y-direction for top and bottom blocks
  double mbox_width = mbox_max[1] - mbox_min[1];
  mbox_width = (int)(mbox_width/(2*dy))*2*dy;
  mbox_min[1] = -mbox_width/2.;
  mbox_max[1] =  mbox_width/2.;

  if(equal_mass){
    if(gdimension == 3){
      pmass = rho_1*sph_separation*sph_separation*sph_separation;
      if (lattice_type == 1 or lattice_type == 2)
        pmass *= 1./sqrt(2.);
    }
    if(gdimension == 2){
      pmass = rho_1*sph_separation*sph_separation;
      if (lattice_type == 1 or lattice_type == 2)
        pmass *= sqrt(3)/2;
    }
    sph_sep_tb = sph_separation * sqrt(rho_1/rho_2);
    pmass_tb = pmass;
  }
  else {
    sph_sep_tb = sph_separation;
    pmass_tb = pmass * rho_2/rho_1;
  }
  dy_tb = dy * sph_sep_tb/sph_separation;

  // adjust top and bottom blocks
  tbox_min[1] = mbox_max[1] - dy + 0.5*(dy_tb + dy);
  double bbox_width = 2*dy_tb * floor((bbox_max[1] - bbox_min[1])/(2*dy_tb));
  bbox_max[1] = mbox_min[1] - 0.5*(dy_tb + dy) + dy;
  bbox_min[1] = bbox_max[1] - bbox_width - dy_tb;

  // count the number of particles
  np_middle = particle_lattice::count(lattice_type,gdimension,mbox_min,mbox_max,
                                      sph_separation,0);
  np_top    = particle_lattice::count(lattice_type,gdimension,tbox_min,tbox_max,
                                      sph_sep_tb, np_middle);
  np_bottom = particle_lattice::count(lattice_type,gdimension,bbox_min,bbox_max,
                                      sph_sep_tb, np_middle + np_top);

  SET_PARAM(nparticles, np_middle + np_bottom + np_top);

}

//----------------------------------------------------------------------------//
int main(int argc, char * argv[]){
  using namespace param;

  // launch MPI
  int rank, size, provided;
  MPI_Init_thread(&argc,&argv,MPI_THREAD_MULTIPLE,&provided);
  assert(provided>=MPI_THREAD_MULTIPLE);
  MPI_Comm_rank(MPI_COMM_WORLD,&rank);
  MPI_Comm_size(MPI_COMM_WORLD,&size);
  clog_set_output_rank(0);

  // check options list: exactly one option is allowed
  if (argc != 2) {
    print_usage();
    MPI_Finalize();
    exit(0);
  }

  // anything other than 2D is not implemented yet
  assert (gdimension == 2 || gdimension == 3);
  assert (domain_type == 0);

  // set simulation parameters
  param::mpi_read_params(argv[1]);
  set_derived_params();

  // screen output
  std::cout << "Kelvin-Helmholtz instability setup in " << gdimension
       << "D:" << std::endl << " - number of particles: " << nparticles << std::endl
       << " - generated initial data file: " << initial_data_file << std::endl;

  // allocate arrays
  // Position
  double* x = new double[nparticles]();
  double* y = new double[nparticles]();
  double* z = new double[nparticles]();
  // Velocity
  double* vx = new double[nparticles]();
  double* vy = new double[nparticles]();
  double* vz = new double[nparticles]();
  // Acceleration
  double* ax = new double[nparticles]();
  double* ay = new double[nparticles]();
  double* az = new double[nparticles]();
  // Smoothing length
  double* h = new double[nparticles]();
  // Density
  double* rho = new double[nparticles]();
  // Internal Energy
  double* u = new double[nparticles]();
  // Pressure
  double* P = new double[nparticles]();
  // Mass
  double* m = new double[nparticles]();
  // Id
  int64_t* id = new int64_t[nparticles]();
  // Timestep
  double* dt = new double[nparticles]();

  // generate the lattice
  assert (np_middle == particle_lattice::generate( lattice_type,gdimension,
          mbox_min,mbox_max,sph_separation,0,x,y,z));
  assert (np_top    == particle_lattice::generate( lattice_type,gdimension,
          tbox_min,tbox_max,sph_sep_tb,np_middle,x,y,z));
  assert (np_bottom == particle_lattice::generate( lattice_type,gdimension,
          bbox_min,bbox_max,sph_sep_tb,nparticles-np_bottom,x,y,z));

  // max. value for the speed of sound
  double cs = sqrt(poly_gamma*std::max(pressure_1/rho_1,pressure_2/rho_2));

  // suggested timestep
  double timestep = timestep_cfl_factor
                  * sph_separation/std::max(cs, flow_velocity);

  // particle id number
  int64_t posid = 0;
  for(int64_t part=0; part<nparticles; ++part){
    id[part] = posid++;
    if (particle_lattice::in_domain_1d(y[part],
        mbox_min[1], mbox_max[1], domain_type)) {
      P[part] = pressure_1;
      rho[part] = rho_1;
      vx[part] = vx_1;
      m[part] = pmass;
    }
    else {
      P[part] = pressure_2;
      rho[part] = rho_2;
      vx[part] = vx_2;
      m[part] = pmass_tb;
    }

    vy[part] = 0.;

    // Add velocity perturbation a-la Price (2008)
    if(y[part] < 0.25 and y[part] > 0.25 - 0.025)
      vy[part] = KH_A*sin(-2*M_PI*(x[part]+.5)/KH_lambda);
    if(y[part] >-0.25 and y[part] <-0.25 + 0.025)
      vy[part] = KH_A*sin(2*M_PI*(x[part]+.5)/KH_lambda);

    // compute internal energy using gamma-law eos
    u[part] = P[part]/(poly_gamma-1.)/rho[part];

    // particle masses and smoothing length
    m[part] = pmass;
    h[part] = sph_eta * kernels::kernel_width
                      * pow(m[part]/rho[part],1./gdimension);

  } // for part=0..nparticles

  // delete the output file if exists
  remove(initial_data_file.c_str());

  hid_t dataFile = H5P_openFile(initial_data_file.c_str(),H5F_ACC_RDWR);

  int use_fixed_timestep = 1;
  // add the global attributes
  H5P_writeAttribute(dataFile,"nparticles",&nparticles);
  H5P_writeAttribute(dataFile,"timestep",&timestep);
  int dim = gdimension;
  H5P_writeAttribute(dataFile,"dimension",&dim);
  H5P_writeAttribute(dataFile,"use_fixed_timestep",&use_fixed_timestep);

  H5P_setNumParticles(nparticles);
  H5P_setStep(dataFile,0);

  //H5PartSetNumParticles(dataFile,nparticles);
  H5P_writeDataset(dataFile,"x",x,nparticles);
  H5P_writeDataset(dataFile,"y",y,nparticles);
  H5P_writeDataset(dataFile,"z",z,nparticles);
  H5P_writeDataset(dataFile,"vx",vx,nparticles);
  H5P_writeDataset(dataFile,"vy",vy,nparticles);
  H5P_writeDataset(dataFile,"h",h,nparticles);
  H5P_writeDataset(dataFile,"rho",rho,nparticles);
  H5P_writeDataset(dataFile,"u",u,nparticles);
  H5P_writeDataset(dataFile,"P",P,nparticles);
  H5P_writeDataset(dataFile,"m",m,nparticles);
  H5P_writeDataset(dataFile,"id",id,nparticles);

  H5P_closeFile(dataFile);

  delete[] x, y, z, vx, vy, vz, ax, ay, az, h, rho, u, P, m, id, dt;

  MPI_Finalize();
  return 0;
}
