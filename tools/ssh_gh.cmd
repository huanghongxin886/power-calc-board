@echo off
rem git ssh wrapper: bypass MSYS sh (sandbox blocks its signal pipe)
"C:\Windows\System32\OpenSSH\ssh.exe" -o StrictHostKeyChecking=no -o UserKnownHostsFile=NUL -o BatchMode=yes -o ConnectTimeout=20 %*
