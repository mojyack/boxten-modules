if individual_compile
    project('boxten alsa-output module', 'cpp')
    add_project_arguments(['-std=c++2a'], language : 'cpp')
    add_project_link_arguments(['-std=c++2a'], language : 'cpp')
endif

module_name = 'ALSA output module'
prefix      = get_option('prefix')

alsa_dep   = dependency('alsa', version: '>= 1.0.16')
boxten_dep = dependency('libboxten')

boxten_include = include_directories(join_paths(get_option('prefix'),'usr/include/libboxten'))
install_dir    = prefix / 'lib/boxten-modules'

config_data = configuration_data()
config_data.set('module_name', module_name)
configure_file( input : 'config.h.in',
                output : 'config.h',
                configuration : config_data)
config_include = include_directories('.')

shared_module(
    'alsa-output', ['alsa-output.cpp'],
    dependencies: [alsa_dep, boxten_dep],
    include_directories: [boxten_include, config_include],
    install: true,
    install_dir: install_dir)