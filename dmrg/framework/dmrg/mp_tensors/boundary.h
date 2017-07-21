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

#ifndef BOUNDARY_H
#define BOUNDARY_H


#include <iostream>
#include <set>
#include <boost/archive/binary_oarchive.hpp>

#include "dmrg/utils/storage.h"
#include "dmrg/block_matrix/block_matrix.h"
#include "dmrg/block_matrix/indexing.h"
#include "utils/function_objects.h"
#include "dmrg/utils/aligned_allocator.hpp"
#include "dmrg/utils/parallel.hpp"

namespace detail {

    template <class T, class SymmGroup>
    typename boost::enable_if<symm_traits::HasSU2<SymmGroup>, T>::type
    conjugate_correction(typename SymmGroup::charge lc, typename SymmGroup::charge rc)
    {
        assert( SymmGroup::spin(lc) >= 0);
        assert( SymmGroup::spin(rc) >= 0);

        typename SymmGroup::subcharge S = std::min(SymmGroup::spin(rc), SymmGroup::spin(lc));
        typename SymmGroup::subcharge spin_diff = SymmGroup::spin(rc) - SymmGroup::spin(lc);

        switch (spin_diff) {
            case  0: return  T(1.);                           break;
            case  1: return -T( sqrt((S + 1.)/(S + 2.)) );    break;
            case -1: return  T( sqrt((S + 2.)/(S + 1.)) );    break;
            case  2: return -T( sqrt((S + 1.) / (S + 3.)) );  break;
            case -2: return -T( sqrt((S + 3.) / (S + 1.)) );  break;
            default:
                throw std::runtime_error("hermitian conjugate for reduced tensor operators only implemented up to rank 1");
        }
    }

    template <class T, class SymmGroup>
    typename boost::disable_if<symm_traits::HasSU2<SymmGroup>, T>::type
    conjugate_correction(typename SymmGroup::charge lc, typename SymmGroup::charge rc)
    {
        return T(1.);
    }
}

template<class Matrix, class SymmGroup>
class BoundaryIndex
{
    typedef typename Matrix::value_type value_type;
    typedef typename SymmGroup::charge charge;

    template <class, class> friend class BoundaryIndex;

public:

    BoundaryIndex() {}
    BoundaryIndex(Index<SymmGroup> const & bra, Index<SymmGroup> const & ket)
    : bra_index(bra), ket_index(ket), lb_rb_ci(bra.size())  {}

    template <class OtherMatrix>
    BoundaryIndex(BoundaryIndex<OtherMatrix, SymmGroup> const& rhs)
    {
        bra_index = rhs.bra_index;
        ket_index = rhs.ket_index;

        lb_rb_ci = rhs.lb_rb_ci;
        offsets  = rhs.offsets;
        conjugate_scales = rhs.conjugate_scales;
        transposes = rhs.transposes;
    }

    unsigned   n_cohorts      ()                        const { return offsets.size(); }
    long int   offset         (unsigned ci, unsigned b) const { return offsets[ci][b]; }
    value_type conjugate_scale(unsigned ci, unsigned b) const { return conjugate_scales[ci][b]; }
    bool       trans          (unsigned ci, unsigned b) const { return transposes[ci][b]; }

    bool has(unsigned lb, unsigned rb) const
    {
        if (lb >= lb_rb_ci.size())
            return false;

        for (auto pair : lb_rb_ci[lb])
            if (rb == pair.first) return true;

        return false;
    }

    unsigned cohort_index(unsigned lb, unsigned rb) const
    {
        if (lb >= lb_rb_ci.size())
            return n_cohorts();

        for (auto pair : lb_rb_ci[lb])
            if (rb == pair.first) return pair.second;

        return n_cohorts();
    }

    unsigned cohort_index(charge lc, charge rc) const
    {
        return cohort_index(bra_index.position(lc), ket_index.position(rc));
    }

    unsigned add_cohort(unsigned lb, unsigned rb, std::vector<long int> const & off_)
    {
        assert(lb < lb_rb_ci.size());

        unsigned ci = n_cohorts();
        lb_rb_ci[lb].push_back(std::make_pair(rb, ci));

        offsets.push_back(off_);
        conjugate_scales.push_back(std::vector<value_type>(off_.size(), 1.));
        transposes      .push_back(std::vector<char>      (off_.size()));

        return ci;
    }

