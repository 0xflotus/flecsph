/*~--------------------------------------------------------------------------~*
 *  @@@@@@@@  @@           @@@@@@   @@@@@@@@ @@
 * /@@/////  /@@          @@////@@ @@////// /@@
 * /@@       /@@  @@@@@  @@    // /@@       /@@
 * /@@@@@@@  /@@ @@///@@/@@       /@@@@@@@@@/@@
 * /@@////   /@@/@@@@@@@/@@       ////////@@/@@
 * /@@       /@@/@@//// //@@    @@       /@@/@@
 * /@@       @@@//@@@@@@ //@@@@@@  @@@@@@@@ /@@
 * //       ///  //////   //////  ////////  //
 *
 * Copyright (c) 2016 Los Alamos National Laboratory, LLC
 * All rights reserved
 *~--------------------------------------------------------------------------~*/

#ifndef flecsi_topology_tree_branch_h
#define flecsi_topology_tree_branch_h

/*!
  \file tree_branch.h
  \authors jloiseau@lanl.gov
  \date Initial file creation: Oct 9 2018
 */

#include <map>
#include <vector>
#include <array>
#include <map>
#include <cmath>
#include <bitset>
#include <algorithm>
#include <cassert>
#include <iostream>
#include <set>
#include <functional>
#include <mutex>
#include <stack>
#include <math.h>
#include <float.h>

#include "flecsi/geometry/point.h"

#include "tree_entity_id.h"
#include "key_id.h"

namespace flecsi {
namespace topology {

/*!
  Tree branch base class.
 */
template<
  typename T,
  size_t D,
  typename E
>
class tree_branch
{

public:
  enum b_locality: size_t{
    LOCAL=0,EMPTY=1,NONLOCAL=2,SHARED=3
  };

public:
  using branch_int_t = T;
  static const size_t dimension = D;
  using branch_id_t = key_id__<T,D>;
  using id_t = branch_id_t;
  static constexpr size_t num_children = 1 << dimension;
  using point_t = point__<E,D>;
  using element_t = E;

  tree_branch()
  {
    int rank;
    MPI_Comm_rank(MPI_COMM_WORLD,&rank);
    owner_ = rank;
  }

  tree_branch(const branch_id_t& id)
  : id_(id)
  {
    int rank;
    MPI_Comm_rank(MPI_COMM_WORLD,&rank);
    owner_ = rank;
  }

  tree_branch(
    const branch_id_t& id,
    const point_t& coordinates,
    const element_t& mass,
    const point_t& bmin,
    const point_t& bmax,
    const b_locality& locality,
    const int& owner)
  : id_(id), coordinates_(coordinates), mass_(mass),
   bmin_(bmin),bmax_(bmax), locality_(locality), owner_(owner)
  {}

  branch_id_t id() const {return id_;}
  branch_id_t key() const {return id_;}
  point_t coordinates(){return coordinates_;};
  element_t mass(){return mass_;};
  //element_t radius(){return radius_;};
  point_t bmin(){return bmin_;};
  point_t bmax(){return bmax_;};

  void set_coordinates(const point_t& coordinates){coordinates_=coordinates;};
  void set_mass(const element_t& mass){mass_ = mass;};
  //void set_radius(const element_t& radius){radius_ = radius;};
  void set_bmax(const point_t& bmax){bmax_ = bmax;};
  void set_bmin(const point_t& bmin){bmin_ = bmin;};
  void set_begin_tree_entities(const size_t& begin_tree_entities){
    begin_tree_entities_ = begin_tree_entities;
  }
  void set_end_tree_entities(const size_t& end_tree_entities){
    end_tree_entities_ = end_tree_entities;
  }
  size_t begin_tree_entities(){ return begin_tree_entities_;}
  size_t end_tree_entities(){ return end_tree_entities_;}



  bool is_leaf() const {return leaf_;}
  void set_leaf(bool leaf){leaf_ = leaf;}
  bool is_valid() const { return true; }
  uint64_t sub_entities() const { return sub_entities_; }
  void set_sub_entities(uint64_t sub_entities){sub_entities_ = sub_entities;}
  void set_locality( b_locality locality){locality_ = locality;}
  b_locality locality() {return locality_;}
  bool is_local() const {
    return locality_ == LOCAL || locality_ == EMPTY || locality_ == SHARED; }
  bool is_shared() const{
    return locality_ == SHARED;
  }
  int owner() {return  owner_;};
  void set_owner(int owner){owner_ = owner;};
  void set_ghosts_local(const bool ghosts_local){ghosts_local_ = ghosts_local;};
  bool ghosts_local(){return ghosts_local_;};
  bool requested(){return requested_;};
  void set_requested(bool requested){requested_ = requested; };

  void insert(const flecsi::topology::entity_id_t& id){
    assert(find(ents_.begin(), ents_.end(), id) == ents_.end());
    ents_.push_back(id);
  } // insert

  int
  size()
  {
    return ents_.size();
  }

  void remove(const flecsi::topology::entity_id_t& id){
    auto itr = find(ents_.begin(), ents_.end(), id);
    assert(itr != ents_.end());
    ents_.erase(itr);
  }

  ~tree_branch(){ents_.clear();}
  auto begin(){return ents_.begin();}
  auto end(){return ents_.end();}
  auto clear(){ents_.clear();}

  char bit_child(){return bit_child_;};
  void add_bit_child(int i){
    assert(!(bit_child_ & 1<<i));
    bit_child_ |= 1<<i;
  };
  void set_bit_child(char bit_child){bit_child_ = bit_child;};
  bool as_child(int i){return bit_child_ & 1<<i;};

protected:
  template<class P>
  friend class tree_topology;

  void
  set_id_(
    branch_id_t id
  )
  {
    id_ = id;
  }

  friend std::ostream& operator<<(std::ostream& os, const tree_branch& b){
    // TODO change regarding to dimension
    os << std::setprecision(10);
    os << "Branch: coord: " <<b.coordinates_;
    os << " mass: " <<b.mass_;
    os << " loc: " << b.locality_;
    os << " id: " << b.id_;
    os << " sub_entities: "<< b.sub_entities_;
    os << " owner: "<<b.owner_;
    os << " bit_child: "<<std::bitset<8>(b.bit_child_);
    return os;
  }

  branch_id_t id_; // Key of this branch
  uint64_t sub_entities_ = 0; // Subentities in this subtree
  bool leaf_ = true;
  b_locality locality_ = EMPTY;
  point_t bmin_;
  point_t bmax_;
  int owner_;
  point_t coordinates_;
  element_t mass_;
  std::vector<flecsi::topology::entity_id_t> ents_;
  //element_t radius_;
  bool ghosts_local_ = true;
  bool requested_ = false;
  char bit_child_ = 0;

  size_t begin_tree_entities_;
  size_t end_tree_entities_;

};

} // namespace topology
} // namespace flecsi

#endif // flecsi_topology_tree_branch_h

/*~-------------------------------------------------------------------------~-*
 * Formatting options for vim.
 * vim: set tabstop=2 shiftwidth=2 expandtab :
 *~-------------------------------------------------------------------------~-*/
