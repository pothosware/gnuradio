########################################################################
## The GrPothosUtil helps to bind GR blocks into the Pothos plugin tree
########################################################################

########################################################################
## debug messages for build verbose
########################################################################
HEADER = '\033[95m'
OKBLUE = '\033[94m'
OKGREEN = '\033[92m'
WARNING = '\033[93m'
FAIL = '\033[91m'
ENDC = '\033[0m'

import sys
def header(msg, *args): sys.stderr.write(HEADER+msg%args+"\n")
def notice(msg, *args): sys.stderr.write(OKGREEN+msg%args+"\n")
def warning(msg, *args): sys.stderr.write(WARNING+msg%args+"\n")
def error(msg, *args): sys.stderr.write(FAIL+msg%args+"\n")

########################################################################
## blacklists -- hopefully we can fix in the future
########################################################################
TARGET_BLACKLIST = [
    'gnuradio-runtime', #no blocks here
    'gnuradio-pmt', #no blocks here
    'gnuradio-qtgui', #compiler errors with binding functions -- fix later
]

NAMESPACE_BLACKLIST = [
]

CLASS_BLACKLIST = [
    'gr::blocks::multiply_matrix_cc', #causing weird linker error -- symbol missing why?
]

########################################################################
## directory and file utils
########################################################################
import fnmatch
import os

def glob_recurse(base, filt):
    for root, dirnames, filenames in os.walk(base):
      for filename in fnmatch.filter(filenames, filt):
          yield os.path.join(root, filename)

def find_dir_root(path, dirname):
    if os.path.dirname(path) == path: raise Exception('find_dir_root FAIL')
    if dirname in os.listdir(path): return os.path.join(path, dirname)
    return find_dir_root(os.path.dirname(path), dirname)

########################################################################
## single header inspection
########################################################################
import sys
import os
sys.path.append(os.path.dirname(__file__))
import CppHeaderParser

KNOWN_BASES = [
    'block',
    'sync_block',
    'sync_interpolator',
    'sync_decimator',
]
def fix_KNOWN_BASES():
    for base in KNOWN_BASES:
        yield base
        yield 'gr::'+base
KNOWN_BASES = list(fix_KNOWN_BASES())

def is_this_class_a_block(className, classInfo):

    if classInfo['namespace'] in NAMESPACE_BLACKLIST:
        warning('Blacklisted namespace: %s', classInfo['namespace'])
        return False

    fully_qualified = classInfo['namespace']+'::'+className
    if fully_qualified in CLASS_BLACKLIST:
        warning('Blacklisted class: %s', fully_qualified)
        return False

    for inherit in classInfo['inherits']:
        if inherit['access'] != 'public': return False
        if inherit['class'] in KNOWN_BASES: return True
    return False

def inspect_header(header_path):
    #notice('Inspecting: %s', header_path)
    contents = open(header_path).read()

    #remove API decl tokens so the lexer doesnt have to
    pp_tokens = list()
    for line in contents.splitlines():
        line = line.strip()
        if line.startswith('class'):
            api_decl = line.split()[1].strip()
            if api_decl.isupper(): pp_tokens.append(api_decl)
    for tok in set(pp_tokens): contents = contents.replace(tok, '')

    try: cppHeader = CppHeaderParser.CppHeader(contents, argType='string')
    except Exception as ex:
        warning('Inspect %s failed with %s', header_path, str(ex))
        return

    for className, classInfo in cppHeader.CLASSES.iteritems():
        if not is_this_class_a_block(className, classInfo): continue
        #notice('  Found: %s::%s', classInfo['namespace'], className)
        yield (className, classInfo, cppHeader)

def gather_header_data(tree_paths):
    for tree_path in tree_paths:
        for header in glob_recurse(find_dir_root(tree_path, 'include'), "*.h"):
            for className, classInfo, cppHeader in inspect_header(os.path.abspath(header)):
                yield className, classInfo, cppHeader, header

########################################################################
## class info into a C++ source
########################################################################
REGISTRATION_TMPL_FILE = os.path.join(os.path.dirname(__file__), 'registration.tmpl.cpp')
from Cheetah import Template

def classInfoIntoRegistration(**kwargs):
    tmpl_str = open(REGISTRATION_TMPL_FILE, 'r').read()
    return str(Template.Template(tmpl_str, kwargs))

########################################################################
## gather grc data
########################################################################
import xmltodict
import difflib
import copy

