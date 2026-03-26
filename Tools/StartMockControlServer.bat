@echo off
set URL=http://127.0.0.1:8080/ui
set NODE_EXE=C:\Program Files\nodejs\node.exe
set SERVER_JS=F:\FoX_CodeX\Designer_Base\Tools\MockControlServerNode\server.js

powershell -NoProfile -Command "try { $r = Invoke-WebRequest -UseBasicParsing '%URL%' -TimeoutSec 2; if ($r.StatusCode -eq 200) { exit 0 } else { exit 1 } } catch { exit 1 }"
if %ERRORLEVEL% EQU 0 (
  echo Mock control server is already running.
  start "" "%URL%"
  exit /b 0
)

if not exist "%NODE_EXE%" (
  echo Node.js was not found at %NODE_EXE%
  echo Please install Node.js or update StartMockControlServer.bat.
  exit /b 1
)

title Designer Mock Control Server
cd /d "F:\FoX_CodeX\Designer_Base\Tools\MockControlServerNode"
start "Designer Mock Control Server" "%NODE_EXE%" "%SERVER_JS%"
timeout /t 2 >nul
start "" "%URL%"
