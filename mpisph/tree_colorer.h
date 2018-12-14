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
 * @file tree_colorer.h
 * @author Julien Loiseau
 * @date April 2017
 * @brief Function needed for MPI distribution of the bodies
 */

#ifndef _mpisph_tree_colorer_h_
#define _mpisph_tree_colorer_h_

#include <fstream>
#include <iostream>
#include <vector>
#include <numeric>
#include <omp.h>

#include <boost/sort/sort.hpp>

#include "tree.h"
#include "utils.h"
#include "default_physics.h"

#include "params.h" // For the variable smoothing length

using namespace mpi_utils;

//#define BOOST_PARALLEL 1
// Output the data regarding the distribution for debug
#define OUTPUT_TREE_INFO 1

/**
* Structrue for branch distribution
*/
struct mpi_branch_t{
  point_t coordinates;
  double mass;
  point_t min;
  point_t max;
  entity_key_t key;
  int owner;
  size_t sub_entities;
};


/**
 * @brief       Structure to keep the data during the ghosts sharing.
 * Fill the structure during compute_ghosts and then exchange during
 * refresh_ghosts ''
 */
struct mpi_ghosts_t{
  std::vector<body> sbodies;
  std::vector<body> rbodies;
  std::vector<int>  nsbodies;
  std::vector<int>  soffsets;
  std::vector<int>  nrbodies;
  std::vector<int>  roffsets;
  std::vector<body_holder*> sholders;
  std::vector<body_holder*> rholders;
};

struct body_holder_mpi_t{
  static const size_t dimension = gdimension;
  using element_t = type_t;
  using point_t = flecsi::point__<element_t, dimension>;

  point_t position;
  int owner;
  double mass;
  flecsi::topology::entity_id_t id;
  double h;
  entity_key_t key;
};

/**
 * @brief      All the function and buffers for the tree_colorer.
 *
 * @tparam     T     Type of the class
 * @tparam     D     Dimension for the problem
 * @todo fix it for type
 */
template<
  typename T,
  size_t D
>
class tree_colorer
{
private:
  // For all the MPI_Alltoallv communications
  std::vector<int> rcount;
  std::vector<int> scount;
  std::vector<int> roffset;
  std::vector<int> soffset;

  // Communication of body_holders
  MPI_Datatype MPI_BH_T;

  // To share the ghosts data within the radius
  mpi_ghosts_t ghosts_data;

  const int criterion_branches = 1; // Number of sub-entities in the branches
  const size_t noct = 256*1024;        // Number of octets used for quicksort

  void reset_buffers()
  {
    int rank, size;
    MPI_Comm_size(MPI_COMM_WORLD,&size);
    MPI_Comm_rank(MPI_COMM_WORLD,&rank);

    rcount.resize(size);
    scount.resize(size);
    roffset.resize(size);
    soffset.resize(size);

    ghosts_data.sbodies.resize(size);
    ghosts_data.rbodies.resize(size);
    ghosts_data.nsbodies.resize(size);
    ghosts_data.nrbodies.resize(size);
    ghosts_data.roffsets.resize(size);
    ghosts_data.soffsets.resize(size);

    std::fill(rcount.begin(),rcount.end(),0);
    std::fill(scount.begin(),scount.end(),0);
    std::fill(roffset.begin(),roffset.end(),0);
    std::fill(soffset.begin(),soffset.end(),0);
  }

public:
  static const size_t dimension = D;
  using point_t = flecsi::point__<T,dimension>;


