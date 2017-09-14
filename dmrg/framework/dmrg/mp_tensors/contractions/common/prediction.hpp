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

#ifndef ENGINE_COMMON_PREDICTION_H
#define ENGINE_COMMON_PREDICTION_H

#include "dmrg/mp_tensors/mpstensor.h"
#include "dmrg/mp_tensors/mpotensor.h"
#include "dmrg/mp_tensors/reshapes.h"
#include "dmrg/block_matrix/indexing.h"

namespace contraction {
    namespace common {

        template<class Matrix, class OtherMatrix, class SymmGroup, class LBTM>
        static std::pair<MPSTensor<Matrix, SymmGroup>, truncation_results>
        predict_new_state_l2r_sweep(MPSTensor<Matrix, SymmGroup> const & mps,
                                    MPOTensor<Matrix, SymmGroup> const & mpo,
                                    Boundary<OtherMatrix, SymmGroup> const & left,
                                    Boundary<OtherMatrix, SymmGroup> const & right,
                                    LBTM lbtm,
                                    double alpha, double cutoff, std::size_t Mmax)
        {
            typedef typename SymmGroup::charge charge;
            mps.make_left_paired();
            block_matrix<Matrix, SymmGroup> dm;
            gemm(mps.data(), transpose(conjugate(mps.data())), dm);
            
            Boundary<Matrix, SymmGroup> half_dm = lbtm(mps, left, mpo, NULL);

            omp_for(unsigned lb, parallel::range<unsigned>(0,mps.data().basis().size()),
            {
                charge lc = mps.data().basis().left_charge(lb);
                size_t ls = mps.data().basis().left_size(lb);
                Matrix tdm(ls, ls);

                for (auto lcci : half_dm.index(lc))
                {
                    unsigned ci = lcci.second;
                    assert (half_dm.data()[ci].size() % ls == 0);

                    typename Matrix::value_type one(1);
                    typename Matrix::value_type alpha_v(alpha);
                    int M = ls, N = ls, K = half_dm.data()[ci].size() / ls;
                    blas_gemm('N', 'T', M, N, K, alpha_v, &half_dm.data()[ci][0], M, &half_dm.data()[ci][0], N, one, &tdm(0,0), M);
                }

                dm[lb] += tdm;
            });
            
            assert( weak_equal(dm.left_basis(), mps.data().left_basis()) );
            
            block_matrix<Matrix, SymmGroup> U;
            block_matrix<typename alps::numeric::associated_real_diagonal_matrix<Matrix>::type, SymmGroup> S;
            truncation_results trunc = heev_truncate(dm, U, S, cutoff, Mmax);
          
            MPSTensor<Matrix, SymmGroup> ret = mps;
            ret.replace_left_paired(U);
            return std::make_pair(ret, trunc);
        }
        
        template<class Matrix, class OtherMatrix, class SymmGroup>
        static MPSTensor<Matrix, SymmGroup>
        predict_lanczos_l2r_sweep(MPSTensor<Matrix, SymmGroup> B,
                                  MPSTensor<Matrix, SymmGroup> const & psi,
                                  MPSTensor<Matrix, SymmGroup> const & A)
        {
            psi.make_left_paired();
            A.make_left_paired();
            
            block_matrix<Matrix, SymmGroup> tmp;
            gemm(transpose(conjugate(A.data())), psi.data(), tmp);
            B.multiply_from_left(tmp);
            
            return B;
        }
        
        template<class Matrix, class OtherMatrix, class SymmGroup, class RBTM>
        static std::pair<MPSTensor<Matrix, SymmGroup>, truncation_results>
        predict_new_state_r2l_sweep(MPSTensor<Matrix, SymmGroup> const & mps,
                                        MPOTensor<Matrix, SymmGroup> const & mpo,
                                        Boundary<OtherMatrix, SymmGroup> const & left,
                                        Boundary<OtherMatrix, SymmGroup> const & right,
                                        RBTM rbtm,
                                        double alpha, double cutoff, std::size_t Mmax)
        {
            typedef typename SymmGroup::charge charge;
            mps.make_right_paired();
            block_matrix<Matrix, SymmGroup> dm;
            gemm(transpose(conjugate(mps.data())), mps.data(), dm);

            Boundary<Matrix, SymmGroup> half_dm = rbtm(mps, right, mpo, NULL);
            
            omp_for(unsigned lb, parallel::range<unsigned>(0,mps.data().basis().size()),
            {
                charge lc = mps.data().basis().left_charge(lb);

                for (auto rcci : half_dm.index(lc))
                {
                    unsigned rb = mps.row_dim().position(rcci.first);
                    unsigned ci = rcci.second;
                    unsigned ls = half_dm.index.left_size(ci);
                    unsigned rs = half_dm.index.right_size(ci);

                    Matrix tdm(rs, rs);
                    assert (half_dm.data()[ci].size() % rs == 0);

                    typename Matrix::value_type one(1);
                    typename Matrix::value_type alpha_v(alpha);
                    int M = rs, N = rs, K = half_dm.data()[ci].size() / rs;
                    blas_gemm('T', 'N', M, N, K, alpha_v, &half_dm.data()[ci][0], K, &half_dm.data()[ci][0], K, one, &tdm(0,0), M);
                    //for (std::size_t b = 0; b < half_dm.aux_dim(); ++b)
                    //{
                    //    long int offset = half_dm.index.offset(ci, b);
                    //    if (offset == -1) continue;

                    //    int M = rs, N = rs, K = ls;
                    //    blas_gemm('T', 'N', M, N, K, alpha_v, &half_dm.data()[ci][offset], K, &half_dm.data()[ci][offset], K,
                    //              one, &tdm(0,0), M);
                    //}

                    parallel_critical
                    dm[rb] += tdm;
                }
            });
            
            assert( weak_equal(dm.right_basis(), mps.data().right_basis()) );
            
            block_matrix<Matrix, SymmGroup> U;
            block_matrix<typename alps::numeric::associated_real_diagonal_matrix<Matrix>::type, SymmGroup> S;
            truncation_results trunc = heev_truncate(dm, U, S, cutoff, Mmax);
            
            MPSTensor<Matrix, SymmGroup> ret = mps;
            ret.replace_right_paired(adjoint(U));
            return std::make_pair(ret, trunc);
        }
        
        template<class Matrix, class OtherMatrix, class SymmGroup>
        static MPSTensor<Matrix, SymmGroup>
        predict_lanczos_r2l_sweep(MPSTensor<Matrix, SymmGroup> B,
                                  MPSTensor<Matrix, SymmGroup> const & psi,
                                  MPSTensor<Matrix, SymmGroup> const & A)
        {
            psi.make_right_paired();
            A.make_right_paired();
            
            block_matrix<Matrix, SymmGroup> tmp;
            gemm(psi.data(), transpose(conjugate(A.data())), tmp);
            
            B.multiply_from_right(tmp);
            
            return B;
        }
    } // namespace common
} // namespace contraction

#endif
