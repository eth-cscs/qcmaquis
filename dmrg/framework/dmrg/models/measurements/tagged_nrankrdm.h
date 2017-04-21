/*****************************************************************************
 *
 * ALPS MPS DMRG Project
 *
 * Copyright (C) 2014 Institute for Theoretical Physics, ETH Zurich
 *               2011-2011 by Bela Bauer <bauerb@phys.ethz.ch>
 *               2011-2013    Michele Dolfi <dolfim@phys.ethz.ch>
 *               2014-2014    Sebastian Keller <sebkelle@phys.ethz.ch>
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

#ifndef MEASUREMENTS_TAGGED_NRANKRDM_H
#define MEASUREMENTS_TAGGED_NRANKRDM_H

#include <algorithm>
#include <functional>
#include <boost/iterator/counting_iterator.hpp>
#include <boost/filesystem.hpp>

#include "dmrg/block_matrix/symmetry/nu1pg.h"
#include "dmrg/models/measurement.h"

#include "dmrg/models/chem/su2u1/term_maker.h"

namespace measurements_details {

    template <class symm, class = void>
    class checkpg
    {
    public:

        template <class matrix>
        bool operator()(term_descriptor<typename matrix::value_type> const & term,
                        boost::shared_ptr<TagHandler<matrix, symm> > tag_handler, Lattice const & lat) 
        {
            return true;
        }
    };

    template <class symm>
    class checkpg<symm, typename boost::enable_if<symm_traits::HasPG<symm> >::type>
    {
    public:
        typedef typename symm::charge charge;
        typedef typename symm::subcharge subcharge;

        template <class matrix>
        bool operator()(term_descriptor<typename matrix::value_type> const & term,
                boost::shared_ptr<TagHandler<matrix, symm> > tag_handler,
                Lattice const & lat) 
        {
            typedef typename TagHandler<matrix, symm>::op_t op_t;

            charge acc = symm::IdentityCharge;
            for (std::size_t p = 0; p < term.size(); ++p) {
                charge local = symm::IdentityCharge;
                if (tag_handler->is_fermionic(term.operator_tag(p)))
                    symm::irrep(local) = lat.get_prop<subcharge>("type", term.position(p));

                acc = symm::fuse(acc, local);
            }
         
            if (acc == symm::IdentityCharge)
                return true;

            return false;
        }
    };

}

namespace measurements {
    
    template <class Matrix, class SymmGroup, class = void>
    class TaggedNRankRDM : public measurement<Matrix, SymmGroup> {

        typedef measurement<Matrix, SymmGroup> base;

        typedef typename Model<Matrix, SymmGroup>::term_descriptor term_descriptor;

        typedef Lattice::pos_t pos_t;
        typedef std::vector<pos_t> positions_type;

        typedef typename base::op_t op_t;
        typedef typename OPTable<Matrix, SymmGroup>::tag_type tag_type;
        typedef typename Matrix::value_type value_type;

        typedef std::vector<tag_type> tag_vec;
        typedef std::vector<tag_vec> bond_term;
        typedef std::pair<std::vector<tag_vec>, value_type> scaled_bond_term;
    
    public:
        TaggedNRankRDM(std::string const& name_, const Lattice & lat,
                       boost::shared_ptr<TagHandler<Matrix, SymmGroup> > tag_handler_,
                       tag_vec const & identities_, tag_vec const & fillings_, std::vector<scaled_bond_term> const& ops_,
                       bool half_only_, positions_type const& positions_ = positions_type(),
                       std::string const& ckp_ = std::string(""))
        : base(name_)
        , lattice(lat)
        , tag_handler(tag_handler_)
        , positions_first(positions_)
        , identities(identities_)
        , fillings(fillings_)
        , operator_terms(ops_)
        , bra_ckp(ckp_)
        {
            pos_t extent = operator_terms.size() > 2 ? lattice.size() : lattice.size()-1;
            if (positions_first.size() == 0)
                std::copy(boost::counting_iterator<pos_t>(0), boost::counting_iterator<pos_t>(extent),
                          back_inserter(positions_first));
            
            //this->cast_to_real = is_hermitian_meas(ops[0]);
            this->cast_to_real = false;
        }
        
        void evaluate(MPS<Matrix, SymmGroup> const& ket_mps, boost::optional<reduced_mps<Matrix, SymmGroup> const&> rmps = boost::none)
        {
            this->vector_results.clear();
            this->labels.clear();

            MPS<Matrix, SymmGroup> bra_mps;
            if (bra_ckp != "") {
                if(boost::filesystem::exists(bra_ckp))
                    load(bra_ckp, bra_mps);
                else
                    throw std::runtime_error("The bra checkpoint file " + bra_ckp + " was not found\n");
            }

            if (operator_terms[0].first.size() == 2)
                measure_correlation(bra_mps, ket_mps);
            else if (operator_terms[0].first.size() == 4)
                measure_2rdm(bra_mps, ket_mps);
            else if (operator_terms[0].first.size() == 6)
                measure_3rdm(bra_mps, ket_mps);
            else
                throw std::runtime_error("correlation measurements at the moment supported with 2, 4, 6 and 8 operators, size is "
                                          + boost::lexical_cast<std::string>(operator_terms[0].first.size()));
        }
        
    protected:

        measurement<Matrix, SymmGroup>* do_clone() const
        {
            return new TaggedNRankRDM(*this);
        }
        
        void measure_correlation(MPS<Matrix, SymmGroup> const & dummy_bra_mps,
                                 MPS<Matrix, SymmGroup> const & ket_mps)
        {
            // Test if a separate bra state has been specified
            bool bra_neq_ket = (dummy_bra_mps.length() > 0);
            MPS<Matrix, SymmGroup> const & bra_mps = (bra_neq_ket) ? dummy_bra_mps : ket_mps;

            #ifdef MAQUIS_OPENMP
            #pragma omp parallel for
            #endif
            for (std::size_t i = 0; i < positions_first.size(); ++i) {
                pos_t p1 = positions_first[i];
                boost::shared_ptr<TagHandler<Matrix, SymmGroup> > tag_handler_local(new TagHandler<Matrix, SymmGroup>(*tag_handler));

                std::vector<typename MPS<Matrix, SymmGroup>::scalar_type> dct;
                std::vector<std::vector<pos_t> > num_labels;
                for (pos_t p2 = p1+1; p2 < lattice.size(); ++p2)
                { 
                    pos_t pos_[2] = {p1, p2};
                    std::vector<pos_t> positions(pos_, pos_ + 2);

                    tag_vec operators(2);
                    operators[0] = operator_terms[0].first[0][lattice.get_prop<typename SymmGroup::subcharge>("type", p1)];
                    operators[1] = operator_terms[0].first[1][lattice.get_prop<typename SymmGroup::subcharge>("type", p2)];

                    // check if term is allowed by symmetry
                    term_descriptor term = generate_mpo::arrange_operators(positions, operators, tag_handler_local);
                    if(measurements_details::checkpg<SymmGroup>()(term, tag_handler_local, lattice))
                    {
                        MPO<Matrix, SymmGroup> mpo = generate_mpo::sign_and_fill(term, identities, fillings, tag_handler_local, lattice);
                        typename MPS<Matrix, SymmGroup>::scalar_type value = operator_terms[0].second * expval(bra_mps, ket_mps, mpo);

                        dct.push_back(value);
                        num_labels.push_back(positions);
                    }
                    else {
                        dct.push_back(0.0);
                        num_labels.push_back(positions);
                    }
                }

                std::vector<std::string> lbt = label_strings(lattice,  num_labels);

                // save results and labels
                #ifdef MAQUIS_OPENMP
                #pragma omp critical
                #endif
                {
                    this->vector_results.reserve(this->vector_results.size() + dct.size());
                    std::copy(dct.rbegin(), dct.rend(), std::back_inserter(this->vector_results));

                    this->labels.reserve(this->labels.size() + dct.size());
                    std::copy(lbt.rbegin(), lbt.rend(), std::back_inserter(this->labels));
                }
            }
        }

        void measure_2rdm(MPS<Matrix, SymmGroup> const & dummy_bra_mps,
                          MPS<Matrix, SymmGroup> const & ket_mps)
        {
            // Test if a separate bra state has been specified
            bool bra_neq_ket = (dummy_bra_mps.length() > 0);
            MPS<Matrix, SymmGroup> const & bra_mps = (bra_neq_ket) ? dummy_bra_mps : ket_mps;

            #ifdef MAQUIS_OPENMP
            #pragma omp parallel for collapse(1)
            #endif
            for (pos_t p1 = 0; p1 < lattice.size(); ++p1)
            for (pos_t p2 = 0; p2 < lattice.size(); ++p2)
            {
                boost::shared_ptr<TagHandler<Matrix, SymmGroup> > tag_handler_local(new TagHandler<Matrix, SymmGroup>(*tag_handler));

                // Permutation symmetry for bra == ket: pqrs == rspq == qpsr == srqp
                // if bra != ket, pertmutation symmetry is only pqrs == qpsr
                pos_t subref = 0;
                if (bra_neq_ket)
                    subref = 0;
                else
                    subref = std::min(p1, p2);

                for (pos_t p3 = subref; p3 < lattice.size(); ++p3)
                { 
                    std::vector<typename MPS<Matrix, SymmGroup>::scalar_type> dct;
                    std::vector<std::vector<pos_t> > num_labels;

                    for (pos_t p4 = p3; p4 < lattice.size(); ++p4)
                    { 
                        pos_t pos_[4] = {p1, p2, p3, p4};
                        std::vector<pos_t> positions(pos_, pos_ + 4);

                        // Loop over operator terms that are measured synchronously and added together
                        // Used e.g. for the four spin combos of the 2-RDM
                        typename MPS<Matrix, SymmGroup>::scalar_type value = 0;
                        bool checkpass = false;
                        for (std::size_t synop = 0; synop < operator_terms.size(); ++synop) {

                            tag_vec operators(4);
                            operators[0] = operator_terms[synop].first[0][lattice.get_prop<typename SymmGroup::subcharge>("type", p1)];
                            operators[1] = operator_terms[synop].first[1][lattice.get_prop<typename SymmGroup::subcharge>("type", p2)];
                            operators[2] = operator_terms[synop].first[2][lattice.get_prop<typename SymmGroup::subcharge>("type", p3)];
                            operators[3] = operator_terms[synop].first[3][lattice.get_prop<typename SymmGroup::subcharge>("type", p4)];

                            // check if term is allowed by symmetry
                            term_descriptor term = generate_mpo::arrange_operators(positions, operators, tag_handler_local);
                            if(checkpass || measurements_details::checkpg<SymmGroup>()(term, tag_handler_local, lattice))
                            {
                                checkpass = true;
                                MPO<Matrix, SymmGroup> mpo = generate_mpo::sign_and_fill(term, identities, fillings, tag_handler_local, lattice);
                                value += operator_terms[synop].second * expval(bra_mps, ket_mps, mpo);
                            }
                            else break;
                        }
                        if(checkpass)
                        {
                             dct.push_back(value);
                             num_labels.push_back(positions);
                        }
                    }

                    std::vector<std::string> lbt = label_strings(lattice,  num_labels);

                    // save results and labels
                    #ifdef MAQUIS_OPENMP
                    #pragma omp critical
                    #endif
                    {
                        this->vector_results.reserve(this->vector_results.size() + dct.size());
                        std::copy(dct.rbegin(), dct.rend(), std::back_inserter(this->vector_results));

                        this->labels.reserve(this->labels.size() + dct.size());
                        std::copy(lbt.rbegin(), lbt.rend(), std::back_inserter(this->labels));
                    }
                }
            }
        }

        void measure_3rdm(MPS<Matrix, SymmGroup> const & dummy_bra_mps,
                          MPS<Matrix, SymmGroup> const & ket_mps)
        {
            // Test if a separate bra state has been specified bool bra_neq_ket = (dummy_bra_mps.length() > 0);
            bool bra_neq_ket = (dummy_bra_mps.length() > 0);
            MPS<Matrix, SymmGroup> const & bra_mps = (bra_neq_ket) ? dummy_bra_mps : ket_mps;

            // if bra != ket, no transpose symmetry
            #ifdef MAQUIS_OPENMP
            // Intel Composer v15 patch 0 does not accept collapse(2) - FIXME! test v15 patch 5
            //#pragma omp parallel for collapse(2) schedule (dynamic,1)
            #pragma omp parallel for collapse(1) schedule (dynamic,1)
            #endif
            for (pos_t p1 = 0; p1 < lattice.size(); ++p1)
            for (pos_t p2 = 0; p2 < lattice.size(); ++p2)
            {
                if(p2 > p1) continue;

                boost::shared_ptr<TagHandler<Matrix, SymmGroup> > tag_handler_local(new TagHandler<Matrix, SymmGroup>(*tag_handler));

                for (pos_t p3 = std::min(p1, p2); p3 < lattice.size(); ++p3)
                {
                    if(p1 == p2 && p1 == p3)
                        continue;

                    for (pos_t p4 = ((bra_neq_ket) ? 0 : std::min(p1, p2)); p4 < lattice.size(); ++p4)
                    {
    
                        for (pos_t p5 = ((bra_neq_ket) ? 0 : std::min(p1, p2)); p5 < lattice.size(); ++p5)
                        { 
                            std::vector<typename MPS<Matrix, SymmGroup>::scalar_type> dct;
                            std::vector<std::vector<pos_t> > num_labels;
    
                            for (pos_t p6 = std::min(p4, p5); p6 < lattice.size(); ++p6)
                            {
                                // sixth index must be different if p4 == p5 
                                if(p4 == p5 && p4 == p6)
                                      continue;
    
                                // defines position vector for spin-free 3-RDM element
                                pos_t pos_[6] = {p1, p2, p3, p4, p5, p6};
                                std::vector<pos_t> positions(pos_, pos_ + 6);
    
                                // Loop over operator terms that are measured synchronously and added together
                                // Used e.g. for the spin combos of the 3-RDM
                                typename MPS<Matrix, SymmGroup>::scalar_type value = 0;
                                bool measured = false;
                                for (std::size_t synop = 0; synop < operator_terms.size(); ++synop) {
    
                                    tag_vec operators(6);
                                    operators[0] = operator_terms[synop].first[0][lattice.get_prop<typename SymmGroup::subcharge>("type", positions[0])];
                                    operators[1] = operator_terms[synop].first[1][lattice.get_prop<typename SymmGroup::subcharge>("type", positions[1])];
                                    operators[2] = operator_terms[synop].first[2][lattice.get_prop<typename SymmGroup::subcharge>("type", positions[2])];
                                    operators[3] = operator_terms[synop].first[3][lattice.get_prop<typename SymmGroup::subcharge>("type", positions[3])];
                                    operators[4] = operator_terms[synop].first[4][lattice.get_prop<typename SymmGroup::subcharge>("type", positions[4])];
                                    operators[5] = operator_terms[synop].first[5][lattice.get_prop<typename SymmGroup::subcharge>("type", positions[5])];
    
                                    // check if term is allowed by symmetry
                                    term_descriptor term = generate_mpo::arrange_operators(positions, operators, tag_handler_local);
                                    if(not measurements_details::checkpg<SymmGroup>()(term, tag_handler_local, lattice))
                                         continue;
                                    measured = true;
    
                                    MPO<Matrix, SymmGroup> mpo = generate_mpo::sign_and_fill(term, identities, fillings, tag_handler_local, lattice);
                                    value += operator_terms[synop].second * expval(bra_mps, ket_mps, mpo);
    
                                }
                                if(measured)
                                {
                                     dct.push_back(value);
                                     num_labels.push_back(positions);
                                }
                            }
    
                            std::vector<std::string> lbt = label_strings(lattice,  num_labels);

                            // save results and labels
                            #ifdef MAQUIS_OPENMP
                            #pragma omp critical
                            #endif
                            {
                                this->vector_results.reserve(this->vector_results.size() + dct.size());
                                std::copy(dct.rbegin(), dct.rend(), std::back_inserter(this->vector_results));

                                this->labels.reserve(this->labels.size() + dct.size());
                                std::copy(lbt.rbegin(), lbt.rend(), std::back_inserter(this->labels));
                            }
                        }
                    }
                }
            }
        }

    private:
        Lattice lattice;
        boost::shared_ptr<TagHandler<Matrix, SymmGroup> > tag_handler;
        positions_type positions_first;
        tag_vec identities, fillings;
        std::vector<scaled_bond_term> operator_terms;

        std::string bra_ckp;
    };


    
    template <class Matrix, class SymmGroup>
    class TaggedNRankRDM<Matrix, SymmGroup, typename boost::enable_if<symm_traits::HasSU2<SymmGroup> >::type >
        : public measurement<Matrix, SymmGroup>
    {
        typedef measurement<Matrix, SymmGroup> base;

        typedef typename Model<Matrix, SymmGroup>::term_descriptor term_descriptor;

        typedef Lattice::pos_t pos_t;
        typedef std::vector<pos_t> positions_type;

        typedef typename base::op_t op_t;
        typedef typename OPTable<Matrix, SymmGroup>::tag_type tag_type;
        typedef typename Matrix::value_type value_type;

        typedef std::vector<tag_type> tag_vec;
        typedef std::vector<tag_vec> bond_term;
        typedef std::pair<std::vector<tag_vec>, value_type> scaled_bond_term;

        typedef TermMakerSU2<Matrix, SymmGroup> TM;
    
    public:
        TaggedNRankRDM(std::string const& name_, const Lattice & lat,
                       boost::shared_ptr<TagHandler<Matrix, SymmGroup> > tag_handler_,
                       typename TM::OperatorCollection const & op_collection_,
                       positions_type const& positions_ = positions_type(),
                       std::string const& ckp_ = std::string(""))
        : base(name_)
        , lattice(lat)
        , tag_handler(tag_handler_)
        , op_collection(op_collection_)
        , positions_first(positions_)
        , identities(op_collection.ident.no_couple)
        , fillings(op_collection.fill.no_couple)
        , bra_ckp(ckp_)
        {
            pos_t extent = lattice.size();
            if (positions_first.size() == 0)
                std::copy(boost::counting_iterator<pos_t>(0), boost::counting_iterator<pos_t>(extent),
                          back_inserter(positions_first));
            
            //this->cast_to_real = is_hermitian_meas(ops[0]);
            this->cast_to_real = false;
        }
        
        void evaluate(MPS<Matrix, SymmGroup> const& ket_mps, boost::optional<reduced_mps<Matrix, SymmGroup> const&> rmps = boost::none)
        {
            this->vector_results.clear();
            this->labels.clear();

            MPS<Matrix, SymmGroup> bra_mps;
            if (bra_ckp != "") {
                if(boost::filesystem::exists(bra_ckp))
                    load(bra_ckp, bra_mps);
                else
                    throw std::runtime_error("The bra checkpoint file " + bra_ckp + " was not found\n");
            }

            if (this->name() == "oneptdm")
                measure_correlation(bra_mps, ket_mps);
            else if (this->name() == "twoptdm")
                measure_2rdm(bra_mps, ket_mps);
        }
        
    protected:

        measurement<Matrix, SymmGroup>* do_clone() const
        {
            return new TaggedNRankRDM(*this);
        }

        void extract(std::vector<value_type> & results, std::vector<std::vector<Lattice::pos_t> > & num_labels)
        {
            results = this->vector_results;
            num_labels = numeric_labels;
        }
        
        void measure_correlation(MPS<Matrix, SymmGroup> const & dummy_bra_mps,
                                 MPS<Matrix, SymmGroup> const & ket_mps)
        {
            // Test if a separate bra state has been specified
            bool bra_neq_ket = (dummy_bra_mps.length() > 0);
            MPS<Matrix, SymmGroup> const & bra_mps = (bra_neq_ket) ? dummy_bra_mps : ket_mps;

            #ifdef MAQUIS_OPENMP
            #pragma omp parallel for schedule(dynamic)
            #endif
            for (std::size_t i = 0; i < positions_first.size(); ++i) {
                pos_t p1 = positions_first[i];
                boost::shared_ptr<TagHandler<Matrix, SymmGroup> > tag_handler_local(new TagHandler<Matrix, SymmGroup>(*tag_handler));

                std::vector<typename MPS<Matrix, SymmGroup>::scalar_type> dct;
                std::vector<std::vector<pos_t> > num_labels;
                for (pos_t p2 = p1; p2 < lattice.size(); ++p2)
                { 
                    pos_t pos_[2] = {p1, p2};
                    std::vector<pos_t> positions(pos_, pos_ + 2);

                    std::vector<term_descriptor> terms;
                    if (p1 != p2) 
                        // The sqrt(2.) balances the magnitudes of Clebsch coeffs C^{1/2 1/2 0}_{mrm'} which apply at the second spin-1/2 operator
                        terms.push_back(TermMakerSU2<Matrix, SymmGroup>::positional_two_term(
                            true, op_collection.ident.no_couple, std::sqrt(2.), p1, p2, op_collection.create.couple_down, op_collection.create.fill_couple_up,
                                                              op_collection.destroy.couple_down, op_collection.destroy.fill_couple_up, lattice
                        ));
                    else {
                        term_descriptor term;
                        term.coeff = 1.;
                        term.push_back( boost::make_tuple(p1, op_collection.count.no_couple[lattice.get_prop<typename SymmGroup::subcharge>("type", p1)]) );
                        terms.push_back(term);
                    }

                    // check if term is allowed by symmetry
                    if(not measurements_details::checkpg<SymmGroup>()(terms[0], tag_handler_local, lattice))
                           continue;
                    
                    generate_mpo::TaggedMPOMaker<Matrix, SymmGroup> mpo_m(lattice, op_collection.ident.no_couple, op_collection.ident_full.no_couple,
                                                                          op_collection.fill.no_couple, tag_handler_local, terms);
                    MPO<Matrix, SymmGroup> mpo = mpo_m.create_mpo();
                    typename MPS<Matrix, SymmGroup>::scalar_type value = expval(bra_mps, ket_mps, mpo);

                    dct.push_back(value);
                    positions[0] = lattice.get_prop<pos_t>("nlabel", positions[0]);
                    positions[1] = lattice.get_prop<pos_t>("nlabel", positions[1]);
                    num_labels.push_back(positions);
                }

                // the lattice knows the ordering and provides the correct orbital label for each position
                //std::vector<std::string> lbt = label_strings(lattice,  num_labels);
                std::vector<std::string> lbt = label_strings(num_labels);

                // save results and labels
                #ifdef MAQUIS_OPENMP
                #pragma omp critical
                #endif
                {
                    this->vector_results.reserve(this->vector_results.size() + dct.size());
                    std::copy(dct.rbegin(), dct.rend(), std::back_inserter(this->vector_results));

                    this->labels.reserve(this->labels.size() + dct.size());
                    std::copy(lbt.rbegin(), lbt.rend(), std::back_inserter(this->labels));

                    this->numeric_labels.reserve(this->numeric_labels.size() + dct.size());
                    std::copy(num_labels.rbegin(), num_labels.rend(), std::back_inserter(numeric_labels));
                }
            }
        }

        void measure_2rdm(MPS<Matrix, SymmGroup> const & dummy_bra_mps,
                          MPS<Matrix, SymmGroup> const & ket_mps)
        {
            // Test if a separate bra state has been specified
            bool bra_neq_ket = (dummy_bra_mps.length() > 0);
            MPS<Matrix, SymmGroup> const & bra_mps = (bra_neq_ket) ? dummy_bra_mps : ket_mps;

            #ifdef MAQUIS_OPENMP
            #pragma omp parallel for collapse(1) schedule(dynamic)
            #endif
            for (pos_t p1 = 0; p1 < lattice.size(); ++p1)
            for (pos_t p2 = 0; p2 < lattice.size(); ++p2)
            {
                boost::shared_ptr<TagHandler<Matrix, SymmGroup> > tag_handler_local(new TagHandler<Matrix, SymmGroup>(*tag_handler));

                // Permutation symmetry for bra == ket: pqrs == rspq == qpsr == srqp
                pos_t subref = std::min(p1, p2);

                // if bra != ket, pertmutation symmetry is only pqrs == qpsr
                if (bra_neq_ket)
                    pos_t subref = 0;

                std::vector<typename MPS<Matrix, SymmGroup>::scalar_type> dct;
                std::vector<std::vector<pos_t> > num_labels;
                num_labels.reserve(lattice.size() * lattice.size());

                for (pos_t p3 = subref; p3 < lattice.size(); ++p3)
                { 
                    for (pos_t p4 = p3; p4 < lattice.size(); ++p4)
                    { 
                        pos_t pos_[4] = {p1, p2, p3, p4};
                        std::vector<pos_t> positions(pos_, pos_ + 4);

                        std::vector<term_descriptor> terms = SpinSumSU2<Matrix, SymmGroup>::V_term(1., p1, p2, p3, p4, op_collection, lattice);

                        // check if term is allowed by symmetry
                        if(not measurements_details::checkpg<SymmGroup>()(terms[0], tag_handler_local, lattice))
                               continue;
                        
                        generate_mpo::TaggedMPOMaker<Matrix, SymmGroup> mpo_m(lattice, op_collection.ident.no_couple,
                                                                              op_collection.ident_full.no_couple,
                                                                              op_collection.fill.no_couple, tag_handler_local, terms);
                        MPO<Matrix, SymmGroup> mpo = mpo_m.create_mpo();
                        typename MPS<Matrix, SymmGroup>::scalar_type value = expval(bra_mps, ket_mps, mpo);

                        dct.push_back(value);
                        std::transform(positions.begin(), positions.end(), positions.begin(),
                                       boost::bind(static_cast<pos_t(Lattice::*)(std::string, pos_t) const>(&Lattice::get_prop),
                                                   &lattice, std::string("nlabel"), boost::lambda::_1));
                        num_labels.push_back(positions);
                    }
                }

                //std::vector<std::string> lbt = label_strings(lattice,  num_labels);
                std::vector<std::string> lbt = label_strings(num_labels);

                // save results and labels
                #ifdef MAQUIS_OPENMP
                #pragma omp critical
                #endif
                {
                    this->vector_results.reserve(this->vector_results.size() + dct.size());
                    std::copy(dct.rbegin(), dct.rend(), std::back_inserter(this->vector_results));

                    this->labels.reserve(this->labels.size() + dct.size());
                    std::copy(lbt.rbegin(), lbt.rend(), std::back_inserter(this->labels));

                    this->numeric_labels.reserve(this->numeric_labels.size() + dct.size());
                    std::copy(num_labels.rbegin(), num_labels.rend(), std::back_inserter(numeric_labels));
                }
            }
        }

    private:
        Lattice lattice;
        boost::shared_ptr<TagHandler<Matrix, SymmGroup> > tag_handler;

        typename TM::OperatorCollection op_collection;

        positions_type positions_first;
        tag_vec identities, fillings;

        std::string bra_ckp;

        std::vector<std::vector<pos_t> > numeric_labels;
    };
}

#endif
