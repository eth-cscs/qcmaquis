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

#ifndef CONTRACTIONS_SU2_SITE_HAMIL_HPP
#define CONTRACTIONS_SU2_SITE_HAMIL_HPP

namespace contraction {

    // forward declarations
    template<class Matrix, class OtherMatrix, class SymmGroup>
    MPSTensor<Matrix, SymmGroup>
    site_hamil_lbtm(MPSTensor<Matrix, SymmGroup> ket_tensor,
                    Boundary<OtherMatrix, SymmGroup> const & left,
                    Boundary<OtherMatrix, SymmGroup> const & right,
                    MPOTensor<Matrix, SymmGroup> const & mpo);

    template<class Matrix, class OtherMatrix, class SymmGroup>
    MPSTensor<Matrix, SymmGroup>
    site_hamil_rbtm(MPSTensor<Matrix, SymmGroup> ket_tensor,
                    Boundary<OtherMatrix, SymmGroup> const & left,
                    Boundary<OtherMatrix, SymmGroup> const & right,
                    MPOTensor<Matrix, SymmGroup> const & mpo,
                    std::vector<common::task_capsule<Matrix, SymmGroup> > const & tasks);

    template<class Matrix, class OtherMatrix, class SymmGroup>
    MPSTensor<Matrix, SymmGroup>
    site_hamil_shtm(MPSTensor<Matrix, SymmGroup> ket_tensor,
                    Boundary<OtherMatrix, SymmGroup> const & left,
                    Boundary<OtherMatrix, SymmGroup> const & right,
                    MPOTensor<Matrix, SymmGroup> const & mpo,
                    typename common::Schedule<Matrix, SymmGroup>::schedule_t const & tasks);
    // *************************************************************


    template<class Matrix, class OtherMatrix, class SymmGroup>
    MPSTensor<Matrix, SymmGroup>
    Engine<Matrix, OtherMatrix, SymmGroup, typename boost::enable_if<symm_traits::HasSU2<SymmGroup> >::type>::
    site_hamil2(MPSTensor<Matrix, SymmGroup> ket_tensor,
                Boundary<OtherMatrix, SymmGroup> const & left,
                Boundary<OtherMatrix, SymmGroup> const & right,
                MPOTensor<Matrix, SymmGroup> const & mpo,
                //std::vector<common::task_capsule<Matrix, SymmGroup> > const & tasks)
                typename common::Schedule<Matrix, SymmGroup>::schedule_t const & tasks)
    {
        //if ( (mpo.row_dim() - mpo.num_one_rows()) < (mpo.col_dim() - mpo.num_one_cols()) )
        //    return site_hamil_lbtm(ket_tensor, left, right, mpo);
        //else
        //    return site_hamil_rbtm(ket_tensor, left, right, mpo, tasks);
        return site_hamil_shtm(ket_tensor, left, right, mpo, tasks); 
    }

    // *************************************************************
    // specialized variants

