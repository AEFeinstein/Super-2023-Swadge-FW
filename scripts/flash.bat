cd "%~dp0"
cd ..
idf.py -p %SWADGE_COM_PORT% -b 2000000 build flash monitor
