#include "subst.h"

#include "mm/setmm.h"
#include "utils/utils.h"

std::map< SymTok, std::tuple< LabTok, LabTok, LabTok > > compute_equalities(const LibraryToolbox &tb) {
    return {
        { tb.get_symbol("wff"), { tb.get_label("wb"), tb.get_label("biid"), tb.get_label("wnf") } },
        { tb.get_symbol("class"), { tb.get_label("wceq"), tb.get_label("eqid"), tb.get_label("wnfc") } },
        //{ tb.get_symbol("set"), { tb.get_label(""), tb.get_label("") } },
    };
}

int subst_search_main(int argc, char *argv[]) {
    (void) argc;
    (void) argv;

    auto &data = get_set_mm();
    //auto &lib = data.lib;
    auto &tb = data.tb;

    auto &ders = tb.get_derivations();

    LabTok imp_lab = tb.get_label("wi");
    LabTok ph_lab = tb.get_label("wph");

    auto equalities = compute_equalities(tb);
    SymTok var_type = tb.get_symbol("set");

    // Find an equality symbol for each type
    /*for (const auto &der : ders) {
        auto type = der.first;
        std::cout << "Searching for a reflexive equality theorem for " << tb.resolve_symbol(type) << std::endl;
        auto ass_list = tb.list_assertions();
        const Assertion *ass;
        while ((ass = ass_list()) != nullptr) {
            if (ass->get_ess_hyps().size() != 0) {
                continue;
            }
            if (ass->get_float_hyps().size() != 1) {
                continue;
            }
            const auto &float_hyp = tb.get_parsed_sents().at(ass->get_float_hyps()[0]);
            assert(float_hyp.children.empty());
            const auto var_lab = float_hyp.label;
            if (tb.get_var_lab_to_type_sym(var_lab) != type) {
                continue;
            }
            const auto &pt = tb.get_parsed_sents().at(ass->get_thesis());
            const auto &sent = tb.get_sentence(ass->get_thesis());
            if (sent[0] != tb.get_turnstile()) {
                continue;
            }
            if (pt.children.size() != 2 || pt.children[0] != float_hyp || pt.children[1] != float_hyp) {
                continue;
            }
            std::cout << "  Found theorem " << tb.resolve_label(ass->get_thesis()) << std::endl;
        }
    }*/

    for (const auto &der : ders) {
        const auto type = der.first;
        for (const auto &der2 : der.second) {
            const auto label = der2.first;
            const auto &rule = der2.second;
            bool has_vars = false;
            for (const auto sym : rule) {
                if (ders.find(sym) != ders.end()) {
                    has_vars = true;
                }
            }
            if (!has_vars) {
                continue;
            }
            std::cout << "Considering derivation " << tb.resolve_label(label) <<" for type " << tb.resolve_symbol(type) << ", with variables:";
            for (const auto sym : rule) {
                if (ders.find(sym) != ders.end()) {
                    std::cout << " " << tb.resolve_symbol(sym);
                }
            }
            std::cout << std::endl;

            for (unsigned i = 0; i < rule.size(); i++) {
                auto it = equalities.find(rule[i]);
                if (it != equalities.end()) {
                    bool found = false;
                    bool found_strong = false;
                    std::cout << " * Search for a substitution rule for " << tb.resolve_symbol(it->first) << " in position " << i << std::endl;
                    tb.new_temp_var_frame();
                    ParsingTree< SymTok, LabTok > pt_hyp;
                    pt_hyp.label = std::get<0>(it->second);
                    pt_hyp.type = tb.get_turnstile_alias();
                    ParsingTree< SymTok, LabTok > pt_left;
                    ParsingTree< SymTok, LabTok > pt_right;
                    pt_left.label = label;
                    pt_left.type = type;
                    pt_right.label = label;
                    pt_right.type = type;
                    for (unsigned j = 0; j < rule.size(); j++) {
                        if (ders.find(rule[j]) != ders.end()) {
                            ParsingTree< SymTok, LabTok > pt_var1;
                            ParsingTree< SymTok, LabTok > pt_var2;
                            auto var1 = tb.new_temp_var(rule[j]);
                            pt_var1.label = var1.first;
                            pt_var1.type = rule[j];
                            if (i == j) {
                                auto var2 = tb.new_temp_var(rule[j]);
                                pt_var2.label = var2.first;
                                pt_var2.type = rule[j];
                                pt_hyp.children.push_back(pt_var1);
                                pt_hyp.children.push_back(pt_var2);
                            } else {
                                pt_var2 = pt_var1;
                            }
                            pt_left.children.push_back(pt_var1);
                            pt_right.children.push_back(pt_var2);
                        }
                    }
                    ParsingTree< SymTok, LabTok > pt_thesis;
                    pt_thesis.label = std::get<0>(equalities[type]);
                    pt_thesis.type = tb.get_turnstile_alias();
                    pt_thesis.children.push_back(pt_left);
                    pt_thesis.children.push_back(pt_right);

                    assert(pt_thesis.validate(tb.get_validation_rule()));
                    assert(pt_hyp.validate(tb.get_validation_rule()));
                    std::cout << "   Inference form: searching for " << tb.print_sentence(pt_thesis) << " with hypothesis " << tb.print_sentence(pt_hyp) << std::endl;
                    auto res = tb.unify_assertion({std::make_pair(tb.get_turnstile(), pt_hyp)}, std::make_pair(tb.get_turnstile(), pt_thesis));
                    if (!res.empty()) {
                        std::cout << "     Found match " << tb.resolve_label(std::get<0>(res[0])) << std::endl;
                        found = true;
                    } else {
                        std::cout << "     Found NO match..." << std::endl;
                    }

                    ParsingTree< SymTok, LabTok > pt_thm;
                    pt_thm.type = tb.get_turnstile_alias();
                    pt_thm.label = imp_lab;
                    pt_thm.children.push_back(pt_hyp);
                    pt_thm.children.push_back(pt_thesis);
                    assert(pt_thm.validate(tb.get_validation_rule()));
                    std::cout << "   Inference form: searching for " << tb.print_sentence(pt_thm) << std::endl;
                    res = tb.unify_assertion({}, std::make_pair(tb.get_turnstile(), pt_thm));
                    if (!res.empty()) {
                        std::cout << "     Found match " << tb.resolve_label(std::get<0>(res[0])) << std::endl;
                        found = true;
                        found_strong = true;
                    } else {
                        std::cout << "     Found NO match..." << std::endl;
                    }

                    ParsingTree< SymTok, LabTok > pt_ph;
                    pt_ph.type = tb.get_turnstile_alias();
                    pt_ph.label = ph_lab;
                    ParsingTree< SymTok, LabTok > pt_hypd;
                    pt_hypd.type = tb.get_turnstile_alias();
                    pt_hypd.label = imp_lab;
                    pt_hypd.children.push_back(pt_ph);
                    pt_hypd.children.push_back(pt_hyp);
                    ParsingTree< SymTok, LabTok > pt_thesisd;
                    pt_thesisd.type = tb.get_turnstile_alias();
                    pt_thesisd.label = imp_lab;
                    pt_thesisd.children.push_back(pt_ph);
                    pt_thesisd.children.push_back(pt_thesis);
                    assert(pt_hypd.validate(tb.get_validation_rule()));
                    assert(pt_thesisd.validate(tb.get_validation_rule()));
                    std::cout << "   Deduction form: searching for " << tb.print_sentence(pt_thesisd) << " with hypothesis " << tb.print_sentence(pt_hypd) << std::endl;
                    res = tb.unify_assertion({std::make_pair(tb.get_turnstile(), pt_hypd)}, std::make_pair(tb.get_turnstile(), pt_thesisd));
                    if (!res.empty()) {
                        std::cout << "     Found match " << tb.resolve_label(std::get<0>(res[0])) << std::endl;
                        if (!tb.get_assertion(std::get<0>(res[0])).get_mand_dists().empty()) {
                            std::cout << "     It has DISTINCT VARIABLES provisions!" << std::endl;
                        }
                        found = true;
                        found_strong = true;
                    } else {
                        std::cout << "     Found NO match..." << std::endl;
                    }

                    if (!found) {
                        std::cout << "   NO RULE FOUND" << std::endl;
                    } else if (!found_strong) {
                        std::cout << "   ONLY WEAK RULES WERE FOUND" << std::endl;
                    } else {
                        std::cout << "   Found strong rules!" << std::endl;
                    }

                    tb.release_temp_var_frame();
                } else if (rule[i] == var_type) {
                    std::cout << " * Search for a not-free rule for " << tb.resolve_symbol(rule[i]) << " in position " << i << std::endl;
                    tb.new_temp_var_frame();
                    ParsingTree< SymTok, LabTok > pt_body;
                    pt_body.label = label;
                    pt_body.type = type;
                    ParsingTree< SymTok, LabTok > pt_var;
                    pt_var.type = var_type;
                    for (unsigned j = 0; j < rule.size(); j++) {
                        if (ders.find(rule[j]) != ders.end()) {
                            ParsingTree< SymTok, LabTok > pt_var2;
                            auto var = tb.new_temp_var(rule[j]);
                            pt_var2.label = var.first;
                            pt_var2.type = rule[j];
                            pt_body.children.push_back(pt_var2);
                            if (i == j) {
                                pt_var.label = var.first;
                            }
                        }
                    }
                    ParsingTree< SymTok, LabTok > pt_nf;
                    pt_nf.type = tb.get_turnstile_alias();
                    pt_nf.label = std::get<2>(equalities.at(type));
                    pt_nf.children.push_back(pt_var);
                    pt_nf.children.push_back(pt_body);
                    assert(pt_nf.validate(tb.get_validation_rule()));
                    std::cout << "   Searching for " << tb.print_sentence(pt_nf) << std::endl;
                    auto res = tb.unify_assertion({}, std::make_pair(tb.get_turnstile(), pt_nf));
                    if (!res.empty()) {
                        std::cout << "     Found match " << tb.resolve_label(std::get<0>(res[0])) << std::endl;
                        if (!tb.get_assertion(std::get<0>(res[0])).get_mand_dists().empty()) {
                            std::cout << "     It has DISTINCT VARIABLES provisions!" << std::endl;
                        }
                    } else {
                        std::cout << "     Found NO match..." << std::endl;
                    }
                    tb.release_temp_var_frame();
                }
            }
        }
    }

    return 0;
}
static_block {
    register_main_function("subst_search", subst_search_main);
}

