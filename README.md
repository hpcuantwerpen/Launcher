# Launcher
Launcher is a GUI application for
 - easy resource allocation and PBS job script creation for VSC clusters
 - submitting your jobs
 - synchronizing your job output with your local file system (for postprocessing,  analysis, or storage)  

Launcher is tested on Windows/Linux(Ubuntu 14.04, 15.04)/MacOSX)

If You experience any problems during installation or operation contact engelbert.tijskens@uantwerpen.be

##Installation
###Preparing your environment
Launcher requires
 - Python 2.7 
 - wxPython 2.8 or later
 - paramiko 1.15.2

Paramiko is easily installed with pip, but wxPython can be harder.

If your are not experienced in installing Python and Python modules on your OS we recommend that you install the free *Anaconda Python distribution* from Continuum Analytics. If you are a windows user, we strongly recommend you to take this route. You can currently get it at https://store.continuum.io/cshop/anaconda. Install it as your default Python. If you need different Python versions on your system and you do not want Anaconda as your default Python, we suggest you to familiarise yourself with virtual Python environments ([virtualenv](http://docs.python-guide.org/en/latest/dev/virtualenvs/)). Next, fire up a terminal and install the components that Launcher needs with the Anaconda packages install tool:
$ conda install -c https://conda.binstar.org/anaconda wxpython
$ conda install -c https://conda.binstar.org/anaconda paramiko

###Installing Launcher - Linux/MacOSX
Download the Launcher installer and run is as follows:

Open a terminal and cd into some temporary directory, then execute:

$ mkdir some_tmp_dir && cd some_tmp_dir

Downlaod the installer from the github repository:

$ wget https://github.com/hpcuantwerpen/Launcher/raw/master/installer.py

Run the installer. It is recommended that you use the Python version that you want to use for running Launcher. The installer will check wether the running Python version satisfies the Launcher prerequisites. It will also build a Launcher startup script that references the python you use for the installer. If this python does not meet the prerequisites, that script may fail.

$ python installer.py

The installer produces a report that you should check to assure that the installation was successful. If so, you should have a Launcher folder in your home folder, which serves as the default location for log files, and for storing the files of your jobs (PBS script, input files, output files). It also contains a shell script launcher.sh that you can use to start Launcher.

Happy Launching!

###Installing Launcher - Windows
Open a Powershell window and execute:

Make some temporary directory and cd into is:

> mkdir some_tmp_dir

> cd some_tmp_dir

Import a module to downlaod the installer from the github repository:

> import-module bitstransfer

> Start-BitsTransfer https://github.com/hpcuantwerpen/Launcher/raw/master/installer.py

Run the installer. It is recommended that you use the Python version (preferably Anaconda) that you want to use for running Launcher as the installer will check wether the running Python version satisfies the Launcher prerequisites. 

> python installer.py

The installer produces a report that you should check to assure that the installation was successful. If so, you should have a Launcher folder in your home folder, which serves as the default location for log files, and for storing the files of your jobs (PBS script, input files, output files). It also contains a shell script launcher.bat that you can use to start Launcher.

Happy Launching!
