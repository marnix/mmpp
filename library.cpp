#include "library.h"
#include "utils.h"
#include "reader.h"
#include "unification.h"
#include "toolbox.h"

#include <algorithm>
#include <iostream>
#include <map>
#include <regex>

using namespace std;

LibraryImpl::LibraryImpl()
{
}

SymTok LibraryImpl::create_symbol(string s)
{
    assert_or_throw(is_symbol(s));
    return this->syms.get_or_create(s);
}

LabTok LibraryImpl::create_label(string s)
{
    assert_or_throw(is_label(s));
    auto res = this->labels.get_or_create(s);
    //cerr << "Resizing from " << this->assertions.size() << " to " << res+1 << endl;
    this->sentences.resize(res+1);
    this->assertions.resize(res+1);
    return res;
}

SymTok LibraryImpl::get_symbol(string s) const
{
    return this->syms.get(s);
}

LabTok LibraryImpl::get_label(string s) const
{
    return this->labels.get(s);
}

string LibraryImpl::resolve_symbol(SymTok tok) const
{
    return this->syms.resolve(tok);
}

string LibraryImpl::resolve_label(LabTok tok) const
{
    return this->labels.resolve(tok);
}

size_t LibraryImpl::get_symbols_num() const
{
    return this->syms.size();
}

size_t LibraryImpl::get_labels_num() const
{
    return this->labels.size();
}

const std::vector<string> &LibraryImpl::get_symbols() const
{
    return this->syms.get_cache();
}

const std::vector<string> &LibraryImpl::get_labels() const
{
    return this->labels.get_cache();
}

void LibraryImpl::add_sentence(LabTok label, std::vector<SymTok> content) {
    //this->sentences.insert(make_pair(label, content));
    assert(label < this->sentences.size());
    this->sentences[label] = content;
}

const std::vector<SymTok> &LibraryImpl::get_sentence(LabTok label) const {
    return this->sentences.at(label);
}

void LibraryImpl::add_assertion(LabTok label, const Assertion &ass)
{
    this->assertions[label] = ass;
}

const Assertion &LibraryImpl::get_assertion(LabTok label) const
{
    return this->assertions.at(label);
}

const std::vector<Assertion> &LibraryImpl::get_assertions() const
{
    return this->assertions;
}

void LibraryImpl::add_constant(SymTok c)
{
    this->consts.insert(c);
}

bool LibraryImpl::is_constant(SymTok c) const
{
    return this->consts.find(c) != this->consts.end();
}

/*std::vector<LabTok> LibraryImpl::prove_type2(const std::vector<SymTok> &type_sent) const
{
    // Iterate over all propositions (maybe just axioms would be enough) with zero essential hypotheses, try to match and recur on all matches;
    // hopefully nearly all branches die early and there is just one real long-standing branch;
    // when the length is 2 try to match with floating hypotheses.
    // The current implementation is probably less efficient and more copy-ish than it could be.
    assert(type_sent.size() >= 2);
    if (type_sent.size() == 2) {
        for (auto &test_type : this->types) {
            if (this->get_sentence(test_type) == type_sent) {
                return { test_type };
            }
        }
    }
    auto &type_const = type_sent.at(0);
    // If a there are no assertions for a certain type (which is possible, see for example "set" in set.mm), then processing stops here
    if (this->assertions_by_type.find(type_const) == this->assertions_by_type.end()) {
        return {};
    }
    for (auto &templ : this->assertions_by_type.at(type_const)) {
        const Assertion &templ_ass = this->get_assertion(templ);
        if (templ_ass.get_ess_hyps().size() != 0) {
            continue;
        }
        const auto &templ_sent = this->get_sentence(templ);
        auto unifications = unify(type_sent, templ_sent, *this);
        for (auto &unification : unifications) {
            bool failed = false;
            unordered_map< SymTok, vector< LabTok > > matches;
            for (auto &unif_pair : unification) {
                const SymTok &var = unif_pair.first;
                const vector< SymTok > &subst = unif_pair.second;
                SymTok type = this->get_sentence(this->types_by_var[var]).at(0);
                vector< SymTok > new_type_sent = { type };
                // TODO This is not very efficient
                copy(subst.begin(), subst.end(), back_inserter(new_type_sent));
                auto res = matches.insert(make_pair(var, this->prove_type2(new_type_sent)));
                assert(res.second);
                if (res.first->second.empty()) {
                    failed = true;
                    break;
                }
            }
            if (!failed) {
                // We have to sort hypotheses by order af appearance; here we assume that numeric orderd of labels coincides with the order of appearance
                vector< pair< LabTok, SymTok > > hyp_labels;
                for (auto &match_pair : matches) {
                    hyp_labels.push_back(make_pair(this->types_by_var[match_pair.first], match_pair.first));
                }
                sort(hyp_labels.begin(), hyp_labels.end());
                vector< LabTok > ret;
                for (auto &hyp_pair : hyp_labels) {
                    auto &hyp_var = hyp_pair.second;
                    copy(matches.at(hyp_var).begin(), matches.at(hyp_var).end(), back_inserter(ret));
                }
                ret.push_back(templ);
                return ret;
            }
        }
    }
    return {};
}*/

