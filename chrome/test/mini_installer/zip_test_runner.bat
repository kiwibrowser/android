@echo off

rem This batch file installs all Python packages needed for the test
rem and automatically runs the test. All args passed into this batch file
rem will be passed to test_installer.py. See
rem chrome\test\mini_installer\test_installer.py for the args that can be
rem passed.

c:\Python27\python -m pip install --upgrade pip
c:\Python27\Scripts\pip install psutil
c:\Python27\Scripts\pip install pywin32
c:\Python27\python chrome\test\mini_installer\test_installer.py {run_args} %*
pause