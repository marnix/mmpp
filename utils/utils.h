#pragma once

#include <set>
#include <string>
#include <vector>
#include <sstream>
#include <ostream>
#include <iostream>
#include <algorithm>
#include <cctype>
#include <locale>
#include <functional>
#include <iomanip>
#include <chrono>
#include <unordered_map>
#include <map>
#include <mutex>
#include <memory>
#include <cstdint>

#include <boost/filesystem.hpp>
#include <boost/archive/text_iarchive.hpp>
#include <boost/archive/text_oarchive.hpp>
#include <boost/iostreams/stream.hpp>
#include <boost/iostreams/concepts.hpp>

#include "platform.h"

/* A template struct without specializations: useful for checking variable types
 * (for example when there is automatic type deduction).
 * For example, if you declare
 *   TD< decltype(x) > x1;
 * the error message will probably mention the type of x.
 */
template< typename T >
struct TD;

const size_t DEFAULT_STACK_SIZE = 8*1024*1024;

extern bool mmpp_abort;

class MMPPException {
public:
    MMPPException(const std::string &reason="");
    const std::string &get_reason() const;
    const PlatformStackTrace &get_stacktrace() const;
    void print_stacktrace(std::ostream &st) const;

private:
    std::string reason;
    PlatformStackTrace stacktrace;
};

template< typename Exception, typename... Args >
inline static void assert_or_throw(bool cond, Args&&... args) {
    if (!cond) {
        throw Exception(std::forward< Args >(args)...);
    }
}

std::string size_to_string(uint64_t size);
bool starts_with(std::string a, std::string b);

struct Tic {
    std::chrono::steady_clock::time_point begin;
};

Tic tic();
void toc(const Tic &t, int reps);

std::map< std::string, std::function< int(int, char*[]) > > &get_main_functions();
void register_main_function(const std::string &name, const std::function< int(int, char*[]) > &main_function);

// static_block implementation from https://stackoverflow.com/a/34321324/807307
#define CONCATENATE(s1, s2) s1##s2
#define EXPAND_THEN_CONCATENATE(s1, s2) CONCATENATE(s1, s2)
#ifdef __COUNTER__
#define UNIQUE_IDENTIFIER(prefix) EXPAND_THEN_CONCATENATE(prefix, __COUNTER__)
#else
#define UNIQUE_IDENTIFIER(prefix) EXPAND_THEN_CONCATENATE(prefix, __LINE__)
#endif /* COUNTER */
#define static_block STATIC_BLOCK_IMPL1(UNIQUE_IDENTIFIER(_static_block_))
#define STATIC_BLOCK_IMPL1(prefix) \
    STATIC_BLOCK_IMPL2(CONCATENATE(prefix,_fn),CONCATENATE(prefix,_var))
#ifdef _MSC_VER
#define STATIC_BLOCK_IMPL2(function_name,var_name) \
static void function_name(); \
static int var_name = (function_name(), 0) ; \
static void function_name()
#else
#define STATIC_BLOCK_IMPL2(function_name,var_name) \
static void function_name(); \
static int var_name __attribute((unused)) = (function_name(), 0) ; \
static void function_name()
#endif

class Hasher {
public:
    virtual void update(const char* s, std::size_t n) = 0;
    virtual std::string get_digest() = 0;
};

class HashSink {
public:
    typedef char char_type;
    typedef boost::iostreams::sink_tag category;

    HashSink();
    std::streamsize write(const char *s, std::streamsize n);
    std::string get_digest();

private:
    std::shared_ptr< Hasher > hasher;
};

template< typename T >
std::string hash_object(const T &obj) {
    HashSink hasher;
    // g++ can automatically deduce the template parameter here, but clang++ cannot
    boost::iostreams::stream< HashSink > fout(hasher);
    {
        boost::archive::text_oarchive archive(fout);
        archive << obj;
    }
    fout.flush();
    return hasher.get_digest();
}

// Funny trick from https://stackoverflow.com/a/41485014/807307
template<typename S>
struct enable_make final : public S
{
    template<typename... T>
    enable_make(T&&... t)
        : S(std::forward<T>(t)...)
    {
    }
};

