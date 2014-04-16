# DOCKER-VERSION 0.9.1
# Docker file for varnish-agent

# Yves Hwang
# yveshwang@gmail.com
# 16.04.2014

FROM tianon/centos:5.8
MAINTAINER Yves Hwang <yves@varnish-software.com>

# install EPEL
RUN rpm --nosignature -Uvh http://dl.fedoraproject.org/pub/epel/5/x86_64/epel-release-5-4.noarch.rpm

# install varnish repo
RUN rpm --nosignature -Uvh http://repo.varnish-cache.org/redhat/varnish-3.0/el5/noarch/varnish-release/varnish-release-3.0-1.el5.centos.noarch.rpm

RUN yum update -y
RUN yum upgrade -y
RUN yum install git -y
RUN yum install varnish -y
RUN yum install varnish-agent -y
RUN yum install nc -y

RUN mkdir TEST
RUN cd TEST && git clone https://github.com/varnish/vagent2.git
WORKDIR TEST/vagent2
RUN git pull
RUN ls -al
RUN pwd
ENTRYPOINT ./tests/integration/test.sh
