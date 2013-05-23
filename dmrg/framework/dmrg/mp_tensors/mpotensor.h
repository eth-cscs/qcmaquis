/*****************************************************************************
 *
 * MAQUIS DMRG Project
 *
 * Copyright (C) 2011-2011 by Bela Bauer <bauerb@phys.ethz.ch>
 *
 *****************************************************************************/

#ifndef MPOTENSOR_H
#define MPOTENSOR_H

#include "dmrg/utils/stream_storage.h"
#include "dmrg/block_matrix/block_matrix.h"
#include "dmrg/block_matrix/indexing.h"
#include "utils/function_objects.h"

#include <iostream>
#include <set>

template<class Matrix, class SymmGroup>
class Boundary
{
public:
    typedef typename maquis::traits::scalar_type<Matrix>::type scalar_type;
    typedef typename Matrix::value_type value_type;
    typedef std::pair<typename SymmGroup::charge, std::size_t> access_type;
    
    Boundary(Index<SymmGroup> const & ud = Index<SymmGroup>(),
             Index<SymmGroup> const & ld = Index<SymmGroup>(),
             std::size_t ad = 1)
    : data_(ad, block_matrix<Matrix, SymmGroup>(ud, ld))
//    , upper_i(ud), lower_i(ld)
    { }
    
    template <class OtherMatrix>
    Boundary(Boundary<OtherMatrix, SymmGroup> const& rhs)
    {
        data_.reserve(rhs.aux_dim());
        for (std::size_t n=0; n<rhs.aux_dim(); ++n)
            data_.push_back(rhs[n]);
    }
    
    std::size_t aux_dim() const { return data_.size(); }
    
    value_type & operator()(std::size_t i, access_type j, access_type k) { return data_[i](j, k); } // I hope this is never used (30.04.2012 / scalar/value discussion)
    value_type const & operator()(std::size_t i, access_type j, access_type k) const { return data_[i](j, k); }
    
    friend struct contraction;
    
    std::vector<scalar_type> traces() const
    {
        std::vector<scalar_type> ret; ret.reserve(data_.size());
        for (size_t k=0; k < data_.size(); ++k) ret.push_back(data_[k].trace());
        return ret;
    }
    
#ifdef HAVE_ALPS_HDF5
    void load(alps::hdf5::archive & ar)
    {
        ar >> alps::make_pvp("data", data_);
    }
    
    void save(alps::hdf5::archive & ar) const
    {
        ar << alps::make_pvp("data", data_);
    }
#endif
    
    block_matrix<Matrix, SymmGroup> & operator[](std::size_t k) { return data_[k]; }
    block_matrix<Matrix, SymmGroup> const & operator[](std::size_t k) const { return data_[k]; }
    
    bool reasonable() const
    {
        for (size_t i=0; i<data_.size(); ++i)
            if (! data_[i].reasonable())
                return false;
        return true;
    }
    
private:
    std::vector<block_matrix<Matrix, SymmGroup> > data_;
//    Index<SymmGroup> upper_i, lower_i;
};

namespace MPOTensor_detail
{
    struct pair_cmp
    {
        bool operator()(std::pair<std::size_t, std::size_t> const & i,
                        std::pair<std::size_t, std::size_t> const & j) const
        {
            if (i.first < j.first)
                return true;
            else if (i.first > j.first)
                return false;
            else
                return i.second < j.second;
        }
    };
}

template <class Matrix, class SymmGroup> class column_iterator;
template <class Matrix, class SymmGroup> class compressor;
template <class Matrix, class SymmGroup> class MPOIndexer;

template<class Matrix, class SymmGroup>
class MPOTensor
{
    typedef std::pair<std::size_t, std::size_t> key_t;
    typedef block_matrix<Matrix, SymmGroup> value_t;
    typedef std::map<key_t, value_t, MPOTensor_detail::pair_cmp> data_t;
    
    typedef std::set<std::size_t> used_set_t;
    
public:
    typedef typename Matrix::value_type value_type;
    typedef typename maquis::traits::scalar_type<Matrix>::type scalar_type;
    typedef std::pair<typename SymmGroup::charge, std::size_t> access_type;
    
    MPOTensor(std::size_t = 1, std::size_t = 1);
    
    std::size_t row_dim() const;
    std::size_t col_dim() const;
    
    value_type & operator()(std::size_t left_index,
                            std::size_t right_index,
                            access_type const & ket_index,
                            access_type const & bra_index);
    value_type const & operator()(std::size_t left_index,
                                  std::size_t right_index,
                                  access_type const & ket_index,
                                  access_type const & bra_index) const;
    
    block_matrix<Matrix, SymmGroup> const & operator()(std::size_t left_index,
                                                       std::size_t right_index) const;
    block_matrix<Matrix, SymmGroup> & operator()(std::size_t left_index,
                                                 std::size_t right_index);
    
    void multiply_by_scalar(const scalar_type&);
    void divide_by_scalar(const scalar_type&);
#ifdef AMBIENT
    void persist() const;
#endif
    
    bool has(std::size_t left_index, std::size_t right_index) const;

    friend class column_iterator<Matrix, SymmGroup>;
    friend class compressor<Matrix, SymmGroup>;
    friend class MPOIndexer<Matrix, SymmGroup>;
    
private:
    data_t data_;
    
    std::size_t left_i, right_i;
};

#include "dmrg/mp_tensors/mpotensor.hpp"

template<class Matrix, class SymmGroup>
Boundary<Matrix, SymmGroup> simplify(Boundary<Matrix, SymmGroup> b)
{
    typedef typename alps::numeric::associated_real_diagonal_matrix<Matrix>::type dmt;
    
    for (std::size_t k = 0; k < b.aux_dim(); ++k)
    {
        block_matrix<Matrix, SymmGroup> U, V, t;
        block_matrix<dmt, SymmGroup> S;
        
        if (b[k].left_basis().sum_of_sizes() == 0)
            continue;
        
        svd_truncate(b[k], U, V, S, 1e-4, 1, false);
        
        gemm(U, S, t);
        gemm(t, V, b[k]);
    }
    
    return b;
}

template<class Matrix, class SymmGroup>
std::size_t size_of(Boundary<Matrix, SymmGroup> const & m)
{
    size_t r = 0;
    for (size_t i = 0; i < m.aux_dim(); ++i)
        r += size_of(m[i]);
    return r;
}


#endif
