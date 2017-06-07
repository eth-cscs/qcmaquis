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

#ifndef ENGINE_COMMON_MOVE_BOUNDARY_H
#define ENGINE_COMMON_MOVE_BOUNDARY_H

#include "dmrg/mp_tensors/mpstensor.h"
#include "dmrg/mp_tensors/mpotensor.h"
#include "dmrg/mp_tensors/reshapes.h"
#include "dmrg/block_matrix/indexing.h"

namespace contraction {
    namespace common {

        // output/input: left_i for bra_tensor, right_i for ket_tensor
        template<class Matrix, class OtherMatrix, class SymmGroup, class Gemm>
        static block_matrix<OtherMatrix, SymmGroup>
        overlap_left_step(MPSTensor<Matrix, SymmGroup> const & bra_tensor,
                          MPSTensor<Matrix, SymmGroup> const & ket_tensor,
                          block_matrix<OtherMatrix, SymmGroup> const & left,
                          block_matrix<OtherMatrix, SymmGroup> * localop = NULL)
        {
            if (localop != NULL)
                throw std::runtime_error("Not implemented!");

            assert(ket_tensor.phys_i == bra_tensor.phys_i);

            bra_tensor.make_left_paired();

            block_matrix<OtherMatrix, SymmGroup> t1;
            block_matrix<Matrix, SymmGroup> t3;
            ket_tensor.make_right_paired();
            typename Gemm::gemm()(left, ket_tensor.data(), t1);

            reshape_right_to_left_new(ket_tensor.site_dim(), bra_tensor.row_dim(), ket_tensor.col_dim(),
                                      t1, t3);
            typename Gemm::gemm()(transpose(conjugate(bra_tensor.data())), t3, t1);
            return t1;

            // original:
            // t3 = transpose(t3);
            // gemm(t3, t2, t1);
            // return transpose(t1);
        }

        template<class Matrix, class OtherMatrix, class SymmGroup, class Gemm>
        static block_matrix<OtherMatrix, SymmGroup>
        overlap_right_step(MPSTensor<Matrix, SymmGroup> const & bra_tensor,
                           MPSTensor<Matrix, SymmGroup> const & ket_tensor,
                           block_matrix<OtherMatrix, SymmGroup> const & right,
                           block_matrix<OtherMatrix, SymmGroup> * localop = NULL)
        {
            if (localop != NULL)
                throw std::runtime_error("Not implemented!");

            assert(ket_tensor.phys_i == bra_tensor.phys_i);

            bra_tensor.make_right_paired();
            ket_tensor.make_left_paired();

            block_matrix<OtherMatrix, SymmGroup> t1;
            block_matrix<Matrix, SymmGroup> t3;
            typename Gemm::gemm()(ket_tensor.data(), transpose(right), t1);
            reshape_left_to_right_new(ket_tensor.site_dim(), ket_tensor.row_dim(), bra_tensor.col_dim(), t1, t3);
            typename Gemm::gemm()(conjugate(bra_tensor.data()), transpose(t3), t1);

            return t1;
        }

        // note: this function changes the internal structure of Boundary,
        //       each block is transposed
        template<class Matrix, class OtherMatrix, class SymmGroup, class Gemm, class Kernel>
        static Boundary<Matrix, SymmGroup>
        left_boundary_tensor_mpo(MPSTensor<Matrix, SymmGroup> mps,
                                 Boundary<OtherMatrix, SymmGroup> const & left,
                                 MPOTensor<Matrix, SymmGroup> const & mpo,
                                 Index<SymmGroup> const * in_low = NULL)
        {
            typedef typename SymmGroup::charge charge;
            typedef typename MPOTensor<Matrix, SymmGroup>::index_type index_type;

            if (in_low == NULL)
                in_low = &mps.row_dim();

            BoundaryMPSProduct<Matrix, OtherMatrix, SymmGroup, Gemm> t(mps, left, mpo);

            Index<SymmGroup> physical_i = mps.site_dim(), left_i = *in_low, right_i = mps.col_dim(),
                                          out_left_i = physical_i * left_i;
            ProductBasis<SymmGroup> out_left_pb(physical_i, left_i);
            ProductBasis<SymmGroup> in_right_pb(physical_i, right_i,
                                    boost::lambda::bind(static_cast<charge(*)(charge, charge)>(SymmGroup::fuse),
                                            -boost::lambda::_1, boost::lambda::_2));

            index_type loop_max = mpo.col_dim();

    #ifdef USE_AMBIENT
            ContractionGrid<Matrix, SymmGroup> contr_grid(mpo, left.aux_dim(), mpo.col_dim());

            // TODO add separate allocate / execute Kernel templates
            parallel_for(index_type b2, parallel::range<index_type>(0,loop_max), {
                ::contraction::abelian::lbtm_kernel(b2, contr_grid, left, t, mpo, mps.data().basis(), right_i, out_left_i, in_right_pb, out_left_pb);
            });

            return contr_grid.make_boundary();
    #else
            Boundary<Matrix, SymmGroup> ret;
            ret.resize(mpo.col_dim());

            omp_for(index_type b2, parallel::range<index_type>(0,loop_max), {
                ContractionGrid<Matrix, SymmGroup> contr_grid(mpo, 0, 0);
                Kernel()(b2, contr_grid, left, t, mpo, mps.data().basis(), right_i, out_left_i, in_right_pb, out_left_pb);
                swap(ret[b2], contr_grid(0,0));
            });

            return ret;
    #endif
        }

