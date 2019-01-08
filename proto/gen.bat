

if exist %~dp0bin\ (
    echo "已经存在文件夹"
) else (
    md %~dp0bin\
)

protoc -I=%~dp0 --java_out=%~dp0bin\ %~dp0IM.*.proto

echo "生成文件目录到当前目录的bin目录中"

pause