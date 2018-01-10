
#include <boost/filesystem/fstream.hpp>

#include "toolbox.h"
#include "utils/utils.h"
#include "old/unification.h"
#include "parsing/unif.h"
#include "parsing/earley.h"
#include "reader.h"

using namespace std;

ostream &operator<<(ostream &os, const SentencePrinter &sp)
{
    bool first = true;
    if (sp.style == SentencePrinter::STYLE_ALTHTML) {
        // TODO - HTML fixing could be done just once
        os << fix_htmlcss_for_qt(sp.tb.get_addendum().get_htmlcss());
        os << "<SPAN " << sp.tb.get_addendum().get_htmlfont() << ">";
    }
    Sentence sent2;
    const Sentence *sentp = NULL;
    if (sp.sent != NULL) {
        sentp = sp.sent;
    } else if (sp.pt != NULL) {
        sent2 = sp.tb.reconstruct_sentence(*sp.pt);
        sentp = &sent2;
    } else if (sp.pt2 != NULL) {
        sent2 = sp.tb.reconstruct_sentence(pt2_to_pt(*sp.pt2));
        sentp = &sent2;
    } else {
        assert("Should never arrive here" == NULL);
    }
    const Sentence &sent = *sentp;
    for (auto &tok : sent) {
        if (first) {
            first = false;
        } else {
            os << string(" ");
        }
        if (sp.style == SentencePrinter::STYLE_PLAIN) {
            os << sp.tb.resolve_symbol(tok);
        } else if (sp.style == SentencePrinter::STYLE_HTML) {
            os << sp.tb.get_addendum().get_htmldef(tok);
        } else if (sp.style == SentencePrinter::STYLE_ALTHTML) {
            os << sp.tb.get_addendum().get_althtmldef(tok);
        } else if (sp.style == SentencePrinter::STYLE_LATEX) {
            os << sp.tb.get_addendum().get_latexdef(tok);
        } else if (sp.style == SentencePrinter::STYLE_ANSI_COLORS_SET_MM) {
            LabTok label = sp.tb.get_var_sym_to_lab().at(tok);
            if (sp.tb.get_standard_is_var()(label)) {
                string type_str = sp.tb.resolve_symbol(sp.tb.get_var_sym_to_type_sym().at(tok));
                if (type_str == "set") {
                    os << "\033[91m";
                } else if (type_str == "class") {
                    os << "\033[35m";
                } else if (type_str == "wff") {
                    os << "\033[94m";
                }
            } else {
                //os << "\033[37m";
            }
            os << sp.tb.resolve_symbol(tok);
            os << "\033[39m";
        }
    }
    if (sp.style == SentencePrinter::STYLE_ALTHTML) {
        os << "</SPAN>";
    }
    return os;
}

ostream &operator<<(ostream &os, const ProofPrinter &sp)
{
    bool first = true;
    for (auto &label : sp.proof) {
        if (sp.only_assertions && !(sp.tb.get_assertion(label).is_valid() && sp.tb.get_sentence(label).at(0) == sp.tb.get_turnstile())) {
            continue;
        }
        if (first) {
            first = false;
        } else {
            os << string(" ");
        }
        os << sp.tb.resolve_label(label);
    }
    return os;
}

LibraryToolbox::LibraryToolbox(const ExtendedLibrary &lib, string turnstile, bool compute, shared_ptr<ToolboxCache> cache) :
    lib(lib), turnstile(lib.get_symbol(turnstile)), turnstile_alias(lib.get_parsing_addendum().get_syntax().at(this->turnstile)),
    type_labels(lib.get_final_stack_frame().types), type_labels_set(lib.get_final_stack_frame().types_set),
    cache(cache), temp_syms(lib.get_symbols_num()+1), temp_labs(lib.get_labels_num()+1)
{
    assert(this->lib.is_immutable());
    this->standard_is_var = [this](LabTok x)->bool {
        /*const auto &types_set = this->get_types_set();
        if (types_set.find(x) == types_set.end()) {
            return false;
        }
        return !this->is_constant(this->get_sentence(x).at(1));*/
        if (x >= this->lib.get_labels_num()) {
            return true;
        }
        return this->get_is_var_by_type()[x];
    };
    this->standard_is_var_sym = [this](SymTok x)->bool {
        if (x >= this->lib.get_symbols_num()) {
            return true;
        }
        return !this->lib.is_constant(x);
    };
    if (compute) {
        this->compute_everything();
    }
}

LibraryToolbox::~LibraryToolbox()
{
    delete this->parser;
}

void LibraryToolbox::set_cache(std::shared_ptr<ToolboxCache> cache)
{
    this->cache = cache;
}

std::vector<SymTok> LibraryToolbox::substitute(const std::vector<SymTok> &orig, const std::unordered_map<SymTok, std::vector<SymTok> > &subst_map) const
{
    vector< SymTok > ret;
    for (auto it = orig.begin(); it != orig.end(); it++) {
        const SymTok &tok = *it;
        if (this->is_constant(tok)) {
            ret.push_back(tok);
        } else {
            const vector< SymTok > &subst = subst_map.at(tok);
            copy(subst.begin(), subst.end(), back_inserter(ret));
        }
    }
    return ret;
}

// Computes second o first (map composition)
std::unordered_map<SymTok, std::vector<SymTok> > LibraryToolbox::compose_subst(const std::unordered_map<SymTok, std::vector<SymTok> > &first, const std::unordered_map<SymTok, std::vector<SymTok> > &second) const
{
    throw "Buggy implementation, do not use";
    std::unordered_map< SymTok, std::vector< SymTok > > ret;
    for (auto &first_pair : first) {
        auto res = ret.insert(make_pair(first_pair.first, this->substitute(first_pair.second, second)));
        assert(res.second);
    }
    return ret;
}

static vector< size_t > invert_perm(const vector< size_t > &perm) {
    vector< size_t > ret(perm.size());
    for (size_t i = 0; i < perm.size(); i++) {
        ret[perm[i]] = i;
    }
    return ret;
}

static void type_proving_helper_unwind_tree(const ParsingTree< SymTok, LabTok > &tree, ProofEngine &engine, const Library &lib, const std::unordered_map<LabTok, const Prover*> &var_provers) {
    // We need to sort children according to their order as floating hypotheses of this assertion
    // If this is not an assertion, then there are no children
    const Assertion &ass = lib.get_assertion(tree.label);
    auto it = var_provers.find(tree.label);
    if (ass.is_valid()) {
        assert(it == var_provers.end());
        unordered_map< SymTok, const ParsingTree< SymTok, LabTok >* > children;
        auto it2 = tree.children.begin();
        for (auto &tok : lib.get_sentence(tree.label)) {
            if (!lib.is_constant(tok)) {
                children[tok] = &(*it2);
                it2++;
            }
        }
        assert(it2 == tree.children.end());
        for (auto &hyp : ass.get_float_hyps()) {
            SymTok tok = lib.get_sentence(hyp).at(1);
            type_proving_helper_unwind_tree(*children.at(tok), engine, lib, var_provers);
        }
        engine.process_label(tree.label);
    } else {
        assert(tree.children.size() == 0);
        if (it == var_provers.end()) {
            engine.process_label(tree.label);
        } else {
            (*it->second)(engine);
        }
    }
}