    template<class Matrix, class OtherMatrix, class SymmGroup>
    MPSTensor<Matrix, SymmGroup>
    site_hamil_lbtm(MPSTensor<Matrix, SymmGroup> ket_tensor,
                    Boundary<OtherMatrix, SymmGroup> const & left,
                    Boundary<OtherMatrix, SymmGroup> const & right,
                    MPOTensor<Matrix, SymmGroup> const & mpo)
    {
        typedef typename SymmGroup::charge charge;
        typedef typename MPOTensor<Matrix, SymmGroup>::index_type index_type;
        typedef typename Matrix::value_type value_type;

        common::BoundaryMPSProduct<Matrix, OtherMatrix, SymmGroup, ::SU2::SU2Gemms> t(ket_tensor, left, mpo);

        Index<SymmGroup> const & physical_i = ket_tensor.site_dim(),
                               & left_i = ket_tensor.row_dim();
        Index<SymmGroup> right_i = ket_tensor.col_dim(),
                         out_left_i = physical_i * left_i;

        common_subset(out_left_i, right_i);
        ProductBasis<SymmGroup> out_left_pb(physical_i, left_i);
        ProductBasis<SymmGroup> in_right_pb(physical_i, right_i,
                                boost::lambda::bind(static_cast<charge(*)(charge, charge)>(SymmGroup::fuse),
                                        -boost::lambda::_1, boost::lambda::_2));

        MPSTensor<Matrix, SymmGroup> ret;
        ret.phys_i = ket_tensor.site_dim(); ret.left_i = ket_tensor.row_dim(); ret.right_i = ket_tensor.col_dim();
        index_type loop_max = mpo.col_dim();

        DualIndex<SymmGroup> ket_basis_transpose = ket_tensor.data().basis().transpose();

#ifdef USE_AMBIENT
        {
            block_matrix<Matrix, SymmGroup> empty;
            swap(ket_tensor.data(), empty); // deallocating mpstensor before exiting the stack
        }
        parallel::sync();
        ContractionGrid<Matrix, SymmGroup> contr_grid(mpo, left.aux_dim(), mpo.col_dim());

        parallel_for(index_type b2, parallel::range<index_type>(0,loop_max), {
            SU2::lbtm_kernel(b2, contr_grid, left, t, mpo, ket_tensor.data().basis(), right_i, out_left_i, in_right_pb, out_left_pb);
        });
        omp_for(index_type b2, parallel::range<index_type>(0,loop_max), {
            contr_grid.multiply_column(b2, right[b2]);
        });
        t.clear();
        parallel::sync();

        swap(ret.data(), contr_grid.reduce());
        parallel::sync();

#else
        omp_for(index_type b2, parallel::range<index_type>(0,loop_max), {
            ContractionGrid<Matrix, SymmGroup> contr_grid(mpo, 0, 0);
            block_matrix<Matrix, SymmGroup> tmp, tmp2;

            typename MPOTensor<OtherMatrix, SymmGroup>::col_proxy cp = mpo.column(b2);
            index_type num_ops = std::distance(cp.begin(), cp.end());
            if (num_ops > 3) {
                SU2::lbtm_kernel_rp(b2, contr_grid, left, t, mpo, ket_basis_transpose, right_i, out_left_i, in_right_pb, out_left_pb);
                reshape_right_to_left_new(physical_i, left_i, right_i, contr_grid(0,0), tmp2);

                contr_grid(0,0).clear();
                swap(contr_grid(0,0), tmp2);
            }
            else {
                SU2::lbtm_kernel(b2, contr_grid, left, t, mpo, ket_basis_transpose, right_i, out_left_i, in_right_pb, out_left_pb);
            }

            if (mpo.herm_info.right_skip(b2)) {
                block_matrix<typename maquis::traits::transpose_view<Matrix>::type, SymmGroup> trv = transpose(right[mpo.herm_info.right_conj(b2)]);
                std::vector<value_type> phases = common::conjugate_phases(trv.basis(), mpo, b2, false, true);
                ::SU2::gemm_trim(contr_grid(0,0), trv, tmp, phases, false);
            }
            else
                ::SU2::gemm_trim(contr_grid(0,0), right[b2], tmp, std::vector<value_type>(contr_grid(0,0).n_blocks(), 1.), true);

            contr_grid(0,0).clear();

            if (num_ops > 3)
            for (std::size_t k = 0; k < tmp.n_blocks(); ++k)
                if (!out_left_i.has(tmp.basis().left_charge(k)))
                    tmp.remove_block(k--);

            parallel_critical
            for (std::size_t k = 0; k < tmp.n_blocks(); ++k)
                ret.data().match_and_add_block(tmp[k], tmp.basis().left_charge(k), tmp.basis().right_charge(k));
        });
#endif
        return ret;
    }