        template<class Matrix, class OtherMatrix, class SymmGroup, class Gemm, class Kernel>
        static Boundary<Matrix, SymmGroup>
        right_boundary_tensor_mpo(MPSTensor<Matrix, SymmGroup> mps,
                                  Boundary<OtherMatrix, SymmGroup> const & right,
                                  MPOTensor<Matrix, SymmGroup> const & mpo,
                                  Index<SymmGroup> const * in_low = NULL)
        {
            typedef typename SymmGroup::charge charge;
            typedef typename MPOTensor<Matrix, SymmGroup>::index_type index_type;
            parallel::scheduler_permute scheduler(mpo.placement_l, parallel::groups_granularity);

            if (in_low == NULL)
                in_low = &mps.col_dim();

            contraction::common::MPSBoundaryProduct<Matrix, OtherMatrix, SymmGroup, Gemm> t(mps, right, mpo);

            Index<SymmGroup> physical_i = mps.site_dim(), left_i = mps.row_dim(), right_i = *in_low,
                             out_right_i = adjoin(physical_i) * right_i;

            ProductBasis<SymmGroup> in_left_pb(physical_i, left_i);
            ProductBasis<SymmGroup> out_right_pb(physical_i, right_i,
                                                 boost::lambda::bind(static_cast<charge(*)(charge, charge)>(SymmGroup::fuse),
                                                                     -boost::lambda::_1, boost::lambda::_2));
            Boundary<Matrix, SymmGroup> ret;
            ret.resize(mpo.row_dim());

            index_type loop_max = mpo.row_dim();

    #ifdef USE_AMBIENT

            // TODO: add separate allocate / execute Kernel templates
            parallel_for(index_type b1, parallel::range<index_type>(0,loop_max), {
                parallel::guard group(scheduler(b1), parallel::groups_granularity);
                ::contraction::abelian::rbtm_kernel(b1, ret[b1], right, t, mpo, mps.data().basis(), left_i, out_right_i, in_left_pb, out_right_pb);
            });
    #else
            omp_for(index_type b1, parallel::range<index_type>(0,loop_max), {
                parallel::guard group(scheduler(b1), parallel::groups_granularity);
                Kernel()(b1, ret[b1], right, t, mpo, mps.data().basis(), left_i, out_right_i, in_left_pb, out_right_pb);
            });

    #endif
            return ret;
        }

