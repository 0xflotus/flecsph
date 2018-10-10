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

#ifndef flecsi_topology_morton_branch_id_h
#define flecsi_topology_morton_branch_id_h

/*!
  \file tree_topology.h
  \authors nickm@lanl.gov
  \date Initial file creation: Apr 5, 2016
 */

#include <map>
#include <unordered_map>
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
#include "flecsi/concurrency/thread_pool.h"
#include "flecsi/data/storage.h"
#include "flecsi/data/data_client.h"
#include "flecsi/topology/index_space.h"
 

namespace flecsi{
namespace topology{

/*!
  This class implements a hashed/Morton-style branch id that can be
  parameterized on arbitrary dimension D and integer type T.
 */
template<
  typename T,
  size_t D
>
class branch_id
{
public:
  using int_t = T;

  static const size_t dimension = D;

  static constexpr size_t bits = sizeof(int_t) * 8;

  static constexpr size_t max_depth = (bits - 1)/dimension;

  branch_id()
  : id_(0)
  {}

  /*!
    Construct a branch id from an array of dimensions and range for each
    dimension. The specified depth may be less than the max allowed depth for
    the branch id.
   */
  template<
    typename S
  >
  branch_id(
    const std::array<point__<S, dimension>, 2>& range,
    const point__<S, dimension>& p,
    size_t depth)
  : id_(int_t(1) << depth * dimension + (bits - 1) % dimension)
  {
    std::array<int_t, dimension> coords;

    for(size_t i = 0; i < dimension; ++i)
    {
      S min = range[0][i];
      S scale = range[1][i] - min;
      coords[i] = (p[i] - min)/scale * (int_t(1) << (bits - 1)/dimension);
    }

    size_t k = 0;
    for(size_t i = max_depth - depth; i < max_depth; ++i)
    {
      for(size_t j = 0; j < dimension; ++j)
      {
        int_t bit = (coords[j] & int_t(1) << i) >> i;
        id_ |= bit << (k * dimension + j);
      }
      ++k;
    }
  }

  constexpr branch_id(const branch_id& bid)
  : id_(bid.id_)
  {}

  /*!
    Get the root branch id (depth 0).
   */
  static
  constexpr
  branch_id
  root()
  {
    return branch_id(int_t(1) << (bits - 1) % dimension);
  }

  /*!
    Get the null branch id.
   */
  static
  constexpr
  branch_id
  null()
  {
    return branch_id(0);
  }

  /*!
    Check if branch id is null.
   */
  constexpr
  bool
  is_null() const
  {
    return id_ == int_t(0);
  }

  /*!
    Find the depth of this branch id.
   */
  size_t
  depth() const
  {
    int_t id = id_;
    size_t d = 0;

    while(id >>= dimension)
    {
      ++d;
    }

    return d;
  }

  branch_id&
  operator=(
    const branch_id& bid
  )
  {
    id_ = bid.id_;
    return *this;
  }

  constexpr
  bool
  operator==(
    const branch_id& bid
  ) const
  {
    return id_ == bid.id_;
  }

  constexpr
  bool
  operator!=(
    const branch_id& bid
  ) const
  {
    return id_ != bid.id_;
  }

  /*!
    Push bits onto the end of this branch id.
   */
  void push(int_t bits)
  {
    assert(bits < int_t(1) << dimension);

    id_ <<= dimension;
    id_ |= bits;
  }

  /*!
    Pop the bits of greatest depth off this branch id.
   */
  void pop()
  {
    assert(depth() > 0);
    id_ >>= dimension;
  }

  /*!
    Pop the depth d bits from the end of this this branch id.
   */
  void pop(
    size_t d
  )
  {
    assert(d >= depth());
    id_ >>= d * dimension;
  }

  /*!
    Return the parent of this branch id (depth - 1)
   */
  constexpr
  branch_id
  parent() const
  {
    return branch_id(id_ >> dimension);
  }

  /*!
    Truncate (repeatedly pop) this branch id until it of depth to_depth.
   */
  void
  truncate(
    size_t to_depth
  )
  {
    size_t d = depth();

    if(d < to_depth)
    {
      return;
    }

    id_ >>= (d - to_depth) * dimension;
  }

  void
  output_(
    std::ostream& ostr
  ) const
  {
    constexpr int_t mask = ((int_t(1) << dimension) - 1) << bits - dimension;

    size_t d = max_depth;

    int_t id = id_;

    while((id & mask) == int_t(0))
    {
      --d;
      id <<= dimension;
    }

    if(d == 0)
    {
      ostr << "1";
      return;
    }
    ostr << "1";
    //ostr<<std::bitset<64>(id);
    id <<= 1 + (bits - 1) % dimension;

    for(size_t i = 1; i <= d; ++i)
    {
      int_t val = (id & mask) >> (bits - dimension);
      ostr << std::bitset<D>(val);
      id <<= dimension;
    }
  }

  int_t
  value_() const
  {
    return id_;
  }

  void
  set_value_(
    int_t value
  )
  {
    id_ = value;
  }

  bool
  operator<(
    const branch_id& bid
  ) const
  {
    return id_ < bid.id_;
  }

  /*!
    Convert this branch id to coordinates in range.
   */
  template<
    typename S
  >
  void
  coordinates(
    const std::array<point__<S, dimension>, 2>& range,
    point__<S, dimension>& p) const
  {
    std::array<int_t, dimension> coords;
    coords.fill(int_t(0));

    int_t id = id_;
    size_t d = 0;

    while(id >> dimension != int_t(0))
    {
      for(size_t j = 0; j < dimension; ++j)
      {
        coords[j] |= (((int_t(1) << j) & id) >> j) << d;
      }

      id >>= dimension;
      ++d;
    }

    constexpr int_t m = (int_t(1) << max_depth) - 1;

    for(size_t j = 0; j < dimension; ++j)
    {
      S min = range[0][j];
      S scale = range[1][j] - min;

      coords[j] <<= max_depth - d;
      p[j] = min + scale * S(coords[j])/m;
    }
  }

private:
  int_t id_;

  constexpr
  branch_id(
    int_t id
  )
  : id_(id)
  {}
};

} // namespace topology 
} // namespace flecsi

#endif // flecsi_topology_morton_branch_id_h