bool LibraryToolbox::type_proving_helper(const std::vector<SymTok> &type_sent, ProofEngine &engine, const std::unordered_map<SymTok, Prover> &var_provers) const
{
    SymTok type = type_sent[0];
    ParsingTree tree = this->parse_sentence(type_sent.begin()+1, type_sent.end(), type);
    if (tree.label == 0) {
        return false;
    } else {
        std::unordered_map<LabTok, const Prover*> lab_var_provers;
        for (const auto &x : var_provers) {
            lab_var_provers.insert(make_pair(this->get_var_sym_to_lab().at(x.first), &x.second));
        }
        type_proving_helper_unwind_tree(tree, engine, *this, lab_var_provers);
        return true;
    }
}

bool LibraryToolbox::type_proving_helper(const ParsingTree2<SymTok, LabTok> &pt, ProofEngine &engine, const std::unordered_map<LabTok, Prover> &var_provers) const
{
    std::unordered_map<LabTok, const Prover*> var_provers_ptr;
    for (const auto &x : var_provers) {
        var_provers_ptr.insert(make_pair(x.first, &x.second));
    }
    type_proving_helper_unwind_tree(pt2_to_pt(pt), engine, *this, var_provers_ptr);
    return true;
}

const std::unordered_map<SymTok, std::vector<std::pair<LabTok, std::vector<SymTok> > > > &LibraryToolbox::get_derivations()
{
    if (!this->derivations_computed) {
        this->compute_derivations();
    }
    return this->derivations;
}

const std::unordered_map<SymTok, std::vector<std::pair<LabTok, std::vector<SymTok> > > > &LibraryToolbox::get_derivations() const
{
    if (!this->derivations_computed) {
        throw MMPPException("computation required on const object");
    }
    return this->derivations;
}

void LibraryToolbox::compute_ders_by_label()
{
    this->ders_by_label = compute_derivations_by_label(this->get_derivations());
    this->ders_by_label_computed = true;
}

const std::unordered_map<LabTok, std::pair<SymTok, std::vector<SymTok> > > &LibraryToolbox::get_ders_by_label()
{
    if (!this->ders_by_label_computed) {
        this->compute_ders_by_label();
    }
    return this->ders_by_label;
}

const std::unordered_map<LabTok, std::pair<SymTok, std::vector<SymTok> > > &LibraryToolbox::get_ders_by_label() const
{
    if (!this->ders_by_label_computed) {
        throw MMPPException("computation required on const object");
    }
    return this->ders_by_label;
}

std::vector<SymTok> LibraryToolbox::reconstruct_sentence(const ParsingTree<SymTok, LabTok> &pt, SymTok first_sym)
{
    return ::reconstruct_sentence(pt, this->get_derivations(), this->get_ders_by_label(), first_sym);
}

std::vector<SymTok> LibraryToolbox::reconstruct_sentence(const ParsingTree<SymTok, LabTok> &pt, SymTok first_sym) const
{
    return ::reconstruct_sentence(pt, this->get_derivations(), this->get_ders_by_label(), first_sym);
}

void LibraryToolbox::compute_vars()
{
    this->sentence_vars.emplace_back();
    for (size_t i = 1; i < this->get_parsed_sents2().size(); i++) {
        const auto &pt = this->get_parsed_sents2()[i];
        set< LabTok > vars;
        collect_variables2(pt, this->get_standard_is_var(), vars);
        this->sentence_vars.push_back(vars);
    }
    for (const auto &ass : this->lib.get_assertions()) {
        if (!ass.is_valid()) {
            this->assertion_const_vars.emplace_back();
            this->assertion_unconst_vars.emplace_back();
            continue;
        }
        const auto &thesis_vars = this->sentence_vars[ass.get_thesis()];
        set< LabTok > hyps_vars;
        for (const auto hyp_tok : ass.get_ess_hyps()) {
            const auto &hyp_vars = this->sentence_vars[hyp_tok];
            hyps_vars.insert(hyp_vars.begin(), hyp_vars.end());
        }
        this->assertion_const_vars.push_back(thesis_vars);
        auto &unconst_vars = this->assertion_unconst_vars.emplace_back();
        set_difference(hyps_vars.begin(), hyps_vars.end(), thesis_vars.begin(), thesis_vars.end(), inserter(unconst_vars, unconst_vars.begin()));
    }
    this->vars_computed = true;
}

const std::vector< std::set< LabTok > > &LibraryToolbox::get_sentence_vars()
{
    if (!this->vars_computed) {
        this->compute_vars();
    }
    return this->sentence_vars;
}

const std::vector< std::set< LabTok > > &LibraryToolbox::get_sentence_vars() const
{
    if (!this->vars_computed) {
        throw MMPPException("computation required on a const object");
    }
    return this->sentence_vars;
}

const std::vector< std::set< LabTok > > &LibraryToolbox::get_assertion_unconst_vars()
{
    if (!this->vars_computed) {
        this->compute_vars();
    }
    return this->assertion_unconst_vars;
}

const std::vector< std::set< LabTok > > &LibraryToolbox::get_assertion_unconst_vars() const
{
    if (!this->vars_computed) {
        throw MMPPException("computation required on a const object");
    }
    return this->assertion_unconst_vars;
}

const std::vector< std::set< LabTok > > &LibraryToolbox::get_assertion_const_vars()
{
    if (!this->vars_computed) {
        this->compute_vars();
    }
    return this->assertion_const_vars;
}

const std::vector< std::set< LabTok > > &LibraryToolbox::get_assertion_const_vars() const
{
    if (!this->vars_computed) {
        throw MMPPException("computation required on a const object");
    }
    return this->assertion_const_vars;
}

Prover LibraryToolbox::build_prover(const std::vector<Sentence> &templ_hyps, const Sentence &templ_thesis, const std::unordered_map<string, Prover> &types_provers, const std::vector<Prover> &hyps_provers) const
{
    auto res = this->unify_assertion(templ_hyps, templ_thesis, true);
    if (res.empty()) {
        cerr << string("Could not find the template assertion: ") + this->print_sentence(templ_thesis).to_string() << endl;
    }
    assert_or_throw< MMPPException >(!res.empty(), string("Could not find the template assertion: ") + this->print_sentence(templ_thesis).to_string());
    const auto &res1 = res[0];
    return [=](ProofEngine &engine){
        RegisteredProverInstanceData inst_data;
        inst_data.valid = true;
        inst_data.label = get<0>(res1);
        const vector< size_t > &perm = get<1>(res1);
        inst_data.perm_inv = invert_perm(perm);
        inst_data.ass_map = get<2>(res1);
        return this->proving_helper(inst_data, types_provers, hyps_provers, engine);
    };
}