  tree_colorer(){
    int rank, size;
    MPI_Comm_size(MPI_COMM_WORLD,&size);
    MPI_Comm_rank(MPI_COMM_WORLD,&rank);
    rcount.resize(size);
    scount.resize(size);
    roffset.resize(size);
    soffset.resize(size);

    ghosts_data.sbodies.resize(size);
    ghosts_data.rbodies.resize(size);
    ghosts_data.nsbodies.resize(size);
    ghosts_data.nrbodies.resize(size);
    ghosts_data.roffsets.resize(size);
    ghosts_data.soffsets.resize(size);

    // Create the types for communications
    // Create datatype
    body_holder_mpi_t tmp;
    MPI_Datatype type[5] =
      {MPI_DOUBLE,MPI_INT,MPI_DOUBLE,MPI_INT64_T,MPI_DOUBLE};
    int blocklen[5] = { gdimension, 2, 1, 1, 2};
    MPI_Aint disp[5];
    disp[0] = 0;
    disp[1] = sizeof(double)*gdimension;
    disp[2] = disp[1] + sizeof(int)*2; // Times 2 due to alignement
    disp[3] = disp[2] + sizeof(double);
    disp[4] = disp[3] + sizeof(int64_t);
    MPI_Type_create_struct(5, blocklen, disp, type, &MPI_BH_T);
    MPI_Type_commit(&MPI_BH_T);

    int size_BH;
    MPI_Type_size(MPI_BH_T,&size_BH);
    rank|| clog(trace) << "Size of MPI_BH_T: "<<size_BH<< " BH: "<<sizeof(body_holder_mpi_t)<<std::endl;
  }

  ~tree_colorer(){}

/*~---------------------------------------------------------------------------*
 * Function for sorting and distribution
 *~---------------------------------------------------------------------------*/

