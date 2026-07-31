@echo off
setlocal
pushd "%~dp0"

set "LOGGER=%~dp0calibration_logger.py"
if not exist "%LOGGER%" set "LOGGER=%~dp0tools\calibration_logger.py"
if not exist "%LOGGER%" (
  echo Calibration logger script was not found.
  echo Expected: "%~dp0tools\calibration_logger.py"
  pause
  exit /b 1
)

set "PYTHON_EXE="
for /f "delims=" %%I in ('where python 2^>nul') do if not defined PYTHON_EXE set "PYTHON_EXE=%%I"
if not defined PYTHON_EXE if exist "D:\app\Python\python.exe" set "PYTHON_EXE=D:\app\Python\python.exe"
if not defined PYTHON_EXE (
  echo Python 3 was not found. Install Python and add it to PATH.
  pause
  exit /b 1
)

for %%I in ("%PYTHON_EXE%") do set "PYTHONW_EXE=%%~dpIpythonw.exe"
if not exist "%PYTHONW_EXE%" set "PYTHONW_EXE=%PYTHON_EXE%"

"%PYTHON_EXE%" -c "import serial, openpyxl, tkinter" >nul 2>nul
if errorlevel 1 (
  echo Installing pyserial, openpyxl and pywin32...
  "%PYTHON_EXE%" -m pip install pyserial openpyxl pywin32
  if errorlevel 1 (
    echo Dependency installation failed.
    echo Run: "%PYTHON_EXE%" -m pip install pyserial openpyxl pywin32
    pause
    exit /b 1
  )
)

start "" /D "%~dp0" "%PYTHONW_EXE%" "%LOGGER%"
if errorlevel 1 (
  echo Failed to start calibration logger.
  pause
  exit /b 1
)

popd
endlocal