        template<class Matrix, class OtherMatrix, class SymmGroup, class Gemm, class Kernel>
        static Boundary<OtherMatrix, SymmGroup>
        overlap_mpo_left_step(MPSTensor<Matrix, SymmGroup> const & bra_tensor,
                              MPSTensor<Matrix, SymmGroup> const & ket_tensor,
                              Boundary<OtherMatrix, SymmGroup> const & left,
                              MPOTensor<Matrix, SymmGroup> const & mpo)
        {
            typedef typename SymmGroup::charge charge;
            typedef typename MPOTensor<Matrix, SymmGroup>::index_type index_type;

            MPSTensor<Matrix, SymmGroup> ket_cpy = ket_tensor;
            BoundaryMPSProduct<Matrix, OtherMatrix, SymmGroup, Gemm> t(ket_cpy, left, mpo);

            Index<SymmGroup> const & left_i = bra_tensor.row_dim();
            Index<SymmGroup> right_i = ket_tensor.col_dim();
            Index<SymmGroup> out_left_i = ket_tensor.site_dim() * left_i;
            common_subset(out_left_i, right_i);
            ProductBasis<SymmGroup> out_left_pb(ket_tensor.site_dim(), left_i);
            ProductBasis<SymmGroup> in_right_pb(ket_tensor.site_dim(), right_i,
                                    boost::lambda::bind(static_cast<charge(*)(charge, charge)>(SymmGroup::fuse),
                                            -boost::lambda::_1, boost::lambda::_2));

            index_type loop_max = mpo.col_dim();

            bra_tensor.make_left_paired();
            block_matrix<Matrix, SymmGroup> bra_conj = conjugate(bra_tensor.data());

            DualIndex<SymmGroup> ket_basis_transpose = ket_cpy.data().basis().transpose();

    #ifdef USE_AMBIENT
            ContractionGrid<Matrix, SymmGroup> contr_grid(mpo, left.aux_dim(), mpo.col_dim());

            // TODO: add separate allocate / execute Kernel templates
            parallel_for(index_type b2, parallel::range<index_type>(0,loop_max), {
                ::contraction::abelian::lbtm_kernel(b2, contr_grid, left, t, mpo, ket_cpy.data().basis(), right_i, out_left_i, in_right_pb, out_left_pb);
            });
            for(index_type b2 = 0; b2 < loop_max; b2++){
                // TODO: use SU2 gemm in contr_grid in SU2 runs
                contr_grid.multiply_column_trans(b2, bra_conj);
            };

            return contr_grid.make_boundary();
    #else
            Boundary<Matrix, SymmGroup> ret;
            ret.resize(loop_max);

            omp_for(index_type b2, parallel::range<index_type>(0,loop_max), {
                if (mpo.herm_info.right_skip(b2)) continue;
                ContractionGrid<Matrix, SymmGroup> contr_grid(mpo, 0, 0);
                Kernel()(b2, contr_grid, left, t, mpo, ket_basis_transpose, right_i, out_left_i, in_right_pb, out_left_pb);
                typename Gemm::gemm()(transpose(contr_grid(0,0)), bra_conj, ret[b2], MPOTensor_detail::get_spin(mpo, b2, false));
            });

            /*
            // hermiticity check
            omp_for(index_type b2, parallel::range<index_type>(0,loop_max), {
              if (mpo.herm_info.right_skip(b2))
              {
                block_matrix<Matrix, SymmGroup> conj0 = ret[mpo.herm_info.right_conj(b2)];
                recover_conjugate(conj0, mpo, b2, false, false);

                block_matrix<Matrix, SymmGroup> conj = transpose(conj0);
                block_matrix<Matrix, SymmGroup> diff = conj-ret[b2];
                if ( diff.norm() > 1e-6)
                {
                    maquis::cout << "diff: " << diff << std::endl;
                    maquis::cout << "should:\n" << ret[b2];
                    maquis::cout << "is:\n" << conj;
                    exit(1);
                }
              }
            });
            */

            return ret;
    #endif
        }

