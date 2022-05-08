project(
    'sss',
    'cpp',
    version : '0.1',
    default_options : ['cpp_std=c++20']
)

install_subdir('include', install_dir : 'include')

if get_option('build-tests')
    include = include_directories('include')
    catch_include = include_directories('third_party/catch2')

    test_sources = files(
        'test/main.cpp',
        'test/round_trip.cpp'
    )

    sss_tests = executable(
        'sss_tests', 
        test_sources,
        include_directories : [catch_include, include]
    )

    test('sss', sss_tests)
endif