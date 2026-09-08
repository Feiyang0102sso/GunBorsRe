@echo off
rem Start the rebuilt game from its resource root.
cd /d "%~dp0"
"bin\x64\Release\gun_bros_re.exe" --game %*