bool LibraryToolbox::proving_helper(const RegisteredProverInstanceData &inst_data, const std::unordered_map<string, Prover> &types_provers, const std::vector<Prover> &hyps_provers, ProofEngine &engine) const {
    const Assertion &ass = this->get_assertion(inst_data.label);
    assert(ass.is_valid());

    std::unordered_map<SymTok, Prover> types_provers_sym;
    for (auto &type_pair : types_provers) {
        auto res = types_provers_sym.insert(make_pair(this->get_symbol(type_pair.first), type_pair.second));
        assert(res.second);
    }

    engine.checkpoint();

    // Compute floating hypotheses
    for (auto &hyp : ass.get_float_hyps()) {
        bool res = this->type_proving_helper(this->substitute(this->get_sentence(hyp), inst_data.ass_map), engine, types_provers_sym);
        if (!res) {
            cerr << "Applying " << inst_data.label_str << " a floating hypothesis failed..." << endl;
            engine.rollback();
            return false;
        }
    }

    // Compute essential hypotheses
    for (size_t i = 0; i < ass.get_ess_hyps().size(); i++) {
        bool res = hyps_provers[inst_data.perm_inv[i]](engine);
        if (!res) {
            cerr << "Applying " << inst_data.label_str << " an essential hypothesis failed..." << endl;
            engine.rollback();
            return false;
        }
    }

    // Finally add this assertion's label
    try {
        engine.process_label(ass.get_thesis());
    } catch (const ProofException &e) {
        this->dump_proof_exception(e, cerr);
        engine.rollback();
        return false;
    }

    engine.commit();
    return true;
}

void LibraryToolbox::create_temp_var(SymTok type_sym)
{
    // Create names and variables
    assert(this->is_constant(type_sym));
    size_t idx = ++this->temp_idx[type_sym];
    string sym_name = this->resolve_symbol(type_sym) + to_string(idx);
    assert(this->get_symbol(sym_name) == 0);
    string lab_name = "temp" + sym_name;
    assert(this->get_label(lab_name) == 0);
    SymTok sym = this->temp_syms.create(sym_name);
    LabTok lab = this->temp_labs.create(lab_name);
    this->temp_types[lab] = { type_sym, sym };

    // Add the variables to a few structures
    this->type_labels.push_back(lab);
    this->type_labels_set.insert(lab);
    this->derivations.at(type_sym).push_back(pair< LabTok, vector< SymTok > >(lab, { sym }));
    this->ders_by_label[lab] = pair< SymTok, vector< SymTok > >(type_sym, { sym });
    enlarge_and_set(this->var_lab_to_sym, lab) = sym;
    enlarge_and_set(this->var_sym_to_lab, sym) = lab;
    enlarge_and_set(this->var_lab_to_type_sym, lab) = type_sym;
    enlarge_and_set(this->var_sym_to_type_sym, sym) = type_sym;

    // And insert it to the free list
    this->free_temp_vars[type_sym].push_back(make_pair(lab, sym));
}

std::pair<LabTok, SymTok> LibraryToolbox::new_temp_var(SymTok type_sym)
{
    if (this->free_temp_vars[type_sym].empty()) {
        this->create_temp_var(type_sym);
    }
    auto ret = this->free_temp_vars[type_sym].back();
    this->used_temp_vars[type_sym].push_back(ret);
    this->free_temp_vars[type_sym].pop_back();
    return ret;
}

void LibraryToolbox::new_temp_var_frame()
{
    std::map< SymTok, size_t > x;
    for (const auto &v : this->used_temp_vars) {
        x[v.first] = v.second.size();
    }
    this->temp_vars_stack.push_back(x);
}

void LibraryToolbox::release_temp_var_frame()
{
    const auto &stack_pos = this->temp_vars_stack.back();
    for (auto &v : this->used_temp_vars) {
        SymTok type_sym = v.first;
        auto &used_vars = v.second;
        auto &free_vars = this->free_temp_vars.at(type_sym);
        auto it = stack_pos.find(type_sym);
        size_t pos = 0;
        if (it != stack_pos.end()) {
            pos = it->second;
        }
        for (auto it = used_vars.begin()+pos; it != used_vars.end(); it++) {
            free_vars.push_back(*it);
        }
        used_vars.resize(pos);
    }
    this->temp_vars_stack.pop_back();
}

/*void LibraryToolbox::release_all_temp_vars()
{
    for (auto &v : this->used_temp_vars) {
        for (const auto &p : v.second) {
            this->free_temp_vars[v.first].push_back(p);
        }
    }
    this->used_temp_vars.clear();
}*/

// FIXME Deduplicate with refresh_parsing_tree()
std::pair<std::vector<ParsingTree<SymTok, LabTok> >, ParsingTree<SymTok, LabTok> > LibraryToolbox::refresh_assertion(const Assertion &ass)
{
    // Collect all variables
    std::set< LabTok > var_labs;
    const auto &is_var = this->get_standard_is_var();
    const ParsingTree< SymTok, LabTok > &thesis_pt = this->get_parsed_sents().at(ass.get_thesis());
    collect_variables(thesis_pt, is_var, var_labs);
    for (const auto hyp : ass.get_ess_hyps()) {
        const ParsingTree< SymTok, LabTok > &hyp_pt = this->get_parsed_sents().at(hyp);
        collect_variables(hyp_pt, is_var, var_labs);
    }

    // Build a substitution map
    SubstMap< SymTok, LabTok > subst = this->build_refreshing_subst_map(var_labs);

    // Substitute and return
    ParsingTree< SymTok, LabTok > thesis_new_pt = ::substitute(thesis_pt, is_var, subst);
    vector< ParsingTree< SymTok, LabTok > > hyps_new_pts;
    for (const auto hyp : ass.get_ess_hyps()) {
        const ParsingTree< SymTok, LabTok > &hyp_pt = this->get_parsed_sents().at(hyp);
        hyps_new_pts.push_back(::substitute(hyp_pt, is_var, subst));
    }
    return make_pair(hyps_new_pts, thesis_new_pt);
}

std::pair<std::vector<ParsingTree2<SymTok, LabTok> >, ParsingTree2<SymTok, LabTok> > LibraryToolbox::refresh_assertion2(const Assertion &ass)
{
    // Collect all variables
    std::set< LabTok > var_labs;
    const auto &is_var = this->get_standard_is_var();
    const ParsingTree2< SymTok, LabTok > &thesis_pt = this->get_parsed_sents2().at(ass.get_thesis());
    collect_variables2(thesis_pt, is_var, var_labs);
    for (const auto hyp : ass.get_ess_hyps()) {
        const ParsingTree2< SymTok, LabTok > &hyp_pt = this->get_parsed_sents2().at(hyp);
        collect_variables2(hyp_pt, is_var, var_labs);
    }

    // Build a substitution map
    SimpleSubstMap2< SymTok, LabTok > subst = this->build_refreshing_subst_map2(var_labs);

    // Substitute and return
    ParsingTree2< SymTok, LabTok > thesis_new_pt = ::substitute2_simple(thesis_pt, is_var, subst);
    vector< ParsingTree2< SymTok, LabTok > > hyps_new_pts;
    for (const auto hyp : ass.get_ess_hyps()) {
        const ParsingTree2< SymTok, LabTok > &hyp_pt = this->get_parsed_sents2().at(hyp);
        hyps_new_pts.push_back(::substitute2_simple(hyp_pt, is_var, subst));
    }
    return make_pair(hyps_new_pts, thesis_new_pt);
}

