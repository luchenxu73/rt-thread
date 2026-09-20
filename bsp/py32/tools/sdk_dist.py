"""Make a self-contained PY32 BSP distribution, including shared packages."""

import os
import shutil
import stat

EXCLUDED_FILE_SUFFIXES = ('.o', '.obj', '.d', '.dep', '.elf', '.axf', '.map',
                          '.lst', '.su', '.pyc', '.pyo')


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


def dist_do_building(bsp_root, dist_dir):
    from env_package import find_libraries_path_upward, find_package_path
    from mkdist import bsp_copy_files

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
        if not name.startswith('py32f403_sdk-'):
            continue
        source = find_package_path(bsp_root, name,
                                   marker='sdk/SConscript')
        if not os.path.isfile(os.path.join(source, 'sdk', 'SConscript')):
            raise RuntimeError('PY32 SDK source is missing: ' + source)
        destination = os.path.join(packages, name)
        for filename in ['SConscript', 'LICENSE', 'README.md']:
            path = os.path.join(source, filename)
            if os.path.isfile(path):
                shutil.copy2(path, os.path.join(destination, filename))

        # Replace the Env bridge with the actual package. This also prevents
        # mkdist from copying an editable SDK repository (including its .git).
        def ignore_sdk(path, names):
            ignored = {'.git', '.cache', '__pycache__', 'RTOS', 'RTOS2'}
            if os.path.basename(path) == 'CMSIS':
                ignored.update(['Include', 'Core'])
            if os.path.basename(path) == 'Drivers':
                ignored.add('BSP')
            return [name for name in names
                    if name in ignored or name.lower().endswith(EXCLUDED_FILE_SUFFIXES)
                    or name.startswith('.sconsign')]

        shutil.copytree(os.path.join(source, 'sdk'),
                        os.path.join(destination, 'sdk'),
                        dirs_exist_ok=True, ignore=ignore_sdk)

        # A regular downloaded package was already copied by mkdist. Remove
        # any build products from that first copy as well as from dev mappings.
        for item in os.listdir(destination):
            if item not in ['SConscript', 'LICENSE', 'README.md', 'sdk']:
                _remove_generated(os.path.join(destination, item), dist_dir)
        for current, directories, filenames in os.walk(destination):
            for directory in list(directories):
                if directory in ignore_sdk(current, [directory]):
                    _remove_generated(os.path.join(current, directory), dist_dir)
                    directories.remove(directory)
            for filename in filenames:
                if (filename.lower().endswith(EXCLUDED_FILE_SUFFIXES)
                        or filename.startswith('.sconsign')):
                    _remove_generated(os.path.join(current, filename), dist_dir)
