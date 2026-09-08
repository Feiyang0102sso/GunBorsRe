@echo off
rem Use the independent imported profile, never write to the source saves.
cd /d "%~dp0"
"bin\x64\Release\gun_bros_re.exe" --original-profile --mute %*
