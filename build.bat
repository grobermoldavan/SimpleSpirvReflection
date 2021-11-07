@echo off

call vcvars64

call cl ./test/main.c /TC /I"%VK_SDK_PATH%\Include" /link /DEBUG:FULL

pause