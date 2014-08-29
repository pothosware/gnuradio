########################################################################
## The GrPothosUtil helps to bind GR blocks into the Pothos plugin tree
########################################################################

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
        sys.stderr.write(str(ex))
        return

    for className, classInfo in cppHeader.CLASSES.iteritems():
        if not is_this_class_a_block(classInfo): continue
        yield (className, classInfo)

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
def create_block_path(className, classInfo):
    ns = classInfo['namespace']
    ns = ns.replace('::', '/')
    if ns: return '/' + ns + '/' + className
    else: return '/' + className

def getFactoryInfo(className, classInfo):

    factory_function_path = []
    factory_function_args_types_names = []
    factory_function_args_only_names = []

    for method in classInfo['methods']['public']:
        if not method['static']: continue
        if 'make' not in method['name']: continue

        factory_function_path.append(className)
        factory_function_path.append(method['name'])

        for param in method['parameters']:
            if 'enum' in param['type']: print param
            factory_function_args_types_names.append('%s %s'%(param['type'], param['name']))
            factory_function_args_only_names.append(param['name'])

        break

    if not factory_function_path:
        raise Exception('cant find factory function')

    return dict(
        namespace=classInfo['namespace'],
        factory_function_path='::'.join(factory_function_path),
        factory_function_args_types_names=', '.join(factory_function_args_types_names),
        factory_function_args_only_names=', '.join(factory_function_args_only_names),
        path=create_block_path(className, classInfo),
        name=className
    )

def getBlockInfoJSON(className, classInfo):
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
            for className, classInfo in inspect_header(os.path.abspath(header)):
                try:
                    factories.append(getFactoryInfo(className, classInfo))
                    blockDescs.append(getBlockInfoJSON(className, classInfo))
                    headers.append(header)
                except Exception as ex: sys.stderr.write(str(ex))

    output = classInfoIntoRegistration(
        headers=set(headers),
        factories=factories,
        blockDescs=blockDescs
    )
    if out_path: open(out_path, 'w').write(output)
    #else: print(output)
