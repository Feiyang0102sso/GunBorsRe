@echo off
rem Open the real store using BIG templates and a separate writable demo profile.
cd /d "%~dp0"
"bin\x64\Release\gun_bros_re.exe" --game --skip-intro --menu-page 2 --profile "out\store-template-demo.dat" --mute
