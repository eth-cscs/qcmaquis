/*****************************************************************************
 *
 * ALPS MPS DMRG Project
 *
 * Copyright (C) 2017 Department of Chemistry and the PULSE Institute, Stanford University
 *                    Laboratory for Physical Chemistry, ETH Zurich
 *               2017-2017 by Sebastian Keller <sebkelle@phys.ethz.ch>
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

#ifndef CONTRACTIONS_SU2_SHTM_HPP
#define CONTRACTIONS_SU2_SHTM_HPP

#include "dmrg/block_matrix/symmetry/gsl_coupling.h"
#include "dmrg/mp_tensors/mpstensor.h"
#include "dmrg/mp_tensors/mpotensor.h"
#include "dmrg/mp_tensors/contractions/non-abelian/functors.h"
#include "dmrg/mp_tensors/contractions/non-abelian/micro_kernels.hpp"
#include "dmrg/mp_tensors/contractions/non-abelian/gemm.hpp"

namespace contraction {
namespace SU2 {

    using common::task_capsule;
    using common::ContractionGroup;
    using common::MatrixGroup;

    template<class Matrix, class OtherMatrix, class SymmGroup>
    void shtm_tasks(MPOTensor<Matrix, SymmGroup> const & mpo,
                    Boundary<OtherMatrix, SymmGroup> const & left,
                    Boundary<OtherMatrix, SymmGroup> const & right,
                    DualIndex<SymmGroup> const & ket_basis,
                    Index<SymmGroup> const & right_i,
                    ProductBasis<SymmGroup> const & right_pb,
                    typename SymmGroup::charge lc,
                    typename SymmGroup::charge phys,
                    unsigned out_offset,
                    ContractionGroup<Matrix, SymmGroup> & cgrp)
    {
        typedef typename MPOTensor<Matrix, SymmGroup>::index_type index_type;
        typedef typename MPOTensor<Matrix, SymmGroup>::row_proxy row_proxy;
        typedef typename DualIndex<SymmGroup>::const_iterator const_iterator;
        typedef typename SymmGroup::charge charge;
        typedef typename Matrix::value_type value_type;

        typedef typename task_capsule<Matrix, SymmGroup>::micro_task micro_task;

        charge rc = SymmGroup::fuse(lc, phys);

        size_t l_size = ket_basis.left_block_size(lc, lc);
        size_t r_size = right_i.size_of_block(rc);
        size_t pre_offset = right_pb(phys, rc);
        maquis::cout << "pre_offset " << pre_offset << " rc " << rc << std::endl;

        for (index_type b1 = 0; b1 < mpo.row_dim(); ++b1)
        {
            //maquis::cout << "b1 " << b1 << std::endl;
            //std::vector<value_type> phases = (mpo.herm_info.left_skip(b1)) ? common::conjugate_phases(left[b1], mpo, b1, true, false) :
            //                                                                 std::vector<value_type>(left[b1].n_blocks(),1.);
            const_iterator lit = left[b1].basis().left_lower_bound(lc);
            for ( ; lit != left[b1].basis().end() && lit->lc == lc; ++lit)
            {
                // MatrixGroup for mc
                charge mc = lit->rc;       
                size_t m_size = lit->rs;

                int a = mpo.left_spin(b1).get();
                if (!::SU2::triangle(SymmGroup::spin(mc), a, SymmGroup::spin(lc))) continue;
    
                size_t k = left[b1].basis().position(lc, mc);   if (k == left[b1].basis().size()) continue;
                MatrixGroup<Matrix, SymmGroup> & mg = cgrp.mgroups[boost::make_tuple(out_offset, mc)];
                mg.add_line(b1, k);

                row_proxy row_b1 = mpo.row(b1);
                for (typename row_proxy::const_iterator row_it = row_b1.begin(); row_it != row_b1.end(); ++row_it) {
                    index_type b2 = row_it.index();

                    charge mcprobe(0); mcprobe[0] = 4;
                    bool check = (mc == mcprobe && b1 == 5 && b2 == 167);

                    if (check) maquis::cout << "  mc " << mc << std::endl;
                    if (check) maquis::cout << "    b2 " << b2 << std::endl;

                    MPOTensor_detail::term_descriptor<Matrix, SymmGroup, true> access = mpo.at(b1,b2);

                    for (size_t op_index = 0; op_index < access.size(); ++op_index)
                    {
                        typename operator_selector<Matrix, SymmGroup>::type const & W = access.op(op_index);

                        for (size_t w_block = 0; w_block < W.basis().size(); ++w_block)
                        {
                            charge phys_in = W.basis().left_charge(w_block);
                            charge phys_out = W.basis().right_charge(w_block);
                            if (phys_out != phys) continue;

                            //if (!ket_basis.left_has(SymmGroup::fuse(mc, phys_in))) continue;

                            size_t r_block = right[b2].find_block(SymmGroup::fuse(mc, phys_in), rc);
                            if (r_block == right[b2].n_blocks()) continue;

                            if (!right_i.has(SymmGroup::fuse(mc, phys_in))) continue;

                            size_t in_offset = right_pb(phys_in, SymmGroup::fuse(phys_in, mc));

                            typename Matrix::value_type couplings[4];
                            couplings[0] = 1.;
                            couplings[1] = 1.;
                            couplings[2] = 1.;
                            couplings[3] = 1.;
                            
                            if (check) maquis::cout << "phys_in/out " << phys_in << phys_out << std::endl;
                            micro_task tpl; tpl.l_size = m_size; tpl.stripe = m_size; tpl.b2 = b2; tpl.k = r_block; tpl.out_offset = out_offset;
                            detail::op_iterate_shtm<Matrix, SymmGroup>(W, w_block, couplings, mg.current_row(), tpl,
                                                                       in_offset, 0, r_size, pre_offset, check);
                        } // w_block
                        if (check) maquis::cout << std::endl;
                        
                    } //op_index

                } // b2
            } // mc
        } // b1

    }

} // namespace SU2
} // namespace contraction

#endif