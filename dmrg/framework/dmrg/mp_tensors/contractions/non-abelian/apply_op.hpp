/*****************************************************************************
 *
 * ALPS MPS DMRG Project
 *
 * Copyright (C) 2014 Institute for Theoretical Physics, ETH Zurich
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

#include "dmrg/block_matrix/block_matrix.h"
#include "dmrg/mp_tensors/mpstensor.h"
#include "dmrg/mp_tensors/mpotensor.h"
#include "dmrg/mp_tensors/contractions/non-abelian/gsl_coupling.h"

namespace SU2 {

    inline
    double mod_coupling(int two_ja, int two_jb, int two_jc,
                        int two_jd, int two_je, int two_jf,
                        int two_jg, int two_jh, int two_ji)
    {
        return sqrt( (two_jg+1.) * (two_jh+1.) * (two_jc+1.) * (two_jf+1.) )
                * gsl_sf_coupling_9j(two_ja, two_jb, two_jc,
                                     two_jd, two_je, two_jf,
                                     two_jg, two_jh, two_ji);
    }
}

namespace contraction {
namespace SU2 {

    template<class Matrix, class OtherMatrix, class SymmGroup>
    void lbtm_kernel(size_t b2,
                     ContractionGrid<Matrix, SymmGroup>& contr_grid,
                     Boundary<OtherMatrix, SymmGroup> const & left,
                     std::vector<block_matrix<Matrix, SymmGroup> > const & left_mult_mps,
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
            MPOTensor_detail::const_term_descriptor<Matrix, SymmGroup> access = mpo.at(b1,b2);
            block_matrix<Matrix, SymmGroup> const & W = access.op;
            block_matrix<Matrix, SymmGroup>& ret = contr_grid(b1,b2);

            for (size_t k = 0; k < left[b1].n_blocks(); ++k) {

                charge lc = left[b1].right_basis_charge(k); // left comes as left^T !
                charge mc = left[b1].left_basis_charge(k);

                std::pair<const_iterator, const_iterator>
                  er = std::equal_range(ket_basis.begin(), ket_basis.end(),
                    boost::make_tuple(mc, SymmGroup::IdentityCharge, 0, 0), dual_index_detail::gt_row<SymmGroup>());

                for (const_iterator it = er.first; it != er.second; ++it)
                {
                    size_t matched_block = std::distance(ket_basis.begin(), it);
                    charge rc = boost::tuples::get<1>(ket_basis[matched_block]);
                    size_t t_block = T.basis().position(lc, rc); // t_block != k in general

                    for (size_t w_block = 0; w_block < W.basis().size(); ++w_block)
                    {
                        charge phys_in = W.left_basis_charge(w_block);
                        charge phys_out = W.right_basis_charge(w_block);

                        charge out_r_charge = SymmGroup::fuse(rc, phys_in);
                        if (!right_i.has(out_r_charge)) continue;

                        charge out_l_charge = SymmGroup::fuse(lc, phys_out);
                        if (!right_i.has(out_l_charge) || !out_left_i.has(out_l_charge)) continue;

                        size_t r_size = right_i.size_of_block(out_r_charge);

                        size_t o = ret.find_block(out_l_charge, out_r_charge);
                        if ( o == ret.n_blocks() ) {
                            o = ret.insert_block(Matrix(1,1), out_l_charge, out_r_charge);
                            ret.resize_block(o, out_left_i.size_of_block(out_l_charge), r_size);
                        }

                        int i  = lc[1], ip = out_l_charge[1];
                        int j  = mc[1], jp  = out_r_charge[1];
                        int two_sp = std::abs(i - ip), two_s  = std::abs(j - jp);
                        int a = std::abs(i-j), k = std::abs(std::abs(phys_in[1])-std::abs(phys_out[1]));

                        int ap = std::abs(ip-jp);
                        if (ap >= 3) continue;

                        typename Matrix::value_type coupling_coeff = ::SU2::mod_coupling(j, two_s, jp, a,k,ap, i, two_sp, ip);
                        coupling_coeff *= sqrt((ip+1.)*(j+1.)/((i+1.)*(jp+1.))) * access.scale;

                        size_t phys_s1 = W.left_basis_size(w_block);
                        size_t phys_s2 = W.right_basis_size(w_block);
                        size_t in_right_offset = in_right_pb(phys_in, out_r_charge);
                        size_t out_left_offset = out_left_pb(phys_out, lc);
                        Matrix const & wblock = W[w_block];
                        Matrix const & iblock = T[t_block];
                        Matrix & oblock = ret[o];

                        //maquis::cout << "access " << mc << " + " << phys_in<< "|" << out_r_charge << " -- "
                        //         << lc << " + " << phys_out << "|" << out_l_charge
                        //         << " T(" << lc << "," << rc << "): +" << in_right_offset
                        //         << "(" << T.left_basis_size(t_block) << "x" << r_size << ")|" << T.right_basis_size(t_block) << " -> +"
                        //         << out_left_offset << "(" << T.left_basis_size(t_block) << "x" << r_size << ")|" << out_left_i.size_of_block(out_l_charge)
                        //         << " * " << j << two_s << jp << a << k << ap << i << two_sp << ip << " " << coupling_coeff * wblock(0,0) << std::endl;

                        maquis::dmrg::detail::lb_tensor_mpo(oblock, iblock, wblock,
                                out_left_offset, in_right_offset,
                                phys_s1, phys_s2, T.left_basis_size(t_block), r_size, coupling_coeff);
                    }
                }
            }
        } // b1
    }

    template<class Matrix, class OtherMatrix, class SymmGroup>
    block_matrix<Matrix, SymmGroup>
    rbtm_kernel(size_t b1,
                Boundary<OtherMatrix, SymmGroup> const & right,
                std::vector<block_matrix<Matrix, SymmGroup> > const & right_mult_mps,
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

        block_matrix<Matrix, SymmGroup> ret;

        row_proxy row_b1 = mpo.row(b1);
        for (typename row_proxy::const_iterator row_it = row_b1.begin(); row_it != row_b1.end(); ++row_it) {
            index_type b2 = row_it.index();

            block_matrix<Matrix, SymmGroup> const & T = right_mult_mps[b2];
            MPOTensor_detail::const_term_descriptor<Matrix, SymmGroup> access = mpo.at(b1,b2);
            block_matrix<Matrix, SymmGroup> const & W = access.op;

            for (size_t k = 0; k < ket_basis.size(); ++k) {

                charge lc = boost::tuples::get<0>(ket_basis[k]);
                charge mc = boost::tuples::get<1>(ket_basis[k]);

                std::pair<const_iterator, const_iterator>
                  er = std::equal_range(right[b2].basis().begin(), right[b2].basis().end(),
                    boost::make_tuple(mc, SymmGroup::IdentityCharge, 0, 0), dual_index_detail::gt_row<SymmGroup>());

                for (const_iterator it = er.first; it != er.second; ++it)
                {
                    size_t matched_block = std::distance(right[b2].basis().begin(), it);
                    charge rc = boost::tuples::get<1>(right[b2].basis()[matched_block]);
                    size_t t_block = T.basis().position(lc, rc); // t_block != k in general

                    for (size_t w_block = 0; w_block < W.basis().size(); ++w_block)
                    {
                        charge phys_in = W.left_basis_charge(w_block);
                        charge phys_out = W.right_basis_charge(w_block);

                        charge out_l_charge = SymmGroup::fuse(lc, -phys_in);
                        if (!left_i.has(out_l_charge)) continue;

                        charge out_r_charge = SymmGroup::fuse(rc, -phys_out);
                        if (!left_i.has(out_r_charge) || !out_right_i.has(out_r_charge)) continue;

                        size_t l_size = left_i.size_of_block(out_l_charge);

                        size_t o = ret.find_block(out_l_charge, out_r_charge);
                        if ( o == ret.n_blocks() ) {
                            o = ret.insert_block(Matrix(1,1), out_l_charge, out_r_charge);
                            ret.resize_block(o, l_size, out_right_i.size_of_block(out_r_charge));
                        }

                        int i  = out_r_charge[1], ip = rc[1];
                        int j  = out_l_charge[1], jp  = mc[1];
                        int two_sp = std::abs(i - ip), two_s  = std::abs(j - jp);
                        int a = std::abs(i-j), k = std::abs(std::abs(phys_in[1])-std::abs(phys_out[1]));

                        int ap = std::abs(ip-jp);
                        if (ap >= 3) continue;

                        typename Matrix::value_type coupling_coeff = ::SU2::mod_coupling(j, two_s, jp, a,k,ap, i, two_sp, ip);
                        coupling_coeff *= sqrt((ip+1.)*(j+1.)/((i+1.)*(jp+1.))) * access.scale;

                        size_t phys_s1 = W.left_basis_size(w_block);
                        size_t phys_s2 = W.right_basis_size(w_block);
                        size_t in_left_offset = in_left_pb(phys_in, out_l_charge);
                        size_t out_right_offset = out_right_pb(phys_out, rc);
                        Matrix const & wblock = W[w_block];
                        Matrix const & iblock = T[t_block];
                        Matrix & oblock = ret[o];

                        //maquis::cout << "access " << mc << " - " << phys_out<< "|" << out_r_charge << " -- "
                        //         << lc << " - " << phys_in << "|" << out_l_charge
                        //         << " T(" << lc << "," << rc << "): +" << in_left_offset
                        //         << "(" << l_size << "x" << T.right_basis_size(t_block) << ")|" << T.left_basis_size(t_block) << " -> +"
                        //         << out_right_offset << "(" << l_size << "x" << T.right_basis_size(t_block) << ")|" << out_right_i.size_of_block(out_r_charge)
                        //         << " * " << j << two_s << jp << a << k << ap << i << two_sp << ip << " " << coupling_coeff * wblock(0,0) << std::endl;

                        maquis::dmrg::detail::rb_tensor_mpo(oblock, iblock, wblock,
                                out_right_offset, in_left_offset,
                                phys_s1, phys_s2, l_size, T.right_basis_size(t_block), coupling_coeff);
                    }
                }
            }
        } // b1
        return ret;
    }

} // namespace SU2
} // namespace contraction

#include "dmrg/mp_tensors/mps.h"
#include "dmrg/mp_tensors/mpo.h"

namespace SU2 {

    template<class Matrix, class SymmGroup>
    double expval(MPS<Matrix, SymmGroup> const & mps, MPO<Matrix, SymmGroup> const & mpo,
                  boost::shared_ptr<contraction::Engine<Matrix, Matrix, SymmGroup> > contr,
                  int p1, int p2, std::vector<int> config)
    {
        bool debug = false;
        if (p1 == 12 && p2 == 0) debug = false;

        assert(mpo.length() == mps.length());
        std::size_t L = mps.length();
        Boundary<Matrix, SymmGroup> left = mps.left_boundary();
        block_matrix<Matrix, SymmGroup> left_bm = mps.left_boundary()[0];

        for(size_t i = 0; i < L; ++i) {
            MPSTensor<Matrix, SymmGroup> cpy = mps[i];
            //left_bm = contraction::SU2::apply_operator(cpy, mps[i], left_bm, mpo[i], config, debug);
            left = contr->overlap_mpo_left_step(cpy, mps[i], left, mpo[i]);
        }

        return maquis::real(left[0].trace());
    }

    template<class Matrix, class SymmGroup>
    double expval_r(MPS<Matrix, SymmGroup> const & mps, MPO<Matrix, SymmGroup> const & mpo,
                    boost::shared_ptr<contraction::Engine<Matrix, Matrix, SymmGroup> > contr,
                    int p1, int p2, std::vector<int> config)
    {
        assert(mpo.length() == mps.length());
        std::size_t L = mps.length();
        Boundary<Matrix, SymmGroup> right = mps.right_boundary();

        for(int i = L-1; i >= 0; --i) {
            MPSTensor<Matrix, SymmGroup> cpy = mps[i];
            right = contr->overlap_mpo_right_step(cpy, mps[i], right, mpo[i]);
        }

        return maquis::real(right[0].trace());
    }

} // namespace SU2

#endif