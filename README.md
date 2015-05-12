##Launcher
Launcher is a Gui application for
 - easy resource allocation and PBS job script creation for the VSC clusters
 - submitting your jobs
 - synchronizing your job output with your local file system (for postprocessing, or just to permanently store the results) 

Launcher is tested on Windows/Linux(Ubuntu 14.04, 15.04)/MacOSX)

##Installation
###Preparing your environment
Launcher requires
 - Python 2.7 
 - wxPython 2.8 or later
 - paramiko 1.15.2

If your are not experienced in installing Python and Python modules on your OS we recommend that you install the free Anaconda Python distribution from Continuum Analytics. You can currently get it at https://store.continuum.io/cshop/anaconda. Install it as your default Python. Familiarize yourself with virtual Python environments ([virtualenv](http://docs.python-guide.org/en/latest/dev/virtualenvs/)) if you need different Python versions on your system and you do not want Anaconda as your default Python. Next, fire up a terminal and install missing components:
$ conda install -c https://conda.binstar.org/anaconda wxpython
$ conda install -c https://conda.binstar.org/anaconda paramiko

###Installing Launcher - Linux/MacOSX
Open a terminal and execute:

$ cd some_tmp_dir

$ wget https://github.com/hpcuantwerpen/Launcher/raw/master/installer.py

$ python installer.py

You should now have a Launcher folder in your home folder. 

###Installing Launcher - Windows

