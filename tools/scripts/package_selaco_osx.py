#!/bin/python
# this was based on this 
# original https://github.com/jveitchmichaelis/deeplabel/blob/master/fix_paths_mac.py
# MIT license

import subprocess
import os
import sys
import re
from shutil import copyfile
from shutil import copytree

content_folder = ""
framework_path = ""
post_cmds = []
 
def make_abs_path(path, rpaths = None):
    if "@executable_path" in path:
        path = path.replace("@executable_path", "")
        path = os.path.join(content_folder, path[4:])

    if "@loader_path" in path:
        path = path.replace("@loader_path", framework_path)

    if "@rpath" in path:
        if rpaths:
            for rpath in rpaths:
                testpath = path.replace("@rpath", rpath)
                if not os.path.exists(testpath):
                    continue
                path = testpath
                break
        # if we didn't find it in the rpaths then just use the static framework_path
        if "@rpath" in path:
            path = path.replace("@rpath", framework_path)
    return path

def get_local_dependencies(module_name):
    local_dependencies = []
    o = subprocess.run(['/usr/bin/otool', '-L', module_name], stdout=subprocess.PIPE)
    for line in o.stdout.splitlines():
        line = line.decode()
        if line[0] != '\t':
            continue
        path = line.split(' ', 1)[0][1:]
        if path.startswith("/System/Library/"): # I'm gonna assume if it's here, we're all good.
            continue
        if path.startswith("/usr/lib"): # most of these aren't even on disk, but in the system module cache
            continue
        local_dependencies += [path]
    return local_dependencies

# os.symlink throws an exception if a file or symlink exists already, that's probably good, we don't
# want to overwrite anything but I'd rather just continue if it's the same symlink
def create_symlink(source_file, symlink_name):
    if os.path.islink(symlink_name):
        current_source_file = os.readlink(symlink_name)
        if source_file == current_source_file:
            return
    os.symlink(source_file, symlink_name)

def add_post_command(args):
    global post_cmds
    post_cmds += [args]

def run_post_commmands():
    for  cmd in post_cmds:
        print(' '.join(cmd))
        _ = subprocess.run(cmd, stdout=subprocess.PIPE)


# for the executable, we want to
# 1. remove all rpaths
# 2. add rpath of @executable_path/../Frameworks
# 3. find all LC_LOAD commands that point into a "local" folder
# 4. convert those to load from @rpath
# 5. return the full path names of those depenancy files, so that we can copy them into frameworks
def process_exe(module_name):
    o = subprocess.run(['/usr/bin/otool', '-l', module_name], stdout=subprocess.PIPE)
    oldrpaths = []
    for line in o.stdout.splitlines():
        line = line.decode()
        match = re.search(r'path (.*) \(offset ..\)', line)
        if not match:
            continue
        oldrpaths += [match.group(1)]
    for rpath in oldrpaths:
        add_post_command(['install_name_tool', '-delete_rpath', rpath, module_name])
    add_post_command(['install_name_tool', '-add_rpath', '@executable_path/../Frameworks', module_name])

    # get all the depenencies out
    dependencies = get_local_dependencies(module_name)

    # we're going to look for files, so firstly lets translate anything in the rpath
    for i in range(len(oldrpaths)):
        oldrpaths[i] = make_abs_path(oldrpaths[i])

    # now convert any dylib paths to be in @rpath, then convert the path to be absolute
    for i in range(len(dependencies)):
        path = dependencies[i]
        dependency_dylib_name = os.path.split(path)[-1]
        add_post_command(['install_name_tool', '-change', path, os.path.join("@rpath", dependency_dylib_name), module_name])
        dependencies[i] = make_abs_path(path, oldrpaths)

    return dependencies

# for each dylib, we need to
# 1. copy the dylib (and link) into Frameworks
# 2. convert paths of non-system dylibs to use @loader_path
# 3. return any dependencies that need copying
def process_dylib(module_path):
    if os.path.islink(module_path):
        symlink_dir, symlink_name = os.path.split(module_path)
        target_name = os.readlink(module_path)
        create_symlink(symlink_name, os.path.join(framework_path, target_name))
        return [os.path.join(symlink_dir, target_name)]
    elif match := re.search(r'(.*)/(.*\.framework)/(.*)', module_path):
        framework_base = match.group(1)
        framework_name = match.group(2)
        framework_dylib = match.group(3)
        copytree(os.path.join(framework_base, framework_name), os.path.join(framework_path, framework_name))
        module_path = os.path.join(framework_path, framework_name, framework_dylib)
        # the only framework I use is SDL2 and it already has localized it's dependencies
        return []

    module_dir, module_name = os.path.split(module_path)
    if module_dir != framework_path:
        copyfile(module_path, os.path.join(framework_path, module_name))
    module_path = os.path.join(framework_path, module_name)
    
    dependencies = get_local_dependencies(module_path)
    for i in range(len(dependencies)):
        dependencies[i] = make_abs_path(dependencies[i])

    # a dylib will return itself (or more likely a link to itself) as dependency[0], I'm not sure why this is 
    # but my guess is that if you load an outdated library then it'll follow the link and load the newer copy
    if len(dependencies):
        dependency_dylib_name = os.path.split(dependencies[0])[-1]
        add_post_command(['install_name_tool', '-id', os.path.join("@loader_path", dependency_dylib_name), module_path])

    # all other dependencies
    dependencies = dependencies[1:]
    for path in dependencies:
        dependency_dylib_name = os.path.split(path)[-1]
        add_post_command(['install_name_tool', '-change', path, os.path.join("@loader_path", dependency_dylib_name), module_path])

    return dependencies

def main(argv):
    if len(argv) == 1:
        print('this script packages up dylibs that are statically referenced within an executable')
        print('and removes references to their absolute path.  If you have any dylibs that are')
        print('dynamically loaded, you can pass them in also as arguments after the executable\n')
        print('usage: ' + argv[0] + ' executable_file [dylibs]')
        return

    global content_folder, framework_path

    # if you pass in an absolute path it gets broken here, because os.path.join(os.)
    executable = sys.argv[1]
    app_folder = os.sep.join(executable.split('/')[:-3]) # os.path.join doesn't work well with absolute path
    content_folder = os.path.join(app_folder, "Contents")
    framework_path = os.path.join(content_folder, "Frameworks")

    module_name = argv[1]
    if not os.path.exists(framework_path):
        os.makedirs(framework_path)

    print('Processing {}'.format(module_name))
    dep_list = process_exe(module_name)

    # add known dynamic deps here !
    for dylib in argv[2:]:
        dep_list += [dylib]

    need = set(dep_list)
    done = set()

    while need:
        needed = set(need)
        need = set()
        for f in needed:
            print('Processing {}'.format(f))
            need.update(process_dylib(f))
        done.update(needed)
        need.difference_update(done)
    run_post_commmands()

if __name__ == '__main__':
    print(sys.argv)
    sys.exit(main(sys.argv))