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

#include "utils/io.hpp" // has to be first include because of impi
#include <iostream>
#include <sys/time.h>
#include <sys/stat.h>

#include "dmrg/utils/DmrgOptions.h"

#include "../../applications/dmrg/simulation.h"
#include "dmrg/sim/symmetry_factory.h"

int main(int argc, char ** argv)
{
    std::cout << "  QCMaquis - Quantum Chemical Density Matrix Renormalization group\n"
              << "  available from http://www.reiher.ethz.ch/software\n"
              << "  based on the ALPS MPS codes from http://alps.comp-phys.org/\n"
              << "  copyright (c) 2015 Laboratory of Physical Chemistry, ETH Zurich\n"
              << "  copyright (c) 2012-2015 by Sebastian Keller\n"
              << "  for details see the publication: \n"
              << "  S. Keller et al, arXiv:1510.02026\n"
              << std::endl;

    DmrgOptions opt(argc, argv);
    if (opt.valid) {
        
        maquis::cout.precision(10);
        
        timeval now, then, snow, sthen;
        gettimeofday(&now, NULL);
        
        
        try {
            simulation_traits::shared_ptr sim = dmrg::symmetry_factory<simulation_traits>(opt.parms);
            sim->measure_all();
        } catch (std::exception & e) {
            maquis::cerr << "Exception thrown!" << std::endl;
            maquis::cerr << e.what() << std::endl;
            exit(1);
        }
        
        gettimeofday(&then, NULL);
        double elapsed = then.tv_sec-now.tv_sec + 1e-6 * (then.tv_usec-now.tv_usec);
        
        maquis::cout << "Task took " << elapsed << " seconds." << std::endl;
    }
}
