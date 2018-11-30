# SOLROS: A Data-Centric Operating System Architecture for Heterogeneous Computing (EuroSys'18)

## How to build host kernel
  - cd host-kernel
  - make menuconfig
  - make; make modules
  - sudo make install modules_install headers_install

## How to build phi kernel
  - make phi-kenel

## How to build mpss host modules
  - make host-mpss

## How to build modified nvme driver
  - make nvmep2p

## How to build diod 9p server
  - make diod

## How to install script for xeon phi
  - Put your file at ./scritpt with '_mic' suffix
  - Files are copied to /usr/bin in mic by running 'make phi-install' or 'make
  phi-kernel'
  - Since all files are part of initramfs of xeon phi, mic needs to be
  rebooted.

## How to use
```
  Usage: ctrl.py [options]

  Options:
    -h, --help     show this help message and exit
    --verbose      print out log
    --dryrun       dry run
    --debug=DEBUG  debug out for {host:mic}:{fs:net}
  > boot                : Boot all MICs
  > fs_down             : Umount 9p for all MICs
  > fs_up               : Mount 9p for all MICs
  > net_down            : Shutdown network for all MICs
  > net_up              : Bring up network for all MICs: {sock* | mtcp}
  > reboot              : Reboot all MICs
  > show_config         : Show current configuration
  > shutdown            : Shutdown all MICs and relevant services
```

## Xeon Phi root password
  - phi
  - sshpass -p phi ssh root@mic0
    or simply
    scripts/mic-ssh mic0

## Authors
  - Changwoo Min changwoo@vt.edu
  - Woonhak Kang woonhak.kang@gatech.edu
  - Mohan Kumar mohankumar@gatech.edu
  - Sanidhya Kashyap sanidhya@gatech.edu
  - Steffen Maass steffen.maass@gatech.edu
  - Heeseung Jo heeseung@jbnu.ac.kr
  - Taesoo Kim taesoo@gatech.edu

## Citation
```
@inproceedings{min:solros,
  title        = {{SOLROS: A Data-Centric Operating System Architecture for Heterogeneous Computing}},
  author       = {Changwoo Min and Woon-Hak Kang and Mohan Kumar and Sanidhya Kashyap and Steffen Maass and Heeseung Jo and Taesoo Kim},
  booktitle    = {Proceedings of the 13th European Conference on Computer Systems (EuroSys)},
  month        = apr,
  year         = 2018,
  address      = {Porto, Portugal},
}
```
