########################################################################
## The GrPothosUtil helps to bind GR blocks into the Pothos plugin tree
########################################################################
import sys
def warning(msg, *args): sys.stderr.write('\033[93m'+msg%args+"\n")
def error(msg, *args): sys.stderr.write('\033[91m'+msg%args+"\n")

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

def is_this_class_a_block(classInfo):
    for inherit in classInfo['inherits']:
        if inherit['access'] != 'public': return False
        if inherit['class'] in KNOWN_BASES: return True
    return False

def inspect_header(header_path):
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
        if not is_this_class_a_block(classInfo): continue
        yield (className, classInfo, cppHeader)

########################################################################
## glob_recurse
########################################################################
import fnmatch
import os

def glob_recurse(base, filt):
    for root, dirnames, filenames in os.walk(base):
      for filename in fnmatch.filter(filenames, filt):
          yield os.path.join(root, filename)

########################################################################
## find include root
########################################################################
import os

def find_include_root(path):
    if os.path.dirname(path) == path: raise Exception('find_include_root FAIL')
    if 'include' in os.listdir(path): return os.path.join(path, 'include')
    return find_include_root(os.path.dirname(path))

########################################################################
## class info into a C++ source
########################################################################
REGISTRATION_TMPL_FILE = os.path.join(os.path.dirname(__file__), 'registration.tmpl.cpp')
from Cheetah import Template

def classInfoIntoRegistration(**kwargs):
    tmpl_str = open(REGISTRATION_TMPL_FILE, 'r').read()
    return str(Template.Template(tmpl_str, kwargs))

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

def getBlockInfoJSON(className, classInfo, cppHeader):
    return dict(
        path=create_block_path(className, classInfo),
    )

########################################################################
## main
########################################################################
import sys
from optparse import OptionParser

if __name__ == '__main__':
    parser = OptionParser()
    parser.add_option("--out", dest="out_path", help="write registration here")
    (options, args) = parser.parse_args()

    out_path = options.out_path
    tree_paths = args

    headers = list()
    factories = list()
    blockDescs = list()
    for tree_path in tree_paths:
        for header in glob_recurse(find_include_root(tree_path), "*.h"):
            for className, classInfo, cppHeader in inspect_header(os.path.abspath(header)):
                try:
                    factories.append(getFactoryInfo(className, classInfo, cppHeader))
                    blockDescs.append(getBlockInfoJSON(className, classInfo, cppHeader))
                    headers.append(header)
                except Exception as ex: warning(str(ex))

    output = classInfoIntoRegistration(
        headers=set(headers),
        factories=factories,
        blockDescs=blockDescs
    )
    if out_path: open(out_path, 'w').write(output)
    #else: print(output)
