/*****************************************************************************
 *
 * ALPS MPS DMRG Project
 *
 * Copyright (C) 2014 Institute for Theoretical Physics, ETH Zurich
 *               2011-2011 by Bela Bauer <bauerb@phys.ethz.ch>
 * 
 * This software is part of the ALPS Applications, published under the ALPS
 * Application License; you can use, redistribute it and/or modify it under
 * the terms of the license, either version 1 or (at your option) any later
 * version.
 * 
 * You should have received a copy of the ALPS Application License along with
 * the ALPS Applications; see the file LICENSE.txt. If not, the license is also
 * available from http://alps.comp-phys.org/.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR 
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY, 
 * FITNESS FOR A PARTICULAR PURPOSE, TITLE AND NON-INFRINGEMENT. IN NO EVENT 
 * SHALL THE COPYRIGHT HOLDERS OR ANYONE DISTRIBUTING THE SOFTWARE BE LIABLE 
 * FOR ANY DAMAGES OR OTHER LIABILITY, WHETHER IN CONTRACT, TORT OR OTHERWISE, 
 * ARISING FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER 
 * DEALINGS IN THE SOFTWARE.
 *
 *****************************************************************************/

#ifndef MPS_H
#define MPS_H

#include "dmrg/mp_tensors/mpstensor.h"
#include "dmrg/mp_tensors/mpotensor.h"
#include "dmrg/mp_tensors/boundary.h"

#include <limits>

template<class Matrix, class SymmGroup>
struct mps_initializer;

template<class Matrix, class SymmGroup>
class MPS
{
    typedef std::vector<MPSTensor<Matrix, SymmGroup> > data_t;
public:
    typedef std::size_t size_t;

    // reproducing interface of std::vector
    typedef typename data_t::size_type size_type;
    typedef typename data_t::value_type value_type;
    typedef typename data_t::iterator iterator;
    typedef typename data_t::const_iterator const_iterator;
    typedef typename MPSTensor<Matrix, SymmGroup>::scalar_type scalar_type;
    
    MPS();
    MPS(size_t L);  
    MPS(size_t L, mps_initializer<Matrix, SymmGroup> & init);
    
    size_t size() const { return data_.size(); }
    size_t length() const { return size(); }
    Index<SymmGroup> const & site_dim(size_t i) const { return data_[i].site_dim(); }
    Index<SymmGroup> const & row_dim(size_t i) const { return data_[i].row_dim(); }
    Index<SymmGroup> const & col_dim(size_t i) const { return data_[i].col_dim(); }
    
    value_type const & operator[](size_t i) const;
    value_type& operator[](size_t i);
    
    void resize(size_t L);
    
    const_iterator begin() const {return data_.begin();}
    const_iterator end() const {return data_.end();}
    const_iterator const_begin() const {return data_.begin();}
    const_iterator const_end() const {return data_.end();}
    iterator begin() {return data_.begin();}
    iterator end() {return data_.end();}

    void make_right_paired() const;
    
    size_t canonization(bool=false) const;
    void canonize(size_t center, DecompMethod method = DefaultSolver());
    
    void normalize_left();
    void normalize_right();
    
    void move_normalization_l2r(size_t p1, size_t p2, DecompMethod method=DefaultSolver());
    void move_normalization_r2l(size_t p1, size_t p2, DecompMethod method=DefaultSolver());
    
    std::string description() const;
   
    template<class OtherMatrix>
    truncation_results grow_l2r_sweep(MPOTensor<Matrix, SymmGroup> const & mpo,
                                      Boundary<OtherMatrix, SymmGroup> const & left,
                                      Boundary<OtherMatrix, SymmGroup> const & right,
                                      std::size_t l, double alpha,
                                      double cutoff, std::size_t Mmax);
    template<class OtherMatrix>
    truncation_results grow_r2l_sweep(MPOTensor<Matrix, SymmGroup> const & mpo,
                                      Boundary<OtherMatrix, SymmGroup> const & left,
                                      Boundary<OtherMatrix, SymmGroup> const & right,
                                      std::size_t l, double alpha,
                                      double cutoff, std::size_t Mmax);
    
