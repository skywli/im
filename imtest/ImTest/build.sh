PWD=`pwd`
echo $PWD
export GOPATH=$PWD:$PWD/../httpTest:$PWD/../gopkg
go install main
