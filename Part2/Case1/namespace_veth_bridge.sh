#!/bin/bash

# Function to clean up previous configurations
cleanup() {
    echo "Cleaning up namespaces and interfaces..."

    # Delete the namespaces
    ip netns del NS1 2>/dev/null
    ip netns del NS2 2>/dev/null
    ip netns del NS3 2>/dev/null

    # Delete veth interfaces
    ip link del veth1 2>/dev/null
    ip link del veth2 2>/dev/null
    ip link del veth3 2>/dev/null
    ip link del veth4 2>/dev/null
    
}

# Function to create namespaces, veth interfaces, and bridge
create() {
    echo "Creating namespaces and interfaces..."

    # Create namespaces
    ip netns add NS1
    ip netns add NS2
    ip netns add NS3

    # Create veth interfaces for NS1 and NS2
    ip link add veth1 type veth peer name veth2
    ip link add veth3 type veth peer name veth4

    # Assign veth interfaces to namespaces
    ip link set veth1 netns NS1
    ip link set veth4 netns NS3
    ip link set veth2 netns NS2
    ip link set veth3 netns NS2

    # Configure IP addresses for veth interfaces in namespaces
    ip netns exec NS1 ip addr add 192.168.1.1/24 dev veth1
    ip netns exec NS2 ip addr add 192.168.1.2/24 dev veth2
    ip netns exec NS2 ip addr add 192.168.1.3/24 dev veth3
    ip netns exec NS3 ip addr add 192.168.1.4/24 dev veth4


    # Create a bridge in NS2 (NS2 acts as the bridge)
    ip netns exec NS2 brctl addbr br0
    ip netns exec NS2 brctl addif br0 veth2
    ip netns exec NS2 brctl addif br0 veth3

    # Bring up interfaces in namespaces
    ip netns exec NS1 ip link set veth1 up
    ip netns exec NS2 ip link set veth2 up
    ip netns exec NS2 ip link set veth3 up
    ip netns exec NS3 ip link set veth4 up

    # Bring up the bridge interface
    ip netns exec NS2 ip link set br0 up

    ip netns exec NS3 tc qdisc add dev veth4 root netem delay 10ms loss 1%

    echo "Setup complete!"
}

# Main script execution
case "$1" in
    create)
        create
        ;;
    cleanup)
        cleanup
        ;;
    *)
        echo "Usage: $0 {create|cleanup}"
        exit 1
        ;;
esac
