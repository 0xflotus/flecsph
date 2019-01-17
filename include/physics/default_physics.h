/*~--------------------------------------------------------------------------~*
 * Copyright (c) 2017 Los Alamos National Security, LLC
 * All rights reserved.
 *~--------------------------------------------------------------------------~*/

 /*~--------------------------------------------------------------------------~*
 *
 * /@@@@@@@@  @@           @@@@@@   @@@@@@@@ @@@@@@@  @@      @@
 * /@@/////  /@@          @@////@@ @@////// /@@////@@/@@     /@@
 * /@@       /@@  @@@@@  @@    // /@@       /@@   /@@/@@     /@@
 * /@@@@@@@  /@@ @@///@@/@@       /@@@@@@@@@/@@@@@@@ /@@@@@@@@@@
 * /@@////   /@@/@@@@@@@/@@       ////////@@/@@////  /@@//////@@
 * /@@       /@@/@@//// //@@    @@       /@@/@@      /@@     /@@
 * /@@       @@@//@@@@@@ //@@@@@@  @@@@@@@@ /@@      /@@     /@@
 * //       ///  //////   //////  ////////  //       //      //
 *
 *~--------------------------------------------------------------------------~*/

/**
 * @file physics.h
 * @author Julien Loiseau
 * @date April 2017
 * @brief Basic physics implementation
 */

#ifndef _default_physics_h_
#define _default_physics_h_

#include <vector>

// Basic elements for the physics
namespace physics{
  double dt = 0.0;
  double totaltime = 0.0;
  int64_t iteration = 0;
}

#include "params.h"
#include "utils.h"
#include "kernels.h"
#include "tree.h"
#include "eforce.h"

#include "eos.h"
#include "integration.h"
#include "boundary.h"
#include "viscosity.h"

namespace physics{
  using namespace param;

  /**
   * @brief      Compute the density
   * Based on Fryer/05 eq(10)
   * @param      srch  The source's body holder
   * @param      nbsh  The neighbors' body holders
   */
  void
  compute_density(
      body_holder* srch,
      std::vector<body_holder*>& nbsh)
  {
    body* source = srch->getBody();
    double density = 0;
    mpi_assert(nbsh.size()>0);
    for(auto nbh : nbsh){
      body* nb = nbh->getBody();
      double dist = flecsi::distance(source->coordinates(),nb->coordinates());
      mpi_assert(dist>=0.0);
      double kernelresult = kernels::kernel(dist,
            .5*(source->radius()+nb->radius()));
      density += kernelresult*nb->mass();
    } // for
    mpi_assert(density>0);
    source->setDensity(density);
  } // compute_density


  /**
   * @brief      Calculates total energy for every particle
   * @param      srch  The source's body holder
   */
  void set_total_energy (body_holder* srch) {
    assert(srch->is_local());
    body* source = srch->getBody();
    const point_t pos = source->coordinates(),
                  vel = source->getVelocity();
    const double eint = source->getInternalenergy(),
                 epot = external_force::potential(pos);
    double ekin = vel[0]*vel[0];
    for (unsigned short i=1; i<gdimension; ++i)
      ekin += vel[i]*vel[i];
    ekin *= .5;
    source->setTotalenergy(eint + epot + ekin);
  } // set_total_energy


  /**
   * @brief      Subtracts mechanical energy from total energy
   *             to recover internal energy
   * @param      srch  The source's body holder
   */
  void recover_internal_energy (body_holder* srch) {
    body* source = srch->getBody();
    const point_t pos = source->coordinates(),
                  vel = source->getVelocity();
    const double etot = source->getTotalenergy(),
                 epot = external_force::potential(pos);
    double ekin = vel[0]*vel[0];
    for (unsigned short i=1; i<gdimension; ++i)
      ekin += vel[i]*vel[i];
    ekin *= .5;
    const double eint = etot - ekin - epot;
    if (eint < 0.0) {
      std::cerr << "ERROR: internal energy is negative!" << std::endl
                << "particle id: " << source->id()    << std::endl
                << "total energy: " << etot              << std::endl
                << "kinetic energy: " << ekin            << std::endl
                << "particle position: " << pos          << std::endl;
      mpi_assert(false);
    }
    source->setInternalenergy(eint);
  } // recover_internal_energy


