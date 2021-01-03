if individual_compile
    project('boxten pcm format conversion module', 'cpp')
    add_project_arguments(['-std=c++2a'], language : 'cpp')
    add_project_link_arguments(['-std=c++2a'], language : 'cpp')
endif

module_name = 'format conversion'
prefix      = get_option('prefix')

boxten_dep = dependency('libboxten')

boxten_include = include_directories(join_paths(prefix,'usr/include/libboxten'))
install_dir    = prefix / 'lib/boxten-modules'

config_data = configuration_data()
config_data.set('module_name', module_name)
configure_file( input : 'config.h.in',
                output : 'config.h',
                configuration : config_data)
config_include = include_directories('.')

shared_module(
    'format-conversion', ['pcm-format.cpp'],
    dependencies: [boxten_dep],
    include_directories: boxten_include,
    install: true,
    install_dir: install_dir)