ParsingTree<SymTok, LabTok> LibraryToolbox::refresh_parsing_tree(const ParsingTree<SymTok, LabTok> &pt)
{
    // Collect all variables
    std::set< LabTok > var_labs;
    const auto &is_var = this->get_standard_is_var();
    collect_variables(pt, is_var, var_labs);

    // Build a substitution map
    SubstMap< SymTok, LabTok > subst = this->build_refreshing_subst_map(var_labs);

    // Substitute and return
    return ::substitute(pt, is_var, subst);
}

ParsingTree2<SymTok, LabTok> LibraryToolbox::refresh_parsing_tree2(const ParsingTree2<SymTok, LabTok> &pt)
{
    // Collect all variables
    std::set< LabTok > var_labs;
    const auto &is_var = this->get_standard_is_var();
    collect_variables2(pt, is_var, var_labs);

    // Build a substitution map
    SimpleSubstMap2< SymTok, LabTok > subst = this->build_refreshing_subst_map2(var_labs);

    // Substitute and return
    return ::substitute2_simple(pt, is_var, subst);
}

SubstMap<SymTok, LabTok> LibraryToolbox::build_refreshing_subst_map(const std::set<LabTok> &vars)
{
    SubstMap< SymTok, LabTok > subst;
    for (const auto var : vars) {
        SymTok type_sym = this->get_sentence(var).at(0);
        SymTok new_sym;
        LabTok new_lab;
        tie(new_lab, new_sym) = this->new_temp_var(type_sym);
        ParsingTree< SymTok, LabTok > new_pt;
        new_pt.label = new_lab;
        new_pt.type = type_sym;
        subst[var] = new_pt;
    }
    return subst;
}

SimpleSubstMap2<SymTok, LabTok> LibraryToolbox::build_refreshing_subst_map2(const std::set<LabTok> &vars)
{
    SimpleSubstMap2< SymTok, LabTok > subst;
    for (const auto var : vars) {
        SymTok type_sym = this->get_sentence(var).at(0);
        LabTok new_lab;
        tie(new_lab, ignore) = this->new_temp_var(type_sym);
        subst[var] = new_lab;
    }
    return subst;
}

SubstMap2<SymTok, LabTok> LibraryToolbox::build_refreshing_full_subst_map2(const std::set<LabTok> &vars)
{
    return subst_to_subst2(this->build_refreshing_subst_map(vars));
}

SymTok LibraryToolbox::get_symbol(string s) const
{
    auto res = this->lib.get_symbol(s);
    if (res != 0) {
        return res;
    }
    return this->temp_syms.get(s);
}

LabTok LibraryToolbox::get_label(string s) const
{
    auto res = this->lib.get_label(s);
    if (res != 0) {
        return res;
    }
    return this->temp_labs.get(s);
}

string LibraryToolbox::resolve_symbol(SymTok tok) const
{
    auto res = this->lib.resolve_symbol(tok);
    if (res != "") {
        return res;
    }
    return this->temp_syms.resolve(tok);
}

string LibraryToolbox::resolve_label(LabTok tok) const
{
    auto res = this->lib.resolve_label(tok);
    if (res != "") {
        return res;
    }
    return this->temp_labs.resolve(tok);
}

size_t LibraryToolbox::get_symbols_num() const
{
    return this->lib.get_symbols_num() + this->temp_syms.size();
}

size_t LibraryToolbox::get_labels_num() const
{
    return this->lib.get_labels_num() + this->temp_labs.size();
}

bool LibraryToolbox::is_constant(SymTok c) const
{
    // Things in temp_* can only ve variable
    return this->lib.is_constant(c);
}

const Sentence &LibraryToolbox::get_sentence(LabTok label) const
{
    const Sentence *sent = this->lib.get_sentence_ptr(label);
    if (sent != NULL) {
        return *sent;
    }
    return this->temp_types.at(label);
}

SentenceType LibraryToolbox::get_sentence_type(LabTok label) const
{
    return this->lib.get_sentence_type(label);
}

const Assertion &LibraryToolbox::get_assertion(LabTok label) const
{
    return this->lib.get_assertion(label);
}

std::function<const Assertion *()> LibraryToolbox::list_assertions() const
{
    return this->lib.list_assertions();
}

const StackFrame &LibraryToolbox::get_final_stack_frame() const
{
    return this->lib.get_final_stack_frame();
}

const LibraryAddendum &LibraryToolbox::get_addendum() const
{
    return this->lib.get_addendum();
}

const ParsingAddendumImpl &LibraryToolbox::get_parsing_addendum() const
{
    return this->lib.get_parsing_addendum();
}

/*Prover cascade_provers(const Prover &a,  const Prover &b)
{
    return [=](ProofEngine &engine) {
        bool res;
        res = a(engine);
        if (res) {
            return true;
        }
        res = b(engine);
        return res;
    };
}*/

std::vector<SymTok> LibraryToolbox::read_sentence(const string &in) const
{
    auto toks = tokenize(in);
    vector< SymTok > res;
    for (auto &tok : toks) {
        auto tok_num = this->get_symbol(tok);
        assert_or_throw< MMPPException >(tok_num != 0, "not a symbol");
        res.push_back(tok_num);
    }
    return res;
}

SentencePrinter LibraryToolbox::print_sentence(const std::vector<SymTok> &sent, SentencePrinter::Style style) const
{
    return SentencePrinter({ &sent, {}, {}, *this, style });
}

SentencePrinter LibraryToolbox::print_sentence(const ParsingTree<SymTok, LabTok> &pt, SentencePrinter::Style style) const
{
    return SentencePrinter({ {}, &pt, {}, *this, style });
}

SentencePrinter LibraryToolbox::print_sentence(const ParsingTree2<SymTok, LabTok> &pt, SentencePrinter::Style style) const
{
    return SentencePrinter({ {}, {}, &pt, *this, style });
}

ProofPrinter LibraryToolbox::print_proof(const std::vector<LabTok> &proof, bool only_assertions) const
{
    return ProofPrinter({ proof, *this, only_assertions });
}

std::vector<std::tuple<LabTok, std::vector<size_t>, std::unordered_map<SymTok, std::vector<SymTok> > > > LibraryToolbox::unify_assertion_uncached(const std::vector<std::vector<SymTok> > &hypotheses, const std::vector<SymTok> &thesis, bool just_first, bool up_to_hyps_perms) const
{
    std::vector<std::tuple< LabTok, std::vector< size_t >, std::unordered_map<SymTok, std::vector<SymTok> > > > ret;

    vector< SymTok > sent;
    for (auto &hyp : hypotheses) {
        copy(hyp.begin(), hyp.end(), back_inserter(sent));
        sent.push_back(0);
    }
    copy(thesis.begin(), thesis.end(), back_inserter(sent));

    auto assertions_gen = this->list_assertions();
    while (true) {
        const Assertion *ass2 = assertions_gen();
        if (ass2 == NULL) {
            break;
        }
        const Assertion &ass = *ass2;
        if (ass.is_usage_disc()) {
            continue;
        }
        if (ass.get_ess_hyps().size() != hypotheses.size()) {
            continue;
        }
        // We have to generate all the hypotheses' permutations; fortunately usually hypotheses are not many
        // TODO Is there a better algorithm?
        // The i-th specified hypothesis is matched with the perm[i]-th assertion hypothesis
        vector< size_t > perm;
        for (size_t i = 0; i < hypotheses.size(); i++) {
            perm.push_back(i);
        }
        do {
            vector< SymTok > templ;
            for (size_t i = 0; i < hypotheses.size(); i++) {
                auto &hyp = this->get_sentence(ass.get_ess_hyps()[perm[i]]);
                copy(hyp.begin(), hyp.end(), back_inserter(templ));
                templ.push_back(0);
            }
            auto &th = this->get_sentence(ass.get_thesis());
            copy(th.begin(), th.end(), back_inserter(templ));
            auto unifications = unify_old(sent, templ, *this);
            if (!unifications.empty()) {
                for (auto &unification : unifications) {
                    // Here we have to check that substitutions are of the corresponding type
                    // TODO - Here we immediately drop the type information, which probably mean that later we have to compute it again
                    bool wrong_unification = false;
                    for (auto &float_hyp : ass.get_float_hyps()) {
                        const Sentence &float_hyp_sent = this->get_sentence(float_hyp);
                        Sentence type_sent;
                        type_sent.push_back(float_hyp_sent.at(0));
                        auto &type_main_sent = unification.at(float_hyp_sent.at(1));
                        copy(type_main_sent.begin(), type_main_sent.end(), back_inserter(type_sent));
                        ProofEngine engine(*this);
                        if (!this->type_proving_helper(type_sent, engine)) {
                            wrong_unification = true;
                            break;
                        }
                    }
                    // If this is the case, then we have found a legitimate unification
                    if (!wrong_unification) {
                        ret.emplace_back(ass.get_thesis(), perm, unification);
                        if (just_first) {
                            return ret;
                        }
                    }
                }
            }
            if (!up_to_hyps_perms) {
                break;
            }
        } while (next_permutation(perm.begin(), perm.end()));
    }

    return ret;
}

std::vector<std::tuple<LabTok, std::vector<size_t>, std::unordered_map<SymTok, std::vector<SymTok> > > > LibraryToolbox::unify_assertion_uncached2(const std::vector<std::vector<SymTok> > &hypotheses, const std::vector<SymTok> &thesis, bool just_first, bool up_to_hyps_perms) const
{
    std::vector<std::tuple< LabTok, std::vector< size_t >, std::unordered_map<SymTok, std::vector<SymTok> > > > ret;

    // Parse inputs
    vector< ParsingTree< SymTok, LabTok > > pt_hyps;
    for (auto &hyp : hypotheses) {
        pt_hyps.push_back(this->parse_sentence(hyp.begin()+1, hyp.end(), this->turnstile_alias));
    }
    ParsingTree< SymTok, LabTok > pt_thesis = this->parse_sentence(thesis.begin()+1, thesis.end(), this->turnstile_alias);

    auto assertions_gen = this->list_assertions();
    const auto &is_var = this->get_standard_is_var();
    while (true) {
        const Assertion *ass2 = assertions_gen();
        if (ass2 == NULL) {
            break;
        }
        const Assertion &ass = *ass2;
        if (ass.is_usage_disc()) {
            continue;
        }
        if (ass.get_ess_hyps().size() != hypotheses.size()) {
            continue;
        }
        if (thesis[0] != this->get_sentence(ass.get_thesis())[0]) {
            continue;
        }
        UnilateralUnificator< SymTok, LabTok > unif(is_var);
        auto &templ_pt = this->get_parsed_sents().at(ass.get_thesis());
        unif.add_parsing_trees(templ_pt, pt_thesis);
        if (!unif.is_unifiable()) {
            continue;
        }
        // We have to generate all the hypotheses' permutations; fortunately usually hypotheses are not many
        // TODO Is there a better algorithm?
        // The i-th specified hypothesis is matched with the perm[i]-th assertion hypothesis
        vector< size_t > perm;
        for (size_t i = 0; i < hypotheses.size(); i++) {
            perm.push_back(i);
        }
        do {
            UnilateralUnificator< SymTok, LabTok > unif2 = unif;
            bool res = true;
            for (size_t i = 0; i < hypotheses.size(); i++) {
                res = (hypotheses[i][0] == this->get_sentence(ass.get_ess_hyps()[perm[i]])[0]);
                if (!res) {
                    break;
                }
                auto &templ_pt = this->get_parsed_sents().at(ass.get_ess_hyps()[perm[i]]);
                unif2.add_parsing_trees(templ_pt, pt_hyps[i]);
                res = unif2.is_unifiable();
                if (!res) {
                    break;
                }
            }
            if (!res) {
                continue;
            }
            SubstMap< SymTok, LabTok > subst;
            tie(res, subst) = unif2.unify();
            if (!res) {
                continue;
            }
            std::unordered_map< SymTok, std::vector< SymTok > > subst2;
            for (auto &s : subst) {
                subst2.insert(make_pair(this->get_sentence(s.first).at(1), this->reconstruct_sentence(s.second)));
            }
            ret.emplace_back(ass.get_thesis(), perm, subst2);
            if (just_first) {
                return ret;
            }
            if (!up_to_hyps_perms) {
                break;
            }
        } while (next_permutation(perm.begin(), perm.end()));
    }

    return ret;
}

std::vector<std::tuple<LabTok, std::vector<size_t>, std::unordered_map<SymTok, std::vector<SymTok> > > > LibraryToolbox::unify_assertion_cached(const std::vector<std::vector<SymTok> > &hypotheses, const std::vector<SymTok> &thesis, bool just_first)
{
    // Cache is used only when requesting just the first result
    if (!just_first) {
        return this->unify_assertion_uncached(hypotheses, thesis, just_first);
    }
    auto idx = make_tuple(hypotheses, thesis);
    if (this->unification_cache.find(idx) == this->unification_cache.end()) {
        this->unification_cache[idx] = this->unify_assertion_uncached(hypotheses, thesis, just_first);
    }
    return this->unification_cache.at(idx);
}

