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
    except: return

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
    return dict(
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

if __name__ == '__main__':
    if len(sys.argv) < 3:
        print('Usage: GrPothosUtil out.cpp paths/to/tree/with/includes')
        exit(-1)

    out_path = sys.argv[1]
    tree_paths = sys.argv[2:]

    headers = list()
    factories = list()
    blockDescs = list()
    for tree_path in tree_paths:
        for header in glob_recurse(find_include_root(tree_path), "*.h"):
            for className, classInfo in inspect_header(os.path.abspath(header)):
                factories.append(getFactoryInfo(className, classInfo))
                blockDescs.append(getBlockInfoJSON(className, classInfo))
                headers.append(header)

    open(out_path, 'w').write(classInfoIntoRegistration(
        headers=set(headers),
        factories=factories,
        blockDescs=blockDescs
    ))
