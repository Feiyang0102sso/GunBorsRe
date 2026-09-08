@echo off
rem Silent gameplay; resource parsing and game simulation remain enabled.
cd /d "%~dp0"
"bin\x64\Release\gun_bros_re.exe" --game --mute %*
