# alt+F4 will still quit the program, but crashes will restart it

while($true)
{
    .\swadge_emulator.exe --start-mode "Swadge Bros" --lock --fullscreen
    if(0 -eq $LASTEXITCODE)
    {
        return 0;
    }
}
