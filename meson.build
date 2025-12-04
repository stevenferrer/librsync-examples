project(
  'librsync-examples',
  'cpp',
  version: '0.1',
  meson_version: '>= 1.3.0',
  default_options: ['warning_level=3', 'cpp_std=c++20'],
)

inc_dir = include_directories('include')
librsync_inc_dir = include_directories('/usr/local/include')

cpp = meson.get_compiler('cpp')
librsync_dep = cpp.find_library('rsync', required: true, dirs: ['/usr/local/lib'])
boost_dep = dependency('boost', version: '>=1.89')

executable(
  'server',
  'src/server.cpp',
  include_directories: [
    librsync_inc_dir,
    inc_dir,
  ],
  dependencies: [boost_dep, librsync_dep],
)

executable(
  'client',
  'src/client.cpp',
  include_directories: [
    librsync_inc_dir,
    inc_dir,
  ],
  dependencies: [boost_dep, librsync_dep],
)
