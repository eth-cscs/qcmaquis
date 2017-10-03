/*****************************************************************************
 *
 * ALPS MPS DMRG Project
 *
 * Copyright (C) 2014 Institute for Theoretical Physics, ETH Zurich
 *               2011-2013 by Michele Dolfi <dolfim@phys.ethz.ch>
 *                            Bela Bauer <bauerb@comp-phys.org>
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
#include <iostream>
#include <sstream>
#include <fstream>
#include <boost/shared_ptr.hpp>
#include <alps/hdf5.hpp>

#include "matrix_selector.hpp" /// define matrix
#include "symm_selector.hpp"   /// define grp

#include "dmrg/models/lattice.h"
#include "dmrg/models/model.h"
#include "dmrg/models/generate_mpo.hpp"

#include "dmrg/mp_tensors/mps.h"
#include "dmrg/mp_tensors/twositetensor.h"
#include "dmrg/mp_tensors/contractions.h"

#include "dmrg/optimize/ietl_lanczos_solver.h"
#include "dmrg/optimize/ietl_jacobi_davidson.h"
#include "dmrg/optimize/site_problem.h"

#include "dmrg/utils/DmrgOptions.h"
#include "dmrg/utils/DmrgParameters.h"
#include "dmrg/utils/parallel/placement.hpp"

#include "utils/timings.h"


bool can_clean(int k, int site, int L, int lr){
    if(k == site || k == site+1) return false;
    if(lr == -1 && site > 0   && k == site-1) return false;
    if(lr == +1 && site < L-2 && k == site+2) return false;
    return true;
}

int main(int argc, char ** argv)
{
    try {
        DmrgOptions opt(argc, argv);
        if (!opt.valid) return 0;
        DmrgParameters parms = opt.parms;

        maquis::cout.precision(10);
        
        /// Timers
        Timer
        time_model      ("Parsing model"),  time_load       ("Load MPS"),
        time_l_boundary ("Left boundary"),  time_r_boundary ("Right boundary"),
        time_optim_jcd  ("Optim. JCD"   ),  time_truncation ("Truncation"),
        time_ts_obj     ("Two site obj.");
        
        /// Parsing model
        time_model.begin();
        Lattice lattice(parms);
        Model<matrix, grp> model(lattice, parms);
        
        MPO<matrix, grp> mpo = make_mpo(lattice, model);
        time_model.end();
        maquis::cout << "Parsing model done!\n";

        boost::filesystem::path chkpfile(parms["chkpfile"].str());
        
        /// Initialize & load MPS
        time_load.begin();
        int L = lattice.size();
        MPS<matrix, grp> mps;
        load(parms["chkpfile"].str(), mps);
        int _site;
        {
            alps::hdf5::archive ar(chkpfile / "props.h5");
            ar["/status/site"] >> _site;
        }
        if(_site == -1) _site = L/2;
        int site, lr;
        if (_site < L) {
            site = _site;
            lr = 1;
        } else {
            site = 2*L-_site-1;
            lr = -1;
        }
        time_load.end();
        maquis::cout << "Load MPS done!\n";
        maquis::cout << "Optimization at site " << site << " in " << lr << " direction." << std::endl;
        
        /// Canonize MPS
        mps.canonize(site);
        

        std::string boundary_name;
        parallel::construct_placements(mpo);
        
        /// Create TwoSite objects
        time_ts_obj.begin();
        TwoSiteTensor<matrix, grp> tst(mps[site], mps[site+1]);
        MPSTensor<matrix, grp> twin_mps = tst.make_mps();
        tst.clear();
        MPOTensor<matrix, grp> ts_mpo = make_twosite_mpo<matrix,matrix>(mpo[site], mpo[site+1], mps[site].site_dim(), mps[site+1].site_dim());
        if(lr == +1){
            ts_mpo.placement_l = mpo[site].placement_l;
            ts_mpo.placement_r = parallel::get_right_placement(ts_mpo, mpo[site].placement_l, mpo[site+1].placement_r);
        }else{
            ts_mpo.placement_l = parallel::get_left_placement(ts_mpo, mpo[site].placement_l, mpo[site+1].placement_r);
            ts_mpo.placement_r = mpo[site+1].placement_r;
        }
        time_ts_obj.end();
        maquis::cout << "Two site obj done!\n";
        
        /// Compute left boundary
        time_l_boundary.begin();
        Boundary<matrix, grp> left;
        boundary_name = "left" + boost::lexical_cast<std::string>(site) + ".h5";
        #ifndef USE_AMBIENT
        if ( exists(chkpfile / boundary_name) ) {
            maquis::cout << "Loading existing left boundary." << std::endl;
            storage::archive ar(chkpfile.string() +"/"+ boundary_name);
            ar["/tensor"] >> left;
        } else 
        #endif
        {
            left = mps.left_boundary(); parallel::sync();
            for (size_t i = 0; i < site; ++i){
                left = contraction::Engine<matrix, matrix, grp>::overlap_mpo_left_step(mps[i], mps[i], left, mpo[i]);
                if(can_clean(i, site, L, lr)) mps[i].data().clear();
                parallel::sync();
            }
        }
        time_l_boundary.end();
        maquis::cout << "Left boundary done!\n";
        
        /// Compute right boundary
        time_r_boundary.begin();
        Boundary<matrix, grp> right;
        boundary_name = "right" + boost::lexical_cast<std::string>(site+2) + ".h5";
        #ifndef USE_AMBIENT
        if ( exists(chkpfile / boundary_name) ) {
            maquis::cout << "Loading existing right boundary." << std::endl;
            storage::archive ar(chkpfile.string() +"/"+ boundary_name);
            ar["/tensor"] >> right;
        } else 
        #endif
        {
            right = mps.right_boundary(); parallel::sync();
            for (int i = L-1; i > site+1; --i){
                right = contraction::Engine<matrix, matrix, grp>::overlap_mpo_right_step(mps[i], mps[i], right, mpo[i]);
                if(can_clean(i, site, L, lr)) mps[i].data().clear();
                parallel::sync();
            }
        }
        time_r_boundary.end();
        maquis::cout << "Right boundary done!\n";
        
        // Clearing unneeded MPS Tensors
        for (int k = 0; k < mps.length(); k++){
            if(can_clean(k, site, L, lr)) mps[k].data().clear();
        }
        
        std::vector<MPSTensor<matrix, grp> > ortho_vecs;
        std::pair<double, MPSTensor<matrix, grp> > res;
        SiteProblem<matrix, matrix, grp> sp(twin_mps, left, right, ts_mpo);

        /// Optimization: JCD
        time_optim_jcd.begin();
        res = solve_ietl_jcd(sp, twin_mps, parms, ortho_vecs);
        tst << res.second;
        res.second.clear();
        twin_mps.clear();
        maquis::cout.precision(10);
        maquis::cout << "Energy " << lr << " " << res.first << std::endl;
        time_optim_jcd.end();
        maquis::cout << "Optim. JCD done!\n";
        
        double alpha = parms["alpha_main"];
        double cutoff = parms["truncation_final"];
        std::size_t Mmax = parms["max_bond_dimension"];
        
        /// Truncation of MPS
        time_truncation.begin();
        truncation_results trunc;
        if (lr == +1)
        {
            if(parms["twosite_truncation"] == "svd")
                boost::tie(mps[site], mps[site+1], trunc) = tst.split_mps_l2r(Mmax, cutoff);
            else
                boost::tie(mps[site], mps[site+1], trunc) = contraction::Engine<matrix, matrix, grp>::
                    predict_split_l2r(tst, Mmax, cutoff, alpha, left, mpo[site]);
            tst.clear();
            
            block_matrix<matrix, grp> t;
            t = mps[site+1].normalize_left(DefaultSolver());
            if (site+1 < L-1) mps[site+2].multiply_from_left(t);
        }
        if (lr == -1){
            if(parms["twosite_truncation"] == "svd")
                boost::tie(mps[site], mps[site+1], trunc) = tst.split_mps_r2l(Mmax, cutoff);
            else
                boost::tie(mps[site], mps[site+1], trunc) = contraction::Engine<matrix, matrix, grp>::
                    predict_split_r2l(tst, Mmax, cutoff, alpha, right, mpo[site+1]);
            tst.clear();
            
            block_matrix<matrix, grp> t;
            t = mps[site].normalize_right(DefaultSolver());
            if (site > 0) mps[site-1].multiply_from_right(t);
        }
        time_truncation.end();
        maquis::cout << "Truncation done!\n";

        parallel::meminfo();
        
    } catch (std::exception & e) {
        maquis::cerr << "Exception caught:" << std::endl << e.what() << std::endl;
        exit(1);
    }
}

