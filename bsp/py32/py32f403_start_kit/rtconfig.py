import os
import shutil
import subprocess


def _compiler_path(path):
    path = os.path.abspath(path)
    # RT-Thread's libc detector splits DEVICE on whitespace. Windows short
    # paths also keep tool invocations usable when an installation has spaces.
    if any(character.isspace() for character in path):
        if os.name == 'nt':
            import ctypes
            buffer = ctypes.create_unicode_buffer(32768)
            if ctypes.windll.kernel32.GetShortPathNameW(path, buffer, len(buffer)):
                path = buffer.value
        if any(character.isspace() for character in path):
            raise ValueError('Use a toolchain path without spaces: ' + path)
    return path.replace('\\', '/')

ARCH = 'arm'
CPU = 'cortex-m4'
CROSS_TOOL = os.getenv('RTT_CC', 'gcc')
BUILD = 'debug'
BSP_LIBRARY_TYPE = None

if CROSS_TOOL == 'gcc':
    PLATFORM = 'gcc'
    PREFIX = os.getenv('RTT_CC_PREFIX', 'arm-none-eabi-')
    CC = PREFIX + 'gcc'
    CXX = PREFIX + 'g++'
    AS = CC
    AR = PREFIX + 'ar'
    LINK = CC
    SIZE = PREFIX + 'size'
    OBJCPY = PREFIX + 'objcopy'
    OBJDUMP = PREFIX + 'objdump'
    TARGET_EXT = 'elf'
    DEVICE = ' -mcpu=cortex-m4 -mthumb -mfpu=fpv4-sp-d16 -mfloat-abi=hard'
    CFLAGS = DEVICE + ' -ffunction-sections -fdata-sections -Wall -std=c99'
    AFLAGS = DEVICE + ' -c -x assembler-with-cpp'
    LFLAGS = DEVICE + ' -nostartfiles --specs=nano.specs --specs=nosys.specs'
    LFLAGS += ' -Wl,--gc-sections,-Map=rtthread.map,-cref -T board/linker_scripts/link.lds'
    CXXFLAGS = CFLAGS.replace(' -std=c99', '') + ' -fno-exceptions -fno-rtti'
elif CROSS_TOOL == 'llvm-arm':
    # Clang + LLD + GNU newlib-nano + LLVM compiler-rt. "gcc" selects the
    # existing GNU ABI/newlib integration in RT-Thread, not the GCC compiler.
    # In particular, it avoids the llvm-arm helper's hard-coded picolibc scan.
    PLATFORM = 'gcc'
    PREFIX = 'arm-none-eabi-'
    llvm_bin = os.getenv('RTT_EXEC_PATH') or os.path.dirname(shutil.which('clang') or '')
    if not llvm_bin:
        raise ValueError('Set RTT_EXEC_PATH to the Arm Toolchain for Embedded bin directory')
    llvm_bin = _compiler_path(llvm_bin)
    executable_suffix = '.exe' if os.name == 'nt' else ''

    def llvm_tool(name):
        path = os.path.join(llvm_bin, name + executable_suffix)
        if not os.path.isfile(path):
            raise ValueError('Missing Arm Toolchain for Embedded tool: ' + path)
        return path

    CC = llvm_tool('clang')
    CXX = llvm_tool('clang++')
    AS = CC
    AR = llvm_tool('llvm-ar')
    LINK = CC
    SIZE = llvm_tool('llvm-size')
    OBJCPY = llvm_tool('llvm-objcopy')
    OBJDUMP = llvm_tool('llvm-objdump')
    TARGET_EXT = 'elf'

    gnu_root = os.getenv('RTT_GNU_ROOT')
    if not gnu_root:
        gnu_gcc = shutil.which('arm-none-eabi-gcc')
        if gnu_gcc:
            gnu_root = os.path.dirname(os.path.dirname(gnu_gcc))
    if not gnu_root:
        raise ValueError('Set RTT_GNU_ROOT to an Arm GNU Toolchain installation with newlib-nano')
    GNU_ROOT = _compiler_path(gnu_root)
    gnu_include = GNU_ROOT + '/arm-none-eabi/include'
    GNU_LIB_DIR = GNU_ROOT + '/arm-none-eabi/lib/thumb/v7e-m+fp/hard'
    for marker in [gnu_include + '/newlib-nano/newlib.h',
                   gnu_include + '/stdlib.h',
                   GNU_LIB_DIR + '/libc_nano.a',
                   GNU_LIB_DIR + '/libm.a',
                   GNU_LIB_DIR + '/libnosys.a']:
        if not os.path.isfile(marker):
            raise ValueError('Incomplete Arm GNU newlib-nano installation: ' + marker)

    cpu_flags = ['--target=arm-none-eabi', '-mcpu=cortex-m4', '-mthumb',
                 '-mfpu=fpv4-sp-d16', '-mfloat-abi=hard', '-fno-exceptions', '-fno-rtti']
    # Query using the unmodified ATfE sysroot so multilib selects compiler-rt.
    compiler_rt = subprocess.check_output([CC] + cpu_flags + ['--print-libgcc-file-name'],
                                         text=True).strip()
    if not os.path.isfile(compiler_rt) or os.path.basename(compiler_rt) != 'libclang_rt.builtins.a':
        raise ValueError('ATfE did not provide a matching compiler-rt: ' + compiler_rt)
    CLANG_RUNTIME_LIBS = [GNU_LIB_DIR + '/libnosys.a', _compiler_path(compiler_rt)]

    DEVICE = ' ' + ' '.join(cpu_flags)
    # Put these in DEVICE as well as CFLAGS: libc detection probes DEVICE.
    DEVICE += ' -nostdlibinc -isystem' + gnu_include + '/newlib-nano'
    DEVICE += ' -isystem' + gnu_include + ' -D_REENT_SMALL'
    CFLAGS = DEVICE + ' -ffunction-sections -fdata-sections -Wall -std=c99'
    AFLAGS = DEVICE + ' -c -x assembler-with-cpp'
    LFLAGS = ' ' + ' '.join(cpu_flags) + ' -nostdlib -fuse-ld=lld'
    LFLAGS += ' -L' + GNU_LIB_DIR + ' -Wl,--gc-sections,-Map=rtthread.map'
    LFLAGS += ' -T board/linker_scripts/link.lds'
    CXXFLAGS = CFLAGS.replace(' -std=c99', '')
