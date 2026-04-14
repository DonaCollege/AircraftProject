@echo off
SET /A "index = 1"
SET /A "count = 2500"
:while
if %index% leq %count% (
    START /MIN ClientApp.exe 192.168.1.100 54000 katl-kefd-B737-700.txt
    SET /A index = %index% + 1
    goto :while
)