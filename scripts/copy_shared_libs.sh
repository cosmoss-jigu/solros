#!/bin/bash

PROJECT_ROOT=$HOME/workspace/pcie-cloud/

SILO_DIR=$PROJECT_ROOT/silo/third-party/lz4
LIBLZ4=liblz4.so 

MIC_LIB_DIR=/opt/intel/composerxe/lib/mic/
MIC_SO=( libimf.so libsvml.so libirng.so libintlc.so.5 libiomp5.dbg libiomp5.so  )
MKL_LIB_DIR=/opt/intel/composerxe/mkl/lib/mic/
MKL_SO=(libmkl_intel_lp64.so libmkl_intel_thread.so libmkl_core.so )

#make lib dir first
ssh mic0 'mkdir ~/lib'

#copy libz
scp $SILO_DIR/$LIBLZ4 mic0:~/lib/

#copy intel math lib
for f in "${MIC_SO[@]}"
do
	scp  $MIC_LIB_DIR/$f mic0:~/lib/
done

#copy intel math lib
for f in "${MKL_SO[@]}"
do
	scp  $MKL_LIB_DIR/$f mic0:~/lib/
done
################
#copy gdb rpm
################
#scp ~/mpss-3.5/k1om/gdb-7.6.50+mpss3.5-1.k1om.rpm mic0:~/
#scp ~/mic0-single.fio mic0:~/


#make new profile for bash
cat > ~/temp.profile <<EOF
#
# profile for mic test
#
PS1='[\u@\h \W]\$ '
export PATH=/usr/bin:/usr/sbin:/bin:/sbin:$HOME/bin
export LD_LIBRARY_PATH=$LD_LIBRARY_PATH:$HOME/lib
EOF

scp ~/temp.profile mic0:~/.profile
rm ~/temp.profile