std::vector<std::tuple<LabTok, std::vector<size_t>, std::unordered_map<SymTok, std::vector<SymTok> > > > LibraryToolbox::unify_assertion_cached(const std::vector<std::vector<SymTok> > &hypotheses, const std::vector<SymTok> &thesis, bool just_first) const
{
    // Cache is used only when requesting just the first result
    if (!just_first) {
        return this->unify_assertion_uncached(hypotheses, thesis, just_first);
    }
    auto idx = make_tuple(hypotheses, thesis);
    if (this->unification_cache.find(idx) == this->unification_cache.end()) {
        return this->unify_assertion_uncached(hypotheses, thesis, just_first);
    }
    return this->unification_cache.at(idx);
}

const std::function<bool (LabTok)> &LibraryToolbox::get_standard_is_var() const {
    return this->standard_is_var;
}

const std::function<bool (SymTok)> &LibraryToolbox::get_standard_is_var_sym() const
{
    return this->standard_is_var_sym;
}

SymTok LibraryToolbox::get_turnstile() const
{
    return this->turnstile;
}

SymTok LibraryToolbox::get_turnstile_alias() const
{
    return this->turnstile_alias;
}

void LibraryToolbox::dump_proof_exception(const ProofException &e, ostream &out) const
{
    out << "Applying " << this->resolve_label(e.get_error().label) << " the proof executor signalled an error..." << endl;
    out << "The reason was " << e.get_reason() << endl;
    out << "On stack there was: " << this->print_sentence(e.get_error().on_stack) << endl;
    out << "Has to match with: " << this->print_sentence(e.get_error().to_subst) << endl;
    out << "Substitution map:" << endl;
    for (const auto &it : e.get_error().subst_map) {
        out << this->resolve_symbol(it.first) << ": " << this->print_sentence(it.second) << endl;
    }
}

std::vector<std::tuple<LabTok, std::vector<size_t>, std::unordered_map<SymTok, std::vector<SymTok> > > > LibraryToolbox::unify_assertion(const std::vector<std::vector<SymTok> > &hypotheses, const std::vector<SymTok> &thesis, bool just_first, bool up_to_hyps_perms) const
{
    auto ret2 = this->unify_assertion_uncached2(hypotheses, thesis, just_first, up_to_hyps_perms);
#ifdef TOOLBOX_SELF_TEST
    auto ret = this->unify_assertion_uncached(hypotheses, thesis, just_first, up_to_hyps_perms);
    assert(ret == ret2);
#endif
    return ret2;
}

void LibraryToolbox::compute_everything()
{
    //cout << "Computing everything" << endl;
    //auto t = tic();
    this->compute_type_correspondance();
    this->compute_is_var_by_type();
    this->compute_assertions_by_type();
    this->compute_derivations();
    this->compute_ders_by_label();
    this->compute_parser_initialization();
    this->compute_sentences_parsing();
    this->compute_registered_provers();
    this->compute_vars();
    //toc(t, 1);
}

const std::vector<LabTok> &LibraryToolbox::get_type_labels() const
{
    return this->type_labels;
}

const std::set<LabTok> &LibraryToolbox::get_type_labels_set() const
{
    return this->type_labels_set;
}

void LibraryToolbox::compute_type_correspondance()
{
    for (auto &var_lab : this->get_type_labels()) {
        const auto &sent = this->get_sentence(var_lab);
        assert(sent.size() == 2);
        const SymTok type_sym = sent[0];
        const SymTok var_sym = sent[1];
        enlarge_and_set(this->var_lab_to_sym, var_lab) = var_sym;
        enlarge_and_set(this->var_sym_to_lab, var_sym) = var_lab;
        enlarge_and_set(this->var_lab_to_type_sym, var_lab) = type_sym;
        enlarge_and_set(this->var_sym_to_type_sym, var_sym) = type_sym;
    }
    this->type_corrsepondance_computed = true;
}

const std::vector<LabTok> &LibraryToolbox::get_var_sym_to_lab()
{
    if (!this->type_corrsepondance_computed) {
        this->compute_type_correspondance();
    }
    return this->var_sym_to_lab;
}

const std::vector<LabTok> &LibraryToolbox::get_var_sym_to_lab() const
{
    if (!this->type_corrsepondance_computed) {
        throw MMPPException("computation required on const object");
    }
    return this->var_sym_to_lab;
}

const std::vector<SymTok> &LibraryToolbox::get_var_lab_to_sym()
{
    if (!this->type_corrsepondance_computed) {
        this->compute_type_correspondance();
    }
    return this->var_lab_to_sym;
}

const std::vector<SymTok> &LibraryToolbox::get_var_lab_to_sym() const
{
    if (!this->type_corrsepondance_computed) {
        throw MMPPException("computation required on const object");
    }
    return this->var_lab_to_sym;
}

const std::vector<SymTok> &LibraryToolbox::get_var_sym_to_type_sym()
{
    if (!this->type_corrsepondance_computed) {
        this->compute_type_correspondance();
    }
    return this->var_sym_to_type_sym;
}

const std::vector<SymTok> &LibraryToolbox::get_var_sym_to_type_sym() const
{
    if (!this->type_corrsepondance_computed) {
        throw MMPPException("computation required on const object");
    }
    return this->var_sym_to_type_sym;
}

const std::vector<SymTok> &LibraryToolbox::get_var_lab_to_type_sym()
{
    if (!this->type_corrsepondance_computed) {
        this->compute_type_correspondance();
    }
    return this->var_lab_to_type_sym;
}

const std::vector<SymTok> &LibraryToolbox::get_var_lab_to_type_sym() const
{
    if (!this->type_corrsepondance_computed) {
        throw MMPPException("computation required on const object");
    }
    return this->var_lab_to_type_sym;
}

void LibraryToolbox::compute_is_var_by_type()
{
    const auto &types_set = this->get_type_labels_set();
    this->is_var_by_type.resize(this->lib.get_labels_num());
    for (LabTok label = 1; label < this->lib.get_labels_num(); label++) {
        this->is_var_by_type[label] = types_set.find(label) != types_set.end() && !this->is_constant(this->get_sentence(label).at(1));
    }
    this->is_var_by_type_computed = true;
}

const std::vector<bool> &LibraryToolbox::get_is_var_by_type()
{
    if (!this->is_var_by_type_computed) {
        this->compute_is_var_by_type();
    }
    return this->is_var_by_type;
}

const std::vector<bool> &LibraryToolbox::get_is_var_by_type() const
{
    if (!this->is_var_by_type_computed) {
        throw MMPPException("computation required on const object");
    }
    return this->is_var_by_type;
}

void LibraryToolbox::compute_assertions_by_type()
{
    auto assertions_gen = this->list_assertions();
    while (true) {
        const Assertion *ass2 = assertions_gen();
        if (ass2 == NULL) {
            break;
        }
        const Assertion &ass = *ass2;
        const auto &label = ass.get_thesis();
        this->assertions_by_type[this->get_sentence(label).at(0)].push_back(label);
    }
    this->assertions_by_type_computed = true;
}

