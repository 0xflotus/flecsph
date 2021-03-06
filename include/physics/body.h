/*~--------------------------------------------------------------------------~*
 * Copyright (c) 2017 Triad National Security, LLC
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
 * @file body.h
 * @author Julien Loiseau
 * @date April 2017
 * @brief Representation of a body, a particle for our SPH implementation
 */

#ifndef body_h
#define body_h

#define OUTPUT

#include "flecsi/geometry/point.h"
#include "tree_topology/entity.h"
#include "user.h"

enum particle_type_t : int {NORMAL = 0 ,WALL = 1};

class body : public flecsi::topology::entity<type_t,uint64_t,gdimension> {

  static const size_t dimension = gdimension;
  using element_t = type_t;
  using point_t = flecsi::point__<element_t, dimension>;

public:

   body(): entity(), type_(NORMAL)
   {};

  double getPressure() const{return pressure_;}
  double getSoundspeed() const{return soundspeed_;}
  double getEntropy() const{return entropy_;}
  double getElectronfraction() const{return electronfraction_;}
  double getDensity() const{return density_;}
  point_t getVelocity() const{return velocity_;}
  point_t getVelocityhalf() const{return velocityhalf_;}
  point_t getAcceleration() const{return acceleration_;}
  particle_type_t type() const {return type_;};

  point_t getLinMomentum() const {
    point_t res = {};
    for(size_t i = 0 ; i < dimension; ++i){
      res[i] = velocity_[i] * mass_;
    }
    return res;
  };
  double getDt(){return dt_;};
  double getMumax(){return mumax_;}
  particle_type_t getType() const {return type_;};
  bool is_wall(){return type_ == 1;};

  void setAcceleration(point_t acceleration){acceleration_ = acceleration;}
  void setVelocity(point_t velocity){velocity_ = velocity;}
  void setVelocityhalf(point_t velocityhalf){velocityhalf_ = velocityhalf;}
  void setSoundspeed(double soundspeed){soundspeed_ = soundspeed;}
  void setPressure(double pressure){pressure_ = pressure;}
  void setEntropy(double entropy){entropy_ = entropy;}
  void setElectronfraction(double electronfraction){electronfraction_ = electronfraction;}
  void setDensity(double density){density_ = density;}
  void setDt(double dt){dt_ = dt;};
  void setMumax(double mumax){mumax_ = mumax;};
  void setType(particle_type_t type){type_ = type;};
  void setType(int type){type_= static_cast<particle_type_t>(type);};

  // Dependent of the problem
    double getInternalenergy() const{return internalenergy_;}
    void setInternalenergy(double internalenergy)
        {internalenergy_=internalenergy;}
    double getTotalenergy() const{return totalenergy_;}
    void setTotalenergy(double totalenergy) {totalenergy_=totalenergy;}
    void setDudt(double dudt){dudt_ = dudt;};
    void setDedt(double dedt){dedt_ = dedt;};
    double getDudt(){return dudt_;};
    double getDedt(){return dedt_;};
    double getAdiabatic() const{return adiabatic_;}
    void setAdiabatic(double adiabatic){adiabatic_ = adiabatic;};
    double getDadt() const{return dadt_;};
    void setDadt(double dadt){dadt_ = dadt;};

  friend std::ostream& operator<<(std::ostream& os, const body& b){
    // TODO change regarding to dimension
    os << std::setprecision(10);
    os << "Particle: Pos: " <<b.coordinates_ << " rho: " << b.density_;
    os << " h: " << b.radius_;
    os << " P: " << b.pressure_;
    os << " v: " << b.velocity_ ;
    os << " m: " << b.mass_;
    os << " u: " << b.internalenergy_;
    os << " cs: " << b.soundspeed_;
    os << " a: " << b.acceleration_;
    os << " id: " << b.id_;
    os << " key: "<<b.key_;
    os << " owner: "<<b.owner_;
    return os;
  }

private:
  point_t velocity_;
  point_t velocityhalf_;
  point_t acceleration_;
  double density_;
  double pressure_;
  double entropy_;
  double electronfraction_;
  double soundspeed_;
  double internalenergy_;
  double totalenergy_;
  double dudt_;
  double dedt_;
  double adiabatic_;
  double dadt_;
  double dt_;
  double mumax_;
  particle_type_t type_;
}; // class body

#endif // body_h
