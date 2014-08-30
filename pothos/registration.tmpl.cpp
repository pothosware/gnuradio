//this is a machine generated file...

\#include <Pothos/Framework.hpp>
\#include <gnuradio/block.h>

using namespace gr;

/***********************************************************************
 * helpers
 * http://stackoverflow.com/questions/6326757/conversion-from-boostshared-ptr-to-stdshared-ptr
 **********************************************************************/
\#include <boost/shared_ptr.hpp>
\#include <memory>

namespace {
    template<class SharedPointer> struct Holder {
        SharedPointer p;

        Holder(const SharedPointer &p) : p(p) {}
        Holder(const Holder &other) : p(other.p) {}
        Holder(Holder &&other) : p(std::move(other.p)) {}

        void operator () (...) { p.reset(); }
    };
}

template<class T> std::shared_ptr<T> to_std_ptr(const boost::shared_ptr<T> &p) {
    typedef Holder<std::shared_ptr<T>> H;
    if(H *h = boost::get_deleter<H, T>(p)) {
        return h->p;
    } else {
        return std::shared_ptr<T>(p.get(), Holder<boost::shared_ptr<T>>(p));
    }
}

template<class T> boost::shared_ptr<T> to_boost_ptr(const std::shared_ptr<T> &p){
    typedef Holder<boost::shared_ptr<T>> H;
    if(H * h = std::get_deleter<H, T>(p)) {
        return h->p;
    } else {
        return boost::shared_ptr<T>(p.get(), Holder<std::shared_ptr<T>>(p));
    }
}

/***********************************************************************
 * include block definitions
 **********************************************************************/
#for $header in $headers
\#include "$header"
#end for

/***********************************************************************
 * register block factories
 **********************************************************************/
#for $factory in $factories

#if $factory.namespace
using namespace $factory.namespace;
#end if

static std::shared_ptr<Pothos::Block> factory__$(factory.name)($factory.factory_function_args_types_names)
{
    auto block = $(factory.factory_function_path)($factory.factory_function_args_only_names);
    #for $method in $factory.block_methods
    block->registerCallable("$method.name", Pothos::Callable(&$factory.namespace::$factory.className::$method.name)
        .bind(std::ref(*static_cast<$factory.namespace::$factory.className *>(block.get())), 0));
    #end for
    return to_std_ptr(boost::dynamic_pointer_cast<Pothos::Block>(block));
}

static Pothos::BlockRegistry register__$(factory.name)("$factory.path", &factory__$(factory.name));

#end for

/***********************************************************************
 * register block descriptions
 **********************************************************************/
\#include <Pothos/Plugin.hpp>

pothos_static_block(registerGrPothosUtilBlockDocs)
{
    #for $path, $blockDesc in $blockDescs
    #set $escaped = ''.join([hex(ord(ch)).replace('0x', '\\x') for ch in $blockDesc])
    Pothos::PluginRegistry::add("/blocks/docs$path", std::string("$escaped"));
    #end for
}
