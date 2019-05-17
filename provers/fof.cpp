
#include "fof.h"

namespace gio::mmpp::provers::fof {

void Functor::print_to(std::ostream &s) const {
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

bool Functor::has_free_var(const std::string &name) const {
    for (const auto &arg : this->args) {
        if (arg->has_free_var(name)) return true;
    }
    return false;
}

std::shared_ptr<const FOT> Functor::replace(const std::string &var_name, const std::shared_ptr<const FOT> &term) const {
    std::vector<std::shared_ptr<const FOT>> new_args;
    for (const auto &arg : this->args) {
        new_args.push_back(arg->replace(var_name, term));
    }
    if (new_args == this->args) {
        return this->virtual_enable_create<Functor>::shared_from_this();
    } else {
        return Functor::create(this->name, new_args);
    }
}

bool Functor::compare(const Functor &x, const Functor &y) {
    if (x.name < y.name) { return true; }
    if (y.name < x.name) { return false; }
    return std::lexicographical_compare(x.args.begin(), x.args.end(), y.args.begin(), y.args.end(), [](const auto &x, const auto &y) {
        return fot_cmp()(*x, *y);
    });
}

Functor::Functor(const std::string &name, const std::vector<std::shared_ptr<const FOT> > &args) : name(name), args(args) {}

void Variable::print_to(std::ostream &s) const {
    s << this->name;
}

bool Variable::has_free_var(const std::string &name) const {
    return name == this->name;
}

std::shared_ptr<const FOT> Variable::replace(const std::string &var_name, const std::shared_ptr<const FOT> &term) const {
    if (var_name == this->name) {
        return term;
    } else {
        return this->virtual_enable_create<Variable>::shared_from_this();
    }
}

const std::string &Variable::get_name() const {
    return this->name;
}

bool Variable::compare(const Variable &x, const Variable &y) {
    return x.name < y.name;
}

Variable::Variable(const std::string &name) : name(name) {}

void Predicate::print_to(std::ostream &s) const {
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

bool Predicate::has_free_var(const std::string &name) const {
    for (const auto &arg : this->args) {
        if (arg->has_free_var(name)) return true;
    }
    return false;
}

std::shared_ptr<const FOF> Predicate::replace(const std::string &var_name, const std::shared_ptr<const FOT> &term) const {
    std::vector<std::shared_ptr<const FOT>> new_args;
    for (const auto &arg : this->args) {
        new_args.push_back(arg->replace(var_name, term));
    }
    if (new_args == this->args) {
        return this->virtual_enable_create<Predicate>::shared_from_this();
    } else {
        return Predicate::create(this->name, new_args);
    }
}

bool Predicate::compare(const Predicate &x, const Predicate &y) {
    if (x.name < y.name) { return true; }
    if (y.name < x.name) { return false; }
    return std::lexicographical_compare(x.args.begin(), x.args.end(), y.args.begin(), y.args.end(), [](const auto &x, const auto &y) {
        return fot_cmp()(*x, *y);
    });
}

Predicate::Predicate(const std::string &name, const std::vector<std::shared_ptr<const FOT> > &args) : name(name), args(args) {}

void True::print_to(std::ostream &s) const {
    s << "⊤";
}

bool True::has_free_var(const std::string &name) const {
    (void) name;
    return false;
}

std::shared_ptr<const FOF> True::replace(const std::string &var_name, const std::shared_ptr<const FOT> &term) const {
    (void) var_name;
    (void) term;
    return this->virtual_enable_create<True>::shared_from_this();
}

bool True::compare(const True &x, const True &y) {
    (void) x;
    (void) y;
    return false;
}

True::True() {}

void False::print_to(std::ostream &s) const {
    s << "⊥";
}

bool False::has_free_var(const std::string &name) const {
    (void) name;
    return false;
}

std::shared_ptr<const FOF> False::replace(const std::string &var_name, const std::shared_ptr<const FOT> &term) const {
    (void) var_name;
    (void) term;
    return this->virtual_enable_create<False>::shared_from_this();
}

bool False::compare(const False &x, const False &y) {
    (void) x;
    (void) y;
    return false;
}

False::False() {}

void Equal::print_to(std::ostream &s) const {
    s << "(" << *this->left << "=" << *this->right << ")";
}

bool Equal::has_free_var(const std::string &name) const {
    return this->left->has_free_var(name) || this->right->has_free_var(name);
}

std::shared_ptr<const FOF> Equal::replace(const std::string &var_name, const std::shared_ptr<const FOT> &term) const {
    auto new_left = this->left->replace(var_name, term);
    auto new_right = this->right->replace(var_name, term);
    if (new_left == this->left && new_right == this->right) {
        return this->virtual_enable_create<Equal>::shared_from_this();
    } else {
        return Equal::create(new_left, new_right);
    }
}

bool Equal::compare(const Equal &x, const Equal &y) {
    if (fot_cmp()(*x.left, *y.left)) { return true; }
    if (fot_cmp()(*y.left, *x.left)) { return false; }
    return fot_cmp()(*x.right, *y.right);
}

Equal::Equal(const std::shared_ptr<const FOT> &x, const std::shared_ptr<const FOT> &y) : left(x), right(y) {}

void Distinct::print_to(std::ostream &s) const {
    s << "(" << *this->left << "≠" << *this->right << ")";
}

bool Distinct::has_free_var(const std::string &name) const {
    return this->left->has_free_var(name) || this->right->has_free_var(name);
}

std::shared_ptr<const FOF> Distinct::replace(const std::string &var_name, const std::shared_ptr<const FOT> &term) const {
    auto new_left = this->left->replace(var_name, term);
    auto new_right = this->right->replace(var_name, term);
    if (new_left == this->left && new_right == this->right) {
        return this->virtual_enable_create<Distinct>::shared_from_this();
    } else {
        return Distinct::create(new_left, new_right);
    }
}

bool Distinct::compare(const Distinct &x, const Distinct &y) {
    if (fot_cmp()(*x.left, *y.left)) { return true; }
    if (fot_cmp()(*y.left, *x.left)) { return false; }
    return fot_cmp()(*x.right, *y.right);
}

Distinct::Distinct(const std::shared_ptr<const FOT> &x, const std::shared_ptr<const FOT> &y) : left(x), right(y) {}

template<typename T>
void FOF2<T>::print_to(std::ostream &s) const {
    s << "(" << *this->left << this->get_symbol() << *this->right << ")";
}

template<typename T>
bool FOF2<T>::has_free_var(const std::string &name) const {
    return this->left->has_free_var(name) || this->right->has_free_var(name);
}

template<typename T>
std::shared_ptr<const FOF> FOF2<T>::replace(const std::string &var_name, const std::shared_ptr<const FOT> &term) const {
    auto new_left = this->left->replace(var_name, term);
    auto new_right = this->right->replace(var_name, term);
    if (new_left == this->left && new_right == this->right) {
        return this->virtual_enable_shared_from_this<FOF2<T>>::shared_from_this();
    } else {
        return T::create(new_left, new_right);
    }
}

template<typename T>
const std::shared_ptr<const FOF> &FOF2<T>::get_left() const {
    return this->left;
}

template<typename T>
const std::shared_ptr<const FOF> &FOF2<T>::get_right() const {
    return this->right;
}

template<typename T>
bool FOF2<T>::compare(const T &x, const T &y) {
    if (fof_cmp()(*x.left, *y.left)) { return true; }
    if (fof_cmp()(*y.left, *x.left)) { return false; }
    return fof_cmp()(*x.right, *y.right);
}

template<typename T>
FOF2<T>::FOF2(const std::shared_ptr<const FOF> &left, const std::shared_ptr<const FOF> &right) : left(left), right(right) {}

const std::string &And::get_symbol() const {
    static const std::string symbol = "∧";
    return symbol;
}

const std::string &Or::get_symbol() const {
    static const std::string symbol = "∨";
    return symbol;
}

const std::string &Iff::get_symbol() const {
    static const std::string symbol = "⇔";
    return symbol;
}

const std::string &Xor::get_symbol() const {
    static const std::string symbol = "⊻";
    return symbol;
}

void Not::print_to(std::ostream &s) const {
    s << "¬" << *this->arg;
}

bool Not::has_free_var(const std::string &name) const {
    return this->arg->has_free_var(name);
}

std::shared_ptr<const FOF> Not::replace(const std::string &var_name, const std::shared_ptr<const FOT> &term) const {
    auto new_arg = this->arg->replace(var_name, term);
    if (new_arg == this->arg) {
        return this->virtual_enable_create<Not>::shared_from_this();
    } else {
        return Not::create(new_arg);
    }
}

const std::shared_ptr<const FOF> &Not::get_arg() const {
    return this->arg;
}

bool Not::compare(const Not &x, const Not &y) {
    return fof_cmp()(*x.arg, *y.arg);
}

Not::Not(const std::shared_ptr<const FOF> &arg) : arg(arg) {}

const std::string &Implies::get_symbol() const {
    static const std::string symbol = "⇒";
    return symbol;
}

const std::string &Oeq::get_symbol() const {
    static const std::string symbol = "≈";
    return symbol;
}

void Forall::print_to(std::ostream &s) const {
    s << "∀" << *this->var << " " << *this->arg;
}

bool Forall::has_free_var(const std::string &name) const {
    if (this->var->get_name() == name) return false;
    return this->arg->has_free_var(name);
}

std::shared_ptr<const FOF> Forall::replace(const std::string &var_name, const std::shared_ptr<const FOT> &term) const {
    if (var_name == this->var->get_name()) {
        // Easy case: we do not have to do anything when replacing the quantified variable
        return this->virtual_enable_create<Forall>::shared_from_this();
    }
    gio::assert_or_throw<std::runtime_error>(!term->has_free_var(this->var->get_name()), "replacement would cause variable capture");
    auto new_arg = this->arg->replace(var_name, term);
    if (new_arg == this->arg) {
        return this->virtual_enable_create<Forall>::shared_from_this();
    } else {
        return Forall::create(this->var, new_arg);
    }
}

const std::shared_ptr<const Variable> &Forall::get_var() const {
    return this->var;
}

const std::shared_ptr<const FOF> &Forall::get_arg() const {
    return this->arg;
}

bool Forall::compare(const Forall &x, const Forall &y) {
    if (fot_cmp()(*x.var, *y.var)) { return true; }
    if (fot_cmp()(*y.var, *x.var)) { return false; }
    return fof_cmp()(*x.arg, *y.arg);
}

Forall::Forall(const std::shared_ptr<const Variable> &var, const std::shared_ptr<const FOF> &x) : var(var), arg(x) {}

void Exists::print_to(std::ostream &s) const {
    s << "∃" << *this->var << " " << *this->arg;
}

bool Exists::has_free_var(const std::string &name) const {
    if (this->var->get_name() == name) return false;
    return this->arg->has_free_var(name);
}

std::shared_ptr<const FOF> Exists::replace(const std::string &var_name, const std::shared_ptr<const FOT> &term) const {
    if (var_name == this->var->get_name()) {
        // Easy case: we do not have to do anything when replacing the quantified variable
        return this->virtual_enable_create<Exists>::shared_from_this();
    }
    gio::assert_or_throw<std::runtime_error>(!term->has_free_var(this->var->get_name()), "replacement would cause variable capture");
    auto new_arg = this->arg->replace(var_name, term);
    if (new_arg == this->arg) {
        return this->virtual_enable_create<Exists>::shared_from_this();
    } else {
        return Exists::create(this->var, new_arg);
    }
}

const std::shared_ptr<const Variable> &Exists::get_var() const {
    return this->var;
}

const std::shared_ptr<const FOF> &Exists::get_arg() const {
    return this->arg;
}

bool Exists::compare(const Exists &x, const Exists &y) {
    if (fot_cmp()(*x.var, *y.var)) { return true; }
    if (fot_cmp()(*y.var, *x.var)) { return false; }
    return fof_cmp()(*x.arg, *y.arg);
}

Exists::Exists(const std::shared_ptr<const Variable> &var, const std::shared_ptr<const FOF> &x) : var(var), arg(x) {}

template class FOF2<And>;
template class FOF2<Or>;
template class FOF2<Iff>;
template class FOF2<Xor>;
template class FOF2<Implies>;
template class FOF2<Oeq>;

}