std::map< LabTok, std::tuple< LabTok, std::vector< LabTok >, std::vector< LabTok >, ParsingTree< SymTok, LabTok > > > compute_defs(const LibraryToolbox &tb) {
    auto &ders = tb.get_derivations();

    auto equalities = compute_equalities(tb);

    std::set< LabTok > primitive_labels = {
        tb.get_label("cv"),     // just an adapter
        tb.get_label("wn"),     // logic negation
        tb.get_label("wi"),     // logic implication
        tb.get_label("wal"),    // universal quantification
    };

    std::map< LabTok, LabTok > special_labels = {
        //{ tb.get_label("wb"), tb.get_label("dfbi1") },      // logic biimplication
        { tb.get_label("cab"), tb.get_label("df-clab") },   // class abstraction
        //{ tb.get_label("wceq"), tb.get_label("dfcleq") },   // class equality
        { tb.get_label("wcel"), tb.get_label("df-clel") },  // class membership
    };

    std::map< LabTok, std::tuple< LabTok, std::vector< LabTok >, std::vector< LabTok >, ParsingTree< SymTok, LabTok > > > defs;
    for (const auto &der : ders) {
        const auto type = der.first;
        for (const auto &der2 : der.second) {
            const auto label = der2.first;
            const auto &rule = der2.second;
            if (rule.size() == 1 && tb.get_standard_is_var_sym()(rule[0])) {
                //std::cout << "   Ignoring because it is a variable" << std::endl;
                continue;
            }
            if (primitive_labels.find(label) != primitive_labels.end()) {
                continue;
            }
            if (special_labels.find(label) != special_labels.end()) {
                continue;
            }
            //std::cout << "Considering derivation " << tb.resolve_label(label) <<" for type " << tb.resolve_symbol(type) << " with rule " << tb.print_sentence(rule) << std::endl;

            tb.new_temp_var_frame();
            ParsingTree< SymTok, LabTok > pt_right;
            pt_right.type = type;
            pt_right.label = tb.new_temp_var(type).first;
            ParsingTree< SymTok, LabTok > pt_left;
            pt_left.type = type;
            pt_left.label = label;
            for (const auto sym : rule) {
                if (ders.find(sym) != ders.end()) {
                    auto temp_var = tb.new_temp_var(sym);
                    ParsingTree< SymTok, LabTok > pt_var;
                    pt_var.type = sym;
                    pt_var.label = temp_var.first;
                    pt_left.children.push_back(pt_var);
                }
            }
            ParsingTree< SymTok, LabTok > pt_def;
            pt_def.type = tb.get_turnstile_alias();
            pt_def.label = std::get<0>(equalities.at(type));
            pt_def.children.push_back(pt_left);
            pt_def.children.push_back(pt_right);
            assert(pt_def.validate(tb.get_validation_rule()));

            //std::cout << "   Searching for " << tb.print_sentence(pt_def) << std::endl;
            LabTok res = {};
            auto assertions_gen = tb.list_assertions();
            std::vector< LabTok > vars;
            std::vector< LabTok > fresh_vars;
            ParsingTree< SymTok, LabTok > def_body;
            while (true) {
                const Assertion *ass2 = assertions_gen();
                // Some notations have hardcoded defaults
                if (label == tb.get_label("wb")) {
                    ass2 = &tb.get_assertion(tb.get_label("dfbi1"));
                }
                if (label == tb.get_label("wceq")) {
                    ass2 = &tb.get_assertion(tb.get_label("dfcleq"));
                }
                if (ass2 == nullptr) {
                    break;
                }
                const Assertion &ass = *ass2;
                /*if (ass.is_usage_disc()) {
                    continue;
                }*/
                if (!ass.get_ess_hyps().empty()) {
                    continue;
                }
                if (tb.get_sentence(ass.get_thesis())[0] != tb.get_turnstile()) {
                    continue;
                }
                const auto &pt = tb.get_parsed_sents().at(ass.get_thesis());
                if (pt.label != pt_def.label) {
                    continue;
                }
                if (pt.children.at(0).label != pt_left.label) {
                    continue;
                }
                res = ass.get_thesis();
                def_body = pt.children.at(1);
                for (const auto &child : pt.children.at(0).children) {
                    assert(child.children.empty());
                    assert(tb.get_standard_is_var()(child.label));
                    vars.push_back(child.label);
                }
                std::set< LabTok > vars_set(vars.begin(), vars.end());
                std::set< LabTok > body_vars;
                collect_variables(def_body, tb.get_standard_is_var(), body_vars);
                std::set_difference(body_vars.begin(), body_vars.end(), vars_set.begin(), vars_set.end(), std::back_inserter(fresh_vars));
                break;
            }
            if (res != LabTok{}) {
                auto label_str = tb.resolve_label(res);
                //std::cout << "     Found match " << label_str << std::endl;
                std::string def_prefix = "df-";
                if (label_str.size() < def_prefix.size() || !std::equal(def_prefix.begin(), def_prefix.end(), label_str.begin())) {
                    //std::cout << "    STRANGE! It does not appear to be a definition!" << std::endl;
                }
                defs[label] = std::make_tuple(res, vars, fresh_vars, def_body);
            } else {
                //std::cout << "     Found NO match..." << std::endl;
            }

            tb.release_temp_var_frame();
        }
    }

    return defs;
}

