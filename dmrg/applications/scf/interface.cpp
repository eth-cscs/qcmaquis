/*****************************************************************************
 *
 * ALPS MPS DMRG Project
 *
 * Copyright (C) 2017 Stanford University Departement of Chemistry
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

#include <iostream>
#include <sys/time.h>
#include <sys/stat.h>

#include "interface.h"

#include "utils/io.hpp"
#include "dmrg/sim/symmetry_factory.h"

DmrgInterface::DmrgInterface(DmrgOptions & opt_)
    : opt(opt_)
    , sim(dmrg::symmetry_factory<simulation_traits>(opt.parms))
{ }

void DmrgInterface::optimize()
{
    if (opt.valid) {
        
        maquis::cout.precision(10);
        
        timeval now, then, snow, sthen;
        gettimeofday(&now, NULL);
        
        try {
            sim->run(opt.parms);

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

void DmrgInterface::measure(std::string name,
                            std::vector<double> & results,
                            std::vector<std::vector<int> > & labels)
{
    sim->measure_observable(opt.parms, name, results, labels);
}
