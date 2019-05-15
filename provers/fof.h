#pragma once

#include <iostream>

#include <giolib/memory.h>
#include <giolib/inheritance.h>
#include <giolib/containers.h>

namespace gio::mmpp::provers::fof {

class FOT;
class Variable;
class Functor;

struct fot_inheritance {
    typedef FOT base;
    typedef boost::mpl::vector<Variable, Functor> subtypes;
};

struct fot_cmp {
    bool operator()(const FOT &x, const FOT &y) const {
        return compare_base<fot_inheritance>(x, y);
    }
};

class FOT : public inheritance_base<fot_inheritance> {
public:
    virtual void print_to(std::ostream &s) const = 0;

protected:
    FOT() = default;
};

class Functor : public FOT, public gio::virtual_enable_create<Functor>, public inheritance_impl<Functor, fot_inheritance> {
public:
    void print_to(std::ostream &s) const override {
        s << this->name;
        if (!this->args.empty()) {
            s << "(";
            bool first = true;
            for (const auto &arg : this->args) {
                if (!first) {
                    s << ", ";
                }
                first = false;
                s << *arg;
            }
            s << ")";
        }
    }

    static bool compare(const Functor &x, const Functor &y) {
        if (x.name < y.name) { return true; }
        if (y.name < x.name) { return false; }
        return std::lexicographical_compare(x.args.begin(), x.args.end(), y.args.begin(), y.args.end(), [](const auto &x, const auto &y) {
            return fot_cmp()(*x, *y);
        });
    }

protected:
    Functor(const std::string &name, const std::vector<std::shared_ptr<const FOT>> &args) : name(name), args(args) {}

private:
    std::string name;
    std::vector<std::shared_ptr<const FOT>> args;
};

class Variable : public FOT, public gio::virtual_enable_create<Variable>, public inheritance_impl<Variable, fot_inheritance> {
public:
    void print_to(std::ostream &s) const override {
        s << this->name;
    }

    static bool compare(const Variable &x, const Variable &y) {
        return x.name < y.name;
    }

protected:
    Variable(const std::string &name) : name(name) {}

private:
    std::string name;
};

class FOF;
class Predicate;
class True;
class False;
class Equal;
class Distinct;
class And;
class Or;
class Iff;
class Not;
class Xor;
class Implies;
class Oeq;
class Forall;
class Exists;

struct fof_inheritance {
    typedef FOF base;
    typedef boost::mpl::vector<Predicate, True, False, Equal, Distinct, And, Or, Iff, Not, Xor, Implies, Oeq, Forall, Exists> subtypes;
};

struct fof_cmp {
    bool operator()(const FOF &x, const FOF &y) const {
        return compare_base<fof_inheritance>(x, y);
    }
};

class FOF : public inheritance_base<fof_inheritance> {
public:
    virtual void print_to(std::ostream &s) const = 0;

protected:
    FOF() = default;
};

class Predicate : public FOF, public gio::virtual_enable_create<Predicate>, public inheritance_impl<Predicate, fof_inheritance> {
public:
    void print_to(std::ostream &s) const override {
        s << this->name;
        if (!this->args.empty()) {
            s << "(";
            bool first = true;
            for (const auto &arg : this->args) {
                if (!first) {
                    s << ", ";
                }
                first = false;
                s << *arg;
            }
            s << ")";
        }
    }

    static bool compare(const Predicate &x, const Predicate &y) {
        if (x.name < y.name) { return true; }
        if (y.name < x.name) { return false; }
        return std::lexicographical_compare(x.args.begin(), x.args.end(), y.args.begin(), y.args.end(), [](const auto &x, const auto &y) {
            return fot_cmp()(*x, *y);
        });
    }

protected:
    Predicate(const std::string &name, const std::vector<std::shared_ptr<const FOT>> &args) : name(name), args(args) {}

private:
    std::string name;
    std::vector<std::shared_ptr<const FOT>> args;
};

class True : public FOF, public gio::virtual_enable_create<True>, public inheritance_impl<True, fof_inheritance> {
public:
    void print_to(std::ostream &s) const override {
        s << "⊤";
    }

    static bool compare(const True &x, const True &y) {
        (void) x;
        (void) y;
        return false;
    }

protected:
    True() {}
};

class False : public FOF, public gio::virtual_enable_create<False>, public inheritance_impl<False, fof_inheritance> {
public:
    void print_to(std::ostream &s) const override {
        s << "⊥";
    }

    static bool compare(const False &x, const False &y) {
        (void) x;
        (void) y;
        return false;
    }

protected:
    False() {}
};

class Equal : public FOF, public gio::virtual_enable_create<Equal>, public inheritance_impl<Equal, fof_inheritance> {
public:
    void print_to(std::ostream &s) const override {
        s << "(" << *this->x << "=" << *this->y << ")";
    }

    static bool compare(const Equal &x, const Equal &y) {
        if (fot_cmp()(*x.x, *y.x)) { return true; }
        if (fot_cmp()(*y.x, *x.x)) { return false; }
        return fot_cmp()(*x.y, *y.y);
    }

protected:
    Equal(const std::shared_ptr<const FOT> &x, const std::shared_ptr<const FOT> &y) : x(x), y(y) {}

private:
    std::shared_ptr<const FOT> x;
    std::shared_ptr<const FOT> y;
};

class Distinct : public FOF, public gio::virtual_enable_create<Distinct>, public inheritance_impl<Distinct, fof_inheritance> {
public:
    void print_to(std::ostream &s) const override {
        s << "(" << *this->x << "≠" << *this->y << ")";
    }