  /**
   * @brief      Compute the density, EOS and spundspeed in the same function
   * reduce time to gather the neighbors
   *
   * @param      srch  The source's body holder
   * @param      nbsh  The neighbors' body holders
   */
  void
  compute_density_pressure_soundspeed(
    body_holder* srch,
    std::vector<body_holder*>& nbsh)
  {
    compute_density(srch,nbsh);
    if (thermokinetic_formulation)
      recover_internal_energy(srch);
    eos::compute_pressure(srch);
    eos::compute_soundspeed(srch);
  }


  /**
   * @brief      Calculates the hydro acceleration
   * From CES-Seminar 13/14 - Smoothed Particle Hydrodynamics
   *
   * @param      srch  The source's body holder
   * @param      nbsh  The neighbors' body holders
   */
  void
  compute_acceleration(
    body_holder* srch,
    std::vector<body_holder*>& ngbsh)
  {
    using namespace param;
    body* source = srch->getBody();

    // Reset the accelerastion
    // \TODO add a function to reset in main_driver
    point_t acceleration = {};
    point_t hydro = {};
    const double h_s = source->radius();
    source->setMumax(0.0);

    for(auto nbh : ngbsh){
      body* nb = nbh->getBody();

      if(nb->coordinates() == source->coordinates())
        continue;

      // Compute viscosity
      double visc = viscosity::viscosity(source,nb);

      // Hydro force
      point_t vecPosition = source->coordinates() - nb->coordinates();
      double rho_a = source->getDensity();
      double rho_b = nb->getDensity();
      double pressureDensity
          = source->getPressure()/(rho_a*rho_a)
          + nb->getPressure()/(rho_b*rho_b);

      // Kernel computation
      const double h_n = nb->radius();
      point_t sourcekernelgradient = kernels::gradKernel(
          vecPosition,(h_s+h_n)*.5);
      point_t resultkernelgradient = sourcekernelgradient;

      hydro += nb->mass()*(pressureDensity + visc)
        *resultkernelgradient;

    }
    hydro = -1.0*hydro;
    acceleration += hydro;
    acceleration += external_force::acceleration(srch);
    source->setAcceleration(acceleration);
  } // compute_hydro_acceleration


  /**
   * @brief      Calculates the dudt, time derivative of internal energy.
   * From CES-Seminar 13/14 - Smoothed Particle Hydrodynamics
   *
   * @param      srch  The source's body holder
   * @param      nbsh  The neighbors' body holders
   */
  void compute_dudt(
      body_holder* srch,
      std::vector<body_holder*>& ngbsh)
  {
    body* source = srch->getBody();

    double dudt = 0;
    double dudt_pressure = 0.;
    double dudt_visc = 0.;
    const double h_s = source->radius();

    for(auto nbh: ngbsh){
      body* nb = nbh->getBody();

      if(nb->coordinates() == source->coordinates()){
        continue;
      }

      // Artificial viscosity
      double visc = viscosity::viscosity(source,nb);

      // Compute the gradKernel ij
      const double h_n = nb->radius();
      point_t vecPosition = source->coordinates()-nb->coordinates();
      point_t sourcekernelgradient = kernels::gradKernel(
          vecPosition,(h_s+h_n)*.5);
      space_vector_t resultkernelgradient =
          flecsi::point_to_vector(sourcekernelgradient);

      // Velocity vector
      space_vector_t vecVelocity = flecsi::point_to_vector(
          source->getVelocity() - nb->getVelocity());

      dudt_pressure += nb->mass()*
        flecsi::dot(vecVelocity,resultkernelgradient);
      dudt_visc += visc*nb->mass()*
        flecsi::dot(vecVelocity,resultkernelgradient);
    }

    double P_a = source->getPressure();
    double rho_a = source->getDensity();
    dudt = P_a/(rho_a*rho_a)*dudt_pressure + .5*dudt_visc;

    //Do not change internal energy during relaxation
    if(do_drag && iteration <= relax_steps){
       dudt = 0.0;
    }

    source->setDudt(dudt);
  } // compute_dudt