  /**
  * @brief      Sorting of the input particles or current particles using MPI.
  * This method is composed of several steps to implement the quick sort:
  * - Each process sorts its local particles
  * - Each process generate a subset of particles to fit the byte size limit
  * to send
  * - Each subset if send to the master (Here 0) who generates the pivot for
  * quick sort
  * - Pivot are send to each processes and they create buckets based on the
  * pivot
  * - Each bucket is send to the owner
  * - Each process sorts its local particles again
  * This is the first implementation, it can be long for the first sorting but
  * but then as the particles does not move very fast, the particles on the
  * edge are the only ones shared
  *
  * @param      rbodies       The rbodies, local bodies of this process.
  * @param[in]  totalnbodies  The totalnbodies on the overall simulation.
  */
  void mpi_qsort(
    std::vector<body>& rbodies,
    int totalnbodies)
  {
    int size, rank;
    MPI_Comm_size(MPI_COMM_WORLD,&size);
    MPI_Comm_rank(MPI_COMM_WORLD,&rank);

    // Sort the keys
    // Use boost parallel sort
#ifdef BOOST_PARALLEL
    boost::sort::block_indirect_sort(
#else
    std::sort(
#endif
        rbodies.begin(),rbodies.end(),
        [](auto& left, auto& right){
        if(left.key() < right.key()){
          return true;
        }
        if(left.key() == right.key()){
          return left.id() < right.id();
        }
        return false;
      }); // sort

    // If one process, done
    if(size==1){
      clog(trace)<<"Local particles: "<<totalnbodies<<std::endl;
      return;
    } // if

    std::vector<std::pair<entity_key_t,int64_t>> splitters;
    generate_splitters_samples(splitters,rbodies,totalnbodies);

    // The rbodies array is already sorted. We just need to determine the
    // limits for each process
    // Reset the class buffer
    reset_buffers();

    int cur_proc = 0;

    assert(splitters.size() == size-1+2);

    int64_t nbodies = rbodies.size();
    for(size_t i = 0L ; i < nbodies; ++i){
      if(rbodies[i].key() >= splitters[cur_proc].first &&
        rbodies[i].key() < splitters[cur_proc+1].first){
          scount[cur_proc]++;
        }else{
          i--;
          cur_proc++;
        }
    }

    // Check that we considered all the bodies
    assert( std::accumulate(scount.begin(), scount.end(), 0) == rbodies.size());

    std::vector<body> recvbuffer;

    // Direct exchange using point to point
    mpi_alltoallv_p2p(scount,rbodies,recvbuffer);

    rbodies.clear();
    rbodies = recvbuffer;

#ifdef OUTPUT
    std::vector<int> totalprocbodies;
    totalprocbodies.resize(size);
    int mybodies = rbodies.size();
    // Share the final array size of everybody
    MPI_Allgather(&mybodies,1,MPI_INT,&totalprocbodies[0],1,MPI_INT,
      MPI_COMM_WORLD);
    #ifdef OUTPUT_TREE_INFO
    std::ostringstream oss;
    oss<<"Repartition: ";
    for(auto num: totalprocbodies)
      oss<<num<<";";
    double end = omp_get_wtime();
    rank|| clog(trace)<<oss.str()<<std::endl;
    #endif
#endif // OUTPUT
  } // mpi_qsort


  /**
   * @brief      Exchange the useful branches of the current tree of the procs.
   * There is several ways to share. Here we look for the particles in the
   * global bounding box and share them. The branches are constructed directly
   * by the FleCSI tree structure. A better way should be to share branches
   * instead. But we need to change the tree structure in FleCSI for that.
   *
   * This function provide the non local particles that are use to find the
   * ghosts.
   *
   * @param      tree             The tree
   * @param      rbodies          The rbodies, local bodies of this process
   * @param      ranges           The ranges, the key range of all the processes
   * @param      range_total      The range total of all the particles
   */
  void
  mpi_branches_exchange(
    tree_topology_t& tree,
    std::vector<body>& rbodies,
    std::vector<std::array<point_t,2>>& ranges,
    std::array<point_t,2>& range_total
  )
  {

    int rank,size;
    MPI_Comm_rank(MPI_COMM_WORLD,&rank);
    MPI_Comm_size(MPI_COMM_WORLD,&size);

#ifdef OUTPUT
    MPI_Barrier(MPI_COMM_WORLD);
    #ifdef OUTPUT_TREE_INFO
    rank|| clog(trace)<<"Branches repartition" << std::endl << std::flush;
    #endif
#endif

  // Send them 2 by 2
  // Use hypercube communication
  std::vector<branch_t*> search_branches;
  tree.find_sub_cells(
    tree.root(),
    1,
    search_branches);

  // Copy them localy
  std::vector<mpi_branch_t> branches;
  for(auto b: search_branches){
    assert(b->sub_entities() > 0);
    branches.push_back(mpi_branch_t{b->coordinates(),b->mass(),b->bmin(),b->bmax(),
      b->id(),b->owner(),b->sub_entities()});
  }

  // Do the hypercube communciation to share the branches
  // Add them in the tree in the same time
  int dim = log2(size);
  // In case of non power two, consider dim + 1
  if(1<<dim < size)
    dim++;
  int nsend;
  int last = branches.size();
  for(int i = 0; i < dim; ++i){
    nsend = branches.size();
    int partner = rank ^ (1<<i);
    assert(partner != rank);
    // In case of non power 2
    MPI_Request request;
    if(partner < size){
      // I send
      if(rank < partner){
        // Send size
        MPI_Isend(&(branches[0]),nsend*sizeof(mpi_branch_t),MPI_BYTE,partner,1,
          MPI_COMM_WORLD,&request);
      }else{
        MPI_Status status;
        // Read the size of the message
        MPI_Probe(partner,1,MPI_COMM_WORLD,&status);
        // Get the size
        int nrecv = 0;
        MPI_Get_count(&status, MPI_BYTE, &nrecv);
        branches.resize(branches.size()+nrecv/sizeof(mpi_branch_t));
        MPI_Recv(&(branches[last]), nrecv, MPI_BYTE, partner, 1,
           MPI_COMM_WORLD, MPI_STATUS_IGNORE);
        last = branches.size();
      }
      // Other rank send
      if(rank > partner){
        // Send size
        MPI_Isend(&(branches[0]),nsend*sizeof(mpi_branch_t),MPI_BYTE,partner,1,
          MPI_COMM_WORLD,&request);
      }else{
        MPI_Status status;
        // Read the size of the message
        MPI_Probe(partner,1,MPI_COMM_WORLD,&status);
        // Get the size
        int nrecv = 0;
        MPI_Get_count(&status, MPI_BYTE, &nrecv);
        branches.resize(branches.size()+nrecv/sizeof(mpi_branch_t));
        MPI_Recv(&(branches[last]), nrecv, MPI_BYTE, partner, 1,
           MPI_COMM_WORLD, MPI_STATUS_IGNORE);
        last = branches.size();
      }
      //Wait for request
      MPI_Status status;
      MPI_Wait(&request,&status);
    }
  }

  // Total branches
  rank || clog(trace)<<rank<<"total branches: "<<branches.size()<<std::endl;

#if DEBUG
  // Check if everyone have the same number
  int nbranches = branches.size();
  int result;
  MPI_Reduce(&nbranches, &result, 1, MPI_INT, MPI_SUM, 0, MPI_COMM_WORLD);
  if(rank == 0){
    assert(result == branches.size()*size);
  }
#endif

  // Add these branches informations in the tree
  for(auto b: branches){
    if(b.owner != rank){
      tree.insert_branch(b.coordinates,b.mass,b.min,b.max,b.key,
        b.owner,b.sub_entities);
    }
  }

#ifdef OUTPUT_TREE_INFO
    MPI_Barrier(MPI_COMM_WORLD);
    rank || clog(trace)<<".done "<<std::endl;
#endif

  }

/*~---------------------------------------------------------------------------*
 * Utils functions
 *~---------------------------------------------------------------------------*/

  /**
   * @brief      Compute the global range of all the particle system
   *
   * @param      bodies           The bodies, local of this process
   * @param      range            The range computed in that function
   */
  void
  mpi_compute_range(
    const std::vector<body>& bodies,
    std::array<point_t,2>& range)
  {
    int rank, size;
    MPI_Comm_size(MPI_COMM_WORLD,&size);
    MPI_Comm_rank(MPI_COMM_WORLD,&rank);

    // Compute the local range
    range_t lrange;

    lrange[1] = bodies.back().coordinates();
    lrange[0] = bodies.back().coordinates();

#pragma omp parallel
{
  range_t trange;
  trange[1] = bodies.back().coordinates();
  trange[0] = bodies.back().coordinates();

#pragma omp parallel for
    for(size_t i = 0 ; i < bodies.size(); ++i){
      for(size_t d = 0; d < gdimension; ++d){

        if(bodies[i].coordinates()[d]+
            bodies[i].radius()>trange[1][d])
          trange[1][d] = bodies[i].coordinates()[d]+
                        bodies[i].radius();

        if(bodies[i].coordinates()[d]-
            bodies[i].radius()<trange[0][d])
          trange[0][d] = bodies[i].coordinates()[d]-
                        bodies[i].radius();
      }
    }
#pragma omp critical
    for(size_t d = 0 ; d < gdimension; ++d)
    {
      lrange[1][d] = std::max(lrange[1][d],trange[1][d]);
      lrange[0][d] = std::min(lrange[0][d],trange[0][d]);
    }
}

    double max[gdimension];
    double min[gdimension];
    for(size_t i=0; i<gdimension; ++i){
      max[i] = lrange[1][i];
      min[i] = lrange[0][i];
    }

    // Do the MPI Reduction
    MPI_Allreduce(MPI_IN_PLACE,max,dimension,MPI_DOUBLE,MPI_MAX,
        MPI_COMM_WORLD);
    MPI_Allreduce(MPI_IN_PLACE,min,dimension,MPI_DOUBLE,MPI_MIN,
        MPI_COMM_WORLD);

    for(size_t d = 0; d < gdimension ; ++d){
      range[0][d] = min[d];
      range[1][d] = max[d];
    }
  }


/**
 * @brief      Use in mpi_qsort to generate the splitters to sort the particles
 * in the quick sort algorithm
 * In this function we take some samplers of the total particles and the root
 * determines the splitters
 * This version is based on the sample splitter algorithm but we generate more
 * samples on each process
 *
 * @param      splitters  The splitters used in the qsort in mpi_qsort
 * @param[in]  rbodies  The local bodies of the process
 */
  void
  generate_splitters_samples(
    std::vector<std::pair<entity_key_t,int64_t>>& splitters,
    std::vector<body>& rbodies,
    const int64_t totalnbodies
  ){
    int rank, size;
    MPI_Comm_rank(MPI_COMM_WORLD,&rank);
    MPI_Comm_size(MPI_COMM_WORLD,&size);

    // Create a vector for the samplers
    std::vector<std::pair<entity_key_t,int64_t>> keys_sample;
    // Number of elements for sampling
    // In this implementation we share up to 256KB to
    // the master.
    size_t maxnsamples = noct/sizeof(std::pair<entity_key_t,int64_t>);
    int64_t nvalues = rbodies.size();
    size_t nsample = maxnsamples*((double)nvalues/(double)totalnbodies);

    if(nvalues<(int64_t)nsample){nsample = nvalues;}

    for(size_t i=0;i<nsample;++i){
      int64_t position = (nvalues/(nsample+1.))*(i+1.);
      keys_sample.push_back(std::make_pair(rbodies[position].key(),
      rbodies[position].id()));
    } // for
    assert(keys_sample.size()==(size_t)nsample);

    std::vector<std::pair<entity_key_t,int64_t>> master_keys;
    std::vector<int> master_recvcounts;
    std::vector<int> master_offsets;
    int master_nkeys = 0;

    if(rank==0){
      master_recvcounts.resize(size);
    } // if

    // Echange the number of samples
    MPI_Gather(&nsample,1,MPI_INT,
      &master_recvcounts[0],1,MPI_INT,0,MPI_COMM_WORLD);

    // Master
    // Sort the received keys and create the pivots
    if(rank == 0){
      master_offsets.resize(size);
      master_nkeys = std::accumulate(
        master_recvcounts.begin(),master_recvcounts.end(),0);
      if(totalnbodies<master_nkeys){master_nkeys=totalnbodies;}
      // Number to receiv from each process
      for(int i=0;i<size;++i){
        master_recvcounts[i]*=sizeof(std::pair<entity_key_t,int64_t>);
      } // for
      std::partial_sum(master_recvcounts.begin(),master_recvcounts.end(),
        &master_offsets[0]);
      master_offsets.insert(master_offsets.begin(),0);
      master_keys.resize(master_nkeys);
    } // if

    MPI_Gatherv(&keys_sample[0],nsample*sizeof(std::pair<entity_key_t,int64_t>)
      ,MPI_BYTE,&master_keys[0],&master_recvcounts[0],&master_offsets[0]
      ,MPI_BYTE,0,MPI_COMM_WORLD);

    // Generate the splitters, add zero and max keys
    splitters.resize(size-1+2);
    if(rank==0){
      std::sort(master_keys.begin(),master_keys.end(),
        [](auto& left, auto& right){
          if(left.first < right.first){
            return true;
          }
          if(left.first == right.first){
            return left.second < right.second;
          }
          return false;
        });

      splitters[0].first = entity_key_t::min();
      splitters[0].second = 0L;
      splitters[size].first = entity_key_t::max();
      splitters[size].second = LONG_MAX;

      for(int i=0;i<size-1;++i){
        int64_t position = (master_nkeys/size)*(i+1);
        splitters[i+1] = master_keys[position];
        assert(splitters[i+1].first > splitters[0].first &&
          splitters[i+1].first < splitters[size].first);
      } // for

      // Print the keys
      //for(auto k: splitters){
      //  std::cout<<k.first<<std::endl;
      //}
    } // if

    // Bradcast the splitters
    MPI_Bcast(&splitters[0],(size-1+2)*sizeof(std::pair<entity_key_t,int64_t>)
    ,MPI_BYTE,0,MPI_COMM_WORLD);
  }

}; // class tree_colorer

#endif // _mpisph_tree_colorer_h_