        template<class Matrix, class OtherMatrix, class SymmGroup, class Gemm, class Kernel>
        static Boundary<OtherMatrix, SymmGroup>
        overlap_mpo_right_step(MPSTensor<Matrix, SymmGroup> const & bra_tensor,
                               MPSTensor<Matrix, SymmGroup> const & ket_tensor,
                               Boundary<OtherMatrix, SymmGroup> const & right,
                               MPOTensor<Matrix, SymmGroup> const & mpo)
        {
            typedef typename SymmGroup::charge charge;
            typedef typename MPOTensor<Matrix, SymmGroup>::index_type index_type;
            parallel::scheduler_permute scheduler(mpo.placement_l, parallel::groups_granularity);

            MPSTensor<Matrix, SymmGroup> ket_cpy = ket_tensor;
            contraction::common::MPSBoundaryProduct<Matrix, OtherMatrix, SymmGroup, Gemm> t(ket_cpy, right, mpo);
            t.initialize_indices();

            Index<SymmGroup> const & physical_i = ket_tensor.site_dim(),
                                     right_i = bra_tensor.col_dim();
            Index<SymmGroup> left_i = ket_tensor.row_dim(),
                             out_right_i = adjoin(physical_i) * right_i;

            common_subset(out_right_i, left_i);
            ProductBasis<SymmGroup> in_left_pb(physical_i, left_i);
            ProductBasis<SymmGroup> out_right_pb(physical_i, right_i,
                                                 boost::lambda::bind(static_cast<charge(*)(charge, charge)>(SymmGroup::fuse),
                                                                     -boost::lambda::_1, boost::lambda::_2));
            Boundary<Matrix, SymmGroup> ret;
            ret.resize(mpo.row_dim());

            //ket_tensor.make_right_paired();
            index_type loop_max = mpo.row_dim();

            bra_tensor.make_right_paired();
            block_matrix<Matrix, SymmGroup> bra_conj = conjugate(bra_tensor.data());

    #ifdef USE_AMBIENT
            parallel_for(index_type b1, parallel::range<index_type>(0,loop_max), {
                parallel::guard group(scheduler(b1), parallel::groups_granularity);
                ::contraction::abelian::rbtm_kernel(b1, ret[b1], right, t, mpo, ket_cpy.data().basis(), left_i, out_right_i, in_left_pb, out_right_pb);
            });
            omp_for(index_type b1, parallel::range<index_type>(0,loop_max), {
                block_matrix<Matrix, SymmGroup> tmp;
                gemm(ret[b1], transpose(bra_conj), tmp, parallel::scheduler_size_indexed(ret[b1]));
                swap(ret[b1], tmp);
            });

    #else
            omp_for(index_type b1, parallel::range<index_type>(0,loop_max), {
                if (mpo.herm_info.left_skip(b1)) continue;
                Kernel()(b1, ret[b1], right, t, mpo, ket_cpy.data().basis(), left_i, out_right_i, in_left_pb, out_right_pb);

                block_matrix<Matrix, SymmGroup> tmp;
                typename Gemm::gemm()(ret[b1], transpose(bra_conj), tmp, MPOTensor_detail::get_spin(mpo, b1, true));
                swap(ret[b1], tmp);
            });
    #endif
            return ret;
        }

        template<class Matrix, class OtherMatrix, class SymmGroup, class TaskCalc>
        static Boundary<OtherMatrix, SymmGroup>
        left_boundary_tensor_mpo(MPSTensor<Matrix, SymmGroup> const & ket_tensor,
                                 Boundary<OtherMatrix, SymmGroup> const & left,
                                 MPOTensor<Matrix, SymmGroup> const & mpo,
                                 TaskCalc task_calc)
        {
            typedef typename SymmGroup::charge charge;
            typedef typename MPOTensor<Matrix, SymmGroup>::index_type index_type;
            typedef typename Schedule<Matrix, SymmGroup>::schedule_t schedule_t;
            typedef typename Schedule<Matrix, SymmGroup>::block_type::const_iterator const_iterator;

            LeftIndices<Matrix, OtherMatrix, SymmGroup> left_indices(left, mpo);
            Boundary<OtherMatrix, SymmGroup> ret;
            ret.resize(mpo.col_dim());

            if (!ket_tensor.is_right_paired())
            {
                parallel_critical
                ket_tensor.make_right_paired();
            }

            // MPS indices
            Index<SymmGroup> const & physical_i = ket_tensor.site_dim(),
                                     left_i = ket_tensor.row_dim();
            Index<SymmGroup> right_i = ket_tensor.col_dim(),
                             out_left_i = physical_i * left_i;

            common_subset(out_left_i, right_i);
            ProductBasis<SymmGroup> left_pb(physical_i, left_i);
            ProductBasis<SymmGroup> right_pb(physical_i, right_i,
                                    boost::lambda::bind(static_cast<charge(*)(charge, charge)>(SymmGroup::fuse),
                                            -boost::lambda::_1, boost::lambda::_2));

            // Schedule
            unsigned loop_max = right_i.size();
            schedule_t tasks(loop_max);
            omp_for(unsigned mb, parallel::range<unsigned>(0,loop_max), {
                task_calc(mpo, left_indices, left_i,
                          right_i, physical_i, right_pb, mb, tasks[mb], false);
            });

            // set up the indices of the new boundary
            for(unsigned rb_ket = 0; rb_ket < loop_max; ++rb_ket)
            {
                charge rc_ket = right_i[rb_ket].first;
                unsigned rs_ket = right_i[rb_ket].second;
                for (const_iterator it = tasks[rb_ket].begin(); it != tasks[rb_ket].end(); ++it)
                {
                    charge rc_bra = it->first;
                    unsigned ls_paired = out_left_i.size_of_block(rc_bra);
                    it->second.reserve(rc_bra, rc_ket, ls_paired, rs_ket, ret);

                    for (unsigned s = 0; s < it->second.size(); ++s)
                    {
                        unsigned lb_bra = it->second[s].get_mps_block();
                        charge lc_bra = left_i[lb_bra].first;
                        charge phys_out = SymmGroup::fuse(rc_bra, -lc_bra);

                        // assuming single-site, only need index 0
                        it->second[s][0].offset = left_pb(phys_out, lc_bra);
                    }
                }
            }

            // Contraction
            omp_for(index_type rb_ket, parallel::range<index_type>(0,loop_max), {
                charge rc_ket = right_i[rb_ket].first;
                for (const_iterator it = tasks[rb_ket].begin(); it != tasks[rb_ket].end(); ++it) // mc loop
                {
                    charge rc_bra = it->first;
                    it->second.allocate(rc_bra, rc_ket, ret);
                    for (unsigned s = 0; s < it->second.size(); ++s) // physical index loop
                        it->second[s].lbtm(ket_tensor, it->second.get_b_to_o(), left, ret);
                }
            });

            return ret;
        }

