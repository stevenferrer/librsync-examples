project(
  'librsync-examples',
  'c',
  'cpp',
  version: '0.1',
  meson_version: '>= 1.9.0',
  default_options: ['warning_level=3', 'cpp_std=c++20'],
)

# subproject('librsync')
# librsync_dep = dependency('librsync')

inc_dir = include_directories('include')
inc_dir2 = include_directories('include/rsyncw')
librsync_inc_dir = include_directories('/usr/local/include')

cc = meson.get_compiler('c')
librsync_dep = cc.find_library('rsync', required: true, dirs: ['/usr/local/lib'])
boost_dep = dependency('boost', version: '>=1.89')

executable(
  'server',
  'src/server.c',
  include_directories: [inc_dir, librsync_inc_dir],
  dependencies: [librsync_dep],
)

executable(
  'client',
  'src/client.c',
  include_directories: [inc_dir, librsync_inc_dir],
  dependencies: [librsync_dep],
)

executable(
  'server2',
  'src/server2.cpp',
  include_directories: [
    librsync_inc_dir,
    inc_dir2,
  ],
  dependencies: [boost_dep, librsync_dep],
)

executable(
  'client2',
  'src/client2.cpp',
  include_directories: [
    librsync_inc_dir,
    inc_dir2,
  ],
  dependencies: [boost_dep, librsync_dep],
)
