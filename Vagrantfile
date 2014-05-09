# -*- mode: ruby -*-
# vi: set ft=ruby :

# Yves Hwang
# yveshwang@gmail.com
# 16.04.2014

VAGRANTFILE_API_VERSION = "2"

$centos5_script = <<SCRIPT
    rpm --nosignature -Uvh http://dl.fedoraproject.org/pub/epel/5/x86_64/epel-release-5-4.noarch.rpm
    rpm --nosignature -Uvh http://repo.varnish-cache.org/redhat/varnish-3.0/el5/noarch/varnish-release/varnish-release-3.0-1.el5.centos.noarch.rpm
    yum install varnish -y
    yum --disablerepo=epel install varnish-libs-devel -y
    yum install curl -y
    yum install pkgconfig -y
    yum install yum-utils -y
    yum install m4 -y
    yum install libmicrohttpd-devel -y
    yum install curl-devel -y
    yum install python-docutils -y
    yum install perl-libwww-perl -y
    yum install python-demjson -y
    yum install nc -y
    yum install git -y
    curl ftp://ftp.gnu.org/gnu/autoconf/autoconf-2.63.tar.gz -O
    tar xf autoconf-2.63.tar.gz
    mkdir aconf
    cd autoconf-2.63
    ./configure --prefix=/home/vagrant/aconf/
    make
    make install
    echo "export PATH=/home/vagrant/aconf/bin:/usr/sbin:$PATH" >> /home/vagrant/.bash_profile
    source /home/vagrant/.bash_profile
    cd ..
    curl ftp://ftp.gnu.org/gnu/automake/automake-1.12.tar.gz -O
    tar xf automake-1.12.tar.gz
    cd automake-1.12
    ./configure --prefix=/home/vagrant/aconf/
    make
    make install
SCRIPT

$precise64_script = <<SCRIPT
    sudo apt-get update
    sudo apt-get install build-essential automake pkg-config devscripts equivs -y
SCRIPT
Vagrant.configure(VAGRANTFILE_API_VERSION) do |config|
    config.vm.define :centos5 do |centos5|
        centos5.vm.box = "centos5"
        centos5.vm.box_url = "http://tag1consulting.com/files/centos-5.8-x86-64-minimal.box"
        centos5.vm.provision :shell, :inline => $centos5_script
    end

    config.vm.define :precise64 do |precise64|
        precise64.vm.box = "precise64"
        precise64.vm.box_url = "http://files.vagrantup.com/precise64.box"
        precise64.vm.provision :shell, :inline => $precise64_script
        # once in the vm, use the following if you wanna install all dependencies via the controlfile
        # sudo mk-build-deps --install /vagrant/debian/control 
    end
end