    void complement_transpose(MPOTensor_detail::Hermitian const & herm, bool forward)
    {
        for (unsigned lb = 0; lb < lb_rb_ci.size(); ++lb)
            for (auto pair : lb_rb_ci[lb])
            {
                unsigned rb = pair.first;
                unsigned ci_A = pair.second; // source transpose ci
                unsigned ci_B = cohort_index(rb, lb);
                if (ci_B == n_cohorts())
                    ci_B = add_cohort(rb, lb, std::vector<long int>(herm.size(), -1));

                for (unsigned b = 0; b < herm.size(); ++b)
                {
                    if (herm.skip(b))
                    {
                        assert(offsets[ci_B][b] == -1);
                        offsets[ci_B][b] = offsets[ci_A][herm.conj(b)];
                        conjugate_scales[ci_B][b] = detail::conjugate_correction<value_type, SymmGroup>
                                                        (bra_index[lb].first, ket_index[rb].first)
                                                      * herm.phase( (forward) ? b : herm.conj(b));
                    }
                }
            }
    }

    std::vector<std::pair<unsigned, unsigned>> const & operator[](size_t block) const {
        assert(block < lb_rb_ci.size());
        return lb_rb_ci[block];
    }

    bool sane(MPOTensor_detail::Hermitian const & herm) const
    {
        for (unsigned lb = 0; lb < lb_rb_ci.size(); ++lb)
            for (auto pair : lb_rb_ci[lb])
            {
                unsigned rb = pair.first;
                unsigned ci = pair.second; // source transpose ci
                for (unsigned b = 0; b < herm.size(); ++b)
                    if (herm.skip(b)) if (offsets[ci][b] != -1) return false;
            }

        return true;
    }

    template <class Index>
    bool equivalent(Index const& ref) const
    {
        for (unsigned b = 0; b < ref.size(); ++b)
        {
            for (unsigned k = 0; k < ref[b].size(); ++k)
            {
                charge lc = ref[b].left_charge(k);
                charge rc = ref[b].right_charge(k);

                unsigned lb = bra_index.position(lc);
                unsigned rb = ket_index.position(rc);

                // check we have the block from ref
                if (ref.position(b, lc, rc) != ref[b].size())
                    if (offsets[cohort_index(lb,rb)][b] == -1)
                        return false;

                //if(ref.conj_scales[b][k] != conjugate_scales[cohort_index(lb,rb)][b]);
                //    maquis::cout << ref.conj_scales[b][k] << " " << conjugate_scales[cohort_index(lb,rb)][b] << std::endl;
                assert(ref.conj_scales[b][k] == conjugate_scales[cohort_index(lb,rb)][b]);
            }
        }

        for (unsigned lb = 0; lb < lb_rb_ci.size(); ++lb)
            for (auto pair : lb_rb_ci[lb])
            {
                unsigned rb = pair.first;
                unsigned ci= pair.second; // source transpose ci

                for (unsigned b = 0; b < offsets[ci].size(); ++b)
                {
                    charge lc = bra_index[lb].first;
                    charge rc = ket_index[rb].first;

                    // check if ref has current block
                    if (offsets[ci][b] != -1)
                        if (ref.position(b, lc, rc) == ref[b].size())
                            return false;
                }
            }

        return true;
    }

private:
    Index<SymmGroup> bra_index, ket_index;
    //                                rb_ket     ci
    std::vector<std::vector<std::pair<unsigned, unsigned>>> lb_rb_ci;

    std::vector<std::vector<long int>>   offsets;
    std::vector<std::vector<value_type>> conjugate_scales;
    std::vector<std::vector<char>>       transposes;

    friend class boost::serialization::access;

    template <class Archive>
    void serialize(Archive & ar, const unsigned int version)
    {
        ar & lb_rb_ci & offsets & conjugate_scales & transposes;
    }
};

template<class Matrix, class SymmGroup>
class Boundary : public storage::disk::serializable<Boundary<Matrix, SymmGroup> >
{
public:
    typedef typename maquis::traits::scalar_type<Matrix>::type scalar_type;
    typedef typename Matrix::value_type value_type;
    typedef std::pair<typename SymmGroup::charge, std::size_t> access_type;

