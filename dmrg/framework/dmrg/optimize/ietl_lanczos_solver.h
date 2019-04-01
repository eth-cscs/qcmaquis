/*****************************************************************************
 *
 * ALPS MPS DMRG Project
 *
 * Copyright (C) 2014 Institute for Theoretical Physics, ETH Zurich
 *               2011-2011 by Bela Bauer <bauerb@phys.ethz.ch>
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

#ifndef IETL_LANCZOS_SOLVER_H
#define IETL_LANCZOS_SOLVER_H


#include "dmrg/utils/BaseParameters.h"
#include "dmrg/optimize/ietl_interface.h"

#include <ietl/lanczos.h>

template<class Matrix, class OtherMatrix, class SymmGroup>
std::pair<double, MPSTensor<Matrix, SymmGroup> >
solve_ietl_lanczos(SiteProblem<Matrix, OtherMatrix, SymmGroup> & sp,
                   MPSTensor<Matrix, SymmGroup> const & initial,
                   BaseParameters & params)
{
    typedef MPSTensor<Matrix, SymmGroup> Vector;
    
    SingleSiteVS<Matrix, SymmGroup> vs(initial, std::vector<MPSTensor<Matrix, SymmGroup> >());
    
    typedef ietl::vectorspace<Vector> Vecspace;
    typedef boost::lagged_fibonacci607 Gen;
    
    ietl::lanczos<SiteProblem<Matrix, OtherMatrix, SymmGroup>, SingleSiteVS<Matrix, SymmGroup> > lanczos(sp, vs);
    
    //            Vector test;
    //            ietl::mult(sp, mps[site], test);
    //            test.divide_by_scalar(test.scalar_norm());
    //            test -= mps[site];
    //            maquis::cout << "How close to eigenstate? " << test.scalar_norm() << std::endl;
    
    double rel_tol = sqrt(std::numeric_limits<double>::epsilon());
    double abs_tol = rel_tol;
    int n_evals = 1;
    ietl::lanczos_iteration_nlowest<double> 
    iter(100, n_evals, rel_tol, abs_tol);
    
    std::vector<double> eigen, err;
    std::vector<int> multiplicity;  
    
    try{
        lanczos.calculate_eigenvalues(iter, initial);
        eigen = lanczos.eigenvalues();
        err = lanczos.errors();
        multiplicity = lanczos.multiplicities();
        maquis::cout << "IETL used " << iter.iterations() << " iterations." << std::endl;
    }
    catch (std::runtime_error& e) {
        maquis::cout << "Error in eigenvalue calculation: " << std::endl;
        maquis::cout << e.what() << std::endl;
        exit(1);
    }
    
//    maquis::cout << "Energies: ";
//    std::copy(eigen.begin(), eigen.begin()+n_evals, std::ostream_iterator<double>(maquis::cout, " "));
//    maquis::cout << std::endl;
    //            maquis::cout << "Energy: " << eigen[0] << std::endl;
    
    std::vector<double>::iterator start = eigen.begin();  
    std::vector<double>::iterator end = eigen.begin()+1;
    std::vector<Vector> eigenvectors; // for storing the eigenvectors. 
    ietl::Info<double> info; // (m1, m2, ma, eigenvalue, residualm, status).
    
    try {
        lanczos.eigenvectors(start, end, std::back_inserter(eigenvectors), info, initial, 100);
    }
    catch (std::runtime_error& e) {
        maquis::cout << "Error in eigenvector calculation: " << std::endl;
        maquis::cout << e.what() << std::endl;
        exit(1);
    }
    
    //            for(int i = 0; i < info.size(); i++)
    //                maquis::cout << " m1(" << i+1 << "): " << info.m1(i) << ", m2(" << i+1 << "): "
    //                << info.m2(i) << ", ma(" << i+1 << "): " << info.ma(i) << " eigenvalue("
    //                << i+1 << "): " << info.eigenvalue(i) << " residual(" << i+1 << "): "
    //                << info.residual(i) << " error_info(" << i+1 << "): "
    //                << info.error_info(i) << "\n\n";
    
    assert( eigenvectors[0].scalar_norm() > 1e-8 );
    assert( info.error_info(0) == 0 );
    
    return std::make_pair(*eigen.begin(), eigenvectors[0]);
}

#endif
