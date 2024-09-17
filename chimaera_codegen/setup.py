import setuptools

setuptools.setup(
    name="chimaera_codegen",
    packages=setuptools.find_packages(),
    scripts=['bin/chi_make_config', 'bin/chi_make_macro', 'bin/chi_make_task',
             'bin/chi_refresh_tasks', 'bin/chi_refresh_tasks', 'bin/chi_print_root',
             'bin/chi_clear_temp'],
    version="0.0.1",
    author="Luke Logan",
    author_email="llogan@hawk.iit.edu",
    description="Create basic for applications",
    long_description=open('README.md').read(),
    long_description_content_type='text/markdown',
    url="https://github.com/scs-lab/jarvis-cd",
    classifiers = [
        "Programming Language :: Python",
        "Programming Language :: Python :: 3",
        "Development Status :: 0 - Pre-Alpha",
        "Environment :: Other Environment",
        "Intended Audience :: Developers",
        "License :: None",
        "Operating System :: OS Independent",
        "Topic :: Software Development :: Libraries :: Python Modules",
        "Topic :: Application Configuration",
    ],
    install_requires=[
        'pyyaml',
        # 'pylint==2.15.0',
        # 'coverage==5.5',
        # 'coverage-lcov==0.2.4',
        # 'pytest==6.2.5',
        'jarvis-util @ git+https://github.com/scs-lab/jarvis-util.git#egg=jarvis-util'
    ]
)
