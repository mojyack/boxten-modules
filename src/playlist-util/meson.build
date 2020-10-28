if individual_compile
    project('boxten playlist utility module', 'cpp')
    add_project_arguments(['-std=c++2a'], language : 'cpp')
    add_project_link_arguments(['-std=c++2a'], language : 'cpp')
endif

module_name = 'Playlist util'
prefix      = get_option('prefix')

boxten_dep = dependency('libboxten')
qt_dep     = dependency('qt5', version: '>= 5.2', required: true, modules: ['Core', 'Widgets', 'Gui'])
datadir    = prefix / get_option('datadir') / 'boxten' / module_name

boxten_include = include_directories(join_paths(get_option('prefix'),'usr/include/libboxten'))
install_dir    = prefix / 'lib/boxten-modules'

config_data = configuration_data()
config_data.set('module_name', module_name)
configure_file( input : 'config.h.in',
                output : 'config.h',
                configuration : config_data)
config_include = include_directories('.')

files = [
    'playlist-util.cpp',
    'playlist-viewer.cpp',
]

shared_module(
    'playlist-util', files,
    dependencies: [boxten_dep, qt_dep],
    include_directories: [boxten_include, config_include],
    install: true,
    install_dir: install_dir)