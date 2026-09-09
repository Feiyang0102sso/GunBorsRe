@echo off
rem Keep every milestone available through the permanent executable menu.
cd /d "%~dp0"
"bin\x64\Release\gun_bros_research.exe" --mute --research %*