        template<class Matrix, class OtherMatrix, class SymmGroup, class TaskCalc>
        static Boundary<OtherMatrix, SymmGroup>
        overlap_mpo_left_step(MPSTensor<Matrix, SymmGroup> const & bra_tensor,
                              MPSTensor<Matrix, SymmGroup> const & ket_tensor,
                              Boundary<OtherMatrix, SymmGroup> const & left,
                              MPOTensor<Matrix, SymmGroup> const & mpo,
                              TaskCalc task_calc)
        {
            typedef typename SymmGroup::charge charge;
            typedef typename MPOTensor<Matrix, SymmGroup>::index_type index_type;
            typedef typename Schedule<Matrix, SymmGroup>::schedule_t schedule_t;
            typedef typename Schedule<Matrix, SymmGroup>::block_type::const_iterator const_iterator;

            LeftIndices<Matrix, OtherMatrix, SymmGroup> left_indices(left, mpo);
            Boundary<OtherMatrix, SymmGroup> ret;
            ret.resize(mpo.col_dim());

            if (!ket_tensor.is_right_paired() || !bra_tensor.is_right_paired())
            {
                parallel_critical {
                ket_tensor.make_right_paired();
                bra_tensor.make_right_paired();
                }
            }

            // MPS indices
            Index<SymmGroup> const & physical_i = ket_tensor.site_dim(),
                                     right_i = ket_tensor.col_dim();
            Index<SymmGroup> left_i = ket_tensor.row_dim(),
                             out_right_i = adjoin(physical_i) * right_i;

            common_subset(out_right_i, left_i);
            ProductBasis<SymmGroup> out_right_pb(physical_i, right_i,
                    boost::lambda::bind(static_cast<charge(*)(charge, charge)>(SymmGroup::fuse),
                                    -boost::lambda::_1, boost::lambda::_2));

            // Schedule
            unsigned loop_max = right_i.size();
            schedule_t tasks(loop_max);
            omp_for(unsigned mb, parallel::range<unsigned>(0,loop_max), {
                task_calc(mpo, left_indices, left_i,
                          right_i, physical_i, out_right_pb, mb, tasks[mb], true);
            });

            // set up the indices of the new boundary
            for(size_t rb_ket = 0; rb_ket < loop_max; ++rb_ket)
            {
                charge rc_ket = right_i[rb_ket].first;
                size_t rs_ket = right_i[rb_ket].second;
                for (const_iterator it = tasks[rb_ket].begin(); it != tasks[rb_ket].end(); ++it)
                {
                    charge rc_bra = it->first;
                    size_t rs_bra = right_i.size_of_block(rc_bra);
                    it->second.reserve(rc_bra, rc_ket, rs_bra, rs_ket, ret); // allocate all (rc_bra,rc_ket) blocks
                }
            }

            // Contraction
            omp_for(index_type rb_ket, parallel::range<index_type>(0,loop_max), {
                charge rc_ket = right_i[rb_ket].first;
                for (const_iterator it = tasks[rb_ket].begin(); it != tasks[rb_ket].end(); ++it) // mc loop
                {
                    charge rc_bra = it->first;
                    it->second.allocate(rc_bra, rc_ket, ret);
                    for (size_t s = 0; s < it->second.size(); ++s) // physical index loop
                        it->second[s].prop_l(ket_tensor, bra_tensor, it->second.get_b_to_o(), left, ret);
                }
            });

            return ret;
        }

