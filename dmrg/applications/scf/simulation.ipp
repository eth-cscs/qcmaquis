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

#include "dmrg/sim/matrix_types.h"

#include "../dmrg/dmrg_sim.h"
#include "../measure/measure_sim.h"
#include "simulation.h"

template <class SymmGroup>
void simulation<SymmGroup>::run(DmrgParameters & parms)
{
    if (parms["COMPLEX"]) {
#ifdef HAVE_COMPLEX
        dmrg_sim<cmatrix, SymmGroup> sim(parms);
        sim.run();
#else
        throw std::runtime_error("compilation of complex numbers not enabled, check your compile options\n");
#endif
    } else {
        dmrg_sim<matrix, SymmGroup> sim(parms);
        sim.run();
    }
}

template <class SymmGroup>
void simulation<SymmGroup>::measure_observable(DmrgParameters & parms, std::string name,
                                               std::vector<double> & results,
                                               std::vector<std::vector<Lattice::pos_t> > & labels)
{
    if (parms["COMPLEX"]) {
        throw std::runtime_error("extraction of complex observables not implemented\n");
    } else {

        dmrg_sim<matrix, SymmGroup> dsim(parms);
        dsim.run();

        measure_sim<matrix, SymmGroup> msim(parms);
        msim.measure_observable(name, results, labels);
    }
}
