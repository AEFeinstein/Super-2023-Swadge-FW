# Delete everything in ~\esp\, except the firmware repo
#Get-ChildItem -Path  '~\esp\' -Recurse |
#Select -ExpandProperty FullName |
#Where {$_ -notlike '~\esp\Super-2023-Swadge-FW*'} |
#sort length -Descending |
#Remove-Item -force

# Only delete esp-idf\ in ~\esp\
$FileName = "~\esp\esp-idf\"
if (Test-Path $FileName) {
    Remove-Item $FileName -Force -Recurse
}

# Delete ~\.espressif\ and everything in it
$FileName = "~\.espressif\"
if (Test-Path $FileName) {
    Remove-Item $FileName -Force -Recurse
}

# Re-run the esp-idf setup
cd $PSScriptRoot
.\setup-esp-idf.ps1