    Boundary<Matrix, SymmGroup> left_boundary() const;
    Boundary<Matrix, SymmGroup> right_boundary() const;
    
    void apply(typename operator_selector<Matrix, SymmGroup>::type const&, size_type);
    void apply(typename operator_selector<Matrix, SymmGroup>::type const&,
               typename operator_selector<Matrix, SymmGroup>::type const&, size_type);
    
    friend void swap(MPS& a, MPS& b)
    {
        using std::swap;
        swap(a.data_, b.data_);
        swap(a.canonized_i, b.canonized_i);
    }
    
    template <class Archive> void serialize(Archive & ar, const unsigned int version);
    
private:
    
    data_t data_;
    mutable size_t canonized_i;
};

template<class Matrix, class SymmGroup>
void load(std::string const& dirname, MPS<Matrix, SymmGroup> & mps);
template<class Matrix, class SymmGroup>
void save(std::string const& dirname, MPS<Matrix, SymmGroup> const& mps);

template<class Matrix, class SymmGroup>
struct mps_initializer
{
    virtual ~mps_initializer() {}
    virtual void operator()(MPS<Matrix, SymmGroup> & mps) = 0;
};

template<class Matrix, class SymmGroup>
MPS<Matrix, SymmGroup> join(MPS<Matrix, SymmGroup> const & a,
                            MPS<Matrix, SymmGroup> const & b,
                            double alpha=1., double beta=1.)
{
    assert( a.length() == b.length() );
    
    MPSTensor<Matrix, SymmGroup> aright=a[a.length()-1], bright=b[a.length()-1];
    aright.multiply_by_scalar(alpha);
    bright.multiply_by_scalar(beta);

    MPS<Matrix, SymmGroup> ret(a.length());
    ret[0] = join(a[0],b[0],l_boundary_f);
    ret[a.length()-1] = join(aright,bright,r_boundary_f);
    for (std::size_t p = 1; p < a.length()-1; ++p)
        ret[p] = join(a[p], b[p]);
    return ret;
}


template<class Matrix, class SymmGroup>
Boundary<Matrix, SymmGroup>
make_left_boundary(MPS<Matrix, SymmGroup> const & bra, MPS<Matrix, SymmGroup> const & ket)
{
    assert(ket.length() == bra.length());
    Index<SymmGroup> i = ket[0].row_dim();
    Index<SymmGroup> j = bra[0].row_dim();
    Boundary<Matrix, SymmGroup> ret(i, j, 1);
    
    for(typename Index<SymmGroup>::basis_iterator it1 = i.basis_begin(); !it1.end(); ++it1)
        for(typename Index<SymmGroup>::basis_iterator it2 = j.basis_begin(); !it2.end(); ++it2)
            ret[0](*it1, *it2) = 1;
    
    return ret;
}

template<class Matrix, class SymmGroup>
Boundary<Matrix, SymmGroup>
make_right_boundary(MPS<Matrix, SymmGroup> const & bra, MPS<Matrix, SymmGroup> const & ket)
{
    assert(ket.length() == bra.length());
    std::size_t L = ket.length();
    Index<SymmGroup> i = ket[L-1].col_dim();
    Index<SymmGroup> j = bra[L-1].col_dim();
    Boundary<Matrix, SymmGroup> ret(j, i, 1);
    
    for(typename Index<SymmGroup>::basis_iterator it1 = i.basis_begin(); !it1.end(); ++it1)
        for(typename Index<SymmGroup>::basis_iterator it2 = j.basis_begin(); !it2.end(); ++it2)
            ret[0](*it2, *it1) = 1;
    
    return ret;
}

#include "dmrg/mp_tensors/mps.hpp"

#endif
