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
 * create block factories
 **********************************************************************/
#for $factory in $factories
#if $factory.namespace
using namespace $factory.namespace;
#end if

static std::shared_ptr<Pothos::Block> factory__$(factory.name)($factory.exported_factory_args)
{
    auto __block = $(factory.factory_function_path)($factory.internal_factory_args);
    #if $factory.block_methods
    auto __block_ref = std::ref(*static_cast<$factory.namespace::$factory.className *>(__block.get()));
    #end if
    #for $method in $factory.block_methods
    __block->registerCallable("$method.name", Pothos::Callable(&$factory.namespace::$factory.className::$method.name).bind(__block_ref, 0));
    #end for
    return to_std_ptr(boost::dynamic_pointer_cast<Pothos::Block>(__block));
}
#end for

/***********************************************************************
 * meta block factories
 **********************************************************************/
#for $factory in $meta_factories
static std::shared_ptr<Pothos::Block> factory__$(factory.name)($factory.exported_factory_args)
{
    #for $sub_factory in $factory.sub_factories
    if ($factory.type_key == "$sub_factory.name") return factory__$(sub_factory.name)($sub_factory.internal_factory_args);
    #end for

    throw Pothos::RuntimeException("$factory.name unknown type: "+$factory.type_key);
}
#end for

/***********************************************************************
 * register block factories
 **********************************************************************/
#for $registration in $registrations
static Pothos::BlockRegistry register__$(registration.name)("$registration.path", &factory__$(registration.name));
#end for

/***********************************************************************
 * enum conversions
 **********************************************************************/
#for $enum in $enums
$(enum.namespace)$(enum.name) string_to_$(enum.name)(const std::string &s)
{
    #for $value in $enum.values
    if (s == "$value.name") return $(enum.namespace)$(value.name);
    #end for
    throw Pothos::RuntimeException("convert string to $enum.name unknown value: "+s);
}
#end for

/***********************************************************************
 * register block descriptions and conversions
 **********************************************************************/
\#include <Pothos/Plugin.hpp>

pothos_static_block(registerGrPothosUtilBlockDocs)
{
    #for $path, $blockDesc in $blockDescs.iteritems()
    #set $escaped = ''.join([hex(ord(ch)).replace('0x', '\\x') for ch in $blockDesc])
    Pothos::PluginRegistry::add("/blocks/docs$path", std::string("$escaped"));
    #end for
    #for $enum in $enums
    Pothos::PluginRegistry::add("/object/convert/gr_enums/string_to_$(enum.name)", Pothos::Callable(&string_to_$(enum.name)));
    #end for
}