    typedef std::vector<std::vector<value_type, maquis::aligned_allocator<value_type, ALIGNMENT>>> data_t;

    friend class boost::serialization::access;

    template<class Archive>
    void serialize(Archive &ar, const unsigned int version){
        ar & data_ & index;
    }
    
    Boundary(Index<SymmGroup> const & ud = Index<SymmGroup>(),
             Index<SymmGroup> const & ld = Index<SymmGroup>(),
             std::size_t ad = 1)
    : data_(ad, block_matrix<Matrix, SymmGroup>(ud, ld))
    , index(ud, ld)
    {
        data().resize(ud.size());
        for (std::size_t i = 0; i < ud.size(); ++i)
        {
            //auto lc = ud[i].first;
            //auto rc = ld[i].first;
            std::size_t ls = ud[i].second;
            std::size_t rs = ld[i].second;
            std::size_t block_size = bit_twiddling::round_up<ALIGNMENT/sizeof(value_type)>(ls*rs);
            data()[i].resize(block_size * ad);
            std::vector<long int> offsets(ad);
            for (std::size_t b = 0; b < ad; ++b)
                offsets[b] = b * block_size;
            index.add_cohort(i,i,offsets);
        }
    }
    
    template <class OtherMatrix>
    Boundary(Boundary<OtherMatrix, SymmGroup> const& rhs)
    {
        data_.reserve(rhs.aux_dim());
        for (std::size_t n=0; n<rhs.aux_dim(); ++n)
            data_.push_back(rhs[n]);

        index = rhs.index;
    }

    std::size_t aux_dim() const { 
        return data_.size(); 
    }

    void resize(size_t n){
        if(n < data_.size()) 
            return data_.resize(n);
        data_.reserve(n);
        for(int i = data_.size(); i < n; ++i)
            data_.push_back(block_matrix<Matrix, SymmGroup>());
    }
    
    std::vector<scalar_type> traces() const {
        std::vector<scalar_type> ret; ret.reserve(data_.size());
        for (size_t k=0; k < data_.size(); ++k) ret.push_back(data_[k].trace());
        return ret;
    }

    bool reasonable() const {
        for(size_t i = 0; i < data_.size(); ++i)
            if(!data_[i].reasonable()) return false;
        return true;
    }
   
    template<class Archive> 
    void load(Archive & ar){
        std::vector<std::string> children = ar.list_children("/data");
        data_.resize(children.size());
        parallel::scheduler_balanced scheduler(children.size());
        for(size_t i = 0; i < children.size(); ++i){
             parallel::guard proc(scheduler(i));
             ar["/data/"+children[i]] >> data_[alps::cast<std::size_t>(children[i])];
        }
    }
    
    template<class Archive> 
    void save(Archive & ar) const {
        ar["/data"] << data_;
    }

    block_matrix<Matrix, SymmGroup> & operator[](std::size_t k) { return data_[k]; }
    block_matrix<Matrix, SymmGroup> const & operator[](std::size_t k) const { return data_[k]; }

    data_t const& data() const { return data2; }
    data_t      & data()       { return data2; }

    BoundaryIndex<Matrix, SymmGroup> index;

private:
    data_t data2;

    std::vector<block_matrix<Matrix, SymmGroup> > data_;
};


template<class Matrix, class SymmGroup>
Boundary<Matrix, SymmGroup> simplify(Boundary<Matrix, SymmGroup> b)
{
    typedef typename alps::numeric::associated_real_diagonal_matrix<Matrix>::type dmt;
    
    for (std::size_t k = 0; k < b.aux_dim(); ++k)
    {
        block_matrix<Matrix, SymmGroup> U, V, t;
        block_matrix<dmt, SymmGroup> S;
        
        if (b[k].basis().sum_of_left_sizes() == 0)
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

template<class Matrix, class SymmGroup>
void save_boundary(Boundary<Matrix, SymmGroup> const & b, std::string fname)
{
    std::ofstream ofs(fname.c_str());
    boost::archive::binary_oarchive ar(ofs);
    ar << b;
    ofs.close();
}

#endif