ParsingTree< SymTok, LabTok > subst_defs(const ParsingTree< SymTok, LabTok > &pt, const LibraryToolbox &tb, const std::map< LabTok, std::tuple< LabTok, std::vector< LabTok >, std::vector< LabTok >, ParsingTree< SymTok, LabTok > > > &defs) {
    auto it = defs.find(pt.label);
    if (it != defs.end()) {
        // In most cases we can use the general procedure, substituting the definition
        auto &vars = std::get<1>(it->second);
        auto &fresh_vars = std::get<2>(it->second);
        auto &def_body = std::get<3>(it->second);
        assert(vars.size() == pt.children.size());
        SubstMap< SymTok, LabTok > subst_map;
        for (size_t i = 0; i < vars.size(); i++) {
            subst_map[vars[i]] = pt.children[i];
        }
        for (const auto var : fresh_vars) {
            auto &new_var = subst_map[var];
            new_var.type = tb.get_var_lab_to_type_sym(var);
            new_var.label = tb.new_temp_var(new_var.type).first;
        }
        ParsingTree< SymTok, LabTok > ret = substitute(def_body, tb.get_standard_is_var(), subst_map);
        return subst_defs(ret, tb, defs);
    } else if (pt.label == tb.get_label("wcel")) {
        // Class/set membership requires special handling
        assert(pt.children.size() == 2);

        // If the left-hand operand is a class which is not a set, then first we use df-clel, to reduce to the case where it is a set
        if (pt.children.at(0).label != tb.get_label("cv")) {
            auto pt_clel = tb.get_parsed_sents().at(tb.get_label("df-clel"));
            SubstMap< SymTok, LabTok > subst_map;
            subst_map[pt_clel.children.at(0).children.at(0).label] = pt.children.at(0);
            subst_map[pt_clel.children.at(0).children.at(1).label] = pt.children.at(1);
            ParsingTree< SymTok, LabTok > ret = substitute(pt_clel.children.at(1), tb.get_standard_is_var(), subst_map);
            return subst_defs(ret, tb, defs);
        }

        // Now we can assume the left-hand operand is a set, where no substitution can be made.
        // We thus continue substituting in the right-hand side.
        auto pt_right = subst_defs(pt.children.at(1), tb, defs);

        // If we obtained a class abstraction, then we can use df-clab
        if (pt_right.label == tb.get_label(("cab"))) {
            auto pt_clab = tb.get_parsed_sents().at(tb.get_label("df-clab"));
            SubstMap< SymTok, LabTok > subst_map;
            subst_map[pt_clab.children.at(0).children.at(0).children.at(0).label] = pt.children.at(0).children.at(0);
            subst_map[pt_clab.children.at(0).children.at(1).children.at(0).label] = pt_right.children.at(0);
            subst_map[pt_clab.children.at(0).children.at(1).children.at(1).label] = pt_right.children.at(1);
            ParsingTree< SymTok, LabTok > ret = substitute(pt_clab.children.at(1), tb.get_standard_is_var(), subst_map);
            return subst_defs(ret, tb, defs);
        }

        // In all the other cases we have either a set (in which case the notation is primitive) or a class variable (and we cannot do anything about it)
        ParsingTree< SymTok, LabTok > ret;
        ret.type = pt.type;
        ret.label = pt.label;
        ret.children.push_back(pt.children.at(0));
        ret.children.push_back(pt_right);
        return ret;
    } else {
        // In the end, if we are working with a primitive notation, then we just recur on children
        ParsingTree< SymTok, LabTok > ret;
        ret.type = pt.type;
        ret.label = pt.label;
        ret.children = vector_map(pt.children.begin(), pt.children.end(), [&tb,&defs](const auto &x) { return subst_defs(x, tb, defs); });
        return ret;
    }
}

int find_defs_main(int argc, char *argv[]) {
    (void) argc;
    (void) argv;

    auto &data = get_set_mm();
    //auto &lib = data.lib;
    auto &tb = data.tb;

    auto defs = compute_defs(tb);

    auto pt = tb.parse_sentence(tb.read_sentence("wff E. x ( x = _V /\\ A e. B /\\ ( ph <-> ps ) )"));
    //auto pt = tb.parse_sentence(tb.read_sentence("wff ( ph /\\ ps /\\ ch )"));
    pt.validate(tb.get_validation_rule());
    auto pt_defs = subst_defs(pt, tb, defs);
    pt_defs.validate(tb.get_validation_rule());
    std::cout << tb.print_sentence(pt) << std::endl << "becomes" << std::endl << tb.print_sentence(pt_defs) << std::endl;

    return 0;
}
static_block {
    register_main_function("find_defs", find_defs_main);
}
