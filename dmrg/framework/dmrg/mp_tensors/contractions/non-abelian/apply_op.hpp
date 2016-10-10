/*****************************************************************************
 *
 * ALPS MPS DMRG Project
 *
 * Copyright (C) 2014 Institute for Theoretical Physics, ETH Zurich
 *                    Laboratory for Physical Chemistry, ETH Zurich
 *               2014-2014 by Sebastian Keller <sebkelle@phys.ethz.ch>
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

#ifndef CONTRACTIONS_SU2_APPLY_OP_HPP
#define CONTRACTIONS_SU2_APPLY_OP_HPP

#include "dmrg/block_matrix/symmetry/gsl_coupling.h"
#include "dmrg/block_matrix/block_matrix.h"
#include "dmrg/mp_tensors/mpstensor.h"
#include "dmrg/mp_tensors/mpotensor.h"
#include "dmrg/mp_tensors/contractions/non-abelian/functors.h"

namespace contraction {
namespace SU2 {

    template<class Matrix, class OtherMatrix, class SymmGroup>
    void lbtm_kernel(size_t b2,
                     ContractionGrid<Matrix, SymmGroup>& contr_grid,
                     Boundary<OtherMatrix, SymmGroup> const & left,
                     BoundaryMPSProduct<Matrix, OtherMatrix, SymmGroup, ::SU2::SU2Gemms> const & left_mult_mps,
                     MPOTensor<Matrix, SymmGroup> const & mpo,
                     DualIndex<SymmGroup> const & ket_basis,
                     Index<SymmGroup> const & right_i,
                     Index<SymmGroup> const & out_left_i,
                     ProductBasis<SymmGroup> const & in_right_pb,
                     ProductBasis<SymmGroup> const & out_left_pb)
    {
        typedef typename MPOTensor<OtherMatrix, SymmGroup>::index_type index_type;
        typedef typename MPOTensor<OtherMatrix, SymmGroup>::row_proxy row_proxy;
        typedef typename MPOTensor<OtherMatrix, SymmGroup>::col_proxy col_proxy;
        typedef typename DualIndex<SymmGroup>::const_iterator const_iterator;
        typedef typename SymmGroup::charge charge;

        col_proxy col_b2 = mpo.column(b2);
        for (typename col_proxy::const_iterator col_it = col_b2.begin(); col_it != col_b2.end(); ++col_it) {
            index_type b1 = col_it.index();

            block_matrix<Matrix, SymmGroup> const & T = left_mult_mps[b1];
            MPOTensor_detail::term_descriptor<Matrix, SymmGroup, true> access = mpo.at(b1,b2);

        for (std::size_t op_index = 0; op_index < access.size(); ++op_index)
        {
            typename operator_selector<Matrix, SymmGroup>::type const & W = access.op(op_index);
            block_matrix<Matrix, SymmGroup>& ret = contr_grid(b1,b2);

            int a = mpo.left_spin(b1).get(), k = W.spin().get(), ap = mpo.right_spin(b2).get();

            for (size_t t_block = 0; t_block < T.n_blocks(); ++t_block) {

                charge lc = T.basis().left_charge(t_block);
                charge rc = T.basis().right_charge(t_block);

                    const_iterator it = ket_basis.left_lower_bound(rc); //ket_basis comes transposed!
                    charge mc = it->rc;

                    for (size_t w_block = 0; w_block < W.basis().size(); ++w_block)
                    {
                        charge phys_in = W.basis().left_charge(w_block);
                        charge phys_out = W.basis().right_charge(w_block);

                        charge out_r_charge = SymmGroup::fuse(rc, phys_in);
                        size_t rb = right_i.position(out_r_charge);
                        if (rb == right_i.size()) continue;

                        charge out_l_charge = SymmGroup::fuse(lc, phys_out);
                        if (!right_i.has(out_l_charge)) continue; // can also probe out_left_i, but right_i has the same charges

                        size_t r_size = right_i[rb].second;

                        size_t o = ret.find_block(out_l_charge, out_r_charge);
                        if ( o == ret.n_blocks() )
                            o = ret.insert_block(Matrix(out_left_i.size_of_block(out_l_charge),r_size), out_l_charge, out_r_charge);

                        int i = SymmGroup::spin(lc), ip = SymmGroup::spin(out_l_charge);
                        int j = SymmGroup::spin(mc), jp = SymmGroup::spin(out_r_charge);
                        int two_sp = std::abs(i - ip), two_s  = std::abs(j - jp);

                        //typename Matrix::value_type coupling_coeff = ::SU2::mod_coupling(j, two_s, jp, a,k,ap, i, two_sp, ip);
                        //if (std::abs(coupling_coeff) < 1.e-40) continue;
                        //coupling_coeff *= sqrt((ip+1.)*(j+1.)/((i+1.)*(jp+1.))) * access.scale(op_index);

                        typename Matrix::value_type prefactor = sqrt((ip+1.)*(j+1.)/((i+1.)*(jp+1.))) * access.scale(op_index);
                        typename Matrix::value_type couplings[4];
                        couplings[0] = prefactor * ::SU2::mod_coupling(j, two_s, jp, a,k,ap, i, two_sp, ip);
                        couplings[1] = prefactor * ::SU2::mod_coupling(j, 2,     jp, a,k,ap, i, two_sp, ip);
                        couplings[2] = prefactor * ::SU2::mod_coupling(j, two_s, jp, a,k,ap, i, 2,      ip);
                        couplings[3] = prefactor * ::SU2::mod_coupling(j, 2,     jp, a,k,ap, i, 2,      ip);

                        size_t phys_s1 = W.basis().left_size(w_block);
                        size_t phys_s2 = W.basis().right_size(w_block);
                        size_t in_right_offset = in_right_pb(phys_in, out_r_charge);
                        size_t out_left_offset = out_left_pb(phys_out, lc);
                        Matrix const & wblock = W[w_block];
                        Matrix const & iblock = T[t_block];
                        Matrix & oblock = ret[o];

                        //maquis::dmrg::detail::lb_tensor_mpo(oblock, iblock, wblock,
                        //        out_left_offset, in_right_offset,
                        //        phys_s1, phys_s2, T.basis().left_size(t_block), r_size, coupling_coeff);

                        typedef typename SparseOperator<Matrix, SymmGroup>::const_iterator block_iterator;
                        std::pair<block_iterator, block_iterator> blocks = W.get_sparse().block(w_block);

                        size_t ldim = T.basis().left_size(t_block);
                        for(size_t rr = 0; rr < r_size; ++rr) {
                            for( block_iterator it = blocks.first; it != blocks.second; ++it)
                            {
                                std::size_t ss1 = it->row;
                                std::size_t ss2 = it->col;
                                std::size_t rspin = it->row_spin;
                                std::size_t cspin = it->col_spin;
                                std::size_t casenr = 0;
                                if (rspin == 2 && cspin == 2) casenr = 3;
                                else if (rspin == 2) casenr = 1;
                                else if (cspin == 2) casenr = 2;

                                typename Matrix::value_type alfa_t = it->coefficient * couplings[casenr];
                                maquis::dmrg::detail::iterator_axpy(&iblock(0, in_right_offset + ss1*r_size + rr),
                                                                    &iblock(0, in_right_offset + ss1*r_size + rr) + ldim, // bugbug
                                                                    &oblock(out_left_offset + ss2*ldim, rr),
                                                                    alfa_t);
                            }
                        }

                    } // wblock
            } // lblock
        } // op_index
        } // b1
    }

    template<class Matrix, class OtherMatrix, class SymmGroup>
    void rbtm_kernel(size_t b1,
                block_matrix<Matrix, SymmGroup> & ret,
                Boundary<OtherMatrix, SymmGroup> const & right,
                MPSBoundaryProduct<Matrix, OtherMatrix, SymmGroup, ::SU2::SU2Gemms> const & right_mult_mps,
                MPOTensor<Matrix, SymmGroup> const & mpo,
                DualIndex<SymmGroup> const & ket_basis,
                Index<SymmGroup> const & left_i,
                Index<SymmGroup> const & out_right_i,
                ProductBasis<SymmGroup> const & in_left_pb,
                ProductBasis<SymmGroup> const & out_right_pb)
    {
        typedef typename MPOTensor<OtherMatrix, SymmGroup>::index_type index_type;
        typedef typename MPOTensor<OtherMatrix, SymmGroup>::row_proxy row_proxy;
        typedef typename MPOTensor<OtherMatrix, SymmGroup>::col_proxy col_proxy;
        typedef typename DualIndex<SymmGroup>::const_iterator const_iterator;
        typedef typename SymmGroup::charge charge;

        row_proxy row_b1 = mpo.row(b1);
        for (typename row_proxy::const_iterator row_it = row_b1.begin(); row_it != row_b1.end(); ++row_it) {
            index_type b2 = row_it.index();

            block_matrix<Matrix, SymmGroup> const & T = right_mult_mps[b2];
            MPOTensor_detail::term_descriptor<Matrix, SymmGroup, true> access = mpo.at(b1,b2);

        for (std::size_t op_index = 0; op_index < access.size(); ++op_index)
        {
            typename operator_selector<Matrix, SymmGroup>::type const & W = access.op(op_index);
            int a = mpo.left_spin(b1).get(), k = W.spin().get(), ap = mpo.right_spin(b2).get();

            for (size_t t_block = 0; t_block < T.n_blocks(); ++t_block){

                charge lc = T.basis().left_charge(t_block);
                charge rc = T.basis().right_charge(t_block);

                    const_iterator it = ket_basis.left_lower_bound(lc);
                    charge mc = it->rc;

                    for (size_t w_block = 0; w_block < W.basis().size(); ++w_block)
                    {
                        charge phys_in = W.basis().left_charge(w_block);
                        charge phys_out = W.basis().right_charge(w_block);

                        charge out_l_charge = SymmGroup::fuse(lc, -phys_in);
                        size_t lb = left_i.position(out_l_charge);
                        if (lb == left_i.size()) continue;

                        charge out_r_charge = SymmGroup::fuse(rc, -phys_out);
                        if (!left_i.has(out_r_charge)) continue;

                        size_t l_size = left_i[lb].second;

                        size_t o = ret.find_block(out_l_charge, out_r_charge);
                        if ( o == ret.n_blocks() )
                            o = ret.insert_block(Matrix(l_size, out_right_i.size_of_block(out_r_charge)), out_l_charge, out_r_charge);

                        int i = SymmGroup::spin(out_r_charge), ip = SymmGroup::spin(rc);
                        int j = SymmGroup::spin(out_l_charge), jp = SymmGroup::spin(mc);
                        int two_sp = std::abs(i - ip), two_s  = std::abs(j - jp);

                        typename Matrix::value_type prefactor = sqrt((ip+1.)*(j+1.)/((i+1.)*(jp+1.))) * access.scale(op_index);
                        typename Matrix::value_type couplings[4];
                        couplings[0] = prefactor * ::SU2::mod_coupling(j, two_s, jp, a,k,ap, i, two_sp, ip);
                        couplings[1] = prefactor * ::SU2::mod_coupling(j, 2,     jp, a,k,ap, i, two_sp, ip);
                        couplings[2] = prefactor * ::SU2::mod_coupling(j, two_s, jp, a,k,ap, i, 2,      ip);
                        couplings[3] = prefactor * ::SU2::mod_coupling(j, 2,     jp, a,k,ap, i, 2,      ip);

                        size_t phys_s1 = W.basis().left_size(w_block);
                        size_t phys_s2 = W.basis().right_size(w_block);
                        size_t in_left_offset = in_left_pb(phys_in, out_l_charge);
                        size_t out_right_offset = out_right_pb(phys_out, rc);
                        Matrix const & wblock = W[w_block];
                        Matrix const & iblock = T[t_block];
                        Matrix & oblock = ret[o];

                        typedef typename SparseOperator<Matrix, SymmGroup>::const_iterator block_iterator;
                        std::pair<block_iterator, block_iterator> blocks = W.get_sparse().block(w_block);

                        size_t r_size = T.basis().right_size(t_block);
                        for(size_t rr = 0; rr < r_size; ++rr) {
                            for( block_iterator it = blocks.first; it != blocks.second; ++it)
                            {
                                std::size_t ss1 = it->row;
                                std::size_t ss2 = it->col;
                                std::size_t rspin = it->row_spin;
                                std::size_t cspin = it->col_spin;
                                std::size_t casenr = 0; 
                                if (rspin == 2 && cspin == 2) casenr = 3;
                                else if (rspin == 2) casenr = 1;
                                else if (cspin == 2) casenr = 2;

                                typename Matrix::value_type alfa_t = it->coefficient * couplings[casenr];
                                maquis::dmrg::detail::iterator_axpy(&iblock(in_left_offset + ss1*l_size, rr),
                                                                    &iblock(in_left_offset + ss1*l_size, rr) + l_size,
                                                                    &oblock(0, out_right_offset + ss2*r_size + rr),
                                                                    alfa_t);
                            }
                        }

                    } // wblock
            } // ket block
        } // op_index
        } // b2
    }
} // namespace SU2
} // namespace contraction

#endif