def gather_grc_data(tree_paths):
    for tree_path in tree_paths:
        for xml_file in glob_recurse(find_dir_root(tree_path, 'grc'), "*.xml"):
            yield os.path.basename(xml_file), xmltodict.parse(open(xml_file).read())

def getGrcFileMatch(className, classInfo, grc_files):

    qualified_name = classInfo['namespace'].replace('::', '_')+'_'+className
    if qualified_name.startswith('gr_'): qualified_name = qualified_name[3:]
    matches = difflib.get_close_matches(word=qualified_name, possibilities=grc_files, n=1)
    if matches: return matches[0]

    raise Exception('Cant find GRC match for %s'%className)

def grcCategoryRecurse(data, names=[]):
    if 'name' in data and data['name']:
        names.append(data['name'])

    if 'block' in data:
        blocks = data['block']
        if not isinstance(blocks, list): blocks = [blocks]
        for block in blocks: yield block, names

    if 'cat' in data:
        cats = data['cat']
        if not isinstance(cats, list): cats = [cats]
        for cat in cats:
            for x in grcCategoryRecurse(cat, copy.copy(names)): yield x

def grcBlockKeyToCategoryMap(grc_data):
    key_to_categories = dict()
    for file_name, data in grc_data.iteritems():
        if 'cat' not in data: continue
        for blockKey, catNames in grcCategoryRecurse(data['cat']):
            catName = '/'.join(catNames)
            if blockKey not in key_to_categories:
                key_to_categories[blockKey] = list()
            key_to_categories[blockKey].append(catName)
    return key_to_categories

########################################################################
## extract and process a single class
########################################################################
MAX_ARGS = 8

def create_block_path(className, classInfo):
    ns = classInfo['namespace']
    ns = ns.replace('::', '/')
    if ns: return '/' + ns + '/' + className
    else: return '/' + className

def find_factory_function(className, classInfo, cppHeader):

    for method in classInfo['methods']['public']:
        if not method['static']: continue
        if 'make' not in method['name']: continue
        return method

    for function in cppHeader.functions:
        if 'make' not in function['name']: continue
        return function

    return None

def find_block_methods(classInfo):
    for method in classInfo['methods']['public']:
        if method['static']: continue
        if method['constructor']: continue
        if method['destructor']: continue
        if method['name'] in ('general_work', 'work', 'forecast'): continue
        if len(method['parameters']) > MAX_ARGS:
            warning("Too many parameters %s::%s ignored", classInfo['name'], method['name'])
            continue
        yield method

def getFactoryInfo(className, classInfo, cppHeader):

    #extract the factory method
    factory = find_factory_function(className, classInfo, cppHeader)
    if not factory:
        raise Exception('Cant find factory function for %s'%className)
    if len(factory['parameters']) > MAX_ARGS:
        raise Exception('Too many factory parameters for %s'%className)
    if 'parent' in factory: factory_path = [className, className, factory['name']]
    else: factory_path = [factory['namespace'], factory['name']]

    return dict(
        namespace=classInfo['namespace'],
        className=className,
        factory_function_path='::'.join(factory_path),
        factory_function_args_types_names=', '.join(['%s %s'%(p['type'], p['name']) for p in factory['parameters']]),
        factory_function_args_only_names=', '.join([p['name'] for p in factory['parameters']]),
        block_methods=find_block_methods(classInfo),
        path=create_block_path(className, classInfo),
        name=className
    )

def doxygenToDocLines(doxygen):
    in_ul_list = False

    for doxyline in doxygen.splitlines():

        #strip the front comment chars
        def front_strip(line, key):
            if line.startswith(key+' '): return line[len(key)+1:]
            if line.startswith(key): return line[len(key):]
            return line
        for begin in ('/*!', '*/', '//!', '//', '*'): doxyline = front_strip(doxyline, begin)
        for begin in ('\\brief', '\\details', '\\ingroup'): doxyline = front_strip(doxyline, begin)

        #unordered list support
        encountered_li = False
        if doxyline.startswith('\\li'):
            doxyline = doxyline.replace('\\li', '<li>') + '</li>'
            encountered_li = True

        #deal with adding ul tags
        if encountered_li and not in_ul_list:
            in_ul_list = True
            doxyline = '<ul>' + doxyline
        if in_ul_list and not encountered_li:
            in_ul_list = False
            doxyline = doxyline + '</ul>'

        #bold tags
        if doxyline.startswith('\\b'): doxyline = doxyline.replace('\\b', '<b>') + '</b>'

        #code blocks
        if doxyline.startswith('\\code'): doxyline = doxyline.replace('\\code', '<code>')
        if doxyline.startswith('\\endcode'): doxyline = doxyline.replace('\\endcode', '</code>')

        #formulas -- just do preformatted text for now
        if doxyline.startswith('\\f['): doxyline = doxyline.replace('\\f[', '<pre>')
        if doxyline.startswith('\\f]'): doxyline = doxyline.replace('\\f]', '</pre>')
        if doxyline.startswith('\\f$'): doxyline = doxyline.replace('\\f$', '<pre>') + '</pre>'

        #references -- put in italics
        if doxyline.startswith('\\sa'): doxyline = doxyline.replace('\\sa', '<i>') + '</i>'

        #sections -- become headings
        if doxyline.startswith('\\section'): doxyline = doxyline.replace('\\section', '<h2>') + '</h2>'
        if doxyline.startswith('\\subsection'): doxyline = doxyline.replace('\\subsection', '<h3>') + '</h3>'
        if doxyline.startswith('\\p'): doxyline = doxyline.replace('\\p', '<h4>') + '</h4>'

        if doxyline.startswith('\\'): warning('doxyparse unknown field %s', doxyline)
        yield doxyline

