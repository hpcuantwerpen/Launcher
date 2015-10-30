import os,shutil,subprocess,copy,sys,platform

def mkdir_p(directory):
    if not os.path.isdir(directory):
        os.makedirs(directory)

def walk_dir_recursive(dir,instdir, level=0):
    s=''
    for folder,sub_folders,files in os.walk(dir):
        s += instdir+'\n'
        for fname in files:
            s += '    File "${DISTRIBUTION_DIR}\\'
            if level>0:
                s+=folder
                s+='\\'
            s+= fname
            s+='"\n'
        for fname in sub_folders:
            instdir_fname = copy.copy(instdir)
            instdir_fname+= '\\'
            instdir_fname+= fname
            s += walk_dir_recursive(fname,instdir_fname,level=level+1)
        return s
    


def file_from_template(filename, template, variables):
    o = open(filename,'w+')
    lines = open(template).readlines()
    for line in lines:
        for variable,value in variables.iteritems():
            line = line.replace(variable,value)
        o.write(line)
    o.close()
    
if __name__=='__main__':
    cwd = os.getcwd()
    print 'cwd:',cwd

    build_dir  = os.path.abspath( os.path.join('..','..','..','build-Launcher-Desktop_Qt_5_5_1_MinGW_32bit-Release') )
    source_dir = os.path.abspath( os.path.join('..','..') )
    build_distribute_win7_dir = os.path.join(build_dir,'distribute','win7')
    
    os.chdir( source_dir )
    print os.getcwd()
    #git_revision
    subprocess.call(['bash', 'git_revision.sh'])
    #   msys/1.0/bin must be on the path
    git_revision = open('revision.txt','r').readlines()[0][0:-1]
    _OUT_FILE_ = 'Launcher-installer-win7-'+platform.architecture()[0]+'-'+git_revision+'.exe'

    
    #qmake
    os.chdir(build_dir)
    Qt_mingw492_32_bin = 'C:'+os.sep+'Qt'+os.sep+'5.5'+os.sep+'mingw492_32'+os.sep+'bin'
    qmake_exe = os.path.join(Qt_mingw492_32_bin,'qmake')
    subprocess.call([qmake_exe, os.path.join(source_dir,'Launcher.pro') ,'-r', '-spec', 'win32-g++'])
    #   qmake must be on the path

    #make
    Qt_Tools_mingw492_32_bin = 'C:'+os.sep+'Qt'+os.sep+'\Tools'+os.sep+'mingw492_32'+os.sep+'bin'
    make_exe = os.path.join(Qt_Tools_mingw492_32_bin,'mingw32-make')
    subprocess.call([make_exe])


    #prepare distribution directory
    mkdir_p      (build_distribute_win7_dir)
    shutil.rmtree(build_distribute_win7_dir) 
    mkdir_p      (build_distribute_win7_dir)
    
    #copy the executable
    executable_file = os.path.join(build_dir,'Launcher','release','Launcher.exe')
    print 'copying:', executable_file
    shutil.copy(executable_file, build_distribute_win7_dir)
    assert os.path.exists(os.path.join(build_distribute_win7_dir,'Launcher.exe'))

    #copy the clusters directory
    shutil.copytree(os.path.join(source_dir,'Launcher','clusters'),os.path.join(build_distribute_win7_dir,'clusters'))
    assert os.path.exists(os.path.join(build_distribute_win7_dir,'clusters'))
        
    #windeployqt
    os.chdir(build_distribute_win7_dir)
    subprocess.call(['windeployqt', '--no-translations', 'Launcher.exe'])
    # windeployqt must be on the path

    shutil.copy2( os.path.join(source_dir,'Launcher.ico')              ,'.')
    shutil.copy2( os.path.join(Qt_mingw492_32_bin,'libgcc_s_dw2-1.dll'),'.')

    #create installer using NSIS
    source_distribute_win7_dir = os.path.join(source_dir,'distribute','win7')
    
    os.chdir(build_distribute_win7_dir)
    _MAIN_INSTALL_FOLDER_ = 'Section -MainInstallFolder SEC0000\n'
    _MAIN_INSTALL_FOLDER_+= walk_dir_recursive('.','    SetOutPath $INSTDIR')
    _MAIN_INSTALL_FOLDER_+= ';    WriteRegStr HKLM "${REGKEY}\Components" MainInstallFolder 1\n'  
    _MAIN_INSTALL_FOLDER_+= 'SectionEnd\n'  
 
    _DISTRIBUTION_DIR_=build_distribute_win7_dir 
    
    file_from_template( filename='win7.nsi'
                      , template=os.path.join(source_distribute_win7_dir,'win7-template.nsi')
                      , variables={ '$$_OUT_FILE_$$'           :_OUT_FILE_
                                  , '$$_MAIN_INSTALL_FOLDER_$$':_MAIN_INSTALL_FOLDER_
                                  , '$$_DISTRIBUTION_DIR_$$'   :_DISTRIBUTION_DIR_
                                  }
                      )
  
    subprocess.call(['makensis', 'win7.nsi'])
    # makensis must be in the path
    
    shutil.copy2(_OUT_FILE_,'e:Dropbox')
    
    print '\nCopied', _OUT_FILE_, 'to e:Dropbox (host_etijskens //vboxsrv)'
 
    print '\ndone'
