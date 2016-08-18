/*****************************************************************************
 *
 * ALPS MPS DMRG Project
 *
 * Copyright (C) 2014 Institute for Theoretical Physics, ETH Zurich
 *               2011-2013 by Michele Dolfi <dolfim@phys.ethz.ch>
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

#ifdef USE_AMBIENT
#include <mpi.h>
#endif
#include <cmath>
#include <iterator>
#include <iostream>
#include <sys/time.h>
#include <sys/stat.h>

using std::cerr;
using std::cout;
using std::endl;

#ifdef USE_AMBIENT
    #include "dmrg/block_matrix/detail/ambient.hpp"
    typedef ambient::tiles<ambient::matrix<double> > Matrix;
#else
    #include "dmrg/block_matrix/detail/alps.hpp"
    typedef alps::numeric::matrix<double> Matrix;
#endif

#include "dmrg/block_matrix/indexing.h"
#include "dmrg/mp_tensors/mps.h"
#include "dmrg/mp_tensors/mpo.h"
#include "dmrg/mp_tensors/contractions.h"
#include "dmrg/mp_tensors/mps_mpo_ops.h"
#include "dmrg/mp_tensors/mpo_ops.h"
#include "dmrg/mp_tensors/mps_initializers.h"

#include "dmrg/mp_tensors/mpo_times_mps.hpp"

#include "dmrg/models/generate_mpo.hpp"
#include "dmrg/models/chem/util.h"

#if defined(USE_TWOU1)
typedef TwoU1 grp;
#elif defined(USE_TWOU1PG)
typedef TwoU1PG grp;
#elif defined(USE_SU2U1)
typedef SU2U1 grp;
#elif defined(USE_SU2U1PG)
typedef SU2U1PG grp;
#elif defined(USE_NONE)
typedef TrivialGroup grp;
#elif defined(USE_U1)
typedef U1 grp;
#elif defined(USE_U1DG)
typedef U1DG grp;
#endif

int main(int argc, char ** argv)
{
    try {
        //if (argc != 2) {
        //    std::cout << "Usage: " << argv[0] << " <mps.h5>" << std::endl;
        //    return 1;
        //}

        int L = 4;
        int Nup = 2;
        int Ndown = 2;
        grp::subcharge irrep = 0;

        BaseParameters parms = chem_detail::set_2u1_parameters(L, Nup, Ndown);
        parms.set("init_bond_dimension", 1000);
        parms.set("site_types", "0,0,0,0");

        default_mps_init<Matrix, grp> mpsinit(parms,
                                            chem_detail::make_2u1_site_basis<Matrix, grp>(L, Nup, Ndown, parms["site_types"]),
                                            chem_detail::make_2u1_initc<grp>(Nup, Ndown, irrep), parms["site_types"]);
        MPS<Matrix, grp> mps(L, mpsinit);
        save("mps.h5", mps);
 
        Lattice lat(parms);
        Model<Matrix, grp> model(lat, parms);

        typedef Lattice::pos_t pos_t;
        typedef OPTable<Matrix, grp>::tag_type tag_type;

        std::vector<pos_t> positions;
        std::vector<tag_type> operators;
        std::vector<tag_type> ident, fill;

        positions.push_back(1);
        positions.push_back(2);
        operators.push_back(model.get_operator_tag("create_up", 0));
        operators.push_back(model.get_operator_tag("destroy_up", 0));
        ident.push_back(model.identity_matrix_tag(0));
        fill.push_back(model.filling_matrix_tag(0));
        
        MPO<Matrix, grp> mpo = generate_mpo::make_1D_mpo(positions, operators, ident, fill, model.operators_table(), lat);  

        //for (int p = 0; p < lat.size(); ++p) {
        //    for (int b1 = 0; b1 < mpo[p].row_dim(); ++b1) {
        //        for (int b2 = 0; b2 < mpo[p].col_dim(); ++b2) {
        //            if (mpo[p].has(b1, b2)) maquis::cout << mpo[p].tag_number(b1,b2) << " ";
        //            else maquis::cout << ". ";
        //        }
        //        maquis::cout << std::endl;
        //    }
        //}

        grp::charge delta = grp::IdentityCharge;
        MPS<Matrix, grp> pmps(lat.size());
        for (int p = 0; p < lat.size(); ++p) {
            maquis::cout << "\n****************** site " << p << " ****************\n";
            pmps[p] =  mpo_times_mps(mpo[p], mps[p], delta);
        }
        save("pmps.h5", pmps); 
         
        
    } catch (std::exception& e) {
        std::cerr << "Error:" << std::endl << e.what() << std::endl;
        return 1;
    }
}