  /**
   * @brief      Calculates the dedt, time derivative of either
   *             thermokinetic (internal + kinetic) or total
   *             (internal + kinetic + potential) energy.
   * See e.g. Rosswog (2009) "Astrophysical SPH" eq. (34)
   *
   * @param      srch  The source's body holder
   * @param      nbsh  The neighbors' body holders
   */
  void compute_dedt(
      body_holder* srch,
      std::vector<body_holder*>& ngbsh)
  {
    body* source = srch->getBody();

    double dedt = 0;

    const point_t pos_a = source->coordinates(),
                  vel_a = source->getVelocity();
    const double h_a = source->radius(),
                 P_a = source->getPressure(),
                 rho_a = source->getDensity();
    const double Prho2_a = P_a/(rho_a*rho_a);

    for(auto nbh: ngbsh){
      body* nb = nbh->getBody();
      const point_t pos_b = nb->coordinates();
      if(pos_a == pos_b)
        continue;

      const double h_b = nb->radius();
      // Compute the \nabla_a W_ab
      const point_t Da_Wab = kernels::gradKernel(pos_a - pos_b, (h_a+h_b)*.5),
                    vel_b = nb->getVelocity();

      // va*DaWab and vb*DaWab
      double va_dot_DaWab = vel_a[0]*Da_Wab[0];
      double vb_dot_DaWab = vel_b[0]*Da_Wab[0];
      for (unsigned short i=1; i<gdimension; ++i) {
        va_dot_DaWab += vel_a[i]*Da_Wab[i],
        vb_dot_DaWab += vel_b[i]*Da_Wab[i];
      }

      const double m_b = nb->mass(),
                   P_b = nb->getPressure(),
                   rho_b = nb->getDensity();
      const double Prho2_b = P_b/(rho_b*rho_b),
                   Pi_ab = viscosity::viscosity(source,nb);

      // add this neighbour's contribution
      dedt -= m_b*( Prho2_a*vb_dot_DaWab + Prho2_b*va_dot_DaWab
                + .5*Pi_ab*(va_dot_DaWab + vb_dot_DaWab));
    }

    source->setDedt(dedt);
  } // compute_dedt


  /**
   * @brief      Compute the timestep from acceleration and mu
   * From CES-Seminar 13/14 - Smoothed Particle Hydrodynamics
   *
   * @param      srch   The source's body holder
   */
  void compute_dt(body_holder* srch) {
    body* source = srch->getBody();
    const double tiny = 1e-24;
    const double mc   = 0.6; // constant in denominator for viscosity

    // particles separation around this particle
    const double dx = source->radius()
                    / (sph_eta*kernels::kernel_width);

    // timestep based on particle velocity
    const point_t vel = source->getVelocity();
    const double vn  = norm_point(vel);
    const double dt_v = dx/(vn + tiny);

    // timestep based on acceleration
    const double acc = norm_point(source->getAcceleration());
    const double dt_a = sqrt(dx/(acc + tiny));

    // timestep based on sound speed and viscosity
    const double max_mu_ab = source->getMumax();
    const double cs_a = source->getSoundspeed();
    const double dt_c = dx/ (tiny + cs_a*(1 + mc*sph_viscosity_alpha)
                                  + mc*sph_viscosity_beta*max_mu_ab);

    // minimum timestep
    double dtmin = timestep_cfl_factor * std::min(std::min(dt_v,dt_a), dt_c);

    // timestep based on positivity of internal energy
    if (thermokinetic_formulation) {
      const double eint = source->getInternalenergy();
      const point_t pos = source->coordinates();
      const double epot = external_force::potential(pos);
      double epot_next;
      int i;
      for(i=0; i<20; ++i) {
        epot_next = external_force::potential(pos + dtmin*vel);
        if(epot_next - epot < eint*0.5) break;
        dtmin *= 0.5;
      }
      assert (i<20);
    }

    source->setDt(dtmin);
  }


