import os,shutil,subprocess,copy,sys

def mkdir_p(directory):
    if not os.path.isdir(directory):
        os.makedirs(directory)

def walk_dir_recursive(dir,instdir, level=0):
    for folder,sub_folders,files in os.walk(dir):
        s = instdir
        print s
        for fname in files:
            s = '    File "${DISTRIBUTION_DIR}\\'
            if level>0:
                s+=folder
                s+='\\'
            s+= fname
            s+='"'
            print s
        for fname in sub_folders:
            instdir_fname = copy.copy(instdir)
            instdir_fname+= '\\'
            instdir_fname+= fname
            walk_dir_recursive(fname,instdir_fname,level=level+1)
        break
    
def concatenate_files(output_fname,input_filenames):
    lines = []
    for fname in input_filenames:
        f = open(fname,'r')
        lines.extend(f.readlines())
    ofs = open(output_fname,'w+')
    for line in lines:
        ofs.write(line)
    ofs.close()
       
if __name__=='__main__':
    cwd = os.getcwd()
    print 'cwd:',cwd

    shutil.rmtree( os.path.join('..','build-Launcher-Desktop_Qt_5_5_0_MinGW_32bit-Release','distribute') ) 
    
    dir_exe = os.path.join('..','build-Launcher-Desktop_Qt_5_5_0_MinGW_32bit-Release','Launcher','release')
    distribute_win7 = os.path.join('..','build-Launcher-Desktop_Qt_5_5_0_MinGW_32bit-Release','distribute','win7') 
    
    mkdir_p(distribute_win7)
    
    path_exe = os.path.join(dir_exe,'Launcher.exe')
    print 'copying:', path_exe
    shutil.copy(path_exe, distribute_win7)
    shutil.copytree(os.path.join(cwd,'Launcher','clusters'),os.path.join(distribute_win7,'clusters'))
    assert os.path.exists(os.path.join(distribute_win7,'Launcher.exe'))
    
    os.chdir(distribute_win7)
    subprocess.call(['windeployqt', 'Launcher.exe'])
    # windeployqt must be in the path

    sys_stdout = sys.stdout
    sys.stdout = open(os.path.join(cwd,'win7-part1.nsi'), 'w+')
    
    print 'Section -MainInstallFolder SEC0000'
    walk_dir_recursive('.','    SetOutPath $INSTDIR')
    print '    WriteRegStr HKLM "${REGKEY}\Components" MainInstallFolder 1'  
    print 'SectionEnd'  
    
    sys.stdout.close()
    sys.stdout = sys_stdout
    
    os.chdir(cwd)
    concatenate_files('win7.nsi', ['win7-part0.nsi','win7-part1.nsi','win7-part2.nsi'])
  
    subprocess.call(['makensis', 'win7.nsi'])
    # makensis must be in the path
    
    shutil.move('Install-Launcher-win7.exe',distribute_win7)
  
    print 'done'