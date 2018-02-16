#!/bin/bash

vmname=test1
release=xenial

if [[ $(uvt-simplestreams-libvirt query) ]]; then
    echo "cloud already exists"
else
    echo "Syncing cloud image"
    uvt-simplestreams-libvirt sync release=$release arch=amd64
fi

echo "checking if VM created"

if [[ $(uvt-kvm list | grep $vmname) ]]; then
    echo "$vmname already exists"
else
    echo "Creating VM $vmname"
    uvt-kvm create $vmname release=$release
    echo "Waiting for $vmname"
    uvt-kvm wait $vmname --insecure
fi

echo "Connecting to VM $vmname"
uvt-kvm ssh $vmname --insecure

