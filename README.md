# Launcher
Launcher is a GUI application for
 - easy resource allocation and PBS job script creation for the VSC clusters (Flemish Supercomputer Center)
 - submitting your jobs
 - synchronizing your job output with your local file system (for postprocessing,  analysis, or storage) 

Launcher is tested on Windows/Linux(Ubuntu 14.04, 15.04)/MacOSX, also inside virtual machines)

If You experience any problems during installation or operation contact engelbert.tijskens@uantwerpen.be

###Starting Launcher
Look in your home folder for a folder named "Launcher". It contains a startup script "Launcher.bat" or "Launcher.sh" depending on your OS.

##Installation
###Preparing your environment
Launcher requires
 - Python 2.7.6
 - wxPython 2.8 or later
 - paramiko 1.15.2

Paramiko is easily installed with pip, but wxPython can be harder.

If your are not experienced in installing Python and Python modules on your OS we recommend that you install the free **Enthought Canopy Express Python distribution** . If you are a windows user, we strongly recommend you to take this route. You can currently get it at https://store.enthought.com/downloads. It is easiest if you install it as your default Python. 

\[If you need different Python versions on your system and you do not want Canopy Express as your default Python, we suggest you to familiarise yourself with virtual Python environments ([virtualenv](http://docs.python-guide.org/en/latest/dev/virtualenvs/)).\]

Installing Canopy Express is easy, you can accept all the defaults. At the end of the installation the installer will ask you to open the Canopy application, answer yes, and when the application appears open the Canopy package manager. Click 'installed packages' and verify that you have wxPython. you may also verify that you do not have Paramiko. If you are interested, you may install the package updates.

To install Paramiko, fire up a terminal (or command window, in windows). 
```
$ pip install paramiko
```
This will install Paramiko for the default python (that is the python that is in your path). If you have more than one python version and/or Canopy Express is not your default Python, you must ad the path to the pip that comes with Canopy Express. Depending on your OS that would more or less look like
```
Windows$ C:\Users\*user_name*\AppData\Local\Enthought\Canopy\User\Scripts\pip install paramiko
 Ubuntu$ /home/*user_name*/Enthought/Canopy_64bit/User/bin/pip install paramiko
 MacOSX$ /Users/*user_name*/Library/Enthought/Canopy_64bit/User/bin/pip install paramiko
```

###Installing Launcher - Windows
To download and run the Launcher installer proceed as follows. Open a Powershell window and cd into some temporary directory:
Open a  and execute:

Make some temporary directory and cd into is:
```
$ mkdir some_tmp_dir
$ cd some_tmp_dir
```
Downlaod the installer from the github repository:
```
$ import-module bitstransfer
$ Start-BitsTransfer https://github.com/hpcuantwerpen/Launcher/raw/master/installer.py
```
Run the installer. It is recommended that you use the Python version (preferably Anaconda) that you want to use for running Launcher as the installer will check wether the running Python version satisfies the Launcher prerequisites. 
```
$ python installer.py
```
or, if python is not on your path:
```
$ C:\Users\*user_name*\AppData\Local\Enthought\Canopy\User\Scripts\python installer.py
```
The installer produces a report that you should check to assure that the installation was successful. If so, you should have a Launcher folder in your home folder, which serves as the default location for log files, and for storing the files of your jobs (PBS script, input files, output files). It also contains a shell script launcher.bat that you can use to start Launcher.

The temporary folder can be removed now.
```
$ cd ..
$ rm -rf some_tmp_dir
```
Happy Launching!

###Installing Launcher - Linux
To download and run the Launcher installer proceed as follows. Open a terminal and cd into some temporary directory:
```
$ mkdir some_tmp_dir && cd some_tmp_dir
```
Downlaod the installer from the github repository:
```
$ wget https://github.com/hpcuantwerpen/Launcher/raw/master/installer.py
```
Run the installer. It is recommended that you use the Python version that you want to use for running Launcher. The installer will check wether the running Python version satisfies the Launcher prerequisites. It will also build a Launcher startup script that references the python you use for the installer. If this python does not meet the prerequisites, that script may fail.
```
Ubuntu$ python installer.py
```
or, if python is not in your path:
```
Ubbuntu$ /home/*user_name*/Enthought/Canopy_64bit/User/bin/python installer.py
```
The installer produces a report that you should check to assure that the installation was successful. If so, you should have a Launcher folder in your home folder, which serves as the default location for log files, and for storing the files of your jobs (PBS script, input files, output files). It also contains a shell script launcher.sh that you can use to start Launcher. 

The temporary folder can be removed now.
```
$ cd ..
$ del some_tmp_dir
```

Happy Launching!

###Installing Launcher - MacOSX
This is identical to the Linux version. The only difference is that the path to Canopy Express python is /Users/*user_name*/Library/Enthought/Canopy_64bit/User/bin/.

###Installing updates
Updates can be installed installed from the Launcher menu "Launcher/Check for updates". 