def getBlockInfoJSON(className, classInfo, cppHeader, blockData, key_to_categories):

    #extract GRC data as lists
    def get_as_list(data, key):
        try: out = data[key]
        except KeyError: out = list()
        if not isinstance(out, list): out = [out]
        return out
    grc_make = blockData['make']
    grc_params = get_as_list(blockData, 'param')
    grc_callbacks = get_as_list(blockData, 'callback')
    grc_callbacks_str = ', '.join(grc_callbacks)

    #determine params
    params = list()

    #factory
    args = list()

    #calls (setters/initializers)
    calls = list()
    for method in find_block_methods(classInfo):
        name = method['name']
        if not method['parameters']: continue #ignore getters
        if name not in grc_make and name not in grc_callbacks_str:
            notice("method %s::%s not used in GRC", className, name)

    #category extraction
    categories = list()
    try: categories.append(blockData['category'])
    except KeyError: pass
    try: categories.extend(key_to_categories[blockData['key']])
    except KeyError: pass
    if not categories: warning("Not block categories found: %s", className)
    categories = [c if c.startswith('/') else ('/'+c) for c in categories]

    return dict(
        path=create_block_path(className, classInfo),
        keywords=[className, classInfo['namespace']],
        name=blockData['name'],
        categories=categories,
        calls=calls, #setters list
        params=params, #parameters list
        args=args, #factory function args
        docs=list(doxygenToDocLines(classInfo['doxygen'])),
    )

########################################################################
## main
########################################################################
import sys
import json
from optparse import OptionParser

if __name__ == '__main__':

    #parse the input arguments
    parser = OptionParser()
    parser.add_option("--out", dest="out_path", help="output file path or 'stdout'")
    parser.add_option("--target", help="associated cmake library target name")
    (options, args) = parser.parse_args()
    out_path = options.out_path
    tree_paths = args
    header("GrPothosUtil begin: target=%s, out=%s", options.target, out_path)

    #generator information
    headers = list()
    factories = list()
    blockDescs = list()

    #warning blacklist for issues
    if options.target in TARGET_BLACKLIST:
        warning('Blacklisted target: %s', options.target)

    #otherwise continue to parse
    else:
        #extract grc metadata
        grc_data = dict(gather_grc_data(tree_paths))
        key_to_categories = grcBlockKeyToCategoryMap(grc_data)

        #extract header data
        header_data = gather_header_data(tree_paths)

        #extract info for each block class
        for className, classInfo, cppHeader, headerPath in header_data:
            try:
                metadata = grc_data[getGrcFileMatch(className, classInfo, grc_data.keys())]['block']
                factory = getFactoryInfo(className, classInfo, cppHeader)
                blockDesc = getBlockInfoJSON(className, classInfo, cppHeader, metadata, key_to_categories)
                #FIXME having an issue with POCO stringify and unicode chars
                #just escape out the unicode escape for now to avoid issues...
                blockDesc = (blockDesc['path'], json.dumps(blockDesc).replace('\\u', '\\\\u'))
                factories.append(factory)
                blockDescs.append(blockDesc)
                headers.append(headerPath)
            except Exception as ex: warning(str(ex))

    #generate output source
    output = classInfoIntoRegistration(
        headers=set(headers),
        factories=factories,
        blockDescs=blockDescs
    )

    #send output to file or stdout
    if out_path:
        if out_path == 'stdout': print(output)
        else: open(out_path, 'w').write(output)
