"""Make a self-contained PY32 BSP distribution, including shared packages."""

import os
import shutil
import stat

EXCLUDED_FILE_SUFFIXES = ('.o', '.obj', '.d', '.dep', '.elf', '.axf', '.map',
                          '.lst', '.su', '.pyc', '.pyo')

# Development-only top-level entries of the SDK repository. The package root is
# the SDK root, so these must not reach a distributed board package.
EXCLUDED_TOP_LEVEL = ('.git', '.cache', '.github', 'tools', 'docs', 'dist',
                      'AGENTS.md', 'prompt.md')

BRIDGE_SCONSCRIPT = '''import os
from building import *

cwd = GetCurrentDir()
package = os.path.basename(cwd)
source = os.path.abspath(os.path.join(cwd, '..', '..', 'libraries', package, 'SConscript'))
variant_dir = os.path.join('..', '..', 'packages', package)

objs = []
if os.path.isfile(source):
    result = SConscript(source, variant_dir=variant_dir, duplicate=0)
    if isinstance(result, list):
        objs += result
    elif result:
        objs.append(result)
else:
    print('warning: PY32 SDK source not found: ' + source)

Return('objs')
'''


def _remove_generated(path, dist_dir):
    """Remove copied metadata only after checking the resolved output boundary."""
    boundary = os.path.realpath(dist_dir)
    target = os.path.realpath(path)
    if os.path.commonpath([boundary, target]) != boundary or target == boundary:
        raise RuntimeError('Refusing to remove a path outside the distribution: ' + path)
    if os.path.isdir(path):
        def retry_readonly(function, filename, error):
            if not isinstance(error[1], PermissionError):
                raise error[1]
            os.chmod(filename, stat.S_IREAD | stat.S_IWRITE)
            function(filename)

        shutil.rmtree(path, onerror=retry_readonly)
    else:
        os.remove(path)


def _package_ignore(source_root, excluded_subtrees):
    """Apply the consuming BSP's SDK layout policy while copying its package."""
    def ignore(path, names):
        relative = os.path.relpath(path, source_root).replace(os.sep, '/')
        ignored = set() if relative != '.' else set(EXCLUDED_TOP_LEVEL)
        return [name for name in names
                if name in ignored or name == '__pycache__'
                or os.path.relpath(os.path.join(path, name), source_root).replace(os.sep, '/') in excluded_subtrees
                or name.lower().endswith(EXCLUDED_FILE_SUFFIXES)
                or name.startswith('.sconsign')]

    return ignore


def dist_do_building(bsp_root, dist_dir, sdk_name, excluded_subtrees):
    from env_package import find_libraries_path_upward, find_package_path
    from mkdist import bsp_copy_files

    # mkdist swallows a failed copy of the BSP tree (for example when a
    # previous dist directory is still open in another process) and keeps
    # going, which would leave a silently broken distribution. Fail loudly.
    missing = [name for name in ('SConstruct', 'SConscript', 'Kconfig',
                                 'rtconfig.py', 'board', 'applications')
               if not os.path.exists(os.path.join(dist_dir, name))]
    if missing:
        raise RuntimeError('Incomplete distribution; copy failed earlier: '
                           + ', '.join(missing))

    libraries = find_libraries_path_upward(bsp_root)
    if not libraries:
        raise RuntimeError('PY32 BSP libraries directory is missing')
    dest_libraries = os.path.join(dist_dir, 'libraries')
    os.makedirs(dest_libraries, exist_ok=True)
    bsp_copy_files(os.path.join(libraries, 'drivers'),
                   os.path.join(dest_libraries, 'drivers'))
    shutil.copy2(os.path.join(libraries, 'Kconfig'), dest_libraries)

    # A distributed BSP must also be able to create another distribution.
    os.makedirs(os.path.join(dist_dir, 'tools'), exist_ok=True)
    shutil.copy2(__file__, os.path.join(dist_dir, 'tools', 'sdk_dist.py'))

    packages = os.path.join(dist_dir, 'packages')
    if not os.path.isdir(packages):
        return

    for name in os.listdir(packages):
        if name in ('py32-local.json', 'py32-download.json'):
            _remove_generated(os.path.join(packages, name), dist_dir)
            continue
        metadata = os.path.join(packages, name, '.git')
        if os.path.exists(metadata):
            _remove_generated(metadata, dist_dir)
        if not name.startswith(sdk_name + '-'):
            continue

        source = find_package_path(bsp_root, name, marker='SConscript')
        if not os.path.isfile(os.path.join(source, 'SConscript')):
            raise RuntimeError('PY32 SDK source is missing: ' + source)

        # Ship a pruned SDK in libraries/<package>, the layout Env creates for
        # every hal-sdk package, so the board package is replaceable by
        # pkgs --update like any other BSP.
        destination = os.path.join(dest_libraries, name)
        if os.path.isdir(destination):
            _remove_generated(destination, dist_dir)
        shutil.copytree(source, destination, ignore=_package_ignore(source, excluded_subtrees))

        # Keep packages/<package> as a thin forwarder. It must not look like
        # the Env bridge, otherwise mkdist would copy the unpruned checkout.
        forwarder = os.path.join(packages, name, 'SConscript')
        with open(forwarder, 'w', encoding='utf-8', newline='\n') as stream:
            stream.write(BRIDGE_SCONSCRIPT)

        for current, directories, filenames in os.walk(destination):
            for directory in list(directories):
                if directory == '__pycache__':
                    _remove_generated(os.path.join(current, directory), dist_dir)
                    directories.remove(directory)
            for filename in filenames:
                if (filename.lower().endswith(EXCLUDED_FILE_SUFFIXES)
                        or filename.startswith('.sconsign')):
                    _remove_generated(os.path.join(current, filename), dist_dir)
