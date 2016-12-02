#ifndef WFF_H
#define WFF_H

#include <string>
#include <memory>
#include <set>
#include <vector>

#include "library.h"
#include "proof.h"

class Wff;
typedef std::shared_ptr< Wff > pwff;

class Wff {
public:
    virtual std::string to_string() const = 0;
    virtual pwff imp_not_form() const = 0;
    virtual std::vector< SymTok > to_sentence(const LibraryInterface &lib) const = 0;
    virtual void prove_type(const LibraryInterface &lib, ProofEngine &engine) const;
    virtual bool prove_true(const LibraryInterface &lib, ProofEngine &engine) const;
    virtual bool prove_false(const LibraryInterface &lib, ProofEngine &engine) const;
    std::function< bool(const LibraryInterface&, ProofEngine&) > get_truth_prover() const;
    std::function< bool(const LibraryInterface&, ProofEngine&) > get_falsehood_prover() const;
};

class True : public Wff {
public:
    True();
    std::string to_string() const;
    pwff imp_not_form() const;
    std::vector< SymTok > to_sentence(const LibraryInterface &lib) const;
    void prove_type(const LibraryInterface &lib, ProofEngine &engine) const;
    bool prove_true(const LibraryInterface &lib, ProofEngine &engine) const;
};

class False : public Wff {
public:
    False();
    std::string to_string() const;
    pwff imp_not_form() const;
    std::vector< SymTok > to_sentence(const LibraryInterface &lib) const;
    bool prove_false(const LibraryInterface &lib, ProofEngine &engine) const;
};

class Var : public Wff {
public:
  Var(std::string name);
  std::string to_string() const;
  pwff imp_not_form() const;
  std::vector< SymTok > to_sentence(const LibraryInterface &lib) const;

private:
  std::string name;
};

class Not : public Wff {
public:
  Not(pwff a);
  std::string to_string() const;
  pwff imp_not_form() const;
  std::vector< SymTok > to_sentence(const LibraryInterface &lib) const;
  bool prove_true(const LibraryInterface &lib, ProofEngine &engine) const;
  bool prove_false(const LibraryInterface &lib, ProofEngine &engine) const;

private:
  pwff a;
};

class Imp : public Wff {
public:
  Imp(pwff a, pwff b);
  std::string to_string() const;
  pwff imp_not_form() const;
  std::vector< SymTok > to_sentence(const LibraryInterface &lib) const;
  bool prove_true(const LibraryInterface &lib, ProofEngine &engine) const;
  bool prove_false(const LibraryInterface &lib, ProofEngine &engine) const;

private:
  pwff a, b;
};

class Biimp : public Wff {
public:
  Biimp(pwff a, pwff b);
  std::string to_string() const;
  pwff imp_not_form() const;
  std::vector< SymTok > to_sentence(const LibraryInterface &lib) const;

private:
  pwff a, b;
};

class And : public Wff {
public:
  And(pwff a, pwff b);
  std::string to_string() const;
  pwff imp_not_form() const;
  std::vector< SymTok > to_sentence(const LibraryInterface &lib) const;

private:
  pwff a, b;
};

class Or : public Wff {
public:
  Or(pwff a, pwff b);
  std::string to_string() const;
  pwff imp_not_form() const;
  std::vector< SymTok > to_sentence(const LibraryInterface &lib) const;

private:
  pwff a, b;
};

class Nand : public Wff {
public:
  Nand(pwff a, pwff b);
  std::string to_string() const;
  pwff imp_not_form() const;
  std::vector< SymTok > to_sentence(const LibraryInterface &lib) const;

private:
  pwff a, b;
};

class Xor : public Wff {
public:
  Xor(pwff a, pwff b);
  std::string to_string() const;
  pwff imp_not_form() const;
  std::vector< SymTok > to_sentence(const LibraryInterface &lib) const;

private:
  pwff a, b;
};

#endif // WFF_H
