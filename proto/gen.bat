

if exist %~dp0bin\ (
    echo "�Ѿ������ļ���"
) else (
    md %~dp0bin\
)

protoc -I=%~dp0 --java_out=%~dp0bin\ %~dp0IM.*.proto

echo "�����ļ�Ŀ¼����ǰĿ¼��binĿ¼��"

pause