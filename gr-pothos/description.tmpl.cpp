/***********************************************************************
 * register block descriptions
 **********************************************************************/
\#include <Pothos/Plugin.hpp>

pothos_static_block(registerGrPothosUtilBlockDocs)
{
    #for $path, $blockDesc in $blockDescs.iteritems()
    #set $escaped = ''.join([hex(ord(ch)).replace('0x', '\\x') for ch in $blockDesc])
    Pothos::PluginRegistry::add("/blocks/docs$path", std::string("$escaped"));
    #end for
}
