project('zim-tools', ['c', 'cpp'],
  version : '2.1.1',
  license : 'GPLv3+',
  default_options : ['c_std=c11', 'cpp_std=c++11', 'werror=true'])

add_global_arguments(['-Werror', '-Wall'], language:'cpp')

add_global_arguments('-DVERSION="@0@"'.format(meson.project_version()), language : 'cpp')

static_linkage = get_option('static-linkage')
compiler = meson.get_compiler('cpp')
if static_linkage
  add_global_link_arguments('-static-libstdc++', '--static', language:'cpp')
  if compiler.get_id() == 'gcc' and build_machine.system() == 'linux'
    add_global_link_arguments('-Wl,--whole-archive', '-lpthread', '-Wl,--no-whole-archive', language:'cpp')
  endif
endif

libzim_dep = dependency('libzim', version : '>=6.3.0', static:static_linkage)
with_xapian_support = compiler.has_header_symbol('zim/zim.h', 'LIBZIM_WITH_XAPIAN')

find_library_in_compiler = meson.version().version_compare('>=0.31.0')
rt_dep = dependency('rt', required:false)
docopt_dep = dependency('docopt', static:static_linkage)

with_writer = target_machine.system() != 'windows'

if with_writer
  thread_dep = dependency('threads')
  zlib_dep = dependency('zlib', static:static_linkage)
  gumbo_dep = dependency('gumbo', static:static_linkage)

  magic_include_path = ''
  magic_prefix_install = get_option('magic-install-prefix')
  if magic_prefix_install == ''
    if compiler.has_header('magic.h')
      if find_library_in_compiler
        magic_lib = compiler.find_library('magic')
      else
        magic_lib = find_library('magic')
      endif
      magic_dep = declare_dependency(link_args:['-lmagic'])
    else
      error('magic.h not found')
    endif
  else
    if not find_library_in_compiler
      error('For custom magic_prefix_install you need a meson version >=0.31.0')
    endif
    magic_include_path = magic_prefix_install + '/include'
    magic_include_args = ['-I'+magic_include_path]
    if compiler.has_header('magic.h', args:magic_include_args)
      magic_include_dir = include_directories(magic_include_path, is_system:true)
      magic_lib_path = join_paths(magic_prefix_install, get_option('libdir'))
      magic_lib = compiler.find_library('magic', dirs:magic_lib_path, required:false)
      if not magic_lib.found()
        magic_lib_path = join_paths(magic_prefix_install, 'lib')
        magic_lib = compiler.find_library('magic', dirs:magic_lib_path)
      endif
      magic_link_args = ['-L'+magic_lib_path, '-lmagic']
      magic_dep = declare_dependency(include_directories:magic_include_dir, link_args:magic_link_args)
    else
      error('magic.h not found')
    endif
  endif
endif

subdir('src')
subdir('test')
