#ifndef LIBRARY_H
#define LIBRARY_H

#include <unordered_map>
#include <vector>
#include <utility>
#include <tuple>
#include <limits>
#include <set>
#include <type_traits>
#include <memory>

#include <cassert>

class Library;
class Assertion;

typedef uint16_t SymTok;
typedef uint32_t LabTok;

static_assert(std::is_integral< SymTok >::value);
static_assert(std::is_unsigned< SymTok >::value);
static_assert(std::is_integral< LabTok >::value);
static_assert(std::is_unsigned< LabTok >::value);

#include "proof.h"

struct SentencePrinter {
    const std::vector< SymTok > &sent;
    const Library &lib;
};
SentencePrinter print_sentence(const std::vector< SymTok > &sent, const Library &lib);
std::ostream &operator<<(std::ostream &os, const SentencePrinter &sp);
std::vector< SymTok > parse_sentence(const std::string &in, const Library &lib);

class Assertion {
public:
    Assertion();
    Assertion(bool theorem,
              size_t num_floating,
              std::set< std::pair< SymTok, SymTok > > dists,
              std::set< std::pair< SymTok, SymTok > > opt_dists,
              std::vector< LabTok > hyps,
              std::set< LabTok > opt_hyps,
              LabTok thesis);
    ~Assertion();
    bool is_valid() const;
    bool is_theorem() const;
    size_t get_num_floating() const;
    const std::set< std::pair< SymTok, SymTok > > &get_mand_dists() const;
    const std::set< std::pair< SymTok, SymTok > > &get_opt_dists() const;
    const std::set<std::pair<SymTok, SymTok> > get_dists() const;
    const std::vector< LabTok > &get_mand_hyps() const;
    const std::set< LabTok > &get_opt_hyps() const;
    std::vector<LabTok> get_ess_hyps() const;
    LabTok get_thesis() const;
    std::shared_ptr< ProofExecutor > get_proof_executor(const Library &lib) const;

    void add_proof(std::shared_ptr<Proof> proof);

private:
    bool valid;
    size_t num_floating;
    bool theorem;
    std::set< std::pair< SymTok, SymTok > > dists;
    std::set< std::pair< SymTok, SymTok > > opt_dists;
    std::vector< LabTok > hyps;
    std::set< LabTok > opt_hyps;
    LabTok thesis;
    std::shared_ptr< Proof > proof;

    std::shared_ptr< Proof > get_proof() const;
};

template< typename Tok >
class StringCache {
public:
    Tok get(std::string s) const {
        auto it = this->dir.find(s);
        if (it == this->dir.end()) {
            return 0;
        } else {
            return it->second;
        }
    }
    Tok create(std::string s)
    {
        auto it = this->dir.find(s);
        if (it == this->dir.end()) {
            bool res;
            assert(this->next_id != std::numeric_limits< Tok >::max());
            tie(it, res) = this->dir.insert(make_pair(s, this->next_id));
            assert(res);
            std::tie(std::ignore, res) = this->inv.insert(make_pair(this->next_id, s));
            assert(res);
            this->next_id++;
            return it->second;
        } else {
            return 0;
        }
    }
    std::string resolve(Tok id) const
    {
        return this->inv.at(id);
    }
    Tok get_or_create(std::string s) {
        Tok tok = this->get(s);
        if (tok == 0) {
            tok = this->create(s);
        }
        return tok;
    }
    std::size_t size() const {
        return this->dir.size();
    }
private:
    Tok next_id = 1;
    std::unordered_map< std::string, Tok > dir;
    std::unordered_map< Tok, std::string > inv;
};

class Library
{
public:
    Library();
    SymTok create_symbol(std::string s);
    LabTok create_label(std::string s);
    SymTok get_symbol(std::string s) const;
    LabTok get_label(std::string s) const;
    std::string resolve_symbol(SymTok tok) const;
    std::string resolve_label(LabTok tok) const;
    std::size_t get_symbol_num() const;
    std::size_t get_label_num() const;
    void add_sentence(LabTok label, std::vector< SymTok > content);
    const std::vector<SymTok> &get_sentence(LabTok label) const;
    void add_assertion(LabTok label, const Assertion &ass);
    const Assertion &get_assertion(LabTok label) const;
    const std::vector< Assertion > &get_assertions() const;
    void add_constant(SymTok c);
    bool is_constant(SymTok c) const;
    std::unordered_map<LabTok, std::vector<std::unordered_map<SymTok, std::vector<SymTok> > > > unify_assertion(std::vector< std::vector< SymTok > > hypotheses, std::vector< SymTok > thesis);
    std::vector< LabTok > prove_type(const std::vector< SymTok > &type_sent) const;
    void set_types(const std::vector< LabTok > &types);

private:
    StringCache< SymTok > syms;
    StringCache< LabTok > labels;
    std::set< SymTok > consts;

    // Vector is more efficient if labels are known to be contiguous and starting from 1; in the general case the unordered_map might be better
    //std::unordered_map< LabTok, std::vector< SymTok > > sentences;
    std::vector< std::vector< SymTok > > sentences;
    std::vector< Assertion > assertions;

    // These are not used in parsing or proof execution, but only for later algorithms (such as type inference)
    std::vector< LabTok > types;
    std::vector< LabTok > types_by_var;
    std::unordered_map< SymTok, std::vector< LabTok > > assertions_by_type;
};

#endif // LIBRARY_H
