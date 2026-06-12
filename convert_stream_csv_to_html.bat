@echo off
setlocal EnableExtensions

set "ROOT=%~dp0"
set "PYTHON=python"

echo.
echo MotemaSens CSV to HTML plot converter
echo -------------------------------------
echo.

if "%~1"=="" (
  set /p "CSV_FILE=Enter CSV file path to convert: "
) else (
  set "CSV_FILE=%~1"
)

set "CSV_FILE=%CSV_FILE:"=%"

if "%CSV_FILE%"=="" (
  echo No CSV file entered.
  pause
  exit /b 1
)

if not exist "%CSV_FILE%" (
  echo CSV file not found:
  echo %CSV_FILE%
  pause
  exit /b 1
)

for %%F in ("%CSV_FILE%") do (
  set "CSV_DIR=%%~dpF"
  set "CSV_NAME=%%~nF"
)

set "OUT_FILE=%CSV_DIR%%CSV_NAME%_plot.html"

echo.
echo Input : %CSV_FILE%
echo Output: %OUT_FILE%
echo.

%PYTHON% "%ROOT%tools\csv_log_to_html.py" "%CSV_FILE%" -o "%OUT_FILE%"
if errorlevel 1 (
  echo.
  echo Failed to create HTML plot.
  pause
  exit /b 1
)

echo.
echo Opening plot in browser...
start "" "%OUT_FILE%"
echo Done.
pause