    static bool compare(const Distinct &x, const Distinct &y) {
        if (fot_cmp()(*x.x, *y.x)) { return true; }
        if (fot_cmp()(*y.x, *x.x)) { return false; }
        return fot_cmp()(*x.y, *y.y);
    }

protected:
    Distinct(const std::shared_ptr<const FOT> &x, const std::shared_ptr<const FOT> &y) : x(x), y(y) {}

private:
    std::shared_ptr<const FOT> x;
    std::shared_ptr<const FOT> y;
};

template<typename T>
class FOF2 : public FOF {
public:
    void print_to(std::ostream &s) const override {
        s << "(" << *this->left << this->get_symbol() << *this->right << ")";
    }

    const std::shared_ptr<const FOF> &get_left() const {
        return this->left;
    }

    const std::shared_ptr<const FOF> &get_right() const {
        return this->right;
    }

    static bool compare(const T &x, const T &y) {
        if (fof_cmp()(*x.left, *y.left)) { return true; }
        if (fof_cmp()(*y.left, *x.left)) { return false; }
        return fof_cmp()(*x.right, *y.right);
    }

protected:
    FOF2(const std::shared_ptr<const FOF> &left, const std::shared_ptr<const FOF> &right) : left(left), right(right) {}
    virtual const std::string &get_symbol() const = 0;

    std::shared_ptr<const FOF> left;
    std::shared_ptr<const FOF> right;
};

class And : public FOF2<And>, public gio::virtual_enable_create<And>, public inheritance_impl<And, fof_inheritance> {
    using FOF2::FOF2;
protected:
    virtual const std::string &get_symbol() const override final {
        static const std::string symbol = "∧";
        return symbol;
    }
};

class Or : public FOF2<Or>, public gio::virtual_enable_create<Or>, public inheritance_impl<Or, fof_inheritance> {
    using FOF2::FOF2;
protected:
    virtual const std::string &get_symbol() const override final {
        static const std::string symbol = "∨";
        return symbol;
    }
};

class Iff : public FOF2<Iff>, public gio::virtual_enable_create<Iff>, public inheritance_impl<Iff, fof_inheritance> {
    using FOF2::FOF2;
protected:
    virtual const std::string &get_symbol() const override final {
        static const std::string symbol = "⇔";
        return symbol;
    }
};

class Xor : public FOF2<Xor>, public gio::virtual_enable_create<Xor>, public inheritance_impl<Xor, fof_inheritance> {
    using FOF2::FOF2;
protected:
    virtual const std::string &get_symbol() const override final {
        static const std::string symbol = "⊻";
        return symbol;
    }
};

class Not : public FOF, public gio::virtual_enable_create<Not>, public inheritance_impl<Not, fof_inheritance> {
public:
    void print_to(std::ostream &s) const override {
        s << "¬" << *this->arg;
    }

    const std::shared_ptr<const FOF> &get_arg() const {
        return this->arg;
    }

    static bool compare(const Not &x, const Not &y) {
        return fof_cmp()(*x.arg, *y.arg);
    }

protected:
    Not(const std::shared_ptr<const FOF> &arg) : arg(arg) {}

private:
    std::shared_ptr<const FOF> arg;
};

class Implies : public FOF2<Implies>, public gio::virtual_enable_create<Implies>, public inheritance_impl<Implies, fof_inheritance> {
    using FOF2::FOF2;
protected:
    virtual const std::string &get_symbol() const override final {
        static const std::string symbol = "⇒";
        return symbol;
    }
};

class Oeq : public FOF2<Oeq>, public gio::virtual_enable_create<Oeq>, public inheritance_impl<Oeq, fof_inheritance> {
    using FOF2::FOF2;
protected:
    virtual const std::string &get_symbol() const override final {
        static const std::string symbol = "≈";
        return symbol;
    }
};

class Forall : public FOF, public gio::virtual_enable_create<Forall>, public inheritance_impl<Forall, fof_inheritance> {
public:
    void print_to(std::ostream &s) const override {
        s << "∀" << *this->var << " " << *this->x;
    }

    static bool compare(const Forall &x, const Forall &y) {
        if (fot_cmp()(*x.var, *y.var)) { return true; }
        if (fot_cmp()(*y.var, *x.var)) { return false; }
        return fof_cmp()(*x.x, *y.x);
    }

protected:
    Forall(const std::shared_ptr<const Variable> &var, const std::shared_ptr<const FOF> &x) : var(var), x(x) {}

private:
    std::shared_ptr<const Variable> var;
    std::shared_ptr<const FOF> x;
};

class Exists : public FOF, public gio::virtual_enable_create<Exists>, public inheritance_impl<Exists, fof_inheritance> {
public:
    void print_to(std::ostream &s) const override {
        s << "∃" << *this->var << " " << *this->x;
    }

    static bool compare(const Exists &x, const Exists &y) {
        if (fot_cmp()(*x.var, *y.var)) { return true; }
        if (fot_cmp()(*y.var, *x.var)) { return false; }
        return fof_cmp()(*x.x, *y.x);
    }

protected:
    Exists(const std::shared_ptr<const Variable> &var, const std::shared_ptr<const FOF> &x) : var(var), x(x) {}

private:
    std::shared_ptr<const Variable> var;
    std::shared_ptr<const FOF> x;
};

}
