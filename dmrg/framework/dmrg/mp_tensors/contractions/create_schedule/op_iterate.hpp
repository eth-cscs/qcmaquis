/*****************************************************************************
 *
 * ALPS MPS DMRG Project
 *
 * Copyright (C) 2016 Institute for Theoretical Physics, ETH Zurich
 *                    Laboratory for Physical Chemistry, ETH Zurich
 *               2016-2016 by Sebastian Keller <sebkelle@phys.ethz.ch>
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

#ifndef CONTRACTIONS_OP_ITERATE_HPP
#define CONTRACTIONS_OP_ITERATE_HPP

#include "dmrg/block_matrix/block_matrix.h"

namespace contraction {
namespace common {
namespace detail {

    template <class Matrix, class SymmGroup>
    void op_iterate(typename operator_selector<Matrix, SymmGroup>::type const & W, std::size_t w_block,
                    typename Matrix::value_type couplings[],
                    Cohort<typename Matrix::value_type> & cg,
                    unsigned s,
                    unsigned m2_size,
                    typename common::ScheduleNew<typename Matrix::value_type>::block_type const & mpsb,
                    unsigned mps_offset,
                    unsigned ci,
                    unsigned boundary_col)
    {
        typedef typename SparseOperator<Matrix, SymmGroup>::const_iterator block_iterator;

        std::pair<block_iterator, block_iterator> blocks = W.get_sparse().block(w_block);

        for (block_iterator it = blocks.first; it != blocks.second; ++it)
        {
            unsigned ss1 = it->get_row();
            unsigned ss2 = it->get_col();
            unsigned rspin = it->get_row_spin();
            unsigned cspin = it->get_col_spin();
            unsigned casenr = 0;
            if (rspin == 2 && cspin == 2) casenr = 3;
            else if (rspin == 2) casenr = 1;
            else if (cspin == 2) casenr = 2;

            typename Matrix::value_type scale = it->coefficient * couplings[casenr];
            unsigned ti = mpsb.get_ti(mps_offset + ss1*m2_size, ci);

            if (ti < std::numeric_limits<unsigned>::max())
                cg.push_back(s, ss2, scale, ti, boundary_col);
        }
    }

} // namespace detail
} // namespace common
} // namespace contraction

#endif
