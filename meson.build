project(
  'librsync-examples',
  'c',
  'cpp',
  version: '0.1',
  meson_version: '>= 1.9.0',
  default_options: ['warning_level=3', 'cpp_std=c++17'],
)

# subproject('librsync')
# librsync_dep = dependency('librsync')

inc_dir = include_directories('include')
librsync_inc_dir = include_directories('/usr/local/include')
librsyncw_inc_dir = include_directories('include/rsyncw')

cc = meson.get_compiler('c')
librsync_dep = cc.find_library('rsync', required: true, dirs: ['/usr/local/lib'])
boost_dep = dependency('boost', version: '>=1.89')

server_exe = executable(
  'server',
  'src/server.c',
  include_directories: [inc_dir, librsync_inc_dir],
  dependencies: [librsync_dep],
)

client_exe = executable(
  'client',
  'src/client.c',
  include_directories: [inc_dir, librsync_inc_dir],
  dependencies: [librsync_dep],
)

server2_exe = executable(
  'server2',
  'src/server2.cpp',
  include_directories: [inc_dir, librsync_inc_dir, librsyncw_inc_dir],
  dependencies: [boost_dep, librsync_dep],
)
