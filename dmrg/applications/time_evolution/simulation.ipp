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

#include "tevol_sim.h"
#include "dmrg/evolve/tevol_nn_sim.h"
#include "dmrg/evolve/tevol_mpo_sim.h"
#include "dmrg/evolve/tevol_circuit_sim.h"
#include "simulation.h"

#include <boost/scoped_ptr.hpp>

template <class SymmGroup>
void simulation<SymmGroup>::run(DmrgParameters & parms)
{
    boost::scoped_ptr<abstract_sim> sim;
    if (parms["COMPLEX"]) {
#ifdef HAVE_COMPLEX
        if (parms["te_type"] == "nn")
            sim.reset(new tevol_sim<cmatrix, SymmGroup, nearest_neighbors_evolver<cmatrix, SymmGroup> >(parms));
        else if (parms["te_type"] == "mpo")
            sim.reset(new tevol_sim<cmatrix, SymmGroup, mpo_evolver<cmatrix, SymmGroup> >(parms));
        else if (parms["te_type"] == "circuit")
            sim.reset(new tevol_sim<cmatrix, SymmGroup, circuit_evolver<cmatrix, SymmGroup> >(parms));
#else
        throw std::runtime_error("compilation of complex numbers not enabled, check your compile options\n");
#endif
    } else {
        if (parms["te_type"] == "nn")
            sim.reset(new tevol_sim<matrix, SymmGroup, nearest_neighbors_evolver<matrix, SymmGroup> >(parms));
        else if (parms["te_type"] == "mpo")
            sim.reset(new tevol_sim<matrix, SymmGroup, mpo_evolver<matrix, SymmGroup> >(parms));
        else if (parms["te_type"] == "circuit")
            sim.reset(new tevol_sim<matrix, SymmGroup, circuit_evolver<matrix, SymmGroup> >(parms));
    }
    
    /// Run
    sim->run();
}
