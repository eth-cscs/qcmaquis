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

#ifndef ABELIAN_ENGINE_H
#define ABELIAN_ENGINE_H

#include "dmrg/mp_tensors/mpstensor.h"
#include "dmrg/mp_tensors/mpotensor.h"
#include "dmrg/mp_tensors/twositetensor.h"

#include "dmrg/solver/solver.h"

#include "dmrg/mp_tensors/contractions/create_schedule/site_hamil_tasks.hpp"
#include "dmrg/mp_tensors/contractions/create_schedule/right_tasks.hpp"
#include "dmrg/mp_tensors/contractions/create_schedule/left_tasks.hpp"
#include "dmrg/mp_tensors/contractions/create_schedule/site_hamil_schedule.hpp"
#include "dmrg/mp_tensors/contractions/create_schedule/h_diag.hpp"
#include "dmrg/mp_tensors/contractions/boundary_ops/move_boundary.hpp"
#include "dmrg/mp_tensors/contractions/boundary_ops/prediction.hpp"

namespace contraction {

    template <class Matrix, class OtherMatrix, class SymmGroup, class SymmType = void>
    class Engine
    {
    public:
        typedef common::ScheduleNew<typename Matrix::value_type> schedule_t;

        static Boundary<OtherMatrix, SymmGroup>
        overlap_mpo_left_step(MPSTensor<Matrix, SymmGroup> const & bra_tensor,
                              MPSTensor<Matrix, SymmGroup> const & ket_tensor,
                              Boundary<OtherMatrix, SymmGroup> const & left,
                              MPOTensor<Matrix, SymmGroup> const & mpo,
                              bool symmetric = false)
        {
            return common::overlap_mpo_left_step(bra_tensor, ket_tensor, left, mpo, symmetric);
        }

        static Boundary<OtherMatrix, SymmGroup>
        overlap_mpo_right_step(MPSTensor<Matrix, SymmGroup> const & bra_tensor,
                               MPSTensor<Matrix, SymmGroup> const & ket_tensor,
                               Boundary<OtherMatrix, SymmGroup> const & right,
                               MPOTensor<Matrix, SymmGroup> const & mpo,
                               bool symmetric = true)
        {
            return common::overlap_mpo_right_step(bra_tensor, ket_tensor, right, mpo, symmetric);
        }

        static schedule_t
        contraction_schedule(MPSTensor<Matrix, SymmGroup> & mps,
                             Boundary<OtherMatrix, SymmGroup> const & left,
                             Boundary<OtherMatrix, SymmGroup> const & right,
                             MPOTensor<Matrix, SymmGroup> const & mpo)
        {
            return common::create_contraction_schedule(mps, left, right, mpo, 0);
        }

        static truncation_results
        grow_l2r_sweep(MPS<Matrix, SymmGroup> & mps,
                       MPOTensor<Matrix, SymmGroup> const & mpo,
                       Boundary<OtherMatrix, SymmGroup> const & left,
                       Boundary<OtherMatrix, SymmGroup> const & right,
                       std::size_t l, double alpha,
                       double cutoff, std::size_t Mmax)
        {
            MPSTensor<Matrix, SymmGroup> new_mps;
            truncation_results trunc;

            boost::tie(new_mps, trunc) =
            common::predict_new_state_l2r_sweep<Matrix, OtherMatrix, SymmGroup>
                   (mps[l], mpo, left, alpha, cutoff, Mmax);

            mps[l+1] = common::predict_lanczos_l2r_sweep<Matrix, OtherMatrix, SymmGroup>(mps[l+1], mps[l], new_mps);
            mps[l] = new_mps;
            return trunc;
        }

        static truncation_results
        grow_r2l_sweep(MPS<Matrix, SymmGroup> & mps,
                       MPOTensor<Matrix, SymmGroup> const & mpo,
                       Boundary<OtherMatrix, SymmGroup> const & left,
                       Boundary<OtherMatrix, SymmGroup> const & right,
                       std::size_t l, double alpha,
                       double cutoff, std::size_t Mmax)
        {
            MPSTensor<Matrix, SymmGroup> new_mps;
            truncation_results trunc;

            boost::tie(new_mps, trunc) =
            common::predict_new_state_r2l_sweep<Matrix, OtherMatrix, SymmGroup>
                   (mps[l], mpo, right, alpha, cutoff, Mmax);

            mps[l-1] = common::predict_lanczos_r2l_sweep<Matrix, OtherMatrix, SymmGroup>(mps[l-1], mps[l], new_mps);
            mps[l] = new_mps;
            return trunc;
        }

        static boost::tuple<MPSTensor<Matrix, SymmGroup>, MPSTensor<Matrix, SymmGroup>, truncation_results>
        predict_split_l2r(TwoSiteTensor<Matrix, SymmGroup> & tst,
                          std::size_t Mmax, double cutoff, double alpha,
                          Boundary<OtherMatrix, SymmGroup> const& left,
                          MPOTensor<Matrix, SymmGroup> const& mpo)
        {
            return common::predict_split_l2r(tst, Mmax, cutoff, alpha, left, mpo);
        }

        static boost::tuple<MPSTensor<Matrix, SymmGroup>, MPSTensor<Matrix, SymmGroup>, truncation_results>
        predict_split_r2l(TwoSiteTensor<Matrix, SymmGroup> & tst,
                          std::size_t Mmax, double cutoff, double alpha,
                          Boundary<OtherMatrix, SymmGroup> const& right,
                          MPOTensor<Matrix, SymmGroup> const& mpo)
        {
            return common::predict_split_r2l(tst, Mmax, cutoff, alpha, right, mpo);
        }

    };

} // namespace contraction

#endif