        template<class Matrix, class OtherMatrix, class SymmGroup, class TaskCalc>
        static Boundary<OtherMatrix, SymmGroup>
        overlap_mpo_right_step(MPSTensor<Matrix, SymmGroup> const & bra_tensor,
                               MPSTensor<Matrix, SymmGroup> const & ket_tensor,
                               Boundary<OtherMatrix, SymmGroup> const & right,
                               MPOTensor<Matrix, SymmGroup> const & mpo,
                               TaskCalc task_calc)
        {
            typedef typename SymmGroup::charge charge;
            typedef typename MPOTensor<Matrix, SymmGroup>::index_type index_type;
            typedef typename Schedule<Matrix, SymmGroup>::schedule_t schedule_t;
            typedef typename Schedule<Matrix, SymmGroup>::block_type::const_iterator const_iterator;

            RightIndices<Matrix, OtherMatrix, SymmGroup> right_indices(right, mpo);
            Boundary<OtherMatrix, SymmGroup> ret;
            ret.resize(mpo.row_dim());

            if (!ket_tensor.is_right_paired() || !bra_tensor.is_right_paired())
            {
                parallel_critical {
                ket_tensor.make_right_paired();
                bra_tensor.make_right_paired();
                }
            }

            // MPS indices
            Index<SymmGroup> const & physical_i = ket_tensor.site_dim(),
                                     right_i = ket_tensor.col_dim();
            Index<SymmGroup> left_i = ket_tensor.row_dim(),
                             out_right_i = adjoin(physical_i) * right_i;

            common_subset(out_right_i, left_i);
            ProductBasis<SymmGroup> out_right_pb(physical_i, right_i,
                    boost::lambda::bind(static_cast<charge(*)(charge, charge)>(SymmGroup::fuse),
                                    -boost::lambda::_1, boost::lambda::_2));

            // Schedule
            schedule_t tasks(left_i.size()); // bra
            unsigned loop_max = left_i.size(); // bra
            omp_for(unsigned mb, parallel::range<unsigned>(0,loop_max), {
                task_calc(mpo, right_indices, left_i,
                          right_i, physical_i, out_right_pb, mb, tasks[mb]);
            });

            // set up the indices of the new boundary
            for(size_t mps_block = 0; mps_block < loop_max; ++mps_block)
            {
                charge lc = left_i[mps_block].first;
                size_t l_size = left_i[mps_block].second;
                for (const_iterator it = tasks[mps_block].begin(); it != tasks[mps_block].end(); ++it)
                {
                    charge mc = it->first;
                    size_t m_size = left_i.size_of_block(mc);
                    it->second.reserve(mc, lc, m_size, l_size, ret); // allocate all (mc,lc) blocks
                }
            }

            // Contraction
            omp_for(index_type mps_block, parallel::range<index_type>(0,loop_max), {
                charge lc = left_i[mps_block].first;
                for (const_iterator it = tasks[mps_block].begin(); it != tasks[mps_block].end(); ++it) // mc loop
                {
                    charge mc = it->first;
                    it->second.allocate(mc, lc, ret); // allocate all (mc,lc) blocks
                    for (size_t s = 0; s < it->second.size(); ++s) // physical index loop
                        it->second[s].prop(ket_tensor, bra_tensor.data()[mps_block], it->second.get_b_to_o(), right, ret);
                }
            });

            return ret;
        }

    } // namespace common
} // namespace contraction

#endif
