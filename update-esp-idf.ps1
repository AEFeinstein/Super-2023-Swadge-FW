# Delete everything in ~\esp\, except the firmware repo
#Get-ChildItem -Path  '~\esp\' -Recurse |
#Select -ExpandProperty FullName |
#Where {$_ -notlike '~\esp\Super-2023-Swadge-FW*'} |
#sort length -Descending |
#Remove-Item -force

# Only delete esp-idf\ in ~\esp\
Remove-Item ~\esp\esp-idf\ -Force -Recurse

# Delete ~\.espressif\ and everything in it
Remove-Item ~\.espressif\ -Force -Recurse

# Re-run the esp-idf setup
.\setup-esp-idf.ps1
