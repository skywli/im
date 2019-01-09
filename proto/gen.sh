#!/bin/bash

buildall()
{
	SRC_DIR=./
	DST_DIR=./bin
	if [ ! -f ${DST_DIR} ]; then
		echo "删除目录" $DST_DIR
		rm ${DST_DIR} -rf
	fi

	mkdir ${DST_DIR}

	if [[ "${1}" = "yes" ]]; then
		#C++
		mkdir -p $DST_DIR/cpp
		protoc -I=$SRC_DIR --cpp_out=$DST_DIR/cpp/ $SRC_DIR/IM.*.proto
		#JAVA
		mkdir -p $DST_DIR/java
		protoc -I=$SRC_DIR --java_out=$DST_DIR/java/ $SRC_DIR/IM.*.proto
		#PYTHON
		mkdir -p $DST_DIR/python
		protoc -I=$SRC_DIR --python_out=$DST_DIR/python/ $SRC_DIR/IM.*.proto
	else
		#C++
		mkdir -p $DST_DIR/cpp
		protoc -I=$SRC_DIR --cpp_out=$DST_DIR/cpp/ $SRC_DIR/*.proto
		#JAVA
		mkdir -p $DST_DIR/java
		protoc -I=$SRC_DIR --java_out=$DST_DIR/java/ $SRC_DIR/*.proto
		#PYTHON
		mkdir -p $DST_DIR/python
		protoc -I=$SRC_DIR --python_out=$DST_DIR/python/ $SRC_DIR/*.proto

		if [ "${2}" = "yes" ];
		then
		    if [ ! -d ../src/common/protobuf/ ]; then
			echo "创建目录"
			mkdir -p ../src/common/protobuf/
		    else
			echo "清空目录"
			rm ../src/common/protobuf/*
		    fi		

		    cp $DST_DIR/cpp/* ../src/common/protobuf/
		    echo "拷贝到源码目录"

		fi

	fi	

	#php version
	#the php script should fork from my own repo.
	#since I fix some bug for php compiler.
	#https://github.com/rotencode/php-protobuf
	#mkdir -p $DST_DIR/php
	#files=`find ./ -name "*.proto"`
	#for file in $files
	#do
	#    echo $file
	#    php ./php/protoc-php.php -t $DST_DIR/php $file
	#done

	#if [ "${1}" = "rm" ];
	#then
	#  rm ../src/proto/*
	#fi
}

howto()
{
    echo "    ./gen.sh [option]"
    echo "            -a    仅编译app所需要版本接口."
    echo "            -c    拷贝到源码目录[服务器版本才能使用]."
    echo "            -s    编译服务器完整版."
}


is_app="no"
is_server="no"
is_copy="no"

cd `dirname $0`

# echo "OPTIND starts at $OPTIND"
while getopts ":acsh" optname
do
    case "$optname" in
    "h")
        howto
        exit 1
        ;;
    "a")
        echo "Option $optname is specified"
        is_app="yes"
        ;;
    "c")
        echo "Option $optname is specified"
        is_copy="yes"
        ;;
    "s")
        echo "Option $optname has value $OPTARG"
        is_server="yes"
        ;;
    "?")
        howto
        exit 1
        echo "Unknown option $OPTARG"
        ;;
    ":")
        echo "No argument value for option $OPTARG"
        ;;
    *)
        # Should not occur
        echo "Unknown error while processing options"
        ;;
    esac
    echo "OPTIND is now $OPTIND"
done

if [ "$is_app" == "no" ] && [ "$is_copy" == "no" ] && [ "$is_server" == "no" ]; then
	howto
	exit 1
else
	buildall "${is_app}" "${is_copy}"
fi

