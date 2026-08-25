#!/bin/bash

DST=$(dirname -- "$(realpath -- "${BASH_SOURCE[0]}")")

files=(
  lmdb.h mdb.c midl.h midl.c mdb.c module.c
)

mkdir -p $DST && cd $DST

force="${1:-}"

for f in ${files[@]}
do
  if [ "$force" != "" ] || [ ! -f $f ];
  then
    echo "Download $f ..."
    curl -sL "https://raw.githubusercontent.com/LMDB/lmdb/refs/heads/mdb.master3/libraries/liblmdb/$f" -o $f
  fi
done

if [ ! -f mongoose.h ];
then
  curl -sL https://raw.githubusercontent.com/cesanta/mongoose/b1c2ffe1a0aa13e3d94075b1a2c66b8b43ac9116/mongoose.h -o mongoose.h
fi

if [ ! -f mongoose.c ];
then
  curl -sL https://raw.githubusercontent.com/cesanta/mongoose/b1c2ffe1a0aa13e3d94075b1a2c66b8b43ac9116/mongoose.c -o mongoose.c
fi

if [ ! -f parson.h ];
then
  curl -sL https://raw.githubusercontent.com/kgabis/parson/master/parson.h -o parson.h
fi

if [ ! -f parson.c ];
then
  curl -sL https://raw.githubusercontent.com/kgabis/parson/master/parson.c -o parson.c
fi

sed -i '1098s/mp_ptrs\[0\]/mp_ptrs[]/' mdb.c
sed -i '1108s/mp2_ptrs\[0\]/mp2_ptrs[]/' mdb.c
sed -i '6442s/int excl/int __attribute__((unused)) excl/' mdb.c
sed -i '12621s/*env/__attribute__((unused)) *env/' mdb.c
sed -i '2664s/int rc/int rc __attribute__((unused))/' mdb.c
sed -i '3837s/int count/int count __attribute__((unused))/' mdb.c
sed -i '12111s/*func/__attribute__((unused)) *func/' mdb.c
sed -i '7216s/id3.mptr +/(char *)id3.mptr +/' mdb.c
sed -i '4253s/iov\[0\].iov_base +=/iov[0].iov_base = (char *)iov[0].iov_base +/' mdb.c
sed -i '54s/hookfunc/*(void **)(\&hookfunc)/' module.c
