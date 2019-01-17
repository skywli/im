#!/bin/bash
#生成pb对象的脚本

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
		#GO
		mkdir -p $DST_DIR/go
		protoc -I=$SRC_DIR --go_out=$DST_DIR/go/ $SRC_DIR/IM.*.proto
	else
		#GO
		mkdir -p $DST_DIR/go
		protoc -I=$SRC_DIR --go_out=$DST_DIR/go/ $SRC_DIR/*.proto

		if [ "${2}" = "yes" ];
		then

            #go
		    if [ ! -d ../src/server/go/protobuf/ ]; then
			echo "创建目录"
			mkdir -p ../src/server/go/protobuf/
		    else
			echo "清空目录"
			rm ../src/server/go/protobuf/*
		    fi		

		    cp $DST_DIR/go/* ../src/server/go/protobuf/
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

