PWD=`pwd`
echo $PWD
export GOPATH=$PWD
go build -o bin/main  snodl/main