const std::unordered_map<SymTok, std::vector<LabTok> > &LibraryToolbox::get_assertions_by_type()
{
    if (!this->assertions_by_type_computed) {
        this->compute_assertions_by_type();
    }
    return this->assertions_by_type;
}

const std::unordered_map<SymTok, std::vector<LabTok> > &LibraryToolbox::get_assertions_by_type() const
{
    if (!this->assertions_by_type_computed) {
        throw MMPPException("computation required on const object");
    }
    return this->assertions_by_type;
}

void LibraryToolbox::compute_derivations()
{
    // Build the derivation rules; a derivation is created for each $f statement
    // and for each $a statement without essential hypotheses such that no variable
    // appears more than once and without distinct variables constraints and that does not
    // begin with the turnstile
    for (auto &type_lab : this->get_type_labels()) {
        auto &type_sent = this->get_sentence(type_lab);
        this->derivations[type_sent.at(0)].push_back(make_pair(type_lab, vector<SymTok>({type_sent.at(1)})));
    }
    // FIXME Take it from the configuration
    auto assertions_gen = this->list_assertions();
    while (true) {
        const Assertion *ass2 = assertions_gen();
        if (ass2 == NULL) {
            break;
        }
        const Assertion &ass = *ass2;
        if (ass.get_ess_hyps().size() != 0) {
            continue;
        }
        if (ass.get_mand_dists().size() != 0) {
            continue;
        }
        if (ass.is_theorem()) {
            continue;
        }
        const auto &sent = this->get_sentence(ass.get_thesis());
        if (sent.at(0) == this->turnstile) {
            continue;
        }
        set< SymTok > symbols;
        bool duplicate = false;
        for (const auto &tok : sent) {
            if (this->is_constant(tok)) {
                continue;
            }
            auto res = symbols.insert(tok);
            if (!res.second) {
                duplicate = true;
                break;
            }
        }
        if (duplicate) {
            continue;
        }
        vector< SymTok > sent2;
        for (size_t i = 1; i < sent.size(); i++) {
            const auto &tok = sent[i];
            // Variables are replaced with their types, which act as variables of the context-free grammar
            sent2.push_back(this->is_constant(tok) ? tok : this->get_sentence(this->get_var_sym_to_lab().at(tok)).at(0));
        }
        this->derivations[sent.at(0)].push_back(make_pair(ass.get_thesis(), sent2));
    }
    this->derivations_computed = true;
}

RegisteredProver LibraryToolbox::register_prover(const std::vector<string> &templ_hyps, const string &templ_thesis)
{
    size_t index = LibraryToolbox::registered_provers().size();
    auto &rps = LibraryToolbox::registered_provers();
    rps.push_back({templ_hyps, templ_thesis});
    //cerr << "first: " << &rps << "; size: " << rps.size() << endl;
    return { index, templ_hyps, templ_thesis };
}

Prover LibraryToolbox::build_registered_prover(const RegisteredProver &prover, const std::unordered_map<string, Prover> &types_provers, const std::vector<Prover> &hyps_provers) const
{
    const size_t &index = prover.index;
    if (index >= this->instance_registered_provers.size() || !this->instance_registered_provers[index].valid) {
        throw MMPPException("cannot modify const object");
    }
    const RegisteredProverInstanceData &inst_data = this->instance_registered_provers[index];

    return [=](ProofEngine &engine){
        return this->proving_helper(inst_data, types_provers, hyps_provers, engine);
    };
}

void LibraryToolbox::compute_registered_provers()
{
    for (size_t index = 0; index < LibraryToolbox::registered_provers().size(); index++) {
        this->compute_registered_prover(index, false);
    }
    //cerr << "Computed " << LibraryToolbox::registered_provers().size() << " registered provers" << endl;
}

void LibraryToolbox::compute_parser_initialization()
{
    delete this->parser;
    this->parser = NULL;
    std::function< std::ostream&(std::ostream&, SymTok) > sym_printer = [&](ostream &os, SymTok sym)->ostream& { return os << this->resolve_symbol(sym); };
    std::function< std::ostream&(std::ostream&, LabTok) > lab_printer = [&](ostream &os, LabTok lab)->ostream& { return os << this->resolve_label(lab); };
    const auto &ders = this->get_derivations();
    string ders_digest = hash_object(ders);
    this->parser = new LRParser< SymTok, LabTok >(ders, sym_printer, lab_printer);
    bool loaded = false;
    if (this->cache != NULL) {
        if (this->cache->load()) {
            if (ders_digest == this->cache->get_digest()) {
                this->parser->set_cached_data(this->cache->get_lr_parser_data());
                loaded = true;
            }
        }
    }
    if (!loaded) {
        this->parser->initialize();
        if (this->cache != NULL) {
            this->cache->set_digest(ders_digest);
            this->cache->set_lr_parser_data(this->parser->get_cached_data());
            this->cache->store();
        }
    }
    // Drop the cache so that memory can be recovered
    this->cache = NULL;
    this->parser_initialization_computed = true;
}

const LRParser<SymTok, LabTok> &LibraryToolbox::get_parser()
{
    if (!this->parser_initialization_computed) {
        this->compute_parser_initialization();
    }
    return *this->parser;
}

const LRParser<SymTok, LabTok> &LibraryToolbox::get_parser() const
{
    if (!this->parser_initialization_computed) {
        throw MMPPException("computation required on const object");
    }
    return *this->parser;
}

ParsingTree< SymTok, LabTok > LibraryToolbox::parse_sentence(typename Sentence::const_iterator sent_begin, typename Sentence::const_iterator sent_end, SymTok type) const
{
    return this->get_parser().parse(sent_begin, sent_end, type);
}

ParsingTree< SymTok, LabTok > LibraryToolbox::parse_sentence(const Sentence &sent, SymTok type) const
{
    return this->get_parser().parse(sent, type);
}

ParsingTree<SymTok, LabTok> LibraryToolbox::parse_sentence(const Sentence &sent) const
{
    return this->parse_sentence(sent.begin()+1, sent.end(), sent.at(0));
}

ParsingTree< SymTok, LabTok > LibraryToolbox::parse_sentence(typename Sentence::const_iterator sent_begin, typename Sentence::const_iterator sent_end, SymTok type)
{
    return this->get_parser().parse(sent_begin, sent_end, type);
}

ParsingTree< SymTok, LabTok > LibraryToolbox::parse_sentence(const Sentence &sent, SymTok type)
{
    return this->get_parser().parse(sent, type);
}

ParsingTree<SymTok, LabTok> LibraryToolbox::parse_sentence(const Sentence &sent)
{
    return this->parse_sentence(sent.begin()+1, sent.end(), sent.at(0));
}

