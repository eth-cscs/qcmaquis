/*****************************************************************************
 *
 * QCMaquis DMRG Project
 *
 * Copyright (C) 2015 Laboratory for Physical Chemistry, ETH Zurich
 *               2012-2013 by Sebastian Keller <sebkelle@phys.ethz.ch>
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

#ifndef QC_HAMILTONIANS_H
#define QC_HAMILTONIANS_H

#include <cmath>
#include <sstream>
#include <fstream>
#include <iterator>
#include <boost/shared_ptr.hpp>
#include <boost/tokenizer.hpp>
#include <boost/regex.hpp>

#include "dmrg/models/model.h"
#include "dmrg/models/measurements.h"
#include "dmrg/utils/BaseParameters.h"

#include "dmrg/models/chem/util.h"
#include "dmrg/models/chem/parse_integrals.h"
#include "dmrg/models/chem/pg_util.h"
#include "dmrg/models/chem/2u1/term_maker.h"
#include "dmrg/models/chem/2u1/chem_helper.h"

template<class Matrix, class SymmGroup>
class qc_model : public model_impl<Matrix, SymmGroup>
{
    typedef model_impl<Matrix, SymmGroup> base;
    
    typedef typename base::table_type table_type;
    typedef typename base::table_ptr table_ptr;
    typedef typename base::tag_type tag_type;
    
    typedef typename base::term_descriptor term_descriptor;
    typedef typename base::terms_type terms_type;
    typedef typename base::op_t op_t;
    typedef typename base::measurements_type measurements_type;

    typedef typename Lattice::pos_t pos_t;
    typedef typename Matrix::value_type value_type;

public:
    
    qc_model(Lattice const & lat_, BaseParameters & parms_);

    void create_terms();
    
    void update(BaseParameters const& p)
    {
        // TODO: update this->terms_ with the new parameters
        throw std::runtime_error("update() not yet implemented for this model.");
        return;
    }
    
    // For this model: site_type == point group irrep
    Index<SymmGroup> const & phys_dim(size_t type) const
    {
        return phys_indices[type];
    }
    tag_type identity_matrix_tag(size_t type) const
    {
        return ident[type];
    }
    tag_type filling_matrix_tag(size_t type) const
    {
        return fill[type];
    }

    typename SymmGroup::charge total_quantum_numbers(BaseParameters & parms_) const
    {
        return chem_detail::qn_helper<SymmGroup>().total_qn(parms_);
    }

    tag_type get_operator_tag(std::string const & name, size_t type) const
    {
        if (name == "create_up")
            return create_up[type];
        else if (name == "create_down")
            return create_down[type];
        else if (name == "destroy_up")
            return destroy_up[type];
        else if (name == "destroy_down")
            return destroy_down[type];
        else if (name == "count_up")
            return count_up[type];
        else if (name == "count_down")
            return count_down[type];
        else if (name == "e2d")
            return e2d[type];
        else if (name == "d2e")
            return d2e[type];
        else if (name == "docc")
            return docc[type];
        else
            throw std::runtime_error("Operator not valid for this model.");
        return 0;
    }

    table_ptr operators_table() const
    {
        return tag_handler;
    }

    measurements_type measurements () const
    {
        typedef boost::tokenizer<boost::char_separator<char> > tokenizer;

        std::vector<tag_type> swap_d2u              = (tag_handler->get_product_tags(destroy_down, create_up)).first;
        std::vector<tag_type> swap_u2d              = (tag_handler->get_product_tags(destroy_up, create_down)).first;
        std::vector<tag_type> create_up_count_down  = (tag_handler->get_product_tags(count_down, create_up)  ).first;
        std::vector<tag_type> create_down_count_up  = (tag_handler->get_product_tags(count_up, create_down)  ).first;
        std::vector<tag_type> destroy_up_count_down = (tag_handler->get_product_tags(count_down, destroy_up) ).first;
        std::vector<tag_type> destroy_down_count_up = (tag_handler->get_product_tags(count_up, destroy_down) ).first;

        std::vector<op_t> ident_ops = tag_handler->get_ops(ident);
        std::vector<op_t> fill_ops = tag_handler->get_ops(fill);
        std::vector<op_t> count_up_ops = tag_handler->get_ops(count_up);
        std::vector<op_t> count_down_ops = tag_handler->get_ops(count_down);
        std::vector<op_t> docc_ops = tag_handler->get_ops(docc);

        measurements_type meas;

        {
            if (parms.is_set("MEASURE[ChemEntropy]"))
            {
                parms.set("MEASURE_LOCAL[Nup]", "Nup");
                parms.set("MEASURE_LOCAL[Ndown]", "Ndown");
                parms.set("MEASURE_LOCAL[Nupdown]", "Nup*Ndown");
                parms.set("MEASURE_HALF_CORRELATIONS[dm_up]", "cdag_up:c_up");
                parms.set("MEASURE_HALF_CORRELATIONS[dm_down]", "cdag_down:c_down");

                parms.set("MEASURE_HALF_CORRELATIONS[nupnup]", "Nup:Nup");
                parms.set("MEASURE_HALF_CORRELATIONS[nupndown]", "Nup:Ndown");
                parms.set("MEASURE_HALF_CORRELATIONS[ndownnup]", "Ndown:Nup");
                parms.set("MEASURE_HALF_CORRELATIONS[ndownndown]", "Ndown:Ndown");
                parms.set("MEASURE_HALF_CORRELATIONS[doccdocc]", "Nup*Ndown:Nup*Ndown");

                parms.set("MEASURE_HALF_CORRELATIONS[transfer_up_while_down]", "cdag_up*Ndown:c_up*Ndown");
                parms.set("MEASURE_HALF_CORRELATIONS[transfer_down_while_up]", "cdag_down*Nup:c_down*Nup");

                parms.set("MEASURE_HALF_CORRELATIONS[transfer_up_while_down_at_2]", "cdag_up:c_up*Ndown");
                parms.set("MEASURE_HALF_CORRELATIONS[transfer_up_while_down_at_1]", "cdag_up*Ndown:c_up");
                parms.set("MEASURE_HALF_CORRELATIONS[transfer_down_while_up_at_2]", "cdag_down:c_down*Nup");
                parms.set("MEASURE_HALF_CORRELATIONS[transfer_down_while_up_at_1]", "cdag_down*Nup:c_down");

                parms.set("MEASURE_HALF_CORRELATIONS[transfer_pair]", "cdag_up*cdag_down:c_up*c_down");
                parms.set("MEASURE_HALF_CORRELATIONS[spinflip]", "cdag_up*c_down:cdag_down*c_up");

                parms.set("MEASURE_HALF_CORRELATIONS[nupdocc]", "Nup:Nup*Ndown");
                parms.set("MEASURE_HALF_CORRELATIONS[ndowndocc]", "Ndown:Nup*Ndown");
                parms.set("MEASURE_HALF_CORRELATIONS[doccnup]", "Nup*Ndown:Nup");
                parms.set("MEASURE_HALF_CORRELATIONS[doccndown]", "Nup*Ndown:Ndown");

                parms.set("MEASURE_HALF_CORRELATIONS[splus_sminus]", "splus:sminus");
            }
        }

        typedef std::vector<tag_type> tag_vec;
        typedef std::vector<tag_vec> bond_tag_element;
        typedef std::pair<std::vector<tag_vec>, value_type> scaled_bond_element;
        {
            boost::regex expression("^MEASURE_LOCAL\\[(.*)]$");
            boost::smatch what;
            for (alps::Parameters::const_iterator it=parms.begin();it != parms.end();++it) {
                std::string lhs = it->key();
                if (boost::regex_match(lhs, what, expression)) {

                    std::vector<op_t> meas_op;
                    if (it->value() == "Nup")
                        meas_op = count_up_ops;
                    else if (it->value() == "Ndown")
                        meas_op = count_down_ops;
                    else if (it->value() == "Nup*Ndown" || it->value() == "docc")
                        meas_op = docc_ops;
                    else
                        throw std::runtime_error("Invalid observable\nLocal measurements supported so far are \"Nup\" and \"Ndown\"\n");

                    meas.push_back( new measurements::local<Matrix, SymmGroup>(what.str(1), lat, ident_ops, fill_ops, meas_op) );
                }
            }
        }

        {
        boost::regex expression("^MEASURE_CORRELATIONS\\[(.*)]$");
        boost::regex expression_half("^MEASURE_HALF_CORRELATIONS\\[(.*)]$");
        boost::regex expression_nn("^MEASURE_NN_CORRELATIONS\\[(.*)]$");
        boost::regex expression_halfnn("^MEASURE_HALF_NN_CORRELATIONS\\[(.*)]$");
        boost::regex expression_twoptdm("^MEASURE\\[2rdm\\]");
        boost::regex expression_transition_twoptdm("^MEASURE\\[trans2rdm\\]");
        boost::regex expression_threeptdm("^MEASURE\\[3rdm\\]");
        boost::smatch what;

        for (alps::Parameters::const_iterator it=parms.begin();it != parms.end();++it) {
            std::string lhs = it->key();

            std::string name, value;
            bool half_only, nearest_neighbors_only;
            if (boost::regex_match(lhs, what, expression)) {
                value = it->value();
                name = what.str(1);
                half_only = false;
                nearest_neighbors_only = false;
            }
            if (boost::regex_match(lhs, what, expression_half)) {
                value = it->value();
                name = what.str(1);
                half_only = true;
                nearest_neighbors_only = false;
            }
            if (boost::regex_match(lhs, what, expression_nn)) {
                value = it->value();
                name = what.str(1);
                half_only = false;
                nearest_neighbors_only = true;
            }
            if (boost::regex_match(lhs, what, expression_halfnn)) {
                value = it->value();
                name = what.str(1);
                half_only = true;
                nearest_neighbors_only = true;
            }

            if (boost::regex_match(lhs, what, expression_twoptdm) ||
                    boost::regex_match(lhs, what, expression_transition_twoptdm)) {

                std::string bra_ckp("");
                if(lhs == "MEASURE[trans2rdm]"){
                    name = "transition_twoptdm";
                    bra_ckp = it->value();
                }
                else
                    name = "twoptdm";

                std::vector<scaled_bond_element> synchronous_meas_operators;
                {
                    bond_tag_element meas_operators;
                    meas_operators.push_back(create_up);
                    meas_operators.push_back(create_up);
                    meas_operators.push_back(destroy_up);
                    meas_operators.push_back(destroy_up);
                    synchronous_meas_operators.push_back(std::make_pair(meas_operators, 1));
                }
                {
                    bond_tag_element meas_operators;
                    meas_operators.push_back(create_up);
                    meas_operators.push_back(create_down);
                    meas_operators.push_back(destroy_down);
                    meas_operators.push_back(destroy_up);
                    synchronous_meas_operators.push_back(std::make_pair(meas_operators, 1));
                }
                {
                    bond_tag_element meas_operators;
                    meas_operators.push_back(create_down);
                    meas_operators.push_back(create_up);
                    meas_operators.push_back(destroy_up);
                    meas_operators.push_back(destroy_down);
                    synchronous_meas_operators.push_back(std::make_pair(meas_operators, 1));
                }
                {
                    bond_tag_element meas_operators;
                    meas_operators.push_back(create_down);
                    meas_operators.push_back(create_down);
                    meas_operators.push_back(destroy_down);
                    meas_operators.push_back(destroy_down);
                    synchronous_meas_operators.push_back(std::make_pair(meas_operators, 1));
                }
                half_only = true;
                std::vector<pos_t> positions;
                meas.push_back( new measurements::TaggedNRankRDM<Matrix, SymmGroup>(name, lat, tag_handler, ident, fill, synchronous_meas_operators,
                                                                                    half_only, positions, bra_ckp));
            }

            else if (boost::regex_match(lhs, what, expression_threeptdm)) {

                std::string bra_ckp("");
                name = "threeptdm";

                std::vector<scaled_bond_element> synchronous_meas_operators;
                {
                    bond_tag_element meas_operators;
                    meas_operators.push_back(create_up);
                    meas_operators.push_back(create_up);
                    meas_operators.push_back(create_up);
                    meas_operators.push_back(destroy_up);
                    meas_operators.push_back(destroy_up);
                    meas_operators.push_back(destroy_up);
                    synchronous_meas_operators.push_back(std::make_pair(meas_operators, 1));
                }
                {
                    bond_tag_element meas_operators;
                    meas_operators.push_back(create_down);
                    meas_operators.push_back(create_down);
                    meas_operators.push_back(create_down);
                    meas_operators.push_back(destroy_down);
                    meas_operators.push_back(destroy_down);
                    meas_operators.push_back(destroy_down);
                    synchronous_meas_operators.push_back(std::make_pair(meas_operators, 1));
                }
                {
                    bond_tag_element meas_operators;
                    meas_operators.push_back(create_up);
                    meas_operators.push_back(create_up);
                    meas_operators.push_back(create_down);
                    meas_operators.push_back(destroy_up);
                    meas_operators.push_back(destroy_up);
                    meas_operators.push_back(destroy_down);
                    synchronous_meas_operators.push_back(std::make_pair(meas_operators, 1));
                }
                {
                    bond_tag_element meas_operators;
                    meas_operators.push_back(create_down);
                    meas_operators.push_back(create_down);
                    meas_operators.push_back(create_up);
                    meas_operators.push_back(destroy_down);
                    meas_operators.push_back(destroy_down);
                    meas_operators.push_back(destroy_up);
                    synchronous_meas_operators.push_back(std::make_pair(meas_operators, 1));
                }
                {
                    bond_tag_element meas_operators;
                    meas_operators.push_back(create_up);
                    meas_operators.push_back(create_down);
                    meas_operators.push_back(create_up);
                    meas_operators.push_back(destroy_up);
                    meas_operators.push_back(destroy_down);
                    meas_operators.push_back(destroy_up);
                    synchronous_meas_operators.push_back(std::make_pair(meas_operators, 1));
                }
                {
                    bond_tag_element meas_operators;
                    meas_operators.push_back(create_down);
                    meas_operators.push_back(create_up);
                    meas_operators.push_back(create_up);
                    meas_operators.push_back(destroy_down);
                    meas_operators.push_back(destroy_up);
                    meas_operators.push_back(destroy_up);
                    synchronous_meas_operators.push_back(std::make_pair(meas_operators, 1));
                }
                {
                    bond_tag_element meas_operators;
                    meas_operators.push_back(create_down);
                    meas_operators.push_back(create_up);
                    meas_operators.push_back(create_down);
                    meas_operators.push_back(destroy_down);
                    meas_operators.push_back(destroy_up);
                    meas_operators.push_back(destroy_down);
                    synchronous_meas_operators.push_back(std::make_pair(meas_operators, 1));
                }
                {
                    bond_tag_element meas_operators;
                    meas_operators.push_back(create_up);
                    meas_operators.push_back(create_down);
                    meas_operators.push_back(create_down);
                    meas_operators.push_back(destroy_up);
                    meas_operators.push_back(destroy_down);
                    meas_operators.push_back(destroy_down);
                    synchronous_meas_operators.push_back(std::make_pair(meas_operators, 1));
                }
                half_only = true;
                std::vector<pos_t> positions;
                meas.push_back( new measurements::TaggedNRankRDM<Matrix, SymmGroup>(name, lat, tag_handler, ident, fill, synchronous_meas_operators,
                                                                                    half_only, positions, bra_ckp));
            }

            else if (!name.empty()) {

                int f_ops = 0;
                bond_tag_element meas_operators;
                
                /// split op1:op2:...@p1,p2,p3,... into {op1:op2:...}, {p1,p2,p3,...}
                std::vector<std::string> value_split;
                boost::split( value_split, value, boost::is_any_of("@"));

                /// parse operators op1:op2:...
                boost::char_separator<char> sep(":");
                tokenizer corr_tokens(value_split[0], sep);
                for (tokenizer::iterator it2=corr_tokens.begin();
                     it2 != corr_tokens.end();
                     it2++)
                {
                    if (*it2 == "c_up") {
                        meas_operators.push_back(destroy_up);
                        ++f_ops;
                    }
                    else if (*it2 == "c_down") {
                        meas_operators.push_back(destroy_down);
                        ++f_ops;
                    }
                    else if (*it2 == "cdag_up") {
                        meas_operators.push_back(create_up);
                        ++f_ops;
                    }
                    else if (*it2 == "cdag_down") {
                        meas_operators.push_back(create_down);
                        ++f_ops;
                    }

                    else if (*it2 == "id" || *it2 == "Id") {
                        meas_operators.push_back(ident);
                    }
                    else if (*it2 == "Nup") {
                        meas_operators.push_back(count_up);
                    }
                    else if (*it2 == "Ndown") {
                        meas_operators.push_back(count_down);
                    }
                    else if (*it2 == "docc" || *it2 == "Nup*Ndown") {
                        meas_operators.push_back(docc);
                    }
                    else if (*it2 == "cdag_up*c_down" || *it2 == "splus") {
                        meas_operators.push_back(swap_d2u);
                    }
                    else if (*it2 == "cdag_down*c_up" || *it2 == "sminus") {
                        meas_operators.push_back(swap_u2d);
                    }

                    else if (*it2 == "cdag_up*cdag_down") {
                        meas_operators.push_back(e2d);
                    }
                    else if (*it2 == "c_up*c_down") {
                        meas_operators.push_back(d2e);
                    }

                    else if (*it2 == "cdag_up*Ndown") {
                        meas_operators.push_back(create_up_count_down);
                        ++f_ops;
                    }
                    else if (*it2 == "cdag_down*Nup") {
                        meas_operators.push_back(create_down_count_up);
                        ++f_ops;
                    }
                    else if (*it2 == "c_up*Ndown") {
                        meas_operators.push_back(destroy_up_count_down);
                        ++f_ops;
                    }
                    else if (*it2 == "c_down*Nup") {
                        meas_operators.push_back(destroy_down_count_up);
                        ++f_ops;
                    }
                    else
                        throw std::runtime_error("Unrecognized operator in correlation measurement: " 
                                                    + boost::lexical_cast<std::string>(*it2) + "\n");
                }

                if (f_ops % 2 != 0)
                    throw std::runtime_error("In " + name + ": Number of fermionic operators has to be even in correlation measurements.");

                /// parse positions p1,p2,p3,... (or `space`)
                std::vector<pos_t> positions;
                if (value_split.size() > 1) {
                    boost::char_separator<char> pos_sep(", ");
                    tokenizer pos_tokens(value_split[1], pos_sep);
                    std::transform(pos_tokens.begin(), pos_tokens.end(), std::back_inserter(positions),
                                   static_cast<pos_t (*)(std::string const&)>(boost::lexical_cast<pos_t, std::string>));
                }
                
                std::vector<scaled_bond_element> synchronous_meas_operators;
                synchronous_meas_operators.push_back(std::make_pair(meas_operators, 1));
                meas.push_back( new measurements::TaggedNRankRDM<Matrix, SymmGroup>(name, lat, tag_handler, ident, fill, synchronous_meas_operators,
                                                                                    half_only, positions));
            }
        }
        }
        return meas;
    }

private:
    Lattice const & lat;
    BaseParameters & parms;
    std::vector<Index<SymmGroup> > phys_indices;

    boost::shared_ptr<TagHandler<Matrix, SymmGroup> > tag_handler;
    // Need a vector to store operators corresponding to different irreps
    std::vector<tag_type> ident, fill,
                          create_up, create_down, destroy_up, destroy_down,
                          count_up, count_down, count_up_down, docc, e2d, d2e;

    typename SymmGroup::subcharge max_irrep;

    std::vector<op_t> generate_site_specific_ops(op_t const & op) const
    {
        PGDecorator<SymmGroup> set_symm;
        std::vector<op_t> ret;
        for (typename SymmGroup::subcharge sc=0; sc < max_irrep+1; ++sc) {
            op_t mod(set_symm(op.basis(), sc));
            for (std::size_t b = 0; b < op.n_blocks(); ++b)
                mod[b] = op[b];

            ret.push_back(mod);
        }
        return ret;
    }

    std::vector<tag_type> register_site_specific(std::vector<op_t> const & ops, tag_detail::operator_kind kind)
    {
        std::vector<tag_type> ret;
        for (typename SymmGroup::subcharge sc=0; sc < max_irrep+1; ++sc) {
            std::pair<tag_type, value_type> newtag = tag_handler->checked_register(ops[sc], kind);
            assert( newtag.first < tag_handler->size() );
            assert( std::abs(newtag.second - value_type(1.)) == value_type() );
            ret.push_back(newtag.first);
        }

        return ret;
    }
};


#include "dmrg/models/chem/2u1/model.hpp"

#endif
