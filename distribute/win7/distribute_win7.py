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
    
def print_title(title):
    print '\n'+(80*'=')
    print title+'\n'
    
def enter_dir(path):
    os.chdir(path)
    print 'Entering',path
    
if __name__=='__main__':
    enter_dir( os.getcwd() )
    
    print_title('looking for build directory')
    for build_dir in [ os.path.abspath( os.path.join('..','..','..','build-Launcher-Desktop_Qt_5_5_1_MinGW_32bit-Release') )
                     , os.path.abspath( os.path.join('..','..','..','build-Launcher-Desktop_Qt_5_5_0_MinGW_32bit-Release') )
                     ]:
        if os.path.isdir(build_dir):
            print '  FOUND    :', build_dir, '\n'
            
            break
        else:
            print '  not found:', build_dir
    else:
        raise RuntimeError('No build directory found')
    
    source_dir = os.path.abspath( os.path.join('..','..') )
    build_distribute_win7_dir = os.path.join(build_dir,'distribute','win7')
    
    enter_dir( source_dir )
    #git_revision
    print_title('retrieving git revision number')
    subprocess.call(['bash', 'git_revision.sh'])
    #   msys/1.0/bin must be on the path
    git_revision = open('revision.txt','r').readlines()[0][0:-1]
    _OUT_FILE_ = 'Launcher-installer-win7-'+platform.architecture()[0]+'-'+git_revision+'.exe'

    #qmake
    print_title('Executing qmake')
    os.chdir(build_dir)
    Qt_mingw492_32_bin = 'C:'+os.sep+'Qt'+os.sep+'5.5'+os.sep+'mingw492_32'+os.sep+'bin'
    qmake_exe = os.path.join(Qt_mingw492_32_bin,'qmake')
    subprocess.call([qmake_exe, os.path.join(source_dir,'Launcher.pro') ,'-r', '-spec', 'win32-g++'])
    #   qmake must be on the path

    #make
    Qt_Tools_mingw492_32_bin = 'C:'+os.sep+'Qt'+os.sep+'\Tools'+os.sep+'mingw492_32'+os.sep+'bin'
    make_exe = os.path.join(Qt_Tools_mingw492_32_bin,'mingw32-make')
    
    print_title('Executing mingw32-make clean')
    subprocess.call([make_exe,'clean'])
    
    print_title('Executing ming32-make')
    subprocess.call([make_exe])


    #prepare distribution directory
    mkdir_p      (build_distribute_win7_dir)
    shutil.rmtree(build_distribute_win7_dir) 
    mkdir_p      (build_distribute_win7_dir)
    
    #copy the executable
    executable_file = os.path.join(build_dir,'Launcher','release','Launcher.exe')
    print_title('Copying executable:'+executable_file)
    shutil.copy(executable_file, build_distribute_win7_dir)
    assert os.path.exists(os.path.join(build_distribute_win7_dir,'Launcher.exe'))

    #copy the clusters directory
    print_title('Copying clusters directory (.info files)')
    shutil.copytree(os.path.join(source_dir,'Launcher','clusters'),os.path.join(build_distribute_win7_dir,'clusters'))
    assert os.path.exists(os.path.join(build_distribute_win7_dir,'clusters'))
        
    #windeployqt
    print_title('Deploying')
    os.chdir(build_distribute_win7_dir)
    subprocess.call(['windeployqt', '--no-translations', 'Launcher.exe'])
    # windeployqt must be on the path

    shutil.copy2( os.path.join(source_dir,'Launcher.ico')              ,'.')
    shutil.copy2( os.path.join(Qt_mingw492_32_bin,'libgcc_s_dw2-1.dll'),'.')

    #create installer using NSIS
    print_title('Creating installer: '+_OUT_FILE_ )
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
    
    print_title('Copying installer to Dropbox: '+_OUT_FILE_)
    shutil.copy2(_OUT_FILE_,'e:Dropbox')
    
    print_title('Finished')
