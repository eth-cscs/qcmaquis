/*****************************************************************************
 *
 * ALPS MPS DMRG Project
 *
 * Copyright (C) 2014 Institute for Theoretical Physics, ETH Zurich
 *               2013-2013 by Michele Dolfi <dolfim@phys.ethz.ch>
 *
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

/// Some parts adapted from coded/super_models_none.hpp

#ifndef DMRG_CONTINUUM_SUPER_MODELS_U1_H
#define DMRG_CONTINUUM_SUPER_MODELS_U1_H

#include <sstream>

#include <cmath>

#include "dmrg/models/model.h"
#include "dmrg/models/measurements.h"
#include "dmrg/utils/BaseParameters.h"
#include "dmrg/mp_tensors/dm_op_kron.h"

/* ****************** OPTICAL LATTICE (with symmetry) */
template<class Matrix>
class DMOpticalLattice : public Model<Matrix, U1> {
    typedef Hamiltonian<Matrix, U1> ham;
    typedef typename ham::hamterm_t hamterm_t;
    typedef Measurement_Term<Matrix, U1> mterm_t;
    typedef typename ham::op_t op_t;
    typedef typename Matrix::value_type value_type;
public:
    DMOpticalLattice (const Lattice& lat_, BaseParameters & model_)
    : lat(lat_)
    , model(model_)
    {
        psi_phys.insert(std::make_pair(0, 1));
        psi_ident.insert_block(Matrix(1, 1, 1), 0, 0);
        
        int Nmax = model["Nmax"];
        for (int n=1; n<=Nmax; ++n)
        {
            psi_phys.insert(std::make_pair(n, 1));
            
            psi_ident.insert_block(Matrix(1, 1, 1), n, n);
            
            psi_count.insert_block(Matrix(1, 1, n), n, n);
            if ((n*n-n) != 0)
                psi_interaction.insert_block(Matrix(1, 1, n*n-n), n, n);
            
            
            psi_create.insert_block(Matrix(1, 1, std::sqrt(value_type(n))), n-1, n);
            psi_destroy.insert_block(Matrix(1, 1, std::sqrt(value_type(n))), n, n-1);
        }
        
        phys        = psi_phys * adjoin(psi_phys);
        ident       = identity_matrix<Matrix>(phys);
        count       = adjoint_site_term(psi_count);
        interaction = adjoint_site_term(psi_interaction);
        
        maquis::cout << "phys: " << phys << std::endl;
        std::vector< std::pair<op_t,op_t> > hopops = adjoint_bond_term_new(psi_create, psi_destroy);
        
        for (int p=0; p<lat.size(); ++p)
        {
            std::vector<int> neighs = lat.all(p);
            
            double exp_potential = model["V0"]*std::pow( std::cos(model["k"]*lat.get_prop<double>("x", p)), 2 );
            
            double dx1 = lat.get_prop<double>("dx", p, neighs[0]);
            double dx2;
            if (neighs.size() == 1 && lat.get_prop<bool>("at_open_left_boundary", p))
                dx2 = lat.get_prop<double>("dx", p, p-1);
            else if (neighs.size() == 1 && lat.get_prop<bool>("at_open_right_boundary", p))
                dx2 = lat.get_prop<double>("dx", p, p+1);
            else
                dx2 = lat.get_prop<double>("dx", p, neighs[1]);
            
            double dx0 = lat.get_prop<double>("dx", p);
            
            // Psi''(x) = coeff1 * Psi(x+dx1) + coeff0 * Psi(x) + coeff2 * Psi(x+dx2)
            double coeff1 = 2. / (dx1*dx1 - dx1*dx2);
            double coeff2 = 2. / (dx2*dx2 - dx1*dx2);
            double coeff0 = -(coeff1 + coeff2);
            
            double U = model["c"] / dx0;
            double mu = exp_potential - model["mu"];
            mu += -coeff0 * model["h"];
            
#ifndef NDEBUG
            maquis::cout << "U = " << U << ", mu = " << mu << ", t = " << coeff1 * model["h"] << std::endl;
#endif
            
            if (U != 0.)
            { // U * n * (n-1)
                hamterm_t term;
                term.with_sign = false;
                term.fill_operator = ident;
                term.operators.push_back( std::make_pair(p, U*interaction) );
                terms.push_back(term);
            }

            if (mu != 0.)
            { // mu * n
                hamterm_t term;
                term.with_sign = false;
                term.fill_operator = ident;
                term.operators.push_back( std::make_pair(p, mu*count) );
                terms.push_back(term);
            }

            for (int n=0; n<neighs.size(); ++n) { // hopping
                
                double t;
                if (lat.get_prop<double>("dx", p, neighs[n]) == dx1)
                    t = coeff1 * model["h"];
                else
                    t = coeff2 * model["h"];
                
                if (t != 0.) {
                    for( unsigned i = 0; i < hopops.size(); ++i )
                    {
                        hamterm_t term;
                        term.with_sign = false;
                        term.fill_operator = ident;
                        term.operators.push_back( std::make_pair(p, -t*hopops[i].first) );
                        term.operators.push_back( std::make_pair(neighs[n], hopops[i].second) );
                        terms.push_back(term);
                    }
                }
            }
        }
    }
    
    Index<U1> get_phys() const
    {
        return phys;
    }
    
    Hamiltonian<Matrix, U1> H () const
    {
        return ham(phys, ident, terms);
    }
    
    Measurements<Matrix, U1> measurements () const
    {
        Measurements<Matrix, U1> meas(Measurements<Matrix, U1>::Densitymatrix);
        meas.set_identity(psi_ident);
        
        if (model["MEASURE_CONTINUUM[Density]"]) {
            mterm_t term;
            term.fill_operator = psi_ident;
            term.name = "Density";
            term.type = mterm_t::Average;
            term.operators.push_back( std::make_pair(psi_count, false) );
            
            meas.add_term(term);
        }
        
        if (model["MEASURE_CONTINUUM[Local density]"]) {
            mterm_t term;
            term.fill_operator = psi_ident;
            term.name = "Local density";
            term.type = mterm_t::Local;
            term.operators.push_back( std::make_pair(psi_count, false) );
            
            meas.add_term(term);
        }
        
        if (model["MEASURE_CONTINUUM[Onebody density matrix]"]) {
            mterm_t term;
            term.fill_operator = psi_ident;
            term.name = "Onebody density matrix";
            term.type = mterm_t::HalfCorrelation;
            term.operators.push_back( std::make_pair(psi_create, false) );
            term.operators.push_back( std::make_pair(psi_destroy, false) );
            
            meas.add_term(term);
        }
        
        return meas;
    }
    
private:
    
    op_t adjoint_site_term(op_t h) const
    {
        /// h*rho*1 - 1*rho*h = (1 \otimes h) - (h^T \otimes 1) * rho
        ///                   = rho * (1 \otimes h^T) - (h \otimes 1)
        op_t idh, hid;
        dm_kron(psi_phys, h,         psi_ident, hid);
        h.transpose_inplace();
        dm_kron(psi_phys, psi_ident, h,         idh);
        return idh - hid;
    }

    std::vector<std::pair<op_t,op_t> > adjoint_bond_term(op_t const& h1, op_t const& h2) const
    {
        maquis::cout << "h1:\n" << h1;
        maquis::cout << "h2:\n" << h2;
        
        op_t bond;
        op_kron(psi_phys, h1, h2, bond);
        maquis::cout << "bond:\n" << bond;
        
        Index<U1> phys2 = psi_phys*psi_phys;
        op_t ident_phys2 = identity_matrix<Matrix>(phys2);
        
        /// h*rho*1 - 1*rho*h = (1 \otimes h) - (h^T \otimes 1) * rho
        ///                   = rho * (1 \otimes h^T) - (h \otimes 1)
        op_t idh, hid;
        dm_kron(phys2, bond,        ident_phys2, hid);
        bond.transpose_inplace();
        dm_kron(phys2, ident_phys2, bond,        idh);
//        maquis::cout << "t1:\n" << t1;
//        maquis::cout << "t2:\n" << t2;
//        maquis::cout << "transpose(bond):\n" << bond;
        bond = idh - hid;
        
        bond = reshape_2site_op(phys2, bond);
        op_t U, V, left, right;
        block_matrix<typename alps::numeric::associated_real_diagonal_matrix<Matrix>::type, U1> S, Ssqrt;
        svd(bond, U, V, S);
        Ssqrt = sqrt(S);
        gemm(U, Ssqrt, left);
        gemm(Ssqrt, V, right);
        
        std::vector<op_t> leftops  = reshape_right_to_list(phys, left);
        std::vector<op_t> rightops = reshape_left_to_list (phys, right);
        assert(leftops.size() == rightops.size());

        std::vector<std::pair<op_t,op_t> > ret;
        
        // discard terms with no weight
        size_t j = 0;
        for( size_t k = 0; k < S.n_blocks(); ++k )
            for( unsigned i = 0; i < num_rows(S[k]); ++i )
            {
                if (std::abs(S[k][i]) > 1e-10)
                    ret.push_back(std::make_pair( leftops[j], rightops[j] ));
                ++j;
            }
        return ret;
    }

    std::vector<std::pair<op_t,op_t> > adjoint_bond_term_new(op_t h1, op_t h2) const
    {
        /// 1*rho*h = (h^T \otimes 1) * rho
        ///         = rho * (h \otimes 1)
        op_t h1id, h2id;
        dm_kron(psi_phys, h1, psi_ident, h1id);
        dm_kron(psi_phys, h2, psi_ident, h2id);
        
        /// h*rho*1 = (1 \otimes h) * rho
        ///         = rho * (1 \otimes h^T)
        op_t idh1, idh2;
        h1.transpose_inplace();
        h2.transpose_inplace();
        dm_kron(psi_phys, psi_ident, h1, idh1);
        dm_kron(psi_phys, psi_ident, h2, idh2);
        
        std::vector<std::pair<op_t,op_t> > ret; ret.reserve(2);
        ret.push_back( std::make_pair(idh1, idh2) );
        ret.push_back( std::make_pair(-1.*h1id, h2id) );
        
        return ret;
    }

    
    const Lattice & lat;
    BaseParameters & model;
    
    Index<U1> psi_phys;
    op_t psi_ident, psi_count, psi_interaction, psi_create, psi_destroy;

    Index<U1> phys;
    op_t ident, count, interaction;
    
    std::vector<hamterm_t> terms;
};

#endif

