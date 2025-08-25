@echo off
setlocal enabledelayedexpansion

REM 遍历当前目录下所有 .png 文件
for %%f in (*.png) do (
    REM 获取不带扩展名的文件名
    set "filename=%%~nf"
    echo 正在处理: %%f
    REM 执行 magick 命令
    magick "%%f" -define bmp:header=false -depth 16 -endian LSB RGB:!filename!.bin
)

echo 所有 .png 文件处理完成。
pause