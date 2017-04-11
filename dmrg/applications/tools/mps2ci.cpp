/*****************************************************************************
 *
 * ALPS MPS DMRG Project
 *
 * Copyright (C) 2014 Institute for Theoretical Physics, ETH Zurich
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

#include <cmath>
#include <iterator>
#include <iostream>
#include <string>
#include <sys/time.h>
#include <sys/stat.h>

#include <vector>
#include <boost/lexical_cast.hpp>
#include <boost/tokenizer.hpp>
#include <boost/tuple/tuple.hpp>

using std::cerr;
using std::cout;
using std::endl;

#include "dmrg/sim/matrix_types.h"
#include "dmrg/utils/DmrgParameters.h"
#include "dmrg/block_matrix/indexing.h"
#include "dmrg/mp_tensors/mps.h"
#include "dmrg/models/chem/util.h"

#include "ci_encode.hpp"
#include "sampling.hpp"

typedef TwoU1PG grp; 

/*
    Small function to read determinants from a input file
*/
template<class Matrix, class SymmGroup>
std::vector<std::vector<typename SymmGroup::charge> >
parse_config(std::string file, std::vector<Index<SymmGroup> > const & site_dims)
{
    std::ifstream config_file;
    config_file.open(file.c_str());

    std::vector<std::vector<typename SymmGroup::charge> > configs;

    for (std::string line; std::getline(config_file, line); ) {
        std::vector<std::string> det_coeff;
        boost::split(det_coeff, line, boost::is_any_of(" "));
        
        std::string det = det_coeff[0];

        if (det.size() != site_dims.size())
            throw std::runtime_error("The determinant length doesn't match the mps length\n");

        std::vector<typename SymmGroup::charge> tmp;
        for (std::size_t i = 0; i < det.size(); ++i) {
            int occ = boost::lexical_cast<int>(det[i]); 
            switch(occ) {
                case 4:
                    tmp.push_back(site_dims[i][0].first); // doubly occ
                    break;
                case 3:
                    tmp.push_back(site_dims[i][1].first); // up
                    break;
                case 2:
                    tmp.push_back(site_dims[i][2].first); // down 
                    break;
                case 1:
                    tmp.push_back(site_dims[i][3].first); // empty
                    break;
            }
        }
        configs.push_back(tmp);
    }
    return configs;
}

int main(int argc, char ** argv)
{
    if (argc != 3)
    {
        maquis::cout << "Usage: mps2ci <mps.h5> <determinants_file> " << std::endl;
        maquis::cout << "See J. Chem. Phys. 126, 244109(2007)" << std::endl;
        exit(1);
    }
    
    maquis::cout.precision(10);

    std::ifstream param_file(argv[1]);
    if (!param_file) {
        maquis::cerr << "Could not open the mps." << std::endl;
        exit(1);
    }
    
    typedef int pos_t;

    // load the MPS
    MPS<matrix, grp> mps;
    load(argv[1], mps);

    pos_t L = mps.length();
    grp::subcharge Nup = mps[L-1].col_dim()[0].first[0];
    grp::subcharge Ndown = mps[L-1].col_dim()[0].first[1];

    BaseParameters parms;
    parms.set("site_types", chem::infer_site_types(mps));
    
    // extract physical basis for every site from MPS
    std::vector<grp::subcharge> irreps = parms["site_types"];
    std::vector<Index<grp> > per_site, phys_dims = chem::make_2u1_site_basis<matrix, grp>(L, Nup, Ndown, parms["site_types"]);
    for (pos_t q = 0; q < L; ++q)
        per_site.push_back(phys_dims[irreps[q]]);
    
    // load the determinants
    std::vector<std::vector<grp::charge> > determinants = parse_config<matrix, grp>(std::string(argv[2]), per_site);
    // printout the determinants
    for (pos_t q = 0;q < determinants.size(); ++q){
       for (pos_t p = 0; p < L; ++p){
           std::cout << determinants[q][p];
       }   
       std::cout << std::endl;
    }

    // compute the CI coefficients for all determinants in the input
    int i = 1;
    for (std::vector< std::vector<grp::charge> >::iterator it = determinants.begin();
        it != determinants.end(); ++it){
        maquis::cout << "CI coefficient of det " << i <<": " << extract_coefficient(mps, *it) << std::endl;
        i++;
        }

    maquis::cout << std::endl;

    return 0;
}

