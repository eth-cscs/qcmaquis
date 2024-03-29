/*****************************************************************************
 *
 * ALPS MPS DMRG Project
 *
 * Copyright (C) 2015 Institute for Theoretical Physics, ETH Zurich
 *               2015-2015 by Sebastian Keller <sebkelleb@phys.ethz.ch>
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

#ifndef SITE_OPERATOR_ALGORITHMS_H
#define SITE_OPERATOR_ALGORITHMS_H

//#include <boost/lambda/lambda.hpp>
//#include <boost/function.hpp>

#include "dmrg/utils/logger.h"
#include "dmrg/utils/utils.hpp"
#include "utils/timings.h"
#include "utils/traits.hpp"
#include "utils/bindings.hpp"

#include "dmrg/block_matrix/site_operator.h"

#include "dmrg/utils/parallel.hpp"

template<class Matrix1, class Matrix2, class Matrix3, class SymmGroup>
void gemm(SiteOperator<Matrix1, SymmGroup> const & A,
          SiteOperator<Matrix2, SymmGroup> const & B,
          SiteOperator<Matrix3, SymmGroup> & C)
{
    C.clear();
    assert(B.basis().is_sorted());

    typedef typename SymmGroup::charge charge;
    typedef typename DualIndex<SymmGroup>::const_iterator const_iterator;
    const_iterator B_begin = B.basis().begin();
    const_iterator B_end = B.basis().end();
    for (std::size_t k = 0; k < A.n_blocks(); ++k) {

        charge ar = A.basis().right_charge(k);
        const_iterator it = B.basis().left_lower_bound(ar);

        for ( ; it != B_end && it->lc == ar; ++it)
        {
            std::size_t matched_block = std::distance(B_begin, it);
            Matrix3 tmp(num_rows(A[k]), it->rs);

            gemm(A[k], B[matched_block], tmp);
            C.match_and_add_block(tmp, A.basis().left_charge(k), it->rc);
        }
    }
}


template<class Matrix, class SymmGroup>
SiteOperator<Matrix, SymmGroup> adjoint(SiteOperator<Matrix, SymmGroup> m)
{
    m.adjoint_inplace();
    return m;
}

template<class Matrix, class SymmGroup>
SiteOperator<Matrix, SymmGroup> adjoin(SiteOperator<Matrix, SymmGroup> const & m) // error: it should be adjoin_t_
{
    SiteOperator<Matrix, SymmGroup> ret;
    for (std::size_t k = 0; k < m.n_blocks(); ++k)
        ret.insert_block(m[k],
                         -m.basis().left_charge(k),
                         -m.basis().right_charge(k));
    return ret;
}

template<class Matrix, class SymmGroup>
bool is_hermitian(SiteOperator<Matrix, SymmGroup> const & m)
{
    bool ret = true;
    for (size_t k=0; ret && k < m.n_blocks(); ++k) {
        if (m.basis().left_size(k) != m.basis().right_size(k))
            return false;
        else if (m.basis().left_charge(k) == m.basis().right_charge(k))
            ret = is_hermitian(m[k]);
        else if (! m.has_block(m.basis().right_charge(k), m.basis().left_charge(k)))
            return false;
        else
            ret = ( m[k] == transpose(conj( m(m.basis().right_charge(k), m.basis().left_charge(k)) )) );
    }
    return ret;
}

template <class Matrix, class SymmGroup, class A>
SiteOperator<Matrix, SymmGroup> op_exp_hermitian(Index<SymmGroup> const & phys,
                                                 SiteOperator<Matrix, SymmGroup> M,
                                                 A const & alpha = 1.)
{
    for (typename Index<SymmGroup>::const_iterator it_c = phys.begin(); it_c != phys.end(); it_c++)
        if (M.has_block(it_c->first, it_c->first))
            M(it_c->first, it_c->first) = exp_hermitian(M(it_c->first, it_c->first), alpha);
        else
            M.insert_block(Matrix::identity_matrix(phys.size_of_block(it_c->first)),
                           it_c->first, it_c->first);
    return M;
}

template <class OutOp, class Matrix, class SymmGroup, class A>
OutOp op_exp_hermitian(Index<SymmGroup> const & phys,
                       SiteOperator<Matrix, SymmGroup> const & M,
                       A const & alpha = 1.)
{
    OutOp ret(M.basis());
    for (typename Index<SymmGroup>::const_iterator it_c = phys.begin(); it_c != phys.end(); it_c++)
        if (M.has_block(it_c->first, it_c->first))
            ret(it_c->first, it_c->first) = exp_hermitian(M(it_c->first, it_c->first), alpha);
        else
            ret.insert_block(Matrix::identity_matrix(phys.size_of_block(it_c->first)),
                           it_c->first, it_c->first);
    return ret;
}

namespace detail {
    
    template <class Matrix>
    typename boost::enable_if<boost::is_complex<typename Matrix::value_type>, Matrix>::type
    exp_dispatcher(Matrix const& m, typename Matrix::value_type const& alpha)
    {
        return exp(m, alpha);
    }

    template <class Matrix>
    typename boost::disable_if<boost::is_complex<typename Matrix::value_type>, Matrix>::type
    exp_dispatcher(Matrix const& m, typename Matrix::value_type const& alpha)
    {
        throw std::runtime_error("Exponential of non-hermitian real matrices not implemented!");
        return Matrix();
    }
}

template <class Matrix, class SymmGroup, class A> SiteOperator<Matrix, SymmGroup> op_exp(Index<SymmGroup> const & phys,
                                       SiteOperator<Matrix, SymmGroup> M,
                                       A const & alpha = 1.)
{
    for (typename Index<SymmGroup>::const_iterator it_c = phys.begin(); it_c != phys.end(); it_c++)
        if (M.has_block(it_c->first, it_c->first))
            M(it_c->first, it_c->first) = detail::exp_dispatcher(M(it_c->first, it_c->first), alpha);
        else
            M.insert_block(Matrix::identity_matrix(phys.size_of_block(it_c->first)),
                           it_c->first, it_c->first);
    return M;
}

template<class Matrix1, class Matrix2, class SymmGroup>
void op_kron(Index<SymmGroup> const & phys_A,
             Index<SymmGroup> const & phys_B,
             SiteOperator<Matrix1, SymmGroup> const & A,
             SiteOperator<Matrix1, SymmGroup> const & B,
             SiteOperator<Matrix2, SymmGroup> & C,
             SpinDescriptor<symm_traits::AbelianTag> lspin
           = SpinDescriptor<symm_traits::AbelianTag>(),
             SpinDescriptor<symm_traits::AbelianTag> mspin
           = SpinDescriptor<symm_traits::AbelianTag>(),
             SpinDescriptor<symm_traits::AbelianTag> rspin
           = SpinDescriptor<symm_traits::AbelianTag>(),
             SpinDescriptor<symm_traits::AbelianTag> tspin
           = SpinDescriptor<symm_traits::AbelianTag>())
{
    C = SiteOperator<Matrix2, SymmGroup>();

    ProductBasis<SymmGroup> pb_left(phys_A, phys_B);
    ProductBasis<SymmGroup> const& pb_right = pb_left;

    for (int i = 0; i < A.n_blocks(); ++i) {
        for (int j = 0; j < B.n_blocks(); ++j) {
            typename SymmGroup::charge new_right = SymmGroup::fuse(A.basis().right_charge(i), B.basis().right_charge(j));
            typename SymmGroup::charge new_left = SymmGroup::fuse(A.basis().left_charge(i), B.basis().left_charge(j));


            Matrix2 tmp(pb_left.size(A.basis().left_charge(i), B.basis().left_charge(j)),
                       pb_right.size(A.basis().right_charge(i), B.basis().right_charge(j)),
                       0);

            maquis::dmrg::detail::op_kron(tmp, B[j], A[i],
                                          pb_left(A.basis().left_charge(i), B.basis().left_charge(j)),
                                          pb_right(A.basis().right_charge(i), B.basis().right_charge(j)),
                                          A.basis().left_size(i), B.basis().left_size(j),
                                          A.basis().right_size(i), B.basis().right_size(j));

            C.match_and_add_block(tmp, new_left, new_right);
        }
    }
}

namespace ts_ops_detail
{
    template <class Integer>
    std::vector<Integer> allowed_spins(Integer left, Integer right, Integer k1, Integer k2);
}

template<class Matrix1, class Matrix2, class SymmGroup>
void op_kron(Index<SymmGroup> const & phys_A,
             Index<SymmGroup> const & phys_B,
             SiteOperator<Matrix1, SymmGroup> const & Ao,
             SiteOperator<Matrix1, SymmGroup> const & Bo,
             SiteOperator<Matrix2, SymmGroup> & C,
             SpinDescriptor<symm_traits::SU2Tag> lspin,
             SpinDescriptor<symm_traits::SU2Tag> mspin,
             SpinDescriptor<symm_traits::SU2Tag> rspin,
             SpinDescriptor<symm_traits::SU2Tag> target_spin
              = SpinDescriptor<symm_traits::SU2Tag>(-1,0,0))
{
    typedef typename SymmGroup::charge charge;
    typedef typename SymmGroup::subcharge subcharge;
    typedef typename Matrix2::value_type value_type;

    ProductBasis<SymmGroup> pb_left(phys_A, phys_B);
    ProductBasis<SymmGroup> const& pb_right = pb_left;

    SiteOperator<Matrix1, SymmGroup> A = Ao, B = Bo;

    //*************************************
    // expand the small identity to the full one (Hack)

    if (A.spin().get() > 0 && B.spin().get() == 0)
    {
        charge cb = phys_B[1].first, cc = phys_B[2].first;
        if (!B.has_block(cb,cc))
        {
            B.insert_block(Matrix1(1,1,1), cb, cc);
            B.insert_block(Matrix1(1,1,1), cc, cb);
        }
    }
    if (A.spin().get() == 0 && B.spin().get() > 0)
    {
        charge cb = phys_A[1].first, cc = phys_A[2].first;

        if (!A.has_block(cb,cc))
        {
            A.insert_block(Matrix1(1,1,1), cb, cc);
            A.insert_block(Matrix1(1,1,1), cc, cb);
        }
    }

    //*************************************
    // MPO matrix basis spin QN's

    int k1 = A.spin().get(), k2 = B.spin().get(), k, j, jp, jpp;

    j = lspin.get();
    jpp = mspin.get();
    jp = rspin.get();

    std::vector<int> product_spins = ts_ops_detail::allowed_spins(j,jp, k1, k2);
    k = (target_spin.get() > -1) ? target_spin.get() : product_spins[0];

    //*************************************
    // Tensor + Kronecker product

    typedef std::pair<charge, charge> charge_pair;
    std::map<charge_pair, std::pair<std::vector<subcharge>, std::vector<subcharge> >, compare_pair<charge_pair> > basis_spins;

    block_matrix<Matrix2, SymmGroup> blocks;
    for (std::size_t i = 0; i < A.n_blocks(); ++i) {
        for (std::size_t j = 0; j < B.n_blocks(); ++j) {
            charge  inA = A.basis().left_charge(i);
            charge outA = A.basis().right_charge(i);
            charge  inB = B.basis().left_charge(j);
            charge outB = B.basis().right_charge(j);

            charge new_left = SymmGroup::fuse(inA, inB);
            charge new_right = SymmGroup::fuse(outA, outB);

            Matrix2 tmp(pb_left.size(inA, inB), pb_right.size(outA, outB), 0);

            std::size_t in_offset = pb_left(inA, inB);
            std::size_t out_offset = pb_right(outA, outB);

            maquis::dmrg::detail::op_kron(tmp, B[j], A[i], in_offset, out_offset,
                                          A.basis().left_size(i), B.basis().left_size(j),
                                          A.basis().right_size(i), B.basis().right_size(j));

            int j1  = std::abs(SymmGroup::spin(inA)),  j2  = std::abs(SymmGroup::spin(inB)),  J = productSpin<SymmGroup>(inA, inB);
            int j1p = std::abs(SymmGroup::spin(outA)), j2p = std::abs(SymmGroup::spin(outB)), Jp = productSpin<SymmGroup>(outA, outB);

            // 9j coefficient for the tensor product with normalization prefactor
            typename Matrix2::value_type coupling = SU2::mod_coupling(j1,j2,J,k1,k2,k,j1p,j2p,Jp);
            tmp *= coupling;

            blocks.match_and_add_block(tmp, new_left, new_right);
            // record the spin information
            basis_spins[std::make_pair(new_left, new_right)].first.resize(num_rows(tmp));
            basis_spins[std::make_pair(new_left, new_right)].first[in_offset] = J;
            basis_spins[std::make_pair(new_left, new_right)].second.resize(num_cols(tmp));
            basis_spins[std::make_pair(new_left, new_right)].second[out_offset] = Jp;
        }
    }

    //*************************************
    // Matrix basis coupling coefficient, applies uniformly to whole product

    typename Matrix2::value_type coupling = std::sqrt((jpp+1)*(k+1)) * gsl_sf_coupling_6j(j,jp,k,k2,k1,jpp);
    coupling = (((j+jp+k1+k2)/2)%2) ? -coupling : coupling;
    blocks *= coupling;

    SpinDescriptor<symm_traits::SU2Tag> op_spin(k, j, jp);
    C = SiteOperator<Matrix2, SymmGroup>(blocks, basis_spins);
    C.spin() = op_spin;
}

#endif
