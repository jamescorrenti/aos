#!/bin/bash

vmname=test
release=xenial

if [[ $(uvt-simplestreams-libvirt query) ]]; then
    echo "cloud already exists"
else
    echo "Syncing cloud image"
    uvt-simplestreams-libvirt sync release=$release arch=amd64
fi

echo "checking if VM created"

for((i=0; i <8; i++))
do
    if [[ $(uvt-kvm list | grep $vmname$i) ]]; then
        echo "$vmname already exists"
    else
        echo "Creating VM $vmname$i"
        uvt-kvm create $vmname$i release=$release
        echo "Waiting for $vmname$i"
        uvt-kvm wait $vmname$i --insecure
	virsh shutdown $vmname$i
    fi
done
#echo "Connecting to VM $vmname"
#uvt-kvm ssh $vmname --insecure

