import os
import shutil

ARCH = 'arm'
CPU = 'cortex-m4'
CROSS_TOOL = os.getenv('RTT_CC', 'gcc')
BUILD = 'debug'
BSP_LIBRARY_TYPE = None

CHIP_DEFINE = 'PY32F403xD'

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
    DEVICE += ' -D' + CHIP_DEFINE
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
    suffix = '.exe' if os.name == 'nt' else ''
    CC = 'clang' + suffix
    CXX = 'clang++' + suffix
    AS = CC
    AR = 'llvm-ar' + suffix
    LINK = CC
    SIZE = 'llvm-size' + suffix
    OBJCPY = 'llvm-objcopy' + suffix
    OBJDUMP = 'llvm-objdump' + suffix
    TARGET_EXT = 'elf'

    gnu_root = os.getenv('RTT_GNU_ROOT',
                         os.path.dirname(os.path.dirname(shutil.which('arm-none-eabi-gcc') or '')))
    GNU_ROOT = os.path.abspath(gnu_root).replace('\\', '/')
    gnu_include = GNU_ROOT + '/arm-none-eabi/include'
    GNU_LIB_DIR = GNU_ROOT + '/arm-none-eabi/lib/thumb/v7e-m+fp/hard'

    cpu_flags = ['--target=arm-none-eabi', '-mcpu=cortex-m4', '-mthumb',
                 '-mfpu=fpv4-sp-d16', '-mfloat-abi=hard', '-fno-exceptions', '-fno-rtti']
    DEVICE = ' ' + ' '.join(cpu_flags)
    DEVICE += ' -D' + CHIP_DEFINE
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
    CFLAGS += ' -D' + CHIP_DEFINE
    CXXFLAGS = CFLAGS.replace(' -std=c99', '') + ' -fno-exceptions -fno-rtti'
    AFLAGS = ' --cpu Cortex-M4.fp --apcs=interwork --pd "__MICROLIB SETA 1"'
    LFLAGS = ' --cpu Cortex-M4.fp --library_type=microlib'
    LFLAGS += ' --scatter board/linker_scripts/link.sct --map --list rtthread.map'
    LFLAGS += ' --info sizes --info totals --info unused --strict'

EXEC_PATH = os.getenv('RTT_EXEC_PATH', os.path.dirname(shutil.which(CC) or ''))
if CROSS_TOOL == 'keil' and EXEC_PATH:
    # Accept either the Keil root or the compiler directory; CI passes the
    # root (--exec-path=/opt/Keil_v5) while a user may point at the bin folder.
    armclang_bin = os.path.join(EXEC_PATH, 'ARM', 'ARMCLANG', 'bin')
    if not os.path.isfile(os.path.join(EXEC_PATH, 'armclang.exe' if os.name == 'nt' else 'armclang')) \
            and os.path.isdir(armclang_bin):
        EXEC_PATH = armclang_bin
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
    # Package identity and vendor layout belong to this series' BSP.
    # Keep the device headers; CMSIS-Core comes from its separate package.
    dist_do_building(BSP_ROOT, dist_dir, sdk_name='py32f403_sdk',
                     excluded_subtrees=('Drivers/BSP', 'Drivers/CMSIS/Include',
                                        'Drivers/CMSIS/RTOS', 'Drivers/CMSIS/RTOS2'))
