# Install Visual C++ Build Tools
Invoke-WebRequest -Uri https://aka.ms/vs/17/release/vs_buildtools.exe -Outfile vs_buildtools.exe
.\vs_buildtools.exe --add Microsoft.VisualStudio.Component.VC.Tools.x86.x64 --passive --norestart --theme Dark --wait
Remove-Item vs_buildtools.exe