template< typename T >
struct enable_create : public std::enable_shared_from_this< T > {
    template< typename... Args >
    static std::shared_ptr< T > create(Args&&... args) {
        std::shared_ptr< enable_make< T > > pointer = std::make_shared< enable_make< T > >(std::forward< Args >(args)...);
        static_cast< std::shared_ptr< enable_create< T > > >(pointer)->init();
        return pointer;
    }

#if (!(__cpp_lib_enable_shared_from_this >= 201603))
    std::weak_ptr< T > weak_from_this() {
        return this->shared_from_this();
    }

    std::weak_ptr< const T > weak_from_this() const {
        return this->shared_from_this();
    }
#endif

protected:
    virtual void init() {}
};

// Taken from https://stackoverflow.com/a/45046349/807307 and adapted
static std::mutex mtx_cout;
struct acout
{
        std::unique_lock<std::mutex> lk;
        acout()
            :
              lk(std::unique_lock<std::mutex>(mtx_cout))
        {

        }

        template<typename T>
        acout& operator<<(const T& _t)
        {
            std::cout << _t;
            return *this;
        }

        acout& operator<<(std::ostream& (*fp)(std::ostream&))
        {
            std::cout << fp;
            return *this;
        }
};

class Reportable {
public:
    virtual void report(double current) = 0;
    virtual void set_total(double total) = 0;
    virtual ~Reportable();
};

class TextProgressBar : public Reportable {
public:
    TextProgressBar(size_t length=100, double total=0.0);
    void set_total(double total);
    void report(double current, bool force);
    void report(double current);
    void finished();

private:
    std::chrono::steady_clock::time_point last_report;
    size_t last_len;
    double total;
    size_t length;
};

class Finally {
public:
    Finally(std::function< void() > &&finally) : finally(std::move(finally)) {}
    ~Finally() {
        this->finally();
    }

private:
    std::function< void() > finally;
};

template< class T >
class SafeWeakPtr : public std::weak_ptr< T > {
public:
    constexpr SafeWeakPtr() noexcept : std::weak_ptr< T >() {
    }

    template< class U >
    SafeWeakPtr(const std::shared_ptr< U > &r) noexcept : std::weak_ptr< T >(r) {
    }

    template< class U >
    SafeWeakPtr(const std::weak_ptr< U > &r) noexcept : std::weak_ptr< T >(r) {
    }

    template< class U >
    SafeWeakPtr(const SafeWeakPtr< U > &r) noexcept : std::weak_ptr< T >(r) {
    }

    std::shared_ptr< T > lock() const noexcept {
        auto strong = this->std::weak_ptr<T>::lock();
        assert(strong);
        return strong;
    }
};

template< class It, class URBG >
It random_choose(It first, It last, URBG &&g) {
    return first + std::uniform_int_distribution< size_t >(0, (last - first) - 1)(g);
}

template< typename T >
typename std::vector< T >::reference enlarge_and_set(std::vector< T > &v, typename std::vector< T >::size_type pos) {
    v.resize(std::max(v.size(), pos+1));
    return v[pos];
}

extern std::ostream cnull;

template< typename UnaryOperation, typename InputIt >
auto vector_map(InputIt from, InputIt to, UnaryOperation op) -> std::vector< decltype(op(*from)) > {
    std::vector< decltype(op(*from)) > ret;
    std::transform(from, to, std::back_inserter(ret), op);
    return ret;
}

template< typename T >
using Generator = coroutine_pull< T >;

template< typename It1, typename It2 >
bool is_disjoint(It1 from1, It1 to1, It2 from2, It2 to2) {
    while (true) {
        if (from1 == to1) {
            return true;
        }
        if (from2 == to2) {
            return true;
        }
        if (*from1 < *from2) {
            ++from1;
            continue;
        }
        if (*from2 < *from1) {
            ++from2;
            continue;
        }
        return false;
    }
}

template< typename It1, typename It2 >
bool is_included(It1 from1, It1 to1, It2 from2, It2 to2) {
    while (true) {
        if (from1 == to1) {
            return true;
        }
        if (from2 == to2) {
            return false;
        }
        if (*from1 < *from2) {
            return false;
        }
        if (*from2 < *from1) {
            ++from2;
            continue;
        }
        ++from1;
        ++from2;
    }
}

template< typename It >
bool has_no_diagonal(It from, It end) {
    while (true) {
        if (from == end) {
            return true;
        }
        if (from->first == from->second) {
            return false;
        }
        ++from;
    }
}

void default_exception_handler(std::exception_ptr ptr);