elif CROSS_TOOL == 'keil':
    PLATFORM = 'armclang'
    CC = 'armclang'
    CXX = CC
    AS = 'armasm'
    AR = 'armar'
    LINK = 'armlink'
    TARGET_EXT = 'axf'
    DEVICE = ' --target=arm-arm-none-eabi -mcpu=cortex-m4 -mthumb -mfpu=fpv4-sp-d16 -mfloat-abi=hard'
    CFLAGS = DEVICE + ' -c -ffunction-sections -fdata-sections -funsigned-char'
    CFLAGS += ' -fshort-enums -fshort-wchar -D__MICROLIB -std=c99'
    CXXFLAGS = CFLAGS.replace(' -std=c99', '') + ' -fno-exceptions -fno-rtti'
    AFLAGS = ' --cpu Cortex-M4.fp --apcs=interwork --pd "__MICROLIB SETA 1"'
    LFLAGS = ' --cpu Cortex-M4.fp --library_type=microlib'
    LFLAGS += ' --scatter board/linker_scripts/link.sct --map --list rtthread.map'
    LFLAGS += ' --info sizes --info totals --info unused --strict'
else:
    raise ValueError('Unsupported RTT_CC: ' + CROSS_TOOL + '; use gcc, llvm-arm, or keil')

EXEC_PATH = os.getenv('RTT_EXEC_PATH', os.path.dirname(shutil.which(CC) or ''))
CPATH = ''
LPATH = ''

if BUILD == 'debug':
    CFLAGS += ' -Og -g3' if PLATFORM != 'armclang' else ' -O1 -g'
    CXXFLAGS += ' -Og -g3' if PLATFORM != 'armclang' else ' -O1 -g'
    AFLAGS += ' -g'
else:
    CFLAGS += ' -Os'
    CXXFLAGS += ' -Os'

if PLATFORM == 'armclang':
    POST_ACTION = 'fromelf --bin $TARGET --output rtthread.bin\nfromelf -z $TARGET'
else:
    POST_ACTION = OBJCPY + ' -O binary $TARGET rtthread.bin\n' + SIZE + ' $TARGET'


def dist_handle(BSP_ROOT, dist_dir):
    import sys
    tools_path = os.path.join(BSP_ROOT, 'tools')
    if not os.path.isdir(tools_path):
        tools_path = os.path.join(os.path.dirname(BSP_ROOT), 'tools')
    sys.path.insert(0, tools_path)
    from sdk_dist import dist_do_building
    dist_do_building(BSP_ROOT, dist_dir)