  /**
   * @brief      Reduce adaptive timestep and set its value
   *
   * @param      bodies   Set of bodies
   */
  void set_adaptive_timestep(
      std::vector<body_holder>& bodies
      )
  {
    double dtmin = 1e24; // some ludicrous number

    #pragma omp parallel for reduction(min:dtmin)
    for(size_t i = 0 ; i < bodies.size(); ++i){
      if(!bodies[i].is_local()) continue;
      dtmin = std::min(dtmin, bodies[i].getBody()->getDt());
    }

    mpi_utils::reduce_min(dtmin);

    if (dtmin < physics::dt)
      physics::dt = std::min(dtmin, physics::dt/2.0);

    if (dtmin > 2.0*physics::dt)
      physics::dt = physics::dt*2.0;
  }


  void
  compute_smoothinglength(
      std::vector<body_holder>& bodies)
  {
    if (gdimension == 1) {
      #pragma omp parallel for
      for(size_t i = 0 ; i < bodies.size(); ++i){
        if(!bodies[i].is_local()) continue;
        auto particle = bodies[i].getBody();
        double m_b   = particle->mass();
        double rho_b = particle->getDensity();
        particle->set_radius(
          m_b/rho_b * sph_eta*kernels::kernel_width);
      }
    }
    else if (gdimension == 2) {
      #pragma omp parallel for
      for(size_t i = 0 ; i < bodies.size(); ++i){
        if(!bodies[i].is_local()) continue;
        auto particle = bodies[i].getBody();
        double m_b   = particle->mass();
        double rho_b = particle->getDensity();
        particle->set_radius(
          sqrt(m_b/rho_b) * sph_eta*kernels::kernel_width);
      }
    }
    else {
      #pragma omp parallel for
      for(size_t i = 0 ; i < bodies.size(); ++i){
        if(!bodies[i].is_local()) continue;
        auto particle = bodies[i].getBody();
        double m_b   = particle->mass();
        double rho_b = particle->getDensity();
        particle->set_radius(
          cbrt(m_b/rho_b) * sph_eta*kernels::kernel_width);
      }
    } // if gdimension
  }


  /**
   * @brief update smoothing length for particles (Rosswog'09, eq.51)
   *
   * ha = eta/N \sum_b pow(m_b / rho_b,1/dimension)
   */
  void compute_average_smoothinglength(
      std::vector<body_holder>& bodies,
      int64_t nparticles) {
    compute_smoothinglength(bodies);
    // Compute the total
    double total = 0.;

    #pragma omp parallel for reduction(+:total)
    for(size_t i = 0 ; i < bodies.size(); ++i)
    {
      if(!bodies[i].is_local()) continue;
      total += bodies[i].getBody()->radius();
    }

    // Add up with all the processes
    MPI_Allreduce(MPI_IN_PLACE,&total,1,MPI_DOUBLE,MPI_SUM,MPI_COMM_WORLD);

    // Compute the new smoothing length
    double new_h = 1./(double)nparticles * total;
    #pragma omp parallel for
    for(size_t i = 0 ; i < bodies.size(); ++i){
      if(!bodies[i].is_local()) continue;
      bodies[i].getBody()->set_radius(new_h);
    }
  }
}; // physics

#endif // _default_physics_h_