void LibraryToolbox::compute_sentences_parsing()
{
    /*if (!this->parser_initialization_computed) {
        this->compute_parser_initialization();
    }*/
    for (size_t i = 1; i < this->get_labels_num()+1; i++) {
        const Sentence &sent = this->get_sentence(i);
        auto pt = this->parse_sentence(sent.begin()+1, sent.end(), this->get_parsing_addendum().get_syntax().at(sent[0]));
        if (pt.label == 0) {
            throw MMPPException("Failed to parse a sentence in the library");
        }
        this->parsed_sents.resize(i+1);
        this->parsed_sents[i] = pt;
        this->parsed_sents2.resize(i+1);
        this->parsed_sents2[i] = pt_to_pt2(pt);
        this->parsed_iters.resize(i+1);
        ParsingTreeMultiIterator< SymTok, LabTok > it = this->parsed_sents2[i].get_multi_iterator();
        while (true) {
            auto x = it.next();
            this->parsed_iters[i].push_back(x);
            if (x.first == it.Finished) {
                break;
            }
        }
    }
    this->sentences_parsing_computed = true;
}

const std::vector<ParsingTree<SymTok, LabTok> > &LibraryToolbox::get_parsed_sents()
{
    if (!this->sentences_parsing_computed) {
        this->compute_sentences_parsing();
    }
    return this->parsed_sents;
}

const std::vector<ParsingTree<SymTok, LabTok> > &LibraryToolbox::get_parsed_sents() const
{
    if (!this->sentences_parsing_computed) {
        throw MMPPException("computation required on const object");
    }
    return this->parsed_sents;
}

const std::vector<ParsingTree2<SymTok, LabTok> > &LibraryToolbox::get_parsed_sents2()
{
    if (!this->sentences_parsing_computed) {
        this->compute_sentences_parsing();
    }
    return this->parsed_sents2;
}

const std::vector<ParsingTree2<SymTok, LabTok> > &LibraryToolbox::get_parsed_sents2() const
{
    if (!this->sentences_parsing_computed) {
        throw MMPPException("computation required on const object");
    }
    return this->parsed_sents2;
}

const std::vector<std::vector<std::pair<ParsingTreeMultiIterator< SymTok, LabTok >::Status, ParsingTreeNode<SymTok, LabTok> > > > &LibraryToolbox::get_parsed_iters()
{
    if (!this->sentences_parsing_computed) {
        this->compute_sentences_parsing();
    }
    return this->parsed_iters;
}

const std::vector<std::vector<std::pair<ParsingTreeMultiIterator< SymTok, LabTok >::Status, ParsingTreeNode<SymTok, LabTok> > > > &LibraryToolbox::get_parsed_iters() const
{
    if (!this->sentences_parsing_computed) {
        throw MMPPException("computation required on const object");
    }
    return this->parsed_iters;
}

void LibraryToolbox::compute_registered_prover(size_t index, bool exception_on_failure)
{
    this->instance_registered_provers.resize(LibraryToolbox::registered_provers().size());
    const RegisteredProverData &data = LibraryToolbox::registered_provers()[index];
    RegisteredProverInstanceData &inst_data = this->instance_registered_provers[index];
    if (!inst_data.valid) {
        // Decode input strings to sentences
        std::vector<std::vector<SymTok> > templ_hyps_sent;
        for (auto &hyp : data.templ_hyps) {
            templ_hyps_sent.push_back(this->read_sentence(hyp));
        }
        std::vector<SymTok> templ_thesis_sent = this->read_sentence(data.templ_thesis);

        auto unification = this->unify_assertion(templ_hyps_sent, templ_thesis_sent, true);
        if (unification.empty()) {
            if (exception_on_failure) {
                throw MMPPException("Could not find the template assertion");
            } else {
                return;
            }
        }
        inst_data.label = get<0>(*unification.begin());
        inst_data.label_str = this->resolve_label(inst_data.label);
        const Assertion &ass = this->get_assertion(inst_data.label);
        assert(ass.is_valid());
        const vector< size_t > &perm = get<1>(*unification.begin());
        inst_data.perm_inv = invert_perm(perm);
        inst_data.ass_map = get<2>(*unification.begin());
        //const unordered_map< SymTok, vector< SymTok > > full_map = this->compose_subst(ass_map, subst_map);
        inst_data.valid = true;
    }
}

Prover LibraryToolbox::build_type_prover(const std::vector<SymTok> &type_sent, const std::unordered_map<SymTok, Prover> &var_provers) const
{
    return [=](ProofEngine &engine){
        return this->type_proving_helper(type_sent, engine, var_provers);
    };
}

Prover LibraryToolbox::build_type_prover(const ParsingTree2<SymTok, LabTok> &pt, const std::unordered_map<LabTok, Prover> &var_provers) const
{
    return [=](ProofEngine &engine){
        return this->type_proving_helper(pt, engine, var_provers);
    };
}

string SentencePrinter::to_string() const
{
    ostringstream buf;
    buf << *this;
    return buf.str();
}

string test_prover(Prover prover, const LibraryToolbox &tb) {
    ProofEngine engine(tb);
    bool res = prover(engine);
    if (!res) {
        return "(prover failed)";
    } else {
        if (engine.get_stack().size() == 0) {
            return "(prover did not produce a result)";
        } else {
            string ret = tb.print_sentence(engine.get_stack().back()).to_string();
            if (engine.get_stack().size() > 1) {
                ret += " (prover did produce other stack entries)";
            }
            return ret;
        }
    }
}

ToolboxCache::~ToolboxCache()
{
}

FileToolboxCache::FileToolboxCache(const boost::filesystem::path &filename) : filename(filename) {
}

bool FileToolboxCache::load() {
    boost::filesystem::ifstream lr_fin(this->filename);
    if (lr_fin.fail()) {
        return false;
    }
    boost::archive::text_iarchive archive(lr_fin);
    archive >> this->digest;
    archive >> this->lr_parser_data;
    return true;
}

bool FileToolboxCache::store() {
    boost::filesystem::ofstream lr_fout(this->filename);
    if (lr_fout.fail()) {
        return false;
    }
    boost::archive::text_oarchive archive(lr_fout);
    archive << this->digest;
    archive << this->lr_parser_data;
    return true;
}

string FileToolboxCache::get_digest() {
    return this->digest;
}

void FileToolboxCache::set_digest(string digest) {
    this->digest = digest;
}

LRParser< SymTok, LabTok >::CachedData FileToolboxCache::get_lr_parser_data() {
    return this->lr_parser_data;
}

void FileToolboxCache::set_lr_parser_data(const LRParser< SymTok, LabTok >::CachedData &cached_data) {
    this->lr_parser_data = cached_data;
}

Prover checked_prover(Prover prover, size_t hyp_num, Sentence thesis)
{
    return [=](ProofEngine &engine)->bool {
        size_t stack_len_before = engine.get_stack().size();
        bool res = prover(engine);
        size_t stack_len_after = engine.get_stack().size();
        assert(stack_len_after >= 1);
        assert(stack_len_after - stack_len_before == hyp_num - 1);
        assert(engine.get_stack().back() == thesis);
        return res;
    };
}