void LibraryImpl::set_final_stack_frame(const StackFrame &final_stack_frame)
{
    this->final_stack_frame = final_stack_frame;
}

void LibraryImpl::set_max_number(LabTok max_number)
{
    this->max_number = max_number;
}

Assertion::Assertion() :
    valid(false)
{
}

Assertion::Assertion(bool theorem,
                     std::set<std::pair<SymTok, SymTok> > dists,
                     std::set<std::pair<SymTok, SymTok> > opt_dists,
                     std::vector<LabTok> float_hyps, std::vector<LabTok> ess_hyps, std::set<LabTok> opt_hyps,
                     LabTok thesis, LabTok number, string comment) :
    valid(true), theorem(theorem), mand_dists(dists), opt_dists(opt_dists),
    float_hyps(float_hyps), ess_hyps(ess_hyps), opt_hyps(opt_hyps), thesis(thesis), number(number), proof(NULL),
    comment(comment), modif_disc(false), usage_disc(false)
{
    if (this->comment.find("(Proof modification is discouraged.)") != string::npos) {
        this->modif_disc = true;
    }
    if (this->comment.find("(New usage is discouraged.)") != string::npos) {
        this->usage_disc = true;
    }
}

Assertion::~Assertion()
{
}

const std::set<std::pair<SymTok, SymTok> > Assertion::get_dists() const
{
    std::set<std::pair<SymTok, SymTok> > ret;
    set_union(this->get_mand_dists().begin(), this->get_mand_dists().end(),
              this->get_opt_dists().begin(), this->get_opt_dists().end(),
              inserter(ret, ret.begin()));
    return ret;
}

size_t Assertion::get_mand_hyps_num() const
{
    return this->get_float_hyps().size() + this->get_ess_hyps().size();
}

LabTok Assertion::get_mand_hyp(size_t i) const
{
    if (i < this->get_float_hyps().size()) {
        return this->get_float_hyps()[i];
    } else {
        return this->get_ess_hyps()[i-this->get_float_hyps().size()];
    }
}

const std::vector<LabTok> &Assertion::get_float_hyps() const
{
    return this->float_hyps;
}

const std::set<LabTok> &Assertion::get_opt_hyps() const
{
    return this->opt_hyps;
}

const std::vector<LabTok> &Assertion::get_ess_hyps() const
{
    return this->ess_hyps;
}

LabTok Assertion::get_thesis() const {
    return this->thesis;
}

std::shared_ptr<ProofExecutor> Assertion::get_proof_executor(const Library &lib, bool gen_proof_tree) const
{
    return this->proof->get_executor(lib, *this, gen_proof_tree);
}

void Assertion::set_proof(shared_ptr< Proof > proof)
{
    assert(this->theorem);
    this->proof = proof;
}

shared_ptr< Proof > Assertion::get_proof() const
{
    return this->proof;
}

const StackFrame &LibraryImpl::get_final_stack_frame() const
{
    return this->final_stack_frame;
}

const LibraryAddendumImpl &LibraryImpl::get_addendum() const
{
    return this->addendum;
}

class AssertionGenerator {
public:
    AssertionGenerator(const vector< Assertion > &ref) :
        it(ref.begin()), ref(ref) {
    }
    const Assertion *operator()() {
        while (this->it != this->ref.end()) {
            auto it2 = this->it;
            this->it++;
            if (it2->is_valid()) {
                return &*it2;
            }
        }
        return NULL;
    }

private:
    vector< Assertion >::const_iterator it;
    const vector< Assertion > &ref;
};

function<const Assertion *()> LibraryImpl::list_assertions() const {
    return AssertionGenerator(this->assertions);
}

LabTok LibraryImpl::get_max_number() const
{
    return this->max_number;
}

void LibraryImpl::set_addendum(const LibraryAddendumImpl &add)
{
    this->addendum = add;
}

Library::~Library()
{
}

string fix_htmlcss_for_qt(string s)
{
    string tmp(s);
    tmp = tmp + "\n\n";
    // Qt does not recognize HTML comments in the stylesheet
    tmp = regex_replace(tmp, regex("<!--"), "");
    tmp = regex_replace(tmp, regex("-->"), "");
    // Qt uses the system font (not the one provided by the CSS), so it must use the real font name
    tmp = regex_replace(tmp, regex("XITSMath-Regular"), "XITS Math");
    // Qt does not recognize the LINK tags and stops rendering altogether
    tmp = regex_replace(tmp, regex("<LINK [^>]*>"), "");
    return tmp;
}