    template<class Matrix, class OtherMatrix, class SymmGroup>
    MPSTensor<Matrix, SymmGroup>
    site_hamil_rbtm(MPSTensor<Matrix, SymmGroup> ket_tensor,
                    Boundary<OtherMatrix, SymmGroup> const & left,
                    Boundary<OtherMatrix, SymmGroup> const & right,
                    MPOTensor<Matrix, SymmGroup> const & mpo,
                    std::vector<common::task_capsule<Matrix, SymmGroup> > const & tasks)
    {
        typedef typename SymmGroup::charge charge;
        typedef typename MPOTensor<Matrix, SymmGroup>::index_type index_type;
        typedef typename Matrix::value_type value_type;

        ket_tensor.make_left_paired();
        MPSBoundaryProduct<Matrix, OtherMatrix, SymmGroup, ::SU2::SU2Gemms> t(ket_tensor, right, mpo);
        common::LeftIndices<Matrix, OtherMatrix, SymmGroup> left_indices(left, mpo);

        Index<SymmGroup> const & physical_i = ket_tensor.site_dim(),
                                 right_i = ket_tensor.col_dim();
        Index<SymmGroup> left_i = ket_tensor.row_dim(),
                         out_right_i = adjoin(physical_i) * right_i;

        common_subset(out_right_i, left_i);

        block_matrix<Matrix, SymmGroup> collector;
        MPSTensor<Matrix, SymmGroup> ret;
        ret.phys_i = ket_tensor.site_dim(); ret.left_i = ket_tensor.row_dim(); ret.right_i = ket_tensor.col_dim();

        index_type loop_max = mpo.row_dim();
        omp_for(index_type b1, parallel::range<index_type>(0,loop_max), {

            block_matrix<Matrix, SymmGroup> tmp, tmp2;

            if (mpo.herm_info.left_skip(b1))
                SU2::rbtm_axpy_gemm(b1, tasks[b1], tmp2, out_right_i, left, mpo, left[mpo.herm_info.left_conj(b1)], t);
            else
                SU2::rbtm_axpy_gemm(b1, tasks[b1], tmp2, out_right_i, left, mpo, transpose(left[b1]), t);
            t.free(b1);

            parallel_critical
            for (std::size_t k = 0; k < tmp2.n_blocks(); ++k)
                collector.match_and_add_block(tmp2[k], tmp2.basis().left_charge(k), tmp2.basis().right_charge(k));
        });

        reshape_right_to_left_new(physical_i, left_i, right_i, collector, ret.data());

        return ret;
    }

    template<class Matrix, class OtherMatrix, class SymmGroup>
    MPSTensor<Matrix, SymmGroup>
    site_hamil_shtm(MPSTensor<Matrix, SymmGroup> ket_tensor,
                    Boundary<OtherMatrix, SymmGroup> const & left,
                    Boundary<OtherMatrix, SymmGroup> const & right,
                    MPOTensor<Matrix, SymmGroup> const & mpo,
                    //std::vector<common::MPSBlock<Matrix, SymmGroup> > const & tasks)
                    typename common::Schedule<Matrix, SymmGroup>::schedule_t const & tasks)
    {
        typedef typename SymmGroup::charge charge;
        typedef typename MPOTensor<Matrix, SymmGroup>::index_type index_type;
        typedef typename Matrix::value_type value_type;

        typedef typename common::MPSBlock<Matrix, SymmGroup>::base::const_iterator const_iterator;

        ket_tensor.make_right_paired();
        common::LeftIndices<Matrix, OtherMatrix, SymmGroup> left_indices(left, mpo);
        common::RightIndices<Matrix, OtherMatrix, SymmGroup> right_indices(right, mpo);

        Index<SymmGroup> const & physical_i = ket_tensor.site_dim(),
                                 right_i = ket_tensor.col_dim();
        Index<SymmGroup> left_i = ket_tensor.row_dim(),
                         out_right_i = adjoin(physical_i) * right_i;

        common_subset(out_right_i, left_i);

        MPSTensor<Matrix, SymmGroup> ret;
        ret.phys_i = ket_tensor.site_dim(); ret.left_i = ket_tensor.row_dim(); ret.right_i = ket_tensor.col_dim();
        block_matrix<Matrix, SymmGroup> collector(ket_tensor.data().basis());

        index_type loop_max = tasks.size();
        omp_for(index_type mps_block, parallel::range<index_type>(0,loop_max), {
            for (const_iterator it = tasks[mps_block].begin(); it != tasks[mps_block].end(); ++it)
            {
                charge mc = it->first;
                for (size_t s = 0; s < it->second.size(); ++s)
                {
                    common::ContractionGroup<Matrix, SymmGroup> const & cg = it->second[s];
                    cg.create_T(ket_tensor, right, mpo, right_indices);
                    for (size_t ss1 = 0; ss1 < cg.size(); ++ss1)
                    {
                        if (!cg[ss1].valid) continue;
                        unsigned offset = cg[ss1].offset;
                        Matrix C = cg[ss1].contract(left, cg.T, mpo, left_indices);
                        maquis::dmrg::detail::iterator_axpy(&C(0,0), &C(0,0) + num_rows(C) * num_cols(C),
                                                            &collector[mps_block](0, offset), value_type(1.0));
                    }
                }
            }
        });

        reshape_right_to_left_new(physical_i, left_i, right_i, collector, ret.data());

        return ret;
    }

} // namespace contraction

#endif
